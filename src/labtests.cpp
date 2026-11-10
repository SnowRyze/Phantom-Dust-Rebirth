#include "pch.h"
#include "labtests.h"
#include "gamepatches.h"
#include "gamepads.h"
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
        ULONGLONG g_freeLast = 0;
        Vec g_lastEye{}, g_lastLook{};
        bool g_haveLast = false;

        int32_t Health(int slot) { uint8_t* record = PlayerRecord(slot); return record ? Field<int32_t>(record, 0xc) : 0; }

        bool LocalDown()
        {
            if (g_pretendDown && gamepatches::TestBattle()) return true;
            return Health(LocalSlot()) <= 0;
        }

        // Game type 1 = Tag Team (0x1405277b4, as the lobby has it); team = player object +0x1c.
        bool Followable(int slot)
        {
            int local = LocalSlot();
            uint8_t* object = PlayerObject(slot);
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
            if (KeysHeld(keys))
            {
                if (keys['W']) ly += 1;
                if (keys['S']) ly -= 1;
                if (keys['D']) lx += 1;
                if (keys['A']) lx -= 1;
                if (keys['E']) rx += 1;
                if (keys['Q']) rx -= 1;
                if (keys['R']) up += 1;
                if (keys['F']) up -= 1;
            }
            auto dead = [](float v) { return std::fabs(v) < 0.2f ? 0.0f : v; };
            lx = dead(lx); ly = dead(ly); rx = dead(rx); ry = dead(ry);
            constexpr float kSpeed = 150.0f, kTurn = 2.0f;
            g_freeYaw += rx * kTurn * dt;
            g_freePitch = (std::max)(-1.4f, (std::min)(1.4f, g_freePitch + ry * kTurn * dt));
            Vec forward = FreeForward(), right = { std::cos(g_freeYaw), 0, -std::sin(g_freeYaw) };
            g_freePos = Add(g_freePos, Scale(forward, ly * kSpeed * dt));
            g_freePos = Add(g_freePos, Scale(right, lx * kSpeed * dt));
            g_freePos.y += up * kSpeed * dt;
        }

        void UpdateDeadCamera()
        {
            bool down = LocalDown();
            if (down != g_deadOn)
            {
                g_deadOn = down;
                g_freeCam = false;
                g_deadNext = g_deadPrevious = g_deadFree = false;
                g_deadTarget = down ? NextFollowable(LocalSlot(), 1) : -1;
                if (down) Log(L"deadcam: on, following slot %d", g_deadTarget);
                else Log(L"deadcam: off (back in the fight)");
                if (down && g_deadTarget < 0) StartFreeCam();
            }
            if (!g_deadOn) return;
            if (g_deadFree.exchange(false))
            {
                if (g_freeCam) { g_freeCam = false; g_deadTarget = NextFollowable(LocalSlot(), 1); Log(L"deadcam: following slot %d", g_deadTarget); }
                else StartFreeCam();
            }
            if (!g_freeCam)
            {
                int step = g_deadNext.exchange(false) ? 1 : g_deadPrevious.exchange(false) ? -1 : 0;
                if (step) { g_deadTarget = NextFollowable(g_deadTarget, step); Log(L"deadcam: following slot %d", g_deadTarget); }
                if (g_deadTarget >= 0 && !Followable(g_deadTarget))
                {
                    g_deadTarget = NextFollowable(g_deadTarget, 1);
                    Log(L"deadcam: that player is down - following slot %d", g_deadTarget);
                }
                if (g_deadTarget < 0) StartFreeCam();
            }
            else
                MoveFreeCam();
        }

        // The battle HUD (FUN_140150c30, from the HUD object's table at 0x14047b8f0) draws the local player's panel and hand.
        // While the dead camera follows someone, the local slot reads as theirs for that call only.
        using HudDrawFn = void (*)(int64_t hud);
        HudDrawFn g_hudDraw = nullptr;
        void HookedHudDraw(int64_t hud)
        {
            if (!g_deadOn || g_freeCam || g_deadTarget < 0 || !PlayerObject(g_deadTarget)) { g_hudDraw(hud); return; }
            auto& local = Var<int32_t>(0x1404bbe00);
            int32_t mine = local;
            local = g_deadTarget;
            g_hudDraw(hud);
            local = mine;
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
                    for (auto& d : dead)
                    {
                        bool now = deadActive && ((!viewHeld && AnyPadHolds(d.button)) || (haveDeadKeys && keys[d.key]));
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

        void EndOfBattle()
        {
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
            UpdateDeadCamera();

            int follow = g_deadOn ? (g_freeCam ? -1 : g_deadTarget) : g_viewSlot >= 0 ? g_viewSlot : g_playing ? LocalSlot() : -1;
            ViewFrom(original, follow);
        }

        // The view for this frame: the dead camera's free camera, following `follow` (with this player's own camera offset), or
        // the game's own view.
        void ViewFrom(CameraFn original, int follow)
        {
            uint8_t* target = PlayerObject(follow);
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
                Vec position = Field<Vec>(target, kPosition);
                eye = Add(position, g_eyeOffset);
                look = Add(position, g_targetOffset);
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
            original();
            cutscene = 0;
            gameLook = savedLook;
            gameEye = savedEye;
        }
    }

    void Install()
    {
        if (gamepatches::HookCall(0x14017d00e, 0x14012bfc0, reinterpret_cast<void*>(&HookedCamera)))
        {
            WatchInput();
            Log(L"labtest: ready: in a Test Arsenal battle, F6 or View + D-pad Left: camera, F7 or View + D-pad Up: record, F8 or View + D-pad Right: play back");
            Log(L"deadcam: ready (HUD %s); in Test Arsenal, F9 or View + D-pad Down pretends to be down", HookHud() ? L"follows the watched player" : L"NOT hooked");
        }
        else
            Log(L"labtest: not installed - the game code is not what this pdnet expects");
    }
}
