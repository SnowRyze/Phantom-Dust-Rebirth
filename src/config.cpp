#include "pch.h"
#include "config.h"
#include "log.h"
#include <algorithm>
#include <fstream>
#include <mutex>
#include <vector>

namespace pdnet
{
    namespace
    {
        constexpr char kDefaultIni[] =
            "; pdnet settings. Restart the game after editing.\r\n"
            "\r\n"
            "; Your name as other players see it (max 32 characters).\r\n"
            "display_name=Player\r\n";

        std::string Trim(std::string s)
        {
            auto issp = [](unsigned char c) { return std::isspace(c); };
            while (!s.empty() && issp(s.back())) s.pop_back();
            size_t i = 0;
            while (i < s.size() && issp(s[i])) ++i;
            return s.substr(i);
        }

        // pdnet.ini next to this DLL (set per build folder) wins over the one in LocalState (shared by builds).
        std::wstring BuildIniPath()
        {
            HMODULE self = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&BuildIniPath), &self);
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(self, path, MAX_PATH);
            std::wstring dir = path;
            return dir.substr(0, dir.find_last_of(L'\\') + 1) + L"pdnet.ini";
        }

        // A colour typed as RRGGBB or #RRGGBB; anything else keeps the current colour.
        void ReadColour(const std::string& value, uint32_t& colour)
        {
            std::string hex = !value.empty() && value[0] == '#' ? value.substr(1) : value;
            if (hex.size() != 6 || !std::all_of(hex.begin(), hex.end(), [](unsigned char ch) { return std::isxdigit(ch); }))
            {
                Log(L"config: colour '%S' ignored (use 6 hex digits, e.g. FF5A5A)", value.c_str());
                return;
            }
            colour = 0xFF000000u | std::stoul(hex, nullptr, 16);
        }

        // A key name as Windows virtual-key code: a letter or digit, F1-F24, a few named keys, or a number; 0 for none.
        uint32_t KeyCode(const std::string& name)
        {
            if (name.empty() || _stricmp(name.c_str(), "none") == 0 || _stricmp(name.c_str(), "off") == 0) return 0;
            if (name.size() == 1 && std::isalnum(static_cast<unsigned char>(name[0]))) return static_cast<uint32_t>(std::toupper(static_cast<unsigned char>(name[0])));
            if ((name[0] == 'F' || name[0] == 'f') && name.size() <= 3 && std::all_of(name.begin() + 1, name.end(), ::isdigit))
            {
                int n = atoi(name.c_str() + 1);
                if (n >= 1 && n <= 24) return 0x6F + n; // VK_F1 = 0x70
            }
            static const std::pair<const char*, uint32_t> kNames[] = {
                { "Tab", 0x09 }, { "Space", 0x20 }, { "Enter", 0x0D }, { "Shift", 0x10 }, { "Ctrl", 0x11 }, { "Control", 0x11 },
                { "Alt", 0x12 }, { "CapsLock", 0x14 }, { "Backspace", 0x08 }, { "Insert", 0x2D }, { "Delete", 0x2E }, { "Home", 0x24 },
                { "End", 0x23 }, { "PageUp", 0x21 }, { "PageDown", 0x22 }, { "Up", 0x26 }, { "Down", 0x28 }, { "Left", 0x25 },
                { "Right", 0x27 }, { "LeftShift", 0xA0 }, { "RightShift", 0xA1 }, { "LeftCtrl", 0xA2 }, { "RightCtrl", 0xA3 },
            };
            for (auto& [n, code] : kNames)
                if (_stricmp(name.c_str(), n) == 0) return code;
            if (std::all_of(name.begin(), name.end(), ::isdigit)) return static_cast<uint32_t>(atoi(name.c_str()));
            Log(L"config: key '%S' not known - partner_lock_key stays as it was", name.c_str());
            return ~0u;
        }

        // The top of settings.ini. Files written before the second line existed get it on their next save.
        constexpr char kSettingsHeader[] = "; pdnet settings saved from the game's Settings menu (they override pdnet.ini)";
        constexpr char kSettingsHelp[] = "; You can edit this file by hand with the game closed. Colours take any hex code (RRGGBB), e.g. 7FFFD4.";
        constexpr char kSettingsUpdates[] = "; update_channel: stable, experimental or off (off: no automatic updates). Empty: this kit's own kind.";

        // A hand-edited "key = value" or an indented line counts as that key's.
        bool IsKeyLine(const std::string& line, const std::string& key)
        {
            auto eq = line.find('=');
            return eq != std::string::npos && Trim(line.substr(0, eq)) == key;
        }

        std::wstring g_iniPath; // the pdnet.ini Load read

        // Where the in-game Settings menu saves (the game, a UWP app, can write only its own data folders).
        std::wstring SettingsPath() { return LocalStatePath() + L"\\pdnet\\settings.ini"; }

        Config Load()
        {
            Config c;
            std::wstring path = BuildIniPath();
            std::ifstream in(path);
            if (!in)
            {
                path = LocalStatePath() + L"\\pdnet.ini";
                in.open(path);
            }
            if (!in)
            {
                g_iniPath = path;
                std::ofstream(path, std::ios::binary) << kDefaultIni;
                Log(L"config: wrote defaults to %s", path.c_str());
                return c;
            }
            Log(L"config: reading %s", path.c_str());
            g_iniPath = path;
            auto apply = [&](std::istream& in)
            {
            std::string line;
            while (std::getline(in, line))
            {
                line = Trim(line);
                if (line.empty() || line[0] == ';' || line[0] == '#') continue;
                auto eq = line.find('=');
                if (eq == std::string::npos) continue;
                std::string key = Trim(line.substr(0, eq)), value = Trim(line.substr(eq + 1));
                if (key == "menu_debug") c.menuDebug = _stricmp(value.c_str(), "on") == 0;
                else if (key == "voice_chat") c.voiceChat = _stricmp(value.c_str(), "off") != 0;
                else if (key == "voice_listen") c.voiceListen = _stricmp(value.c_str(), "off") != 0;
                else if (key == "voice_mode") c.voiceClassic = _stricmp(value.c_str(), "classic") == 0;
                else if (key == "p2p_debug") c.p2pDebug = _stricmp(value.c_str(), "on") == 0;
                else if (key == "p2p_game_reliable") c.p2pGameReliable = _stricmp(value.c_str(), "off") != 0;
                else if (key == "voice_debug") c.voiceDebug = _stricmp(value.c_str(), "on") == 0;
                else if (key == "voice_talk_icon_x" && !value.empty()) c.voiceTalkIconX = std::stoi(value);
                else if (key == "icon_gallery") c.iconGallery = _stricmp(value.c_str(), "on") == 0;
                else if (key == "nametags_on_bodies") c.nametagsOnBodies = _stricmp(value.c_str(), "off") != 0;
                else if (key == "dead_nametag_color") ReadColour(value, c.deadNametagColor);
                else if (key == "ally_nametag_color") ReadColour(value, c.allyNametagColor);
                else if (key == "enemy_nametag_color") ReadColour(value, c.enemyNametagColor);
                else if (key == "nametag_debug") c.nametagDebug = _stricmp(value.c_str(), "deep") == 0 ? 2 : _stricmp(value.c_str(), "on") == 0 ? 1 : 0;
                else if (key == "name_trace") c.nameTrace = _stricmp(value.c_str(), "memory") == 0 ? 1 : (_stricmp(value.c_str(), "draw") == 0 || _stricmp(value.c_str(), "on") == 0) ? 2 : 0;
                else if (key == "discord_webhook") c.discordWebhook = std::wstring(value.begin(), value.end());
                else if (key == "log_webhook") c.logWebhook = std::wstring(value.begin(), value.end());
                else if (key == "partner_aim") c.partnerClassic = _stricmp(value.c_str(), "classic") == 0;
                else if (key == "partner_lock_key") { if (uint32_t code = KeyCode(value); code != ~0u) c.partnerLockKey = code; }
                else if (key == "update_channel") { if (!value.empty()) c.updateChannel = std::wstring(value.begin(), value.end()); } // empty in settings.ini: pdnet.ini's
                else if (key == "discord_invite")c.discordInvite = std::wstring(value.begin(), value.end());
                else if (key == "discord_lobby_messages") c.discordLobbyMessages = _stricmp(value.c_str(), "off") != 0;
                else if (key == "discord_player_messages") c.discordPlayerMessages = _stricmp(value.c_str(), "off") != 0;
                else if (key == "splash_volume" && !value.empty()) c.splashVolume = (std::clamp)(std::stoi(value), 0, 100);
                else if (key == "aura_beeps") c.auraBeeps = _stricmp(value.c_str(), "off") != 0;
                else if (key == "sound_log") c.soundLog = _stricmp(value.c_str(), "on") == 0;
                else if (key == "more_controllers") c.moreControllers = _stricmp(value.c_str(), "off") != 0;
                else if (key == "display_name" && !value.empty())
                {
                    std::wstring name{ winrt::to_hstring(value) };
                    c.displayName = name.substr(0, 32);
                }
            }
            };
            apply(in);
            // The in-game Settings menu saves to LocalState\pdnet\settings.ini (the game can't write the kit folder):
            // its values override pdnet.ini.
            if (std::ifstream saved(SettingsPath()); saved)
            {
                Log(L"config: reading %s (in-game Settings)", SettingsPath().c_str());
                apply(saved);
            }
            // The file always shows update_channel, to change by hand (an empty line changes nothing).
            bool listed = false;
            if (std::ifstream saved(SettingsPath()); saved)
                for (std::string line; std::getline(saved, line);) listed = listed || IsKeyLine(line, "update_channel");
            if (!listed) SaveSetting("update_channel", "");
            if (c.testPeer) Log(L"config: test_peer=on (fake second player starts when a lobby is created)");
            if (c.nameTrace) Log(L"config: name_trace=%s (debug only)", c.nameTrace == 2 ? L"draw (logs game text draws of other players' names)" : L"memory (logs reads of copies of other players' names)");
            if (!c.discordWebhook.empty())
                Log(L"config: discord_webhook set (lobbies this PC hosts are announced in Discord): lobby messages %s, player messages %s",
                    c.discordLobbyMessages ? L"on" : L"off", c.discordPlayerMessages ? L"on" : L"off");
            if (c.p2p && !c.eos)
            {
                Log(L"config: network_mode=eos needs eos=on; using direct");
                c.p2p = false;
            }
            Log(L"config: network_mode=%s advertise_ip=%s game_port=%u control_port=%u qos_probes=%u%s%s",
                c.teredo ? L"teredo" : c.p2p ? L"eos" : L"direct", c.advertiseIp.c_str(), c.gamePort, c.controlPort, c.qosProbes,
                c.p2p ? L" p2p_relay=" : L"", c.p2p ? c.p2pRelay.c_str() : L"");
            return c;
        }
    }

    Config& EditableConfig()
    {
        static Config config = Load();
        return config;
    }

    const Config& GetConfig() { return EditableConfig(); }

    void SaveSettings(const std::vector<std::pair<std::string, std::string>>& values)
    {
        static std::mutex lock;
        std::lock_guard guard(lock);
        std::wstring path = SettingsPath();
        CreateDirectoryW((LocalStatePath() + L"\\pdnet").c_str(), nullptr);
        std::vector<std::string> lines;
        {
            std::ifstream in(path, std::ios::binary);
            std::string line;
            while (std::getline(in, line))
            {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                lines.push_back(line);
            }
        }
        if (lines.empty()) lines.push_back(kSettingsHeader);
        if (lines[0] == kSettingsHeader && (lines.size() < 2 || lines[1] != kSettingsHelp)) lines.insert(lines.begin() + 1, kSettingsHelp);
        if (lines[0] == kSettingsHeader && (lines.size() < 3 || lines[2] != kSettingsUpdates)) lines.insert(lines.begin() + 2, kSettingsUpdates);
        for (auto& [key, value] : values)
        {
            bool found = false;
            for (auto& line : lines)
                if (IsKeyLine(line, key))
                {
                    line = key + "=" + value;
                    found = true;
                    break;
                }
            if (!found) lines.push_back(key + "=" + value);
        }
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        for (auto& line : lines) out << line << "\r\n";
        out.close();
        if (out.fail()) Log(L"config: could not write %s", path.c_str());
        else Log(L"config: %zu setting(s) saved to %s (in-game Settings)", values.size(), path.c_str());
    }

    void SaveSetting(const std::string& key, const std::string& value) { SaveSettings({ { key, value } }); }
}
