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

        bool InBattle()
        {
            int32_t state = Var<int32_t>(0x1404bc950);
            return gamepatches::TestBattle() && state != 0 && state != 13 && PlayerObject(LocalSlot());
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
                for (;;)
                {
                    Sleep(16);
                    bool active = gamepatches::TestBattle();
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

        void HookedCamera()
        {
            auto original = reinterpret_cast<CameraFn>(At(0x14012bfc0));
            bool battle = InBattle();
            if (!battle)
            {
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

            int follow = g_viewSlot >= 0 ? g_viewSlot : g_playing ? LocalSlot() : -1;
            uint8_t* target = PlayerObject(follow);
            auto& cutscene = Var<int32_t>(0x140534798);
            if (!target || !g_haveOffset || cutscene != 0 || Var<int32_t>(0x1404bbec0) == 0)
            {
                original();
                return;
            }
            Vec position = Field<Vec>(target, kPosition);
            Vec eye = Add(position, g_eyeOffset), look = Add(position, g_targetOffset);
            if (!Finite(eye) || !Finite(look))
            {
                original();
                return;
            }
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
        }
        else
            Log(L"labtest: not installed - the game code is not what this pdnet expects");
    }
}
