#include "pch.h"
#include "gamepads.h"
#include "padshare.h"
#include "log.h"
#include <winrt/Windows.Gaming.Input.h>

// The game reads controllers only through Windows.Gaming.Input.Gamepad (no XInput, no DirectInput), which on Windows 10
// lists Xbox controllers and XInput pads only. pdnet answers the class's factory itself (hooks.cpp): Gamepads is the
// OS list plus one VirtualPad per controller pdlaunch publishes in LocalState\pdnet\pads.bin (padshare.h), and
// GamepadAdded / GamepadRemoved fire for those too. A VirtualPad's reading comes from that file; its Vibration goes
// back through it to pdlaunch, which rumbles the controller.

namespace pdnet
{
    namespace
    {
        using namespace winrt::Windows::Gaming::Input;
        using winrt::Windows::Foundation::EventHandler;
        using winrt::Windows::Foundation::IInspectable;

        pads::Shared* g_shared = nullptr;
        std::mutex g_rumbleLock;

        // Maps LocalState\pdnet\pads.bin (created by whichever of pdnet / pdlaunch comes first).
        pads::Shared* MapShared()
        {
            std::wstring dir = LocalStatePath() + L"\\pdnet";
            CreateDirectoryW(dir.c_str(), nullptr);
            HANDLE file = CreateFileW((LocalStatePath() + pads::kFile).c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                Log(L"gamepads: can't open %s (error %lu) - only Xbox controllers", pads::kFile, GetLastError());
                return nullptr;
            }
            HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READWRITE, 0, sizeof(pads::Shared), nullptr);
            CloseHandle(file);
            void* view = mapping ? MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(pads::Shared)) : nullptr;
            if (mapping) CloseHandle(mapping);
            if (!view) Log(L"gamepads: can't map %s (error %lu) - only Xbox controllers", pads::kFile, GetLastError());
            return static_cast<pads::Shared*>(view);
        }

        // One slot's pad, copied consistently (pdlaunch writes with a sequence number, odd while writing).
        bool ReadPad(int slot, pads::Pad& out)
        {
            if (!g_shared || g_shared->magic != pads::kMagic || g_shared->version != pads::kVersion) return false;
            for (int attempt = 0; attempt < 8; ++attempt)
            {
                uint32_t before = g_shared->sequence;
                if (before & 1) { YieldProcessor(); continue; }
                MemoryBarrier();
                out = g_shared->pads[slot];
                MemoryBarrier();
                if (g_shared->sequence == before) return out.id != 0 && GetTickCount() - g_shared->heartbeat < 2000;
            }
            return false;
        }

        void WriteRumble(int slot, uint32_t id, const GamepadVibration& v)
        {
            if (!g_shared) return;
            std::lock_guard guard(g_rumbleLock);
            g_shared->rumbleSequence = g_shared->rumbleSequence + 1;
            MemoryBarrier();
            g_shared->rumble[slot] = { id, static_cast<float>(v.LeftMotor), static_cast<float>(v.RightMotor), static_cast<float>(v.LeftTrigger),
                                       static_cast<float>(v.RightTrigger) };
            MemoryBarrier();
            g_shared->rumbleSequence = g_shared->rumbleSequence + 1;
        }

        winrt::agile_ref<winrt::Windows::System::User> g_user; // the signed-in Windows user, for VirtualPad::User

        struct VirtualPad : winrt::implements<VirtualPad, IGamepad, IGamepad2, IGameController, IGameControllerBatteryInfo>
        {
            VirtualPad(int slot, uint32_t id) : m_slot(slot), m_id(id) {}
            int m_slot;
            uint32_t m_id;
            GamepadVibration m_vibration{};
            GamepadReading m_last{};
            uint64_t m_timestamp = 0;

            // IGamepad
            GamepadVibration Vibration() { return m_vibration; }
            void Vibration(const GamepadVibration& value)
            {
                m_vibration = value;
                WriteRumble(m_slot, m_id, value);
            }
            GamepadReading GetCurrentReading()
            {
                pads::Pad p{};
                if (!ReadPad(m_slot, p) || p.id != m_id) return m_last; // gone: GamepadRemoved follows
                GamepadReading r{};
                r.Buttons = static_cast<GamepadButtons>(p.buttons);
                r.LeftThumbstickX = p.leftX;
                r.LeftThumbstickY = p.leftY;
                r.RightThumbstickX = p.rightX;
                r.RightThumbstickY = p.rightY;
                r.LeftTrigger = p.leftTrigger;
                r.RightTrigger = p.rightTrigger;
                bool changed = r.Buttons != m_last.Buttons || r.LeftThumbstickX != m_last.LeftThumbstickX || r.LeftThumbstickY != m_last.LeftThumbstickY ||
                               r.RightThumbstickX != m_last.RightThumbstickX || r.RightThumbstickY != m_last.RightThumbstickY ||
                               r.LeftTrigger != m_last.LeftTrigger || r.RightTrigger != m_last.RightTrigger;
                if (changed || !m_timestamp)
                {
                    LARGE_INTEGER now, freq;
                    QueryPerformanceCounter(&now);
                    QueryPerformanceFrequency(&freq);
                    m_timestamp = static_cast<uint64_t>(now.QuadPart / (freq.QuadPart / 1000000));
                }
                r.Timestamp = m_timestamp;
                m_last = r;
                return r;
            }

            // IGamepad2
            GameControllerButtonLabel GetButtonLabel(GamepadButtons) { return GameControllerButtonLabel::None; }

            // IGameController
            using HeadsetHandler = winrt::Windows::Foundation::TypedEventHandler<IGameController, ::winrt::Windows::Gaming::Input::Headset>;
            winrt::event_token HeadsetConnected(const HeadsetHandler& h) { return m_headsetConnected.add(h); }
            void HeadsetConnected(const winrt::event_token& t) noexcept { m_headsetConnected.remove(t); }
            winrt::event_token HeadsetDisconnected(const HeadsetHandler& h) { return m_headsetDisconnected.add(h); }
            void HeadsetDisconnected(const winrt::event_token& t) noexcept { m_headsetDisconnected.remove(t); }
            winrt::event_token UserChanged(const winrt::Windows::Foundation::TypedEventHandler<IGameController, winrt::Windows::System::UserChangedEventArgs>& h) { return m_userChanged.add(h); }
            void UserChanged(const winrt::event_token& t) noexcept { m_userChanged.remove(t); }
            ::winrt::Windows::Gaming::Input::Headset Headset() { return nullptr; }
            bool IsWireless() { return false; }
            winrt::Windows::System::User User() { return g_user ? g_user.get() : nullptr; }

            // IGameControllerBatteryInfo
            winrt::Windows::Devices::Power::BatteryReport TryGetBatteryReport() { return nullptr; }

            winrt::event<HeadsetHandler> m_headsetConnected, m_headsetDisconnected;
            winrt::event<winrt::Windows::Foundation::TypedEventHandler<IGameController, winrt::Windows::System::UserChangedEventArgs>> m_userChanged;
        };

        struct PadFactory : winrt::implements<PadFactory, winrt::Windows::Foundation::IActivationFactory, IGamepadStatics, IGamepadStatics2>
        {
            // C++/WinRT makes activation factories without constructor arguments: the OS factory comes in g_osFactory.
            static inline winrt::Windows::Foundation::IActivationFactory g_osFactory{ nullptr };
            PadFactory() : m_real(g_osFactory.as<IGamepadStatics>()), m_real2(g_osFactory.try_as<IGamepadStatics2>()) {}

            void Start()
            {
                g_shared = MapShared();
                std::thread([this, self = get_strong()] { Watch(); }).detach();
            }

            IInspectable ActivateInstance() { throw winrt::hresult_not_implemented(); }

            winrt::event_token GamepadAdded(const EventHandler<Gamepad>& h)
            {
                std::lock_guard guard(m_lock);
                winrt::event_token token = m_added.add(h);
                m_osTokens[token.value] = m_real.GamepadAdded(h);
                return token;
            }
            void GamepadAdded(const winrt::event_token& t) noexcept
            {
                std::lock_guard guard(m_lock);
                m_added.remove(t);
                if (auto it = m_osTokens.find(t.value); it != m_osTokens.end()) { m_real.GamepadAdded(it->second); m_osTokens.erase(it); }
            }
            winrt::event_token GamepadRemoved(const EventHandler<Gamepad>& h)
            {
                std::lock_guard guard(m_lock);
                winrt::event_token token = m_removed.add(h);
                m_osTokens[token.value] = m_real.GamepadRemoved(h);
                return token;
            }
            void GamepadRemoved(const winrt::event_token& t) noexcept
            {
                std::lock_guard guard(m_lock);
                m_removed.remove(t);
                if (auto it = m_osTokens.find(t.value); it != m_osTokens.end()) { m_real.GamepadRemoved(it->second); m_osTokens.erase(it); }
            }

            winrt::Windows::Foundation::Collections::IVectorView<Gamepad> Gamepads()
            {
                std::vector<Gamepad> all;
                for (const Gamepad& pad : m_real.Gamepads()) all.push_back(pad);
                {
                    std::lock_guard guard(m_lock);
                    for (auto& pad : m_pads) if (pad) all.push_back(pad);
                }
                return winrt::single_threaded_vector(std::move(all)).GetView();
            }

            Gamepad FromGameController(const IGameController& controller)
            {
                {
                    std::lock_guard guard(m_lock);
                    for (auto& pad : m_pads)
                        if (pad && winrt::get_abi(pad.as<IGameController>()) == winrt::get_abi(controller)) return pad;
                }
                return m_real2 ? m_real2.FromGameController(controller) : nullptr;
            }

        private:
            // Every 50 ms: pads pdlaunch added or removed become GamepadAdded / GamepadRemoved.
            void Watch()
            {
                if (!g_user)
                {
                    try
                    {
                        auto users = winrt::Windows::System::User::FindAllAsync().get();
                        if (users.Size()) g_user = winrt::make_agile(users.GetAt(0));
                    }
                    catch (...) {}
                }
                uint32_t ids[pads::kMaxPads] = {};
                for (;;)
                {
                    Sleep(50);
                    for (int slot = 0; slot < pads::kMaxPads; ++slot)
                    {
                        pads::Pad p{};
                        uint32_t id = ReadPad(slot, p) ? p.id : 0;
                        if (id == ids[slot]) continue;
                        Gamepad gone{ nullptr }, added{ nullptr };
                        {
                            std::lock_guard guard(m_lock);
                            gone = m_pads[slot];
                            m_pads[slot] = nullptr;
                            if (id) m_pads[slot] = added = winrt::make<VirtualPad>(slot, id).as<Gamepad>();
                        }
                        if (gone)
                        {
                            Log(L"gamepads: controller %u removed", ids[slot]);
                            m_removed(nullptr, gone);
                        }
                        ids[slot] = id;
                        if (added)
                        {
                            p.name[sizeof(p.name) - 1] = 0;
                            Log(L"gamepads: %S added as an Xbox-layout pad (controller %u, SDL type %u)", p.name, id, p.type);
                            m_added(nullptr, added);
                        }
                    }
                }
            }

            IGamepadStatics m_real;
            IGamepadStatics2 m_real2;
            std::mutex m_lock;
            winrt::event<EventHandler<Gamepad>> m_added, m_removed;
            std::map<int64_t, winrt::event_token> m_osTokens; // our token -> the OS factory's, for the same handler
            Gamepad m_pads[pads::kMaxPads]{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
        };
    }

    winrt::Windows::Foundation::IInspectable CreateGamepadFactory(const winrt::Windows::Foundation::IActivationFactory& real)
    {
        Log(L"gamepads: the game's Gamepad class is pdnet's (Xbox controllers from Windows, others from pdlaunch / SDL3)");
        PadFactory::g_osFactory = real;
        auto factory = winrt::make_self<PadFactory>();
        factory->Start();
        return factory.as<winrt::Windows::Foundation::IInspectable>();
    }
}
