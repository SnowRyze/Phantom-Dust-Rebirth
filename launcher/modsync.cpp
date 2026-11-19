// Mod loader step 2 (Conceptual): see modsync.h.
#include "modsync.h"
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cwctype>
#include <cstdio>
#include <map>
#include <set>
#include <unordered_set>
#include <vector>

namespace modsync
{
    namespace
    {
        LogFn g_log = nullptr;

        enum class Source { Catalog, Local, Pdpl };
        const wchar_t* SourceName(Source s) { return s == Source::Catalog ? L"catalog" : s == Source::Local ? L"local" : L"pdpl"; }

        struct Mod
        {
            Source source = Source::Local;
            std::wstring path, file, ext; // the archive, its file name, ".zip" / ".7z"
            bool inDisabled = false;      // a player's PDPL archive waiting in RoamingState\mods\disabled
            uint64_t size = 0, mtime = 0;
            std::string id, name, version, author, description, online = "match", problem;
            std::vector<std::string> after, before, touches, files; // files: lowercase game paths
            bool plugins = false, enabled = true, read = false;
        };

        // ---- small helpers ----

        std::string Utf8(const std::wstring& s)
        {
            if (s.empty()) return {};
            int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
            std::string out(n, '\0');
            WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
            return out;
        }

        std::wstring Wide(const std::string& s)
        {
            if (s.empty()) return {};
            int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
            std::wstring out(n, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), n);
            return out;
        }

        std::string Lower(std::string s)
        {
            for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            return s;
        }

        std::wstring LowerW(std::wstring s)
        {
            for (wchar_t& c : s) c = static_cast<wchar_t>(towlower(c));
            return s;
        }

        bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

