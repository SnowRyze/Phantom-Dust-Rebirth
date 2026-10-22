#include "pch.h"
#include "config.h"
#include "log.h"
#include <fstream>

namespace pdnet
{
    namespace
    {
        constexpr char kDefaultIni[] =
            "; pdnet settings. Restart the game after editing.\r\n"
            "\r\n"
            "; How players connect to each other.\r\n"
            ";   direct = pdnet direct UDP connections (no Teredo)\r\n"
            ";   teredo = original Microsoft Teredo / Xbox Secure Device Association\r\n"
            "network_mode=direct\r\n"
            "\r\n"
            "; Address other players use to reach this PC (direct mode).\r\n"
            ";   auto      = this PC's primary network address (fine for LAN play)\r\n"
            ";   1.2.3.4   = a fixed address, e.g. your public IP when port-forwarding\r\n"
            "advertise_ip=auto\r\n"
            "\r\n"
            "; UDP ports. game_port must match networkmanifest.xml; forward both for internet play.\r\n"
            "game_port=43210\r\n"
            "control_port=43211\r\n"
            "\r\n"
            "; Pings per player when the game measures connection quality.\r\n"
            "qos_probes=5\r\n";

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
                std::ofstream(path, std::ios::binary) << kDefaultIni;
                Log(L"config: wrote defaults to %s", path.c_str());
                return c;
            }
            Log(L"config: reading %s", path.c_str());
            std::string line;
            while (std::getline(in, line))
            {
                line = Trim(line);
                if (line.empty() || line[0] == ';' || line[0] == '#') continue;
                auto eq = line.find('=');
                if (eq == std::string::npos) continue;
                std::string key = Trim(line.substr(0, eq)), value = Trim(line.substr(eq + 1));
                if (key == "network_mode")
                {
                    c.teredo = _stricmp(value.c_str(), "teredo") == 0;
                    c.p2p = _stricmp(value.c_str(), "eos") == 0;
                }
                else if (key == "p2p_relay") c.p2pRelay = std::wstring(value.begin(), value.end());
                else if (key == "advertise_ip") c.advertiseIp = std::wstring(value.begin(), value.end());
                else if (key == "game_port") c.gamePort = static_cast<uint16_t>(std::stoul(value));
                else if (key == "control_port") c.controlPort = static_cast<uint16_t>(std::stoul(value));
                else if (key == "qos_probes") c.qosProbes = (std::max)(1ul, std::stoul(value));
                else if (key == "test_peer") c.testPeer = _stricmp(value.c_str(), "on") == 0;
                else if (key == "menu_debug") c.menuDebug = _stricmp(value.c_str(), "on") == 0;
                else if (key == "lobby_arsenal_editor") c.lobbyArsenalEditor = _stricmp(value.c_str(), "off") != 0;
                else if (key == "lobby_gamertag_option") c.lobbyGamertagOption = _stricmp(value.c_str(), "off") != 0;
                else if (key == "voice_chat") c.voiceChat = _stricmp(value.c_str(), "off") != 0;
                else if (key == "voice_listen") c.voiceListen = _stricmp(value.c_str(), "off") != 0;
                else if (key == "voice_talk_mark") c.voiceTalkMark = value.substr(0, 24);
                else if (key == "p2p_debug") c.p2pDebug = _stricmp(value.c_str(), "on") == 0;
                else if (key == "voice_debug") c.voiceDebug = _stricmp(value.c_str(), "on") == 0;
                else if (key == "icon_gallery") c.iconGallery = _stricmp(value.c_str(), "on") == 0;
                else if (key == "nametags_on_bodies") c.nametagsOnBodies = _stricmp(value.c_str(), "off") != 0;
                else if (key == "dead_nametag_color" && value.size() == 6) c.deadNametagColor = 0xFF000000u | std::stoul(value, nullptr, 16);
                else if (key == "nametag_debug") c.nametagDebug = _stricmp(value.c_str(), "deep") == 0 ? 2 : _stricmp(value.c_str(), "on") == 0 ? 1 : 0;
                else if (key == "name_trace") c.nameTrace = _stricmp(value.c_str(), "memory") == 0 ? 1 : (_stricmp(value.c_str(), "draw") == 0 || _stricmp(value.c_str(), "on") == 0) ? 2 : 0;
                else if (key == "eos_test") c.eosTest = _stricmp(value.c_str(), "on") == 0;
                else if (key == "eos") c.eos = _stricmp(value.c_str(), "on") == 0;
                else if (key == "xbox") c.xboxOff = _stricmp(value.c_str(), "off") == 0;
                else if (key == "discord_webhook") c.discordWebhook = std::wstring(value.begin(), value.end());
                else if (key == "discord_lobby_messages") c.discordLobbyMessages = _stricmp(value.c_str(), "off") != 0;
                else if (key == "discord_player_messages") c.discordPlayerMessages = _stricmp(value.c_str(), "off") != 0;
                else if (key == "display_name" && !value.empty())
                {
                    std::wstring name{ winrt::to_hstring(value) };
                    c.displayName = name.substr(0, 32);
                }
            }
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

    const Config& GetConfig()
    {
        static Config config = Load();
        return config;
    }
}
