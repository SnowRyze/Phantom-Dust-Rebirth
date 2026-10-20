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
                else if (key == "eos_test") c.eosTest = _stricmp(value.c_str(), "on") == 0;
                else if (key == "eos") c.eos = _stricmp(value.c_str(), "on") == 0;
                else if (key == "xbox") c.xboxOff = _stricmp(value.c_str(), "off") == 0;
                else if (key == "display_name" && !value.empty())
                {
                    std::wstring name{ winrt::to_hstring(value) };
                    c.displayName = name.substr(0, 32);
                }
            }
            if (c.testPeer) Log(L"config: test_peer=on (fake second player starts when a lobby is created)");
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