        bool FileInfo(const std::wstring& p, uint64_t& size, uint64_t& mtime)
        {
            WIN32_FILE_ATTRIBUTE_DATA d;
            if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &d)) return false;
            size = (static_cast<uint64_t>(d.nFileSizeHigh) << 32) | d.nFileSizeLow;
            mtime = (static_cast<uint64_t>(d.ftLastWriteTime.dwHighDateTime) << 32) | d.ftLastWriteTime.dwLowDateTime;
            return true;
        }

        struct Entry { std::wstring name; bool dir; };
        std::vector<Entry> List(const std::wstring& dir)
        {
            std::vector<Entry> out;
            WIN32_FIND_DATAW fd;
            HANDLE find = FindFirstFileW((dir + L"\\*").c_str(), &fd);
            if (find == INVALID_HANDLE_VALUE) return out;
            do
            {
                std::wstring n = fd.cFileName;
                if (n != L"." && n != L"..") out.push_back({ n, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 });
            } while (FindNextFileW(find, &fd));
            FindClose(find);
            return out;
        }

        std::wstring Extension(const std::wstring& name)
        {
            size_t dot = name.find_last_of(L'.');
            return dot == std::wstring::npos ? L"" : LowerW(name.substr(dot));
        }
        bool IsArchive(const std::wstring& name) { auto e = Extension(name); return e == L".zip" || e == L".7z"; }
        bool IsOurs(const std::wstring& name) { return name.size() > 3 && _wcsnicmp(name.c_str(), L"!rb", 3) == 0; }

        std::string ReadText(const std::wstring& p)
        {
            std::string data;
            if (FILE* f = nullptr; !_wfopen_s(&f, p.c_str(), L"rb") && f)
            {
                char buf[8192];
                for (size_t n; (n = fread(buf, 1, sizeof(buf), f)) > 0;) data.append(buf, n);
                fclose(f);
            }
            if (data.rfind("\xEF\xBB\xBF", 0) == 0) data.erase(0, 3);
            return data;
        }

        bool WriteText(const std::wstring& p, const std::string& text)
        {
            std::wstring tmp = p + L".tmp";
            FILE* f = nullptr;
            if (_wfopen_s(&f, tmp.c_str(), L"wb") || !f) return false;
            bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
            fclose(f);
            return ok && MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING);
        }

        std::vector<std::string> Split(const std::string& s, char sep)
        {
            std::vector<std::string> out;
            std::string cur;
            for (char c : s)
            {
                if (c == sep) { out.push_back(cur); cur.clear(); }
                else cur += c;
            }
            out.push_back(cur);
            for (auto& x : out)
            {
                while (!x.empty() && (x.back() == ' ' || x.back() == '\r' || x.back() == '\t')) x.pop_back();
                while (!x.empty() && (x.front() == ' ' || x.front() == '\t')) x.erase(0, 1);
            }
            out.erase(std::remove(out.begin(), out.end(), std::string()), out.end());
            return out;
        }

        std::string Join(const std::vector<std::string>& v, char sep)
        {
            std::string out;
            for (auto& s : v) { if (!out.empty()) out += sep; out += s; }
            return out;
        }

        // Tabs and line breaks out of a state-file field.
        std::string Field(std::string s)
        {
            for (char& c : s) if (c == '\t' || c == '\r' || c == '\n') c = ' ';
            return s;
        }

        uint64_t Fnv(const std::string& s)
        {
            uint64_t h = 1469598103934665603ull;
            for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
            return h;
        }

        // Runs a hidden program and collects what it prints (at most 16 MB). False if it didn't start, timed out or failed.
        bool RunCapture(std::wstring command, DWORD timeoutMs, std::string& out)
        {
            SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
            HANDLE read = nullptr, write = nullptr;
            if (!CreatePipe(&read, &write, &sa, 1u << 16)) return false;
            SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
            HANDLE nul = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
            STARTUPINFOW si{ sizeof(si) };
            si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            si.hStdInput = nul;
            si.hStdOutput = write;
            si.hStdError = nul;
            PROCESS_INFORMATION pi{};
            BOOL started = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
            CloseHandle(write);
            if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
            if (!started)
            {
                CloseHandle(read);
                return false;
            }
            ULONGLONG start = GetTickCount64();
            bool timedOut = false;
            char buf[65536];
            for (;;)
            {
                DWORD avail = 0;
                if (!PeekNamedPipe(read, nullptr, 0, nullptr, &avail, nullptr)) break; // the program closed its end
                if (avail)
                {
                    DWORD got = 0;
                    if (!ReadFile(read, buf, (std::min<DWORD>)(avail, sizeof(buf)), &got, nullptr) || !got) break;
                    if (out.size() < (16u << 20)) out.append(buf, got);
                    continue;
                }
                if (GetTickCount64() - start > timeoutMs) { timedOut = true; break; }
                Sleep(5);
            }
            if (timedOut) TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 5000);
            DWORD exit = 1;
            GetExitCodeProcess(pi.hProcess, &exit);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(read);
            return !timedOut && exit == 0;
        }

        std::wstring Tar()
        {
            wchar_t system[MAX_PATH];
            GetSystemDirectoryW(system, MAX_PATH);
            return std::wstring(system) + L"\\tar.exe";
        }

        // 7-Zip's 7z.exe, if it's installed (its installer writes Software\7-Zip Path64 / Path), else "". Windows' tar.exe
        // can't read .7z at all ("LZMA codec is unsupported"), so .7z mods need it for their details; PDPL reads them anyway.
        const std::wstring& SevenZip()
        {
            static std::wstring exe = []
            {
                for (HKEY root : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE })
                    for (const wchar_t* value : { L"Path64", L"Path" })
                    {
                        wchar_t dir[MAX_PATH] = {};
                        DWORD size = sizeof(dir);
                        if (RegGetValueW(root, L"Software\\7-Zip", value, RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, dir, &size) != ERROR_SUCCESS) continue;
                        std::wstring path = dir;
                        if (!path.empty() && path.back() != L'\\') path += L'\\';
                        if (Exists(path + L"7z.exe")) return path + L"7z.exe";
                    }
                wchar_t programs[MAX_PATH] = {};
                if (GetEnvironmentVariableW(L"ProgramFiles", programs, MAX_PATH) && Exists(std::wstring(programs) + L"\\7-Zip\\7z.exe"))
                    return std::wstring(programs) + L"\\7-Zip\\7z.exe";
                return std::wstring();
            }();
            return exe;
        }

        // The archive's entries (folders left out), as the tool names them. False if the tool couldn't read it.
        bool ListEntries(const std::wstring& archive, bool sevenZip, std::vector<std::string>& entries)
        {
            std::string out;
            if (!sevenZip)
            {
                if (!RunCapture(L"\"" + Tar() + L"\" -tf \"" + archive + L"\"", 30000, out)) return false;
                for (const auto& line : Split(out, '\n'))
                    if (!line.empty() && line.back() != '/') entries.push_back(line);
                return true;
            }
            // 7z l -slt: a block per entry, "Path = ..." first; folders have "Folder = +" (zip) or "Attributes = D..." (7z).
            if (!RunCapture(L"\"" + SevenZip() + L"\" l -slt -ba -sccUTF-8 \"" + archive + L"\"", 30000, out)) return false;
            std::string path;
            bool folder = false;
            auto flush = [&] { if (!path.empty() && !folder) entries.push_back(path); path.clear(); folder = false; };
            for (const auto& line : Split(out, '\n'))
            {
                if (line.rfind("Path = ", 0) == 0) { flush(); path = line.substr(7); }
                else if (line == "Folder = +" || line.rfind("Attributes = D", 0) == 0) folder = true;
            }
            flush();
            return true;
        }

        bool ReadEntry(const std::wstring& archive, bool sevenZip, const std::string& entry, std::string& out)
        {
            if (!sevenZip) return RunCapture(L"\"" + Tar() + L"\" -xOf \"" + archive + L"\" \"" + Wide(entry) + L"\"", 30000, out);
            return RunCapture(L"\"" + SevenZip() + L"\" e -so -sccUTF-8 \"" + archive + L"\" \"" + Wide(entry) + L"\"", 30000, out);
        }

        // ---- pdmod.json: flat strings and string arrays are all it has ----

        size_t SkipBlank(const std::string& j, size_t i)
        {
            while (i < j.size() && (j[i] == ' ' || j[i] == '\t' || j[i] == '\r' || j[i] == '\n')) ++i;
            return i;
        }

        // A JSON string starting at j[i] == '"'; i ends after its closing quote. Escapes as JSON has them (\uXXXX in UTF-8).
        std::string JsonString(const std::string& j, size_t& i)
        {
            std::string out;
            if (i >= j.size() || j[i] != '"') return out;
            for (++i; i < j.size() && j[i] != '"'; ++i)
            {
                if (j[i] != '\\' || i + 1 >= j.size()) { out += j[i]; continue; }
                char e = j[++i];
                if (e == 'n') out += '\n';
                else if (e == 't') out += '\t';
                else if (e == 'r') out += '\r';
                else if (e == 'u' && i + 4 < j.size())
                {
                    unsigned cp = static_cast<unsigned>(strtoul(j.substr(i + 1, 4).c_str(), nullptr, 16));
                    i += 4;
                    if (cp < 0x80) out += static_cast<char>(cp);
                    else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
                    else { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
                }
                else out += e; // \" \\ \/
            }
            if (i < j.size()) ++i;
            return out;
        }

        // Position of the value of top-level-ish "key" (the first match anywhere; pdmod.json has no nesting), or npos.
        size_t JsonValue(const std::string& j, const char* key)
        {
            std::string quoted = std::string("\"") + key + "\"";
            for (size_t at = j.find(quoted); at != std::string::npos; at = j.find(quoted, at + 1))
            {
                size_t i = SkipBlank(j, at + quoted.size());
                if (i < j.size() && j[i] == ':') return SkipBlank(j, i + 1);
            }
            return std::string::npos;
        }

        std::string JsonGetString(const std::string& j, const char* key)
        {
            size_t i = JsonValue(j, key);
            return i == std::string::npos ? std::string() : JsonString(j, i);
        }

        std::vector<std::string> JsonGetStrings(const std::string& j, const char* key)
        {
            std::vector<std::string> out;
            size_t i = JsonValue(j, key);
            if (i == std::string::npos || j[i] != '[') return out;
            for (i = SkipBlank(j, i + 1); i < j.size() && j[i] != ']';)
            {
                if (j[i] == '"') out.push_back(JsonString(j, i));
                else ++i;
                i = SkipBlank(j, i);
                if (i < j.size() && j[i] == ',') i = SkipBlank(j, i + 1);
            }
            return out;
        }

        // ---- what's inside an archive ----

        std::string CleanId(const std::string& s)
        {
            std::string out;
            for (char c : Lower(s))
            {
                bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
                if (ok) out += c;
                else if (!out.empty() && out.back() != '-') out += '-';
            }
            while (!out.empty() && out.back() == '-') out.pop_back();
            if (out.size() > 32) out.resize(32);
            while (!out.empty() && out.back() == '-') out.pop_back();
            return out.empty() ? "mod" : out;
        }

        // A game path as an archive lists it: "/" separators, no leading "./" or "/", lowercase.
        std::string GamePath(std::string p)
        {
            for (char& c : p) if (c == '\\') c = '/';
            while (p.rfind("./", 0) == 0) p.erase(0, 2);
            while (!p.empty() && p[0] == '/') p.erase(0, 1);
            return Lower(p);
        }

        // Which kinds of game data a mod changes, from its paths (so it can't be out of date).
        std::vector<std::string> Touches(const std::vector<std::string>& files, bool& plugins)
        {
            std::set<std::string> kinds;
            plugins = false;
            for (const auto& p : files)
            {
                size_t slash = p.find_last_of('/');
                std::string name = slash == std::string::npos ? p : p.substr(slash + 1);
                size_t dot = name.find_last_of('.');
                std::string ext = dot == std::string::npos ? "" : name.substr(dot);
                bool root = slash == std::string::npos;
                if (p.rfind("plugins/", 0) == 0) { if (ext == ".dll" || ext == ".exe") { plugins = true; kinds.insert("plugins"); } continue; }
                if (name == "pdmod.json" || (root && (ext == ".txt" || ext == ".md" || ext == ".png" || ext == ".jpg"))) continue;
                if (ext == ".cso") kinds.insert("shaders");
                else if (name.find("gsdata_") != std::string::npos) kinds.insert("skills");
                else if (p.find("/map/") != std::string::npos || (name.size() > 4 && name[0] == 's' && name[1] == 't' && isdigit(static_cast<unsigned char>(name[2])) && isdigit(static_cast<unsigned char>(name[3]))))
                    kinds.insert("maps");
                else if (ext == ".mot" || name.find("exmot") != std::string::npos) kinds.insert("animation");
                else if (ext == ".alr" && p.find("player/") != std::string::npos) kinds.insert("models");
                else if (ext == ".xwb" || ext == ".xsb" || ext == ".xgs" || ext == ".wav" || ext == ".ogg") kinds.insert("audio");
                else if (p.find("/menu") != std::string::npos || p.find("/font") != std::string::npos) kinds.insert("ui");
                else kinds.insert("other");
            }
            return { kinds.begin(), kinds.end() };
        }

        // Cache: LocalState\pdnet\modinfo\<hash of path, size, time>.txt
        std::wstring CachePath(const std::wstring& cacheDir, const Mod& m)
        {
            char key[32];
            sprintf_s(key, "%016llx", static_cast<unsigned long long>(Fnv(Utf8(LowerW(m.path)) + "|" + std::to_string(m.size) + "|" + std::to_string(m.mtime))));
            return cacheDir + L"\\" + Wide(key) + L".txt";
        }

        bool LoadCache(const std::wstring& file, Mod& m)
        {
            std::string text = ReadText(file);
            if (text.rfind("modinfo 1\n", 0) != 0) return false;
            std::string idHint;
            for (const auto& line : Split(text, '\n'))
            {
                if (line.rfind("f ", 0) == 0) { m.files.push_back(line.substr(2)); continue; }
                size_t eq = line.find('=');
                if (eq == std::string::npos) continue;
                std::string k = line.substr(0, eq), v = line.substr(eq + 1);
                if (k == "id") m.id = v;
                else if (k == "name") m.name = v;
                else if (k == "version") m.version = v;
                else if (k == "author") m.author = v;
                else if (k == "description") m.description = v;
                else if (k == "online") m.online = v;
                else if (k == "after") m.after = Split(v, ',');
                else if (k == "before") m.before = Split(v, ',');
            }
            m.read = true;
            return true;
        }

        void SaveCache(const std::wstring& file, const Mod& m)
        {
            std::string t = "modinfo 1\n";
            t += "id=" + Field(m.id) + "\nname=" + Field(m.name) + "\nversion=" + Field(m.version) + "\nauthor=" + Field(m.author) +
                 "\ndescription=" + Field(m.description) + "\nonline=" + m.online + "\nafter=" + Join(m.after, ',') + "\nbefore=" + Join(m.before, ',') + "\n";
            for (const auto& f : m.files) t += "f " + f + "\n";
            WriteText(file, t);
        }

        // Lists the archive and reads its pdmod.json: .zip with Windows' tar.exe (Windows 10 1803 and later), .7z with
        // 7-Zip if it's installed (and a .zip tar can't read). Fills id (from pdmod.json; the caller settles it), name, ...,
        // files. Archives that can't be read aren't cached, so installing 7-Zip later fills them in.
        void Inspect(Mod& m, const std::wstring& cacheDir)
        {
            std::wstring cache = CachePath(cacheDir, m);
            if (LoadCache(cache, m)) return;
            std::vector<std::string> entries;
            bool sevenZip = m.ext == L".7z";
            bool listed = false;
            if (!sevenZip) listed = ListEntries(m.path, false, entries);
            if (!listed && !SevenZip().empty())
            {
                entries.clear();
                sevenZip = true;
                listed = ListEntries(m.path, true, entries);
            }
            if (!listed)
            {
                m.problem = m.ext == L".7z" && SevenZip().empty() ? "7z: install 7-Zip to see its details (PDPL still loads it)" : "couldn't be read";
                g_log(L"mods: %s: %S", m.file.c_str(), m.problem.c_str());
                return;
            }
            std::string manifestEntry;
            for (const auto& raw : entries)
            {
                std::string p = GamePath(raw);
                if (p.empty() || p.find("..") != std::string::npos) continue;
                if (p == "pdmod.json") manifestEntry = raw;
                m.files.push_back(p);
            }
            if (!manifestEntry.empty())
            {
                std::string json;
                if (ReadEntry(m.path, sevenZip, manifestEntry, json))
                {
                    m.id = JsonGetString(json, "id");
                    m.name = JsonGetString(json, "name");
                    m.version = JsonGetString(json, "version");
                    m.author = JsonGetString(json, "author");
                    m.description = JsonGetString(json, "description");
                    std::string online = Lower(JsonGetString(json, "online"));
                    m.online = online == "local" ? "local" : "match";
                    m.after = JsonGetStrings(json, "load_after");
                    m.before = JsonGetStrings(json, "load_before");
                }
                else g_log(L"mods: %s: pdmod.json couldn't be read", m.file.c_str());
            }
            m.read = true;
            SaveCache(cache, m);
            g_log(L"mods: %s: %zu files%s (read with %s)", m.file.c_str(), m.files.size(), manifestEntry.empty() ? L", no pdmod.json" : L"", sevenZip ? L"7-Zip" : L"tar");
        }

        // settings.ini key (the last line wins), and whether it was there at all.
        bool SettingsValue(const std::string& text, const char* key, std::string& value)
        {
            bool found = false;
            for (const auto& line : Split(text, '\n'))
            {
                size_t eq = line.find('=');
                if (eq == std::string::npos || line[0] == ';' || line[0] == '#') continue;
                std::string k = line.substr(0, eq);
                while (!k.empty() && k.back() == ' ') k.pop_back();
                if (k != key) continue;
                value = line.substr(eq + 1);
                while (!value.empty() && value.front() == ' ') value.erase(0, 1);
                found = true;
            }
            return found;
        }

        bool Move(const std::wstring& from, const std::wstring& to)
        {
            if (MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING)) return true;
            g_log(L"mods: couldn't move %s to %s (%lu)", from.c_str(), to.c_str(), GetLastError());
            return false;
        }

        // Files in PDPL's mods folder that aren't archives: loose game files, which win over every archive. PDPL's own
        // DLLs in the folder's root (pdpl.exe copies pd_loader_core.dll there) and plugins\ and disabled\ don't count.
        size_t LooseFiles(const std::wstring& dir, bool root)
        {
            size_t n = 0;
            for (const auto& e : List(dir))
            {
                std::wstring lower = LowerW(e.name);
                if (e.dir)
                {
                    if (root && (lower == L"plugins" || lower == L"disabled")) continue;
                    n += LooseFiles(dir + L"\\" + e.name, false);
                }
                else if (!root || (!IsArchive(e.name) && Extension(e.name) != L".dll" && Extension(e.name) != L".exe" && Extension(e.name) != L".tmp"))
                    ++n;
            }
            return n;
        }
    }

    void Sync(const std::wstring& localState, LogFn log)
    {
        g_log = log;
        size_t cut = localState.find_last_of(L'\\');
        if (localState.empty() || cut == std::wstring::npos) { log(L"mods: no LocalState folder - mods not synced"); return; }
        std::wstring pdnetDir = localState + L"\\pdnet", roaming = localState.substr(0, cut) + L"\\RoamingState";
        std::wstring pdplDir = roaming + L"\\mods", disabledDir = pdplDir + L"\\disabled", localDir = pdnetDir + L"\\mods",
                     storeDir = pdnetDir + L"\\modstore", cacheDir = pdnetDir + L"\\modinfo";
        for (const auto& d : { pdnetDir, roaming, pdplDir, disabledDir, localDir, cacheDir }) CreateDirectoryW(d.c_str(), nullptr);
        if (!Exists(Tar())) log(L"mods: %s not found - .zip details can't be read (names and conflicts unknown)", Tar().c_str());
        log(L"mods: .7z details: %s", SevenZip().empty() ? L"7-Zip isn't installed (PDPL still loads .7z mods)" : SevenZip().c_str());

        // ---- find the mods ----
        std::vector<Mod> mods;
        auto add = [&](Source source, const std::wstring& dir, const std::wstring& name, bool inDisabled, const std::string& catalogId)
        {
            Mod m;
            m.source = source;
            m.path = dir + L"\\" + name;
            m.file = name;
            m.ext = Extension(name);
            m.inDisabled = inDisabled;
            if (!FileInfo(m.path, m.size, m.mtime)) return;
            Inspect(m, cacheDir);
            if (source == Source::Catalog) m.id = catalogId;
            if (m.id.empty())
            {
                std::wstring stem = name.substr(0, name.size() - m.ext.size());
                m.id = Utf8(stem);
            }
            m.id = CleanId(m.id);
            if (m.name.empty()) m.name = Utf8(name.substr(0, name.size() - m.ext.size()));
            mods.push_back(std::move(m));
        };
        for (const auto& d : List(storeDir))
        {
            if (!d.dir) continue;
            std::wstring newest;
            uint64_t newestTime = 0;
            for (const auto& f : List(storeDir + L"\\" + d.name))
            {
                uint64_t size, time;
                if (!f.dir && IsArchive(f.name) && FileInfo(storeDir + L"\\" + d.name + L"\\" + f.name, size, time) && time >= newestTime) { newest = f.name; newestTime = time; }
            }
            if (!newest.empty()) add(Source::Catalog, storeDir + L"\\" + d.name, newest, false, CleanId(Utf8(d.name)));
        }
        for (const auto& f : List(localDir))
            if (!f.dir && IsArchive(f.name)) add(Source::Local, localDir, f.name, false, {});
        for (const auto& f : List(pdplDir))
            if (!f.dir && IsArchive(f.name) && !IsOurs(f.name)) add(Source::Pdpl, pdplDir, f.name, false, {});
        for (const auto& f : List(disabledDir))
            if (!f.dir && IsArchive(f.name)) add(Source::Pdpl, disabledDir, f.name, true, {});

        // Ids are unique: catalog first, then Local, then PDPL archives get -2, -3 ...
        std::set<std::string> taken;
        for (auto& m : mods)
        {
            std::string base = m.id, id = base;
            for (int n = 2; taken.count(id); ++n) id = base + "-" + std::to_string(n);
            if (id != m.id) log(L"mods: %s: id %S is taken - using %S", m.file.c_str(), m.id.c_str(), id.c_str());
            m.id = id;
            taken.insert(id);
            bool plugins = false;
            m.touches = Touches(m.files, plugins);
            m.plugins = plugins;
            // No catalog yet (step 5): a downloaded mod with DLLs needs the catalog's plugin flag, which nothing can give yet.
            if (m.source == Source::Catalog && m.plugins) m.problem = "has plugins (needs the catalog's plugin flag)";
        }

        // ---- order and on / off (settings.ini, written by the Mods menu) ----
        std::string settings = ReadText(pdnetDir + L"\\settings.ini"), orderText, enabledText;
        SettingsValue(settings, "mods_order", orderText);
        bool haveEnabled = SettingsValue(settings, "mods_enabled", enabledText);
        std::vector<std::string> order = Split(orderText, ',');
        std::set<std::string> known(order.begin(), order.end()), enabledSet;
        for (const auto& id : Split(enabledText, ',')) enabledSet.insert(id);
        for (auto& m : mods)
        {
            bool isNew = !known.count(m.id);
            if (m.source == Source::Pdpl && isNew) m.enabled = !m.inDisabled; // as the player left it
            else if (!haveEnabled || isNew) m.enabled = true;
            else m.enabled = enabledSet.count(m.id) > 0;
        }
        auto rank = [&](const Mod& m)
        {
            auto it = std::find(order.begin(), order.end(), m.id);
            return it == order.end() ? order.size() : static_cast<size_t>(it - order.begin());
        };
        std::stable_sort(mods.begin(), mods.end(), [&](const Mod& a, const Mod& b)
        {
            bool aPdpl = a.source == Source::Pdpl, bPdpl = b.source == Source::Pdpl;
            if (aPdpl != bPdpl) return !aPdpl; // our links first: "!rb" sorts before the player's own names
            if (aPdpl) return LowerW(a.file) < LowerW(b.file); // PDPL's order: by name
            size_t ra = rank(a), rb = rank(b);
            if (ra != rb) return ra < rb;
            return a.id < b.id;
        });

        // ---- PDPL's mods folder ----
        std::map<std::string, std::wstring> existing; // id -> our current file there
        for (const auto& f : List(pdplDir))
        {
            if (f.dir || !IsOurs(f.name)) continue;
            size_t dash = f.name.find(L'-'), dot = f.name.find_last_of(L'.');
            std::string id = dash != std::wstring::npos && dot != std::wstring::npos && dot > dash ? Utf8(f.name.substr(dash + 1, dot - dash - 1)) : "";
            if (id.empty() || existing.count(id)) { DeleteFileW((pdplDir + L"\\" + f.name).c_str()); continue; }
            existing[id] = f.name;
        }
        int copied = 0, renamed = 0, removed = 0, moved = 0, slot = 0;
        std::set<std::string> wanted;
        for (auto& m : mods)
        {
            if (m.source == Source::Pdpl)
            {
                if (m.enabled == m.inDisabled) // in the wrong folder
                {
                    std::wstring to = (m.enabled ? pdplDir : disabledDir) + L"\\" + m.file;
                    if (Exists(to)) m.problem = "a file with the same name is in the way";
                    else if (Move(m.path, to)) { m.path = to; m.inDisabled = !m.enabled; ++moved; }
                }
                continue;
            }
            if (!m.enabled || (!m.problem.empty() && m.problem.rfind("has plugins", 0) == 0)) continue;
            wchar_t prefix[16];
            swprintf_s(prefix, L"!rb%03d-", slot++);
            std::wstring name = prefix + Wide(m.id) + m.ext, target = pdplDir + L"\\" + name;
            wanted.insert(m.id);
            if (auto it = existing.find(m.id); it != existing.end() && it->second != name)
            {
                if (Move(pdplDir + L"\\" + it->second, target)) ++renamed;
            }
            uint64_t size = 0, time = 0;
            if (!FileInfo(target, size, time) || size != m.size || time != m.mtime)
            {
                if (CopyFileW(m.path.c_str(), target.c_str(), FALSE)) ++copied; // keeps the write time, so it's copied only when it changes
                else { m.problem = "couldn't be copied to PDPL's mods folder"; log(L"mods: couldn't copy %s to %s (%lu)", m.path.c_str(), target.c_str(), GetLastError()); }
            }
        }
        for (const auto& [id, name] : existing)
            if (!wanted.count(id) && DeleteFileW((pdplDir + L"\\" + name).c_str())) ++removed;

        // ---- conflicts: two enabled mods with the same game file (the higher one wins) ----
        std::vector<std::unordered_set<std::string>> sets;
        std::vector<const Mod*> on;
        for (const auto& m : mods)
            if (m.enabled && (m.problem.empty() || m.problem.rfind("couldn't be read", 0) == 0 || m.problem.rfind("7z:", 0) == 0))
            {
                on.push_back(&m);
                std::unordered_set<std::string> s;
                for (const auto& f : m.files)
                    if (f != "pdmod.json" && f.rfind("plugins/", 0) != 0) s.insert(f);
                sets.push_back(std::move(s));
            }
        std::string conflicts;
        for (size_t a = 0; a < on.size(); ++a)
            for (size_t b = a + 1; b < on.size(); ++b)
            {
                size_t n = 0;
                const auto& fewer = sets[a].size() < sets[b].size() ? sets[a] : sets[b];
                const auto& more = sets[a].size() < sets[b].size() ? sets[b] : sets[a];
                for (const auto& f : fewer) n += more.count(f);
                if (n) conflicts += "conflict\t" + on[a]->id + "\t" + on[b]->id + "\t" + std::to_string(n) + "\n";
            }

        // ---- the state file for pdnet's Mods menu ----
        size_t loose = LooseFiles(pdplDir, true);
        SYSTEMTIME t;
        GetLocalTime(&t);
        char when[32];
        sprintf_s(when, "%04u-%02u-%02u %02u:%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute);
        std::string state = std::string("# pdlaunch, ") + when + ": mods in priority order (first wins); tab-separated\n";
        state += "# mod id on|off source name version author online touches plugins files problem path load_after load_before description\n";
        state += "loose\t" + std::to_string(loose) + "\n";
        for (const auto& m : mods)
            state += "mod\t" + m.id + "\t" + (m.enabled ? "on" : "off") + "\t" + Utf8(SourceName(m.source)) + "\t" + Field(m.name) + "\t" +
                     Field(m.version) + "\t" + Field(m.author) + "\t" + m.online + "\t" + Join(m.touches, ',') + "\t" + (m.plugins ? "1" : "0") + "\t" +
                     std::to_string(m.files.size()) + "\t" + Field(m.problem) + "\t" + Field(Utf8(m.path)) + "\t" + Join(m.after, ',') + "\t" + Join(m.before, ',') + "\t" + Field(m.description) + "\n";
        state += conflicts;
        if (!WriteText(pdnetDir + L"\\mods-state.txt", state)) log(L"mods: couldn't write mods-state.txt");

        int enabled = 0;
        std::wstring summary;
        for (const auto& m : mods)
        {
            if (m.enabled) ++enabled;
            if (!summary.empty()) summary += L", ";
            summary += Wide(m.id) + (m.enabled ? L"" : L" (off)") + (m.problem.empty() ? L"" : L" [" + Wide(m.problem) + L"]");
        }
        log(L"mods: %zu mod(s), %d on, in order: %s", mods.size(), enabled, summary.empty() ? L"none" : summary.c_str());
        if (copied || renamed || removed || moved) log(L"mods: PDPL's mods folder: %d copied, %d renamed, %d removed, %d moved to or from disabled", copied, renamed, removed, moved);
        if (loose) log(L"mods: %zu loose file(s) in PDPL's mods folder: they win over every archive", loose);
        if (!conflicts.empty()) log(L"mods: some mods change the same files (mods-state.txt has the counts)");
    }
}
