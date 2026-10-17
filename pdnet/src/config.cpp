#include "pch.h"
#include "config.h"
#include "log.h"
#include <fstream>
#include <sstream>

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
            "; Connections are found automatically (LAN, IPv6, STUN, UPnP, hole punching); no port forwarding needed.\r\n"
            "; stun: ask public STUN servers what this PC's public address is (they see your IP, nothing else).\r\n"
            "stun=on\r\n"
            "stun_servers=stun.l.google.com:19302,stun.cloudflare.com:3478\r\n"
            "; upnp: ask your router to open the game port automatically, if it supports UPnP.\r\n"
            "upnp=on\r\n"
            "\r\n"
            "; Optional fixed address to offer peers in addition to the automatic ones (e.g. with manual port forwarding).\r\n"
            "advertise_ip=auto\r\n"
            "\r\n"
            "; Game UDP port; must match networkmanifest.xml.\r\n"
            "game_port=43210\r\n"
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

        bool On(const std::string& v)
        {
            return _stricmp(v.c_str(), "on") == 0 || _stricmp(v.c_str(), "true") == 0 || v == "1";
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
                if (key == "network_mode") c.teredo = _stricmp(value.c_str(), "teredo") == 0;
                else if (key == "advertise_ip") c.advertiseIp = std::wstring(value.begin(), value.end());
                else if (key == "game_port") c.gamePort = static_cast<uint16_t>(std::stoul(value));
                else if (key == "qos_probes") c.qosProbes = (std::max)(1ul, std::stoul(value));
                else if (key == "stun") c.stun = On(value);
                else if (key == "upnp") c.upnp = On(value);
                else if (key == "stun_servers")
                {
                    c.stunServers.clear();
                    std::stringstream list(value);
                    for (std::string item; std::getline(list, item, ',');)
                        if (!Trim(item).empty()) c.stunServers.emplace_back(winrt::to_hstring(Trim(item)).c_str());
                }
            }
            Log(L"config: network_mode=%s stun=%d upnp=%d advertise_ip=%s game_port=%u qos_probes=%u",
                c.teredo ? L"teredo" : L"direct", c.stun, c.upnp, c.advertiseIp.c_str(), c.gamePort, c.qosProbes);
            return c;
        }
    }

    const Config& GetConfig()
    {
        static Config config = Load();
        return config;
    }
}
