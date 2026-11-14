#include "pch.h"
#include "labtests.h"
#include "gamepatches.h"
#include "gamepads.h"
#include "config.h"
#include "film.h"
#include "log.h"
#include <cmath>
#include <cstdio>
#include <vector>

// Conceptual branch only (docs\CONCEPTUAL.md): three offline tests, active only inside a Lab > Test Arsenal battle.
//   F6, or View + D-pad Left:  camera: own view -> follow each other player -> own view
//   F7, or View + D-pad Up:    record: start / stop saving every player's position and hand to LocalState\pdnet\labtest.rec
//   F8, or View + D-pad Right: play back: both players are driven only by that file, as network players are. Nobody is
//                              under local control.
//
// What it rests on (Ghidra, PDUWP.exe):
// - FUN_14012bfc0 (one call, at 0x14017d00e) builds the match view from the local player's object: view matrix +0x13f50,
//   eye +0x13f2c. If DAT_140534798 is set (the game's cutscene camera) it uses eye DAT_14059e150, target DAT_14059e130.
//   HookedCamera sets all three only for that call, so nothing else sees them.
// - FUN_14024ec80 (a player's update): object +0x34 == 0 means the player is driven by received data
//   (FUN_14018f330 on object +0x16900), otherwise locally, by a person or (+0x30 == 0) the AI.
// - Stream 0 PlayerPos: FUN_1402502a0(object, 40-byte out) makes it, receive FUN_14027c4f0(slot, state) with state +0x20 =
//   frame / 2 and +0x40 = the data. Stream 1 CardKind: the four hand slots, object +0x3b8 + i * 0x50 (-0x28: slot in use),
//   receive FUN_14027c610 with the same state. Frame counter: DAT_1404bc63c.
namespace pdnet::labtests
{
    namespace
    {
        constexpr uint64_t kGhidraBase = 0x140000000;
        uint8_t* At(uint64_t ghidra) { return reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr)) + (ghidra - kGhidraBase); }
        template <typename T> T& Var(uint64_t ghidra) { return *reinterpret_cast<T*>(At(ghidra)); }

        uint8_t* PlayerObject(int slot) { return slot >= 0 && slot < 4 ? reinterpret_cast<uint8_t**>(At(0x1404bc9d8))[slot] : nullptr; }
        uint8_t* PlayerRecord(int slot) { return slot >= 0 && slot < 4 ? reinterpret_cast<uint8_t**>(At(0x1404bc608))[slot] : nullptr; }
        int LocalSlot() { return Var<int32_t>(0x1404bbe00); }
        int FrameNow() { return Var<int32_t>(0x1404bc63c); }
        template <typename T> T& Field(uint8_t* object, size_t offset) { return *reinterpret_cast<T*>(object + offset); }

        struct Vec { float x, y, z; };
        Vec Sub(Vec a, Vec b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
        Vec Add(Vec a, Vec b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
        Vec Scale(Vec a, float k) { return { a.x * k, a.y * k, a.z * k }; }
        float Dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
        float Length(Vec a) { return std::sqrt(Dot(a, a)); }
        bool Finite(Vec a) { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }

        constexpr size_t kPosition = 0x12e40, kEye = 0x13f2c, kView = 0x13f50;
        constexpr size_t kRing = 0x16900, kRingPlayer = 0x20, kDriven = 0x34;

        struct Frame
        {
            uint32_t tick;
            uint32_t slots; // bit N: slot N is in this frame
            uint8_t position[4][40];
            int16_t hand[4][4];
        };
        constexpr uint32_t kFileMagic = 0x31524450; // "PDR1"
        constexpr size_t kMaxFrames = 60 * 60 * 5;  // five minutes at 60 frames a second

        std::atomic<bool> g_pressCamera{ false }, g_pressRecord{ false }, g_pressPlay{ false };
        int g_viewSlot = -1; // -1: the game's own view
        bool g_haveOffset = false;
        Vec g_eyeOffset{}, g_targetOffset{}; // from the local player, kept for when their own camera stops updating
        int g_lastFrame = -1;
        DWORD g_lastReport = 0;
        bool g_wasInBattle = false;

        std::vector<Frame> g_recording;
        bool g_recordingOn = false;
        DWORD g_recordStart = 0;

        std::vector<Frame> g_playback;
        size_t g_playAt = 0;
        bool g_playing = false;
        int32_t g_savedDriven[4]{};
        DWORD g_playStart = 0;
        int g_playFaults = 0;

        std::wstring RecordingPath() { return LocalStatePath() + L"\\pdnet\\labtest.rec"; }

        bool InMatchNow()
        {
            int32_t state = Var<int32_t>(0x1404bc950);
            return state != 0 && state != 13 && PlayerObject(LocalSlot());
        }
        bool InBattle() { return gamepatches::TestBattle() && InMatchNow(); }

        // ---- dead camera (any match) ----
        // While this PC's player is down (health, status record +0xc, at 0 or below), the view follows another player and the
        // HUD shows theirs; or a free camera. In Tag Team only the partner can be followed (no calling out the other team's
        // hands over team voice); elsewhere anyone still standing. A revive gives the player their own view back.
        //   Keyboard (pdlaunch's keys): Left / Right arrow: previous / next player; Up arrow: free camera on / off;
        //   free camera: W A S D move, Q E turn, R F up / down.
        //   Controller: D-pad Left / Right: previous / next; D-pad Up: free camera; free camera: left stick moves,
        //   right stick turns, LB / RB down / up.
        // In a Test Arsenal battle F9 (or View + D-pad Down) pretends the player is down, to try it against the AI.
        std::atomic<bool> g_deadNext{ false }, g_deadPrevious{ false }, g_deadFree{ false }, g_pretendDown{ false };
        bool g_deadOn = false, g_freeCam = false;
        int g_deadTarget = -1;
        Vec g_freePos{};
        float g_freeYaw = 0, g_freePitch = 0;
        // Following: the view stays on the watched player, as a player's own camera does, but can be swung round them and
        // up / down (right stick, or Q E and R F); a click of the right stick (or C) puts it back behind them.
        float g_orbitYaw = 0, g_orbitPitch = 0;
        ULONGLONG g_orbitLast = 0;

        // Theatre film controls (the user's Halo 3 film-controls card, 2026-11-14), each with a key:
        //   A / Space          play / pause              LT / Z (hold)           slow motion
        //   RT / C (hold)      fast forward
        //   Y / V              flying cam / player cam   B / H                   hide / show HUD
        //   D-pad Down / F2    hide / show the film text D-pad L R / Q E         switch players (player cam)
        //   (the film text: the film bar and these reminders together, apart from the HUD; the user, 2026-11-14)
        //   left stick / WASD  move (flying cam)         right stick / arrows    look, or orbit (player cam; R3 / T resets)
        //   RB LB / R F        up / down (flying cam)    (no thrust: the user, 2026-11-14)
        //   L3 / Ctrl (hold)   pan: the stick's forward / back moves up / down (flying cam)
        // Time: the game's update step (HookedStep) is skipped while paused, run on some frames only in slow motion and run
        // several times a frame fast forwarding; the film clock (g_filmClock, ms) runs at the same rate.
        bool g_filmPaused = false, g_hudHidden = false, g_controlsShown = true;
        float g_filmRate = 1.0f, g_slowAccumulator = 0;
        double g_filmClock = 0;
        bool g_stepHooked = false;
        ULONGLONG g_freeLast = 0;
        Vec g_lastEye{}, g_lastLook{};
        bool g_haveLast = false;

        int32_t Health(int slot) { uint8_t* record = PlayerRecord(slot); return record ? Field<int32_t>(record, 0xc) : 0; }

        // ---- Theatre playback ----
        // A queued film (QueueFilm) plays in the next Test Arsenal battle: from its start every player is driven by received
        // data (object +0x34 = 0, as in the F8 test) and the film's stream records are handed to the game's receive code at
        // their recorded times. Film slots map onto the battle's in order (the battle has slots 0 and 1). For now only
        // PlayerPos (movement, pose) and CardKind (hands) are played; the other streams (skills, hits...) are counted.
        std::vector<film::Event> g_filmEvents;
        film::Info g_filmInfo;
        bool g_filmQueued = false, g_filmPlaying = false, g_filmShown = false; // shown: playing or played in this battle

        // A film's battle is running (match state 6). The film's controls and time control work only then: after it, A on
        // the results screen kept pausing the game's update and it hung on a black screen (Conceptual 36).
        bool FilmActive() { return g_filmShown && Var<int32_t>(0x1404bc950) == 6; }
        size_t g_filmAt = 0;
        ULONGLONG g_filmStart = 0;
        uint32_t g_filmBase = 0;
        int g_filmSlot[256];
        uint32_t g_filmSkipped[26]{};
        uint32_t g_filmPlayed[26]{};
        bool g_filmStreamOff[26]{}; // streams left out after a fault
        // Sequenced streams are resent until acknowledged (and sent to each peer), so a film holds copies: a film slot's
        // stream is skipped when it repeats the last one played exactly (its number isn't always new: DmgEffect keeps one
        // number while its data changes).
        struct LastPlayed { bool any = false; uint32_t number = 0; std::vector<uint8_t> data; };
        LastPlayed g_filmLast[4][26];

        bool LocalDown()
        {
            if (g_filmShown && gamepatches::TestBattle()) return true; // watching a film: the dead camera's controls
            if (g_pretendDown && gamepatches::TestBattle()) return true;
            return Health(LocalSlot()) <= 0;
        }

        // Game type 1 = Tag Team (0x1405277b4, as the lobby has it); team = player object +0x1c.
        bool Followable(int slot)
        {
            int local = LocalSlot();
            uint8_t* object = PlayerObject(slot);
            if (g_filmShown) return object && Field<int32_t>(object, 0x20) != 0; // a film: anyone in it, this slot too
            if (slot == local || !object || Field<int32_t>(object, 0x20) == 0 || Health(slot) <= 0) return false;
            if (gamepatches::TestBattle() || Var<int32_t>(0x1405277b4) != 1) return true;
            uint8_t* mine = PlayerObject(local);
            return mine && Field<int32_t>(mine, 0x1c) == Field<int32_t>(object, 0x1c);
        }

        int NextFollowable(int from, int step)
        {
            for (int i = 1; i <= 4; ++i)
            {
                int s = ((from < 0 ? 0 : from) + step * i + 8) % 4;
                if (Followable(s)) return s;
            }
            return -1;
        }

        Vec FreeForward() { return { std::cos(g_freePitch) * std::sin(g_freeYaw), std::sin(g_freePitch), std::cos(g_freePitch) * std::cos(g_freeYaw) }; }

        void StartFreeCam()
        {
            g_freeCam = true;
            g_freeLast = GetTickCount64();
            if (g_haveLast)
            {
                g_freePos = g_lastEye;
                Vec d = Sub(g_lastLook, g_lastEye);
                float len = Length(d);
                if (len > 0.01f)
                {
                    d = Scale(d, 1.0f / len);
                    g_freeYaw = std::atan2(d.x, d.z);
                    g_freePitch = std::asin((std::max)(-1.0f, (std::min)(1.0f, d.y)));
                }
            }
            Log(L"deadcam: free camera");
        }

        void MoveFreeCam()
        {
            ULONGLONG now = GetTickCount64();
            float dt = (std::min)(0.1f, (now - g_freeLast) / 1000.0f);
            g_freeLast = now;
            float lx = 0, ly = 0, rx = 0, ry = 0, up = 0;
            PadSticks(lx, ly, rx, ry);
            if (AnyPadHolds(0x800)) up += 1;  // RB
            if (AnyPadHolds(0x400)) up -= 1;  // LB
            uint8_t keys[256];
            bool haveKeys = KeysHeld(keys);
            bool pan = false;
            if (g_filmShown)
            {
                pan = AnyPadHolds(0x1000); // L3
                if (haveKeys)
                {
                    if (keys[VK_RIGHT]) rx += 1;
                    if (keys[VK_LEFT]) rx -= 1;
                    if (keys[VK_UP]) ry += 1;
                    if (keys[VK_DOWN]) ry -= 1;
                    if (keys[VK_CONTROL] || keys[VK_LCONTROL]) pan = true;
                }
            }
            if (haveKeys)
            {
                if (keys['W']) ly += 1;
                if (keys['S']) ly -= 1;
                if (keys['D']) lx += 1;
                if (keys['A']) lx -= 1;
                if (!g_filmShown && keys['E']) rx += 1; // in a film, Q / E switch players and the arrows look
                if (!g_filmShown && keys['Q']) rx -= 1;
                if (keys['R']) up += 1;
                if (keys['F']) up -= 1;
            }
            auto dead = [](float v) { return std::fabs(v) < 0.2f ? 0.0f : v; };
            lx = dead(lx); ly = dead(ly); rx = dead(rx); ry = dead(ry);
            if (pan) { up += ly; ly = 0; } // pan: forward / back moves up / down
            float kSpeed = 150.0f;
            constexpr float kTurn = 2.0f;
            g_freeYaw += rx * kTurn * dt;
            g_freePitch = (std::max)(-1.4f, (std::min)(1.4f, g_freePitch + ry * kTurn * dt));
            Vec forward = FreeForward(), right = { std::cos(g_freeYaw), 0, -std::sin(g_freeYaw) };
            g_freePos = Add(g_freePos, Scale(forward, ly * kSpeed * dt));
            g_freePos = Add(g_freePos, Scale(right, lx * kSpeed * dt));
            g_freePos.y += up * kSpeed * dt;
        }

        void OrbitInput()
        {
            ULONGLONG now = GetTickCount64();
            float dt = g_orbitLast ? (std::min)(0.1f, (now - g_orbitLast) / 1000.0f) : 0.0f;
            g_orbitLast = now;
            float lx = 0, ly = 0, rx = 0, ry = 0;
            PadSticks(lx, ly, rx, ry);
            uint8_t keys[256];
            bool reset = AnyPadHolds(0x2000); // right stick click
            if (KeysHeld(keys))
            {
                if (g_filmShown)
                {
                    if (keys[VK_RIGHT]) rx += 1;
                    if (keys[VK_LEFT]) rx -= 1;
                    if (keys[VK_UP]) ry += 1;
                    if (keys[VK_DOWN]) ry -= 1;
                    reset = reset || keys['T'];
                }
                else
                {
                    if (keys['E']) rx += 1;
                    if (keys['Q']) rx -= 1;
                    if (keys['R']) ry += 1;
                    if (keys['F']) ry -= 1;
                    reset = reset || keys['C'];
                }
            }
            if (reset) { g_orbitYaw = g_orbitPitch = 0; return; }
            auto dead = [](float v) { return std::fabs(v) < 0.2f ? 0.0f : v; };
            constexpr float kTurn = 2.0f;
            g_orbitYaw += dead(rx) * kTurn * dt;
            g_orbitPitch = (std::max)(-0.6f, (std::min)(0.9f, g_orbitPitch + dead(ry) * kTurn * dt));
        }

        void UpdateDeadCamera()
        {
            // Only while the battle runs (match state 6): at its end the game moves on to its results (states 12, 9, 10, 11), and
            // the dead camera stayed on over them until the next match (2026-11-13).
            bool fighting = Var<int32_t>(0x1404bc950) == 6;
            bool down = fighting && LocalDown();
            if (down != g_deadOn)
            {
                g_deadOn = down;
                g_freeCam = false;
                g_deadNext = g_deadPrevious = g_deadFree = false;
                g_deadTarget = down ? NextFollowable(LocalSlot(), 1) : -1;
                g_orbitYaw = g_orbitPitch = 0;
                if (down) Log(L"deadcam: on, following slot %d", g_deadTarget);
                else Log(L"deadcam: off (%s)", fighting ? L"back in the fight" : L"the battle is over");
                if (down && g_deadTarget < 0) StartFreeCam();
            }
            if (!g_deadOn) return;
            // What the HUD's hand reads for the watched player (status record +0x350 in use, +0x378 skill, +0x360, +0x384 owner
            // per slot, 0x50 apart), every 3 s, to see why its skills sometimes don't show.
            static ULONGLONG lastHand = 0;
            if (g_deadTarget >= 0 && !g_freeCam && GetTickCount64() - lastHand >= 3000)
            {
                lastHand = GetTickCount64();
                if (uint8_t* r = PlayerRecord(g_deadTarget))
                {
                    wchar_t line[256];
                    int n = swprintf_s(line, L"deadcam: slot %d hand:", g_deadTarget);
                    for (int i = 0; i < 4 && n > 0; ++i)
                        n += swprintf_s(line + n, 256 - n, L" [%d %d %d %d]", Field<int32_t>(r, 0x350 + i * 0x50), Field<int32_t>(r, 0x378 + i * 0x50),
                                        Field<int32_t>(r, 0x360 + i * 0x50), Field<int32_t>(r, 0x384 + i * 0x50));
                    Log(L"%s", line);
                }
            }
            if (g_deadFree.exchange(false))
            {
                if (g_freeCam) { g_freeCam = false; g_deadTarget = NextFollowable(LocalSlot(), 1); Log(L"deadcam: following slot %d", g_deadTarget); }
                else StartFreeCam();
            }
            if (!g_freeCam)
            {
                int step = g_deadNext.exchange(false) ? 1 : g_deadPrevious.exchange(false) ? -1 : 0;
                if (step) { g_deadTarget = NextFollowable(g_deadTarget, step); g_orbitYaw = g_orbitPitch = 0; Log(L"deadcam: following slot %d", g_deadTarget); }
                if (g_deadTarget >= 0 && !Followable(g_deadTarget))
                {
                    g_deadTarget = NextFollowable(g_deadTarget, 1);
                    Log(L"deadcam: that player is down - following slot %d", g_deadTarget);
                }
                if (g_deadTarget < 0) StartFreeCam();
                else OrbitInput();
            }
            else
                MoveFreeCam();
        }

        // The battle HUD (FUN_140150c30, from the HUD object's table at 0x14047b8f0) draws the local player's panel and hand.
        // While the dead camera follows someone, the local slot reads as theirs for that call only.
        //
        // Nametags: the HUD places them from the local player's camera block (object +0x13f10): +0x80 holds 32 entries of
        // 0x14 (shown, screen x, y, 0, distance), one per targetable object (DAT_1404bcb50), which FUN_140253c00(block)
        // works out each frame from the projection DAT_14058bdb0 and the block's view matrix (+0x40), skipping the block's
        // player (+0x300). It runs only for this PC's own player and its own camera, so tags floated in the free camera and
        // were missing on a followed player. While another view is used, a stand-in block with the frame's real view
        // (DAT_140584f40, which FUN_14012bfc0 fills either way) is worked out the same way and lent to the HUD's player.
        using HudDrawFn = void (*)(int64_t hud);
        HudDrawFn g_hudDraw = nullptr;
        bool g_viewOverridden = false; // this frame's view isn't the player's own (set by ViewFrom)
        constexpr size_t kTagEntries = 0x13f90, kTagBytes = 32 * 0x14;

        // Nametags while the view isn't the player's own: the game's tags are turned off for the draw (the HUD's player gets
        // entries all marked off screen) and pdnet draws every player's tag itself from this view, the HUD's way
        // (FUN_1402477e0 at the entry's screen point + (0x140, 0xe6), the name at DAT_1404bca30 + slot * 0x41, then
        // FUN_14028caa0), so the watched player's view and the free camera tag everyone alike: this PC's own player too,
        // standing or a body. Colours as pdnet.ini has them: dead for anyone down, ally for this player and its Tag Team
        // partner, enemy for the rest.
        uint32_t TagColour(int slot)
        {
            if (Health(slot) <= 0) return GetConfig().deadNametagColor;
            int local = LocalSlot();
            if (slot == local) return GetConfig().allyNametagColor;
            uint8_t* mine = PlayerObject(local);
            uint8_t* other = PlayerObject(slot);
            bool partner = !gamepatches::TestBattle() && Var<int32_t>(0x1405277b4) == 1 && mine && other && Field<int32_t>(mine, 0x1c) == Field<int32_t>(other, 0x1c);
            return partner ? GetConfig().allyNametagColor : GetConfig().enemyNametagColor;
        }

        void DrawTag(const uint8_t* entries, int slot)
        {
            uint8_t* record = PlayerRecord(slot);
            uint8_t* object = PlayerObject(slot);
            if (!record || !object || Field<int32_t>(object, 0x20) == 0) return;
            int index = Field<int32_t>(record, 0x60);
            if (index < 0 || index >= 32) return;
            const uint8_t* entry = entries + index * 0x14;
            if (*reinterpret_cast<const int32_t*>(entry) == 0) return; // off screen
            float x = *reinterpret_cast<const float*>(entry + 4), y = *reinterpret_cast<const float*>(entry + 8);
            uint32_t colour = TagColour(slot);
            auto colours = reinterpret_cast<uint32_t*>(At(0x14054c9b0));
            Var<int32_t>(0x14054c9ac) = 8;
            Var<int32_t>(0x14054c9a8) = 0x10;
            for (int i = 0; i < 4; ++i) colours[i] = colour;
            reinterpret_cast<void (*)(int, int, const char*, int)>(At(0x1402477e0))(static_cast<int>(x) + 0x140, static_cast<int>(y) + 0xe6,
                reinterpret_cast<const char*>(At(0x1404bca30) + slot * 0x41), 0);
            reinterpret_cast<void (*)()>(At(0x14028caa0))();
            Var<int32_t>(0x14054c9ac) = 7;
            Var<int32_t>(0x14054c9a8) = 0xe;
            for (int i = 0; i < 4; ++i) colours[i] = 0xffffffff;
        }

        // A viewer for FUN_140253c00 that is nobody (slot -1: nobody is skipped) at the camera's eye (+0xe0, for distances).
        uint8_t* NobodyViewer()
        {
            alignas(16) static uint8_t viewer[0x100];
            memset(viewer, 0, sizeof(viewer));
            *reinterpret_cast<int32_t*>(viewer + 0x14) = -1;
            *reinterpret_cast<Vec*>(viewer + 0xe0) = g_lastEye;
            return viewer;
        }

        // The HUD as hudSlot sees it, with every player but skip tagged from this view.
        void HudWithTags(int64_t hud, int hudSlot, int skip)
        {
            uint8_t* holder = PlayerObject(hudSlot);
            if (!holder) { g_hudDraw(hud); return; }
            alignas(16) static uint8_t block[0x310];
            memset(block, 0, sizeof(block));
            memcpy(block + 0x40, At(0x140584f40), 64);
            *reinterpret_cast<uint8_t**>(block + 0x300) = NobodyViewer();
            static uint8_t saved[kTagBytes];
            bool lent = false;
            __try
            {
                reinterpret_cast<void (*)(uint8_t*)>(At(0x140253c00))(block);
                memcpy(saved, holder + kTagEntries, kTagBytes);
                memset(holder + kTagEntries, 0, kTagBytes); // the game's own tags: none this draw
                lent = true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
            g_hudDraw(hud);
            if (!lent) return;
            memcpy(holder + kTagEntries, saved, kTagBytes);
            for (int s = 0; s < 4; ++s)
            {
                if (s == skip) continue;
                __try { DrawTag(block + 0x80, s); }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }

        void GameHud(int64_t hud)
        {
            int local = LocalSlot();
            bool following = g_deadOn && !g_freeCam && g_deadTarget >= 0 && PlayerObject(g_deadTarget);
            if (!g_viewOverridden && !following) { g_hudDraw(hud); return; }
            if (!following)
            {
                // The free camera: own HUD, everyone tagged. The Lab camera test (F6): the watched player untagged.
                HudWithTags(hud, local, g_deadOn && g_freeCam ? -1 : g_viewSlot);
                return;
            }
            // Following: the watched player's HUD, everyone else tagged, this PC's own player too.
            // A player on another PC: only its hand's skills come over the network (CardKind: status record +0x350 in use,
            // +0x378 the skill). The HUD also reads +0x384 per slot, the player a card came from, and shows that player's
            // name in place of the skill's when it is 0 or more; on another PC's player it is never set, so cards could read
            // as a player's name. For the draw it is -1 (the skill's own name) on such a player.
            uint8_t* watched = PlayerObject(g_deadTarget);
            uint8_t* record = PlayerRecord(g_deadTarget);
            bool remote = watched && record && Field<int32_t>(watched, kDriven) == 0;
            int32_t owners[4]{};
            if (remote)
                for (int i = 0; i < 4; ++i) { owners[i] = Field<int32_t>(record, 0x384 + i * 0x50); Field<int32_t>(record, 0x384 + i * 0x50) = -1; }
            // The HUD also shows the "local" player's lock-on target: when its camera block says a lock is on (object +0x13f1c,
            // with +0x13f10 != 1 and the block +0x14210 ready), it reads the targetable object DAT_1404bcb50[object +0x13f20] and
            // its +0xb8 with no check. The game keeps that block only for this PC's own player; on the AI it pointed at an
            // empty entry and the game crashed (PDUWP+0x15145e, 2026-11-14). For the draw, a lock that doesn't lead to a real
            // target is turned off.
            int32_t& lockOn = Field<int32_t>(watched, 0x13f1c);
            int32_t savedLock = lockOn;
            if (lockOn != 0)
            {
                int32_t index = Field<int32_t>(watched, 0x13f20);
                uint8_t* targetable = index >= 0 && index < 32 ? reinterpret_cast<uint8_t**>(At(0x1404bcb50))[index] : nullptr;
                if (!targetable || !*reinterpret_cast<uint8_t**>(targetable + 0xb8)) lockOn = 0;
            }
            auto& slot = Var<int32_t>(0x1404bbe00);
            slot = g_deadTarget;
            HudWithTags(hud, g_deadTarget, g_filmShown ? -1 : g_deadTarget); // a film's review tags the watched player too
            slot = local;
            lockOn = savedLock;
            if (remote)
                for (int i = 0; i < 4; ++i) Field<int32_t>(record, 0x384 + i * 0x50) = owners[i];
        }

        // Text on the battle screen, as the nametags draw it (about 640 x 460, from the top left).
        void ScreenText(int x, int y, const char* text, uint32_t colour)
        {
            auto colours = reinterpret_cast<uint32_t*>(At(0x14054c9b0));
            for (int i = 0; i < 4; ++i) colours[i] = colour;
            reinterpret_cast<void (*)(int, int, const char*, int)>(At(0x1402477e0))(x, y, text, 0);
            reinterpret_cast<void (*)()>(At(0x14028caa0))();
            for (int i = 0; i < 4; ++i) colours[i] = 0xffffffff;
        }

        void DrawFilmControls()
        {
            if (!g_controlsShown) return;
            auto clock = [](double ms, char* out, size_t n) { int s = static_cast<int>(ms / 1000); sprintf_s(out, n, "%d:%02d", s / 60, s % 60); };
            char now[16], length[16], line[200];
            clock(g_filmClock, now, sizeof(now));
            clock(g_filmEvents.empty() ? 0.0 : static_cast<double>(g_filmEvents.back().ms - g_filmBase), length, sizeof(length));
            const char* speed = g_filmPaused ? "PAUSED" : g_filmRate < 0.99f ? "SLOW MOTION" : g_filmRate > 1.01f ? "FAST FORWARD" : "PLAYING";
            const char* camera = !g_deadOn ? "" : g_freeCam ? "Flying cam" : "Player cam";
            char who[80] = "";
            if (g_deadOn && !g_freeCam && g_deadTarget >= 0)
                sprintf_s(who, ": %.64s", reinterpret_cast<const char*>(At(0x1404bca30) + g_deadTarget * 0x41));
            sprintf_s(line, "FILM %s / %s   %s   %s%s", now, length, speed, camera, who);
            int x = 24, y = 300;
            ScreenText(x, y, line, 0xffffd060);
            static const char* kLines[] = {
                "A / Space: play / pause            LT / Z (hold): slow motion",
                "Y / V: flying / player cam         B / H: hide / show HUD",
                "D-pad L R / Q E: switch players    Right stick / arrows: look",
                "Left stick / WASD: move            RB LB / R F: up / down",
                "RT / C (hold): fast forward        L3 / Ctrl (hold): pan",
                "D-pad Down / F2: hide / show this text",
            };
            for (const char* text : kLines) ScreenText(x, y += 14, text, 0xffffffff);
        }

        void HookedHudDraw(int64_t hud)
        {
            if (!FilmActive()) { GameHud(hud); return; }
            if (!g_hudHidden) GameHud(hud);
            else
            {
                // The HUD sets these render states (FUN_140150c30) before its text; with it hidden, the film text
                // needs them itself.
                reinterpret_cast<void (*)(int, int, int)>(At(0x140020c40))(0, 3, 2);
                reinterpret_cast<void (*)(int, int, int)>(At(0x140020c40))(0, 4, 2);
            }
            __try { DrawFilmControls(); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        bool HookHud()
        {
            auto entry = reinterpret_cast<HudDrawFn*>(At(0x14047b8f0));
            if (*entry != reinterpret_cast<HudDrawFn>(At(0x140150c30))) return false;
            DWORD old = 0;
            if (!VirtualProtect(entry, 8, PAGE_READWRITE, &old)) return false;
            g_hudDraw = *entry;
            *entry = &HookedHudDraw;
            VirtualProtect(entry, 8, old, &old);
            return true;
        }

        // Keyboard: F6, F7, F8 from the keys pdlaunch reads (KeysHeld, padshare.h): the game's window gets no key events on
        // the developer's PC. Controller: hold View, press a D-pad direction (Windows.Gaming.Input.GamepadButtons: View 0x2,
        // Up 0x40, Left 0x100, Right 0x200).
        void WatchInput()
        {
            std::thread([]
            {
                struct { uint32_t button; int key; std::atomic<bool>* press; const wchar_t* name; bool held; } combos[] = {
                    { 0x100, VK_F6, &g_pressCamera, L"camera", false }, { 0x40, VK_F7, &g_pressRecord, L"record", false }, { 0x200, VK_F8, &g_pressPlay, L"play back", false } };
                uint8_t keys[256];
                int keyboardKnown = -1; // what was last logged about pdlaunch's keyboard data, in a battle
                // The dead camera's presses: D-pad / arrow keys, without View (View + D-pad is the Lab tests').
                struct { uint32_t button; int key; std::atomic<bool>* press; bool held; } dead[] = {
                    { 0x200, VK_RIGHT, &g_deadNext, false }, { 0x100, VK_LEFT, &g_deadPrevious, false }, { 0x40, VK_UP, &g_deadFree, false } };
                bool pretendHeld = false;
                for (;;)
                {
                    Sleep(16);
                    bool deadActive = g_deadOn;
                    bool haveDeadKeys = deadActive && KeysHeld(keys);
                    bool viewHeld = AnyPadHolds(0x2);
                    bool film = g_filmShown;
                    for (auto& d : dead)
                    {
                        // In a film the arrows look around: Q / E switch players, V (or Y) switches cameras.
                        int key = !film ? d.key : d.press == &g_deadNext ? 'E' : d.press == &g_deadPrevious ? 'Q' : 'V';
                        bool pad = AnyPadHolds(d.button) || (film && d.press == &g_deadFree && AnyPadHolds(0x20)); // Y
                        bool now = deadActive && ((!viewHeld && pad) || (haveDeadKeys && keys[key]));
                        if (now && !d.held) *d.press = true;
                        d.held = now;
                    }
                    bool active = gamepatches::TestBattle();
                    {
                        uint8_t k[256];
                        bool pretend = active && ((viewHeld && AnyPadHolds(0x80)) || (KeysHeld(k) && k[VK_F9]));
                        if (pretend && !pretendHeld)
                        {
                            g_pretendDown = !g_pretendDown;
                            Log(L"deadcam: Test Arsenal: pretending to be down %s", g_pretendDown ? L"on" : L"off");
                        }
                        pretendHeld = pretend;
                        if (!active) g_pretendDown = false;
                    }
                    bool haveKeys = active && KeysHeld(keys);
                    if (active && keyboardKnown != (haveKeys ? 1 : 0))
                    {
                        keyboardKnown = haveKeys ? 1 : 0;
                        Log(L"labtest: keyboard from pdlaunch: %s", haveKeys ? L"reporting" : L"not reporting");
                    }
                    bool view = active && AnyPadHolds(0x2);
                    for (auto& combo : combos)
                    {
                        bool fromKey = haveKeys && keys[combo.key] != 0, fromPad = view && AnyPadHolds(combo.button);
                        bool now = fromKey || fromPad;
                        if (now && !combo.held)
                        {
                            Log(L"labtest: %s: %s", fromKey ? L"key" : L"View + D-pad", combo.name);
                            *combo.press = true;
                        }
                        combo.held = now;
                    }
                }
            }).detach();
        }

        void LogPlayers(const wchar_t* when)
        {
            Log(L"labtest: %s: local slot %d, game mode %d, match state %d, frame %d", when, LocalSlot(), Var<int32_t>(0x1404bc9d4), Var<int32_t>(0x1404bc950), FrameNow());
            for (int s = 0; s < 4; ++s)
            {
                uint8_t* object = PlayerObject(s);
                if (!object) continue;
                uint8_t* record = PlayerRecord(s);
                Vec pos = Field<Vec>(object, kPosition);
                Log(L"labtest:   slot %d: +0x20 %d, +0x28 %d, computer flag +0x30 %d, driven +0x34 %d, health %d, position %.1f %.1f %.1f, ring's player %s",
                    s, Field<int32_t>(object, 0x20), Field<int32_t>(object, 0x28), Field<int32_t>(object, 0x30), Field<int32_t>(object, kDriven),
                    record ? Field<int32_t>(record, 0xc) : -1, pos.x, pos.y, pos.z,
                    Field<uint8_t*>(object, kRing + kRingPlayer) == object ? L"ok" : L"NOT SET");
            }
        }

        void ReadHand(uint8_t* object, int16_t hand[4])
        {
            for (int i = 0; i < 4; ++i)
                hand[i] = Field<int32_t>(object, 0x3b8 - 0x28 + i * 0x50) != 0 ? Field<int16_t>(object, 0x3b8 + i * 0x50) : -1;
        }

        // ---- camera ----

        // The way the view matrix faces, either layout, and pointing from the eye towards the player.
        bool ViewForward(uint8_t* object, Vec eye, Vec position, Vec& forward)
        {
            auto m = reinterpret_cast<float*>(object + kView);
            bool translationLastRow = std::fabs(m[3]) + std::fabs(m[7]) + std::fabs(m[11]) < 0.001f;
            forward = translationLastRow ? Vec{ m[2], m[6], m[10] } : Vec{ m[8], m[9], m[10] };
            float length = Length(forward);
            if (!Finite(forward) || length < 0.5f || length > 1.5f) return false;
            forward = Scale(forward, 1.0f / length);
            if (Dot(forward, Sub(position, eye)) < 0) forward = Scale(forward, -1.0f);
            return true;
        }

        // Following another player from behind them: each player's facing is a 16-bit angle at object +0x12e68 (sent to the
        // other PCs in PlayerPos +0x18). Which way that angle turns in world space isn't known for sure, so it is learnt from this
        // PC's own player while it stands: of the 8 ways to read an angle as a horizontal direction, the one that best matches
        // where its own camera looks wins. The camera's shape (how far behind, how high, where it looks) comes from that camera
        // too, measured along the player's facing.
        constexpr size_t kFacing = 0x12e68;
        float g_facingScore[8]{};
        int g_facingSamples = 0;
        float g_behind = 0, g_height = 0, g_ahead = 0, g_lookHeight = 0; // the camera's shape, along the facing
        bool g_haveShape = false;

        Vec FacingWay(int way, float angle)
        {
            float s = std::sin(angle), c = std::cos(angle);
            switch (way)
            {
            case 0: return { s, 0, c };   case 1: return { -s, 0, -c };
            case 2: return { c, 0, s };   case 3: return { -c, 0, -s };
            case 4: return { s, 0, -c };  case 5: return { -s, 0, c };
            case 6: return { c, 0, -s };  default: return { -c, 0, s };
            }
        }
        float FacingAngle(uint8_t* object) { return Field<uint16_t>(object, kFacing) * (6.2831853f / 65536.0f); }
        int BestWay()
        {
            int best = 0;
            for (int w = 1; w < 8; ++w) if (g_facingScore[w] > g_facingScore[best]) best = w;
            return best;
        }
        bool FacingKnown() { return g_facingSamples >= 120; }

        void LearnFacing(uint8_t* mine, Vec eye, Vec look)
        {
            Vec view = Sub(look, eye);
            view.y = 0;
            float len = Length(view);
            if (len < 0.01f) return;
            view = Scale(view, 1.0f / len);
            float angle = FacingAngle(mine);
            for (int w = 0; w < 8; ++w) g_facingScore[w] = g_facingScore[w] * 0.995f + Dot(view, FacingWay(w, angle));
            if (++g_facingSamples == 120) Log(L"deadcam: facing angle read as way %d", BestWay());
            // The camera's shape, measured the way the player's own camera sits: its full distance behind (not only the part
            // along the facing, which shrank whenever the camera looked at a target off to one side and put the follow view too
            // close), its height and where it looks. Only frames where the camera is behind the player along its facing count,
            // averaged so one odd frame doesn't move it.
            Vec f = FacingWay(BestWay(), angle);
            Vec position = Field<Vec>(mine, kPosition);
            Vec toEye = Sub(eye, position), toLook = Sub(look, position);
            Vec flat = { toEye.x, 0, toEye.z };
            float distance = Length(flat);
            if (!FacingKnown() || distance < 0.01f || -Dot(flat, f) / distance < 0.9f) return;
            auto blend = [](float& kept, float now) { kept = g_haveShape ? kept * 0.97f + now * 0.03f : now; };
            blend(g_behind, distance);
            blend(g_height, toEye.y);
            blend(g_ahead, Dot(toLook, f));
            blend(g_lookHeight, toLook.y);
            if (!g_haveShape) Log(L"deadcam: follow camera shape: %.1f behind, %.1f up, looking %.1f ahead at %.1f up", g_behind, g_height, g_ahead, g_lookHeight);
            g_haveShape = true;
        }

        // The view from behind `object`, along its facing, smoothed so it doesn't snap.
        bool BehindView(uint8_t* object, Vec& eye, Vec& look)
        {
            static Vec smooth{};
            static uint8_t* last = nullptr;
            if (!FacingKnown() || !g_haveShape) return false;
            Vec f = FacingWay(BestWay(), FacingAngle(object));
            if (object != last || !Finite(smooth)) smooth = f;
            last = object;
            smooth = Add(Scale(smooth, 0.85f), Scale(f, 0.15f));
            float len = Length(smooth);
            if (len < 0.01f) return false;
            Vec d = Scale(smooth, 1.0f / len);
            if (g_deadOn && (g_orbitYaw != 0 || g_orbitPitch != 0)) // swung round by the watcher (OrbitInput)
            {
                float c = std::cos(g_orbitYaw), sn = std::sin(g_orbitYaw);
                d = { d.x * c + d.z * sn, 0, -d.x * sn + d.z * c };
            }
            float reach = std::sqrt(g_behind * g_behind + g_height * g_height);
            float rise = std::atan2(g_height, g_behind) + (g_deadOn ? g_orbitPitch : 0.0f);
            Vec position = Field<Vec>(object, kPosition);
            eye = Add(position, Vec{ -d.x * reach * std::cos(rise), reach * std::sin(rise), -d.z * reach * std::cos(rise) });
            look = Add(position, Vec{ d.x * g_ahead, g_lookHeight, d.z * g_ahead });
            return Finite(eye) && Finite(look);
        }

        void UpdateCameraOffset()
        {
            uint8_t* mine = PlayerObject(LocalSlot());
            Vec position = Field<Vec>(mine, kPosition), eye = Field<Vec>(mine, kEye), forward{};
            Vec toPlayer = Sub(position, eye);
            float distance = Length(toPlayer);
            if (!Finite(eye) || !Finite(position) || distance < 0.01f || !ViewForward(mine, eye, position, forward)) return;
            g_eyeOffset = Sub(eye, position);
            g_targetOffset = Add(g_eyeOffset, Scale(forward, distance));
            g_haveOffset = true;
            LearnFacing(mine, eye, Add(position, g_targetOffset));
        }

        void NextView()
        {
            int local = LocalSlot(), next = -1;
            for (int step = 1; step <= 4; ++step)
            {
                int s = (g_viewSlot < 0 ? local : g_viewSlot) + step;
                s %= 4;
                if (s == local) break; // back round to this player: own view
                uint8_t* object = PlayerObject(s);
                if (object && Field<int32_t>(object, 0x20) != 0) { next = s; break; }
            }
            g_viewSlot = next;
            if (next < 0) Log(L"labtest: camera: own view");
            else Log(L"labtest: camera: following slot %d%s", next, g_haveOffset ? L"" : L" (no camera offset yet: own view until there is one)");
        }

        void ReportFollowed()
        {
            DWORD now = GetTickCount();
            if (g_viewSlot < 0 || now - g_lastReport < 3000) return;
            g_lastReport = now;
            uint8_t* object = PlayerObject(g_viewSlot);
            uint8_t* record = PlayerRecord(g_viewSlot);
            if (!object) return;
            int16_t hand[4];
            ReadHand(object, hand);
            Vec pos = Field<Vec>(object, kPosition);
            Log(L"labtest: camera: slot %d at %.1f %.1f %.1f, health %d, hand %d %d %d %d",
                g_viewSlot, pos.x, pos.y, pos.z, record ? Field<int32_t>(record, 0xc) : -1, hand[0], hand[1], hand[2], hand[3]);
        }

        // ---- recording ----

        void SaveRecording()
        {
            std::wstring path = RecordingPath();
            FILE* file = nullptr;
            bool saved = false;
            if (_wfopen_s(&file, path.c_str(), L"wb") == 0 && file)
            {
                uint32_t header[3] = { kFileMagic, static_cast<uint32_t>(sizeof(Frame)), static_cast<uint32_t>(g_recording.size()) };
                saved = fwrite(header, sizeof(header), 1, file) == 1 &&
                    (g_recording.empty() || fwrite(g_recording.data(), sizeof(Frame), g_recording.size(), file) == g_recording.size());
                fclose(file);
            }
            size_t bytes = g_recording.size() * sizeof(Frame) + 12;
            float seconds = (GetTickCount() - g_recordStart) / 1000.0f;
            uint32_t span = g_recording.empty() ? 0 : g_recording.back().tick - g_recording.front().tick;
            Log(L"labtest: record: stopped, %zu frames over %u game frames in %.1f s (%.1f a second), %zu bytes (%.1f KB a second), %s",
                g_recording.size(), span, seconds, seconds > 0 ? g_recording.size() / seconds : 0.0f, bytes, seconds > 0 ? bytes / 1024.0f / seconds : 0.0f,
                saved ? L"saved to labtest.rec" : L"SAVING FAILED");
        }

        void StopRecording()
        {
            if (!g_recordingOn) return;
            g_recordingOn = false;
            SaveRecording();
            g_recording.clear();
        }

        void ToggleRecord()
        {
            if (g_playing) { Log(L"labtest: record: not while playing back"); return; }
            if (g_recordingOn) { StopRecording(); return; }
            g_recording.clear();
            g_recording.reserve(60 * 60);
            g_recordingOn = true;
            g_recordStart = GetTickCount();
            Log(L"labtest: record: started");
        }

        using MakePositionFn = uint64_t (*)(uint8_t* object, void* out);

        void RecordFrame(int frame)
        {
            Frame f{};
            f.tick = static_cast<uint32_t>(frame);
            auto makePosition = reinterpret_cast<MakePositionFn>(At(0x1402502a0));
            for (int s = 0; s < 4; ++s)
            {
                uint8_t* object = PlayerObject(s);
                if (!object || Field<int32_t>(object, 0x28) == 0) continue;
                if (!makePosition(object, f.position[s])) continue;
                ReadHand(object, f.hand[s]);
                f.slots |= 1u << s;
            }
            g_recording.push_back(f);
            if (g_recording.size() >= kMaxFrames)
            {
                Log(L"labtest: record: five minutes reached");
                StopRecording();
            }
        }

        // ---- playback ----

        bool LoadRecording()
        {
            g_playback.clear();
            FILE* file = nullptr;
            if (_wfopen_s(&file, RecordingPath().c_str(), L"rb") != 0 || !file) { Log(L"labtest: play back: no recording yet (F7 records)"); return false; }
            uint32_t header[3]{};
            bool ok = fread(header, sizeof(header), 1, file) == 1 && header[0] == kFileMagic && header[1] == sizeof(Frame) && header[2] > 0 && header[2] <= kMaxFrames;
            if (ok)
            {
                g_playback.resize(header[2]);
                ok = fread(g_playback.data(), sizeof(Frame), g_playback.size(), file) == g_playback.size();
            }
            fclose(file);
            if (!ok) { g_playback.clear(); Log(L"labtest: play back: labtest.rec could not be read"); }
            return ok;
        }

        void RestoreDriven()
        {
            for (int s = 0; s < 4; ++s)
                if (uint8_t* object = PlayerObject(s)) Field<int32_t>(object, kDriven) = g_savedDriven[s];
        }

        void StopPlayback(const wchar_t* why)
        {
            if (!g_playing) return;
            g_playing = false;
            RestoreDriven();
            Log(L"labtest: play back: %s after %zu of %zu frames, %.1f s, %d fault(s). Local control back.",
                why, g_playAt, g_playback.size(), (GetTickCount() - g_playStart) / 1000.0f, g_playFaults);
            LogPlayers(L"after play back");
            g_playback.clear();
        }

        void TogglePlay()
        {
            if (g_playing) { StopPlayback(L"stopped"); return; }
            if (g_recordingOn) { Log(L"labtest: play back: stop recording first (F7)"); return; }
            if (!LoadRecording()) return;
            LogPlayers(L"before play back");
            for (int s = 0; s < 4; ++s)
            {
                uint8_t* object = PlayerObject(s);
                g_savedDriven[s] = object ? Field<int32_t>(object, kDriven) : 0;
                if (!object) continue;
                if (Field<uint8_t*>(object, kRing + kRingPlayer) != object)
                {
                    Log(L"labtest: play back: slot %d has no received-data state: not started", s);
                    g_playback.clear();
                    return;
                }
            }
            int now = FrameNow();
            for (int s = 0; s < 4; ++s)
            {
                uint8_t* object = PlayerObject(s);
                if (!object) continue;
                Field<int32_t>(object, kRing) = now; // its clock: only this player's ring runs from here
                Field<int32_t>(object, kDriven) = 0;
            }
            g_playAt = 0;
            g_playFaults = 0;
            g_playStart = GetTickCount();
            g_playing = true;
            if (g_viewSlot < 0 && g_haveOffset) Log(L"labtest: play back: the view follows this player from the recording");
            Log(L"labtest: play back: started, %zu frames, no player under local control", g_playback.size());
        }

        using ReceiveFn = uint64_t (*)(uint32_t slot, void* state);

        // The 26 streams' receive functions, as the stream table at 0x1403fb4d0 holds them for this exe (checked before use).
        constexpr uint64_t kFilmReceivers[26] = { 0x14027c4f0, 0x14027c610, 0x14027c660, 0x14027c6a0, 0x14027c6e0, 0x14027c760,
            0x14027c7e0, 0x14027c810, 0x14027c8b0, 0x14027c8d0, 0x14027c930, 0x14027c950, 0x14027c9f0, 0x14027ca30, 0x14027cb00,
            0x14027cbf0, 0x14027cc10, 0x14027cca0, 0x14027cdc0, 0x14027ce50, 0x14027ce90, 0x14027cee0, 0x14027cf80, 0x14027cf90,
            0x14027cfd0, 0x14027d040 };

        // SEH only around the game's own receive functions: a fault ends the test with a log line.
        bool Deliver(ReceiveFn receive, int slot, void* state)
        {
            __try { receive(static_cast<uint32_t>(slot), state); return true; }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        void PlayFrame(int frame)
        {
            if (g_playAt >= g_playback.size()) { StopPlayback(L"finished"); return; }
            Frame& f = g_playback[g_playAt++];
            const Frame* before = g_playAt >= 2 ? &g_playback[g_playAt - 2] : nullptr;
            for (int s = 0; s < 4; ++s)
            {
                if (!(f.slots >> s & 1) || !PlayerObject(s)) continue;
                alignas(16) uint8_t state[0x48]{};
                *reinterpret_cast<int32_t*>(state + 0x20) = (frame + 2) / 2;
                *reinterpret_cast<void**>(state + 0x40) = f.position[s];
                bool ok = Deliver(reinterpret_cast<ReceiveFn>(At(0x14027c4f0)), s, state);
                if (ok && (!before || memcmp(before->hand[s], f.hand[s], sizeof(f.hand[s])) != 0))
                {
                    *reinterpret_cast<void**>(state + 0x40) = f.hand[s];
                    ok = Deliver(reinterpret_cast<ReceiveFn>(At(0x14027c610)), s, state);
                }
                if (!ok)
                {
                    ++g_playFaults;
                    StopPlayback(L"a fault in the game's receive code ended it");
                    return;
                }
            }
        }

        // Each film player keeps their own slot in the battle: the streams name players by slot inside their data
        // (Condition's target, hits, steals), so any other mapping sent them to the wrong player or to none (Conceptual 30:
        // a Condition for film slot 2 faulted in a 2-player battle).
        void MapFilmSlots()
        {
            for (int& s : g_filmSlot) s = -1;
            for (int s = 0; s < 4; ++s)
                if (g_filmInfo.present[s]) g_filmSlot[s] = s;
            for (const auto& e : g_filmEvents) // films with no setup snapshot: the slots their data comes from
                if (e.slot < 4) g_filmSlot[e.slot] = e.slot;
        }

        void StartFilmPlayback()
        {
            g_filmQueued = false;
            MapFilmSlots();
            int now = FrameNow();
            for (int s = 0; s < 4; ++s)
            {
                uint8_t* object = PlayerObject(s);
                if (!object || Field<uint8_t*>(object, kRing + kRingPlayer) != object) continue;
                Field<int32_t>(object, kRing) = now;
                Field<int32_t>(object, kDriven) = 0;
            }
            g_filmAt = 0;
            g_filmBase = g_filmEvents.empty() ? 0 : g_filmEvents.front().ms;
            g_filmStart = GetTickCount64();
            g_filmClock = 0;
            g_filmPaused = g_hudHidden = false;
            g_controlsShown = true;
            g_filmRate = 1.0f;
            g_slowAccumulator = 0;
            memset(g_filmSkipped, 0, sizeof(g_filmSkipped));
            memset(g_filmPlayed, 0, sizeof(g_filmPlayed));
            memset(g_filmStreamOff, 0, sizeof(g_filmStreamOff));
            for (auto& slot : g_filmLast) for (LastPlayed& l : slot) l = LastPlayed{};
            // Nametags: the film's names (DAT_1404bca30, 0x41 per slot).
            char* names = reinterpret_cast<char*>(At(0x1404bca30));
            for (int s = 0; s < 4; ++s)
                if (g_filmSlot[s] >= 0 && !g_filmInfo.name[s].empty() && PlayerObject(s))
                    strncpy_s(names + s * 0x41, 0x41, g_filmInfo.name[s].c_str(), _TRUNCATE);
            g_filmPlaying = g_filmShown = true;
            Log(L"theatre: playing %s: %zu stream records over %.1f min, film slots to battle slots %d %d %d %d", g_filmInfo.file.c_str(), g_filmEvents.size(),
                g_filmEvents.empty() ? 0.0 : (g_filmEvents.back().ms - g_filmBase) / 60000.0, g_filmSlot[0], g_filmSlot[1], g_filmSlot[2], g_filmSlot[3]);
        }

        void FilmFrame()
        {
            if (!g_filmPlaying) return;
            uint64_t elapsed = g_stepHooked ? static_cast<uint64_t>(g_filmClock) : GetTickCount64() - g_filmStart;
            int frame = FrameNow();
            while (g_filmAt < g_filmEvents.size() && g_filmEvents[g_filmAt].ms - g_filmBase <= elapsed)
            {
                film::Event& e = g_filmEvents[g_filmAt++];
                int slot = g_filmSlot[e.slot];
                if (slot < 0 || !PlayerObject(slot)) continue;
                if (e.stream >= 26) continue;
                // ReadyStart (22) is the pre-battle handshake: this battle has already started. DmgEffect (20) is played
                // again: its crash in Conceptual 29 came from data read 4 bytes off.
                if (e.stream == 22 || g_filmStreamOff[e.stream]) { ++g_filmSkipped[e.stream]; continue; }
                // Skills, hits and effects name skills by id: without that player's deck in this battle they aren't loaded.
                if (e.stream >= 2 && !g_filmInfo.haveSkills[e.slot & 3]) { ++g_filmSkipped[e.stream]; continue; }
                if (e.stream >= 2)
                {
                    LastPlayed& last = g_filmLast[e.slot & 3][e.stream];
                    if (last.any && last.number == e.number && last.data == e.data) continue; // a resent copy
                    last.any = true;
                    last.number = e.number;
                    last.data = e.data;
                }
                // The receive function from the game's stream table (0x28 bytes each, receive at +0x18): all take (slot,
                // state) and read the data at state+0x40; PlayerPos also reads frame/2 at state+0x20.
                uint64_t fn = *reinterpret_cast<const uint64_t*>(At(0x1403fb4d0 + e.stream * 0x28 + 0x18));
                if (fn != reinterpret_cast<uint64_t>(At(kFilmReceivers[e.stream])))
                {
                    ++g_filmSkipped[e.stream];
                    continue;
                }
                alignas(16) uint8_t state[0x48]{};
                *reinterpret_cast<int32_t*>(state + 0x20) = (frame + 2) / 2;
                *reinterpret_cast<void**>(state + 0x40) = e.data.data();
                // FireStartReq and FireReq carry the frame the skill started on (int at +4; FUN_14018fae0 / FUN_14018fc70 queue
                // them in the player's ring, which starts them at that frame). PlayerPos is moved to this battle's clock
                // (state+0x20), so these are too: as far behind this battle's frame as they were behind the sender's
                // (packet frame/2 * 2). Left on the match's own frames, skills started early or late, and held ones such as
                // shields were cut short or skipped (Conceptual 33).
                alignas(16) uint8_t retimed[96];
                if ((e.stream == 2 || e.stream == 3) && e.data.size() >= 8 && e.data.size() <= sizeof(retimed))
                {
                    memcpy(retimed, e.data.data(), e.data.size());
                    int32_t& stamp = *reinterpret_cast<int32_t*>(retimed + 4);
                    int behind = (std::max)(0, (std::min)(120, static_cast<int>(e.half * 2) - stamp));
                    stamp = (frame + 2) / 2 * 2 - behind;
                    *reinterpret_cast<void**>(state + 0x40) = retimed;
                }
                ++g_filmPlayed[e.stream];
                if (!Deliver(reinterpret_cast<ReceiveFn>(fn), slot, state))
                {
                    // One stream's fault no longer stops the film: that stream is left out from here on.
                    g_filmStreamOff[e.stream] = true;
                    Log(L"theatre: the game's receive code faulted on stream %d (film slot %d, %u ms) - that stream is left out from here on",
                        e.stream, e.slot, e.ms);
                }
            }
            if (g_filmAt >= g_filmEvents.size())
            {
                g_filmPlaying = false;
                uint32_t skipped = 0;
                for (uint32_t n : g_filmSkipped) skipped += n;
                Log(L"theatre: film finished: skills started %u, fired %u, actions %u, hits %u, damage effects %u; %u records skipped",
                    g_filmPlayed[2], g_filmPlayed[3], g_filmPlayed[4], g_filmPlayed[24] + g_filmPlayed[25], g_filmPlayed[20], skipped);
            }
        }

        // The film's presses, once a frame on the game's thread: edges of A / Space (pause), B / H (HUD), D-pad Down / F2
        // (the film text);
        // the speed from LT / Z held (slow motion) or RT / C held (fast forward).
        void FilmPresses()
        {
            static bool held[3] = {};
            uint8_t keys[256];
            bool haveKeys = KeysHeld(keys);
            bool view = AnyPadHolds(0x2); // View + D-pad is the Lab tests'
            bool now[3] = {
                AnyPadHolds(0x4) || (haveKeys && keys[VK_SPACE]),            // A
                AnyPadHolds(0x8) || (haveKeys && keys['H']),                 // B
                (!view && AnyPadHolds(0x80)) || (haveKeys && keys[VK_F2]),   // D-pad Down
            };
            if (now[0] && !held[0]) { g_filmPaused = !g_filmPaused; Log(L"theatre: %s", g_filmPaused ? L"paused" : L"playing"); }
            if (now[1] && !held[1]) g_hudHidden = !g_hudHidden;
            if (now[2] && !held[2]) g_controlsShown = !g_controlsShown;
            for (int i = 0; i < 3; ++i) held[i] = now[i];
            float lt = 0, rt = 0;
            PadTriggers(lt, rt);
            float rate = 1.0f;
            if (rt > 0.3f || (haveKeys && keys['C'])) rate = 4.0f;
            else if (lt > 0.3f || (haveKeys && keys['Z'])) rate = 0.25f;
            if (rate != g_filmRate) Log(L"theatre: speed x%.2f", rate);
            g_filmRate = rate;
        }

        // The game's per-frame update step (FUN_1401c7630, called at 0x1401c865a): the task groups' updates run only when
        // DAT_140530034 & 0x20001 is 0 (bit 0: the game's own pause); the draws, and with them the camera, run either way.
        using StepFn = void (*)(int64_t);
        void HookedStep(int64_t game)
        {
            auto step = reinterpret_cast<StepFn>(At(0x1401c7630));
            static LARGE_INTEGER frequency{}, last{};
            if (!frequency.QuadPart) QueryPerformanceFrequency(&frequency);
            LARGE_INTEGER t;
            QueryPerformanceCounter(&t);
            double ms = last.QuadPart ? (t.QuadPart - last.QuadPart) * 1000.0 / frequency.QuadPart : 0.0;
            last = t;
            if (!FilmActive())
            {
                g_filmPaused = false; // never left paused once the battle is over
                g_filmRate = 1.0f;
                step(game);
                return;
            }
            ms = (std::min)(ms, 100.0);
            float rate = g_filmPaused ? 0.0f : g_filmRate;
            auto& flags = Var<uint32_t>(0x140530034);
            if (rate > 1.5f)
            {
                int times = static_cast<int>(rate + 0.5f);
                for (int i = 0; i < times; ++i)
                {
                    g_filmClock += ms * rate / times;
                    FilmFrame();
                    step(game);
                }
                return;
            }
            g_filmClock += ms * rate;
            FilmFrame();
            bool run = true;
            if (rate < 0.99f)
            {
                g_slowAccumulator += rate;
                run = g_slowAccumulator >= 1.0f;
                if (run) g_slowAccumulator -= 1.0f;
            }
            if (run) { step(game); return; }
            bool had = (flags & 1) != 0;
            flags |= 1; // this frame: no world update
            step(game);
            if (!had) flags &= ~1u;
        }

        void EndOfBattle()
        {
            if (g_filmShown) Log(L"theatre: battle over");
            g_filmPlaying = g_filmShown = false;
            // The end of a battle is noticed only when the next one starts, after a film for it may already be queued
            // (Conceptual 35: a film after another battle played 0 records): a queued film is kept.
            if (!g_filmQueued) g_filmEvents.clear();
            g_recordingOn = false; // the players are gone: what was recorded is kept only if F7 stopped it first
            g_recording.clear();
            g_playing = false;
            g_playback.clear();
            g_viewSlot = -1;
            g_haveOffset = false;
            g_lastFrame = -1;
            Log(L"labtest: battle over");
        }

        // ---- each frame ----

        using CameraFn = void (*)();
        void ViewFrom(CameraFn original, int follow);

        // A match that isn't a Test Arsenal battle: only the dead camera.
        void DeadCameraFrame(CameraFn original)
        {
            if (!LocalDown()) UpdateCameraOffset(); // the player's own camera, while it still runs
            UpdateDeadCamera();
            ViewFrom(original, g_deadOn ? (g_freeCam ? -1 : g_deadTarget) : -1);
        }

        void HookedCamera()
        {
            auto original = reinterpret_cast<CameraFn>(At(0x14012bfc0));
            if (InMatchNow() && !InBattle())
            {
                DeadCameraFrame(original);
                return;
            }
            bool battle = InBattle();
            if (!battle)
            {
                g_viewOverridden = false;
                if (g_deadOn) { g_deadOn = false; g_deadTarget = -1; Log(L"deadcam: off (match over)"); }
                if (g_wasInBattle) { g_wasInBattle = false; EndOfBattle(); }
                original();
                return;
            }
            // The game doesn't call this in the menus (states 0, 2, 11, 13: FUN_14017cff0), so a battle's end may only
            // show here as the next battle's frame counter starting again.
            if (g_wasInBattle && FrameNow() < g_lastFrame) { g_wasInBattle = false; EndOfBattle(); }
            if (!g_wasInBattle)
            {
                g_wasInBattle = true;
                g_pressCamera = g_pressRecord = g_pressPlay = false; // presses from before this battle
                LogPlayers(L"battle started");
                if (g_filmQueued) StartFilmPlayback();
            }
            // Shows this hook keeps running in the battle, and whether the game's frame counter moves.
            static uint32_t calls = 0;
            static DWORD lastBeat = 0;
            ++calls;
            if (GetTickCount() - lastBeat >= 5000)
            {
                lastBeat = GetTickCount();
                Log(L"labtest: running: %u camera calls, frame %d, match state %d", calls, FrameNow(), Var<int32_t>(0x1404bc950));
            }
            if (g_pressCamera.exchange(false)) NextView();
            if (g_pressRecord.exchange(false)) ToggleRecord();
            if (g_pressPlay.exchange(false)) TogglePlay();

            int frame = FrameNow();
            if (frame != g_lastFrame)
            {
                g_lastFrame = frame;
                if (g_recordingOn) RecordFrame(frame);
                if (g_playing) PlayFrame(frame);
            }
            if (!g_playing) UpdateCameraOffset(); // playing back, this player's own camera doesn't run
            ReportFollowed();
            if (FilmActive()) FilmPresses();
            if (!g_stepHooked) FilmFrame();
            UpdateDeadCamera();

            int follow = g_deadOn ? (g_freeCam ? -1 : g_deadTarget) : g_viewSlot >= 0 ? g_viewSlot : g_playing ? LocalSlot() : -1;
            ViewFrom(original, follow);
        }

        // The view for this frame: the dead camera's free camera, following `follow` (with this player's own camera offset), or
        // the game's own view.
        void ViewFrom(CameraFn original, int follow)
        {
            uint8_t* target = PlayerObject(follow);
            g_viewOverridden = false;
            auto& cutscene = Var<int32_t>(0x140534798);
            bool free = g_deadOn && g_freeCam;
            if ((!free && (!target || !g_haveOffset)) || cutscene != 0 || Var<int32_t>(0x1404bbec0) == 0)
            {
                original();
                return;
            }
            Vec eye, look;
            if (free) { eye = g_freePos; look = Add(g_freePos, Scale(FreeForward(), 50.0f)); }
            else
            {
                if (!BehindView(target, eye, look)) // until the facing is learnt: this player's own camera offset
                {
                    Vec position = Field<Vec>(target, kPosition);
                    eye = Add(position, g_eyeOffset);
                    look = Add(position, g_targetOffset);
                }
            }
            if (!Finite(eye) || !Finite(look))
            {
                original();
                return;
            }
            g_lastEye = eye;
            g_lastLook = look;
            g_haveLast = true;
            auto& gameLook = Var<Vec>(0x14059e130);
            auto& gameEye = Var<Vec>(0x14059e150);
            Vec savedLook = gameLook, savedEye = gameEye;
            gameLook = look;
            gameEye = eye;
            cutscene = 1;
            g_viewOverridden = true; // the HUD, drawn after this, places nametags from this view
            original();
            cutscene = 0;
            gameLook = savedLook;
            gameEye = savedEye;
        }
    }

    bool QueueFilm(const std::wstring& file)
    {
        if (!film::Load(file, g_filmEvents, g_filmInfo))
        {
            Log(L"theatre: %s could not be read", file.c_str());
            return false;
        }
        g_filmInfo.file = file;
        MapFilmSlots();
        g_filmQueued = true;
        Log(L"theatre: %s loaded (%zu stream records), starting its battle", file.c_str(), g_filmEvents.size());
        return true;
    }

    int FilmPlayers()
    {
        if (!g_filmQueued) return 0;
        int n = 2;
        for (int s = 0; s < 4; ++s) if (g_filmSlot[s] >= 0) n = (std::max)(n, s + 1);
        return n;
    }

    int FilmCharacter(int slot)
    {
        return g_filmQueued && slot >= 0 && slot < 4 ? g_filmInfo.character[slot] : -1;
    }

    bool FilmDeck(int slot, int16_t (&skills)[30])
    {
        if (!g_filmQueued) return false;
        for (int s = 0; s < 4; ++s)
        {
            if (g_filmSlot[s] != slot || !g_filmInfo.haveSkills[s]) continue;
            memcpy(skills, g_filmInfo.skills[s], sizeof(skills));
            return true;
        }
        return false;
    }

    void Install()
    {
        if (gamepatches::HookCall(0x14017d00e, 0x14012bfc0, reinterpret_cast<void*>(&HookedCamera)))
        {
            WatchInput();
            Log(L"labtest: ready: in a Test Arsenal battle, F6 or View + D-pad Left: camera, F7 or View + D-pad Up: record, F8 or View + D-pad Right: play back");
            Log(L"deadcam: ready (HUD %s); in Test Arsenal, F9 or View + D-pad Down pretends to be down", HookHud() ? L"follows the watched player" : L"NOT hooked");
            g_stepHooked = gamepatches::HookCall(0x1401c865a, 0x1401c7630, reinterpret_cast<void*>(&HookedStep));
            Log(L"theatre: film controls %s", g_stepHooked ? L"ready (pause, slow motion, fast forward)" : L"without time control (update step NOT hooked)");
        }
        else
            Log(L"labtest: not installed - the game code is not what this pdnet expects");
    }
}
