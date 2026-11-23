// Mod loader step 2 (Conceptual): see modsync.h.
#include "modsync.h"
#include "mapslots.h"
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
        const Net* g_net = nullptr; // null: no catalog, downloads or file hashes
        std::string ReadBytes(const std::wstring& p); // (below)

        enum class Source { Catalog, Local, Pdpl };
        const wchar_t* SourceName(Source s) { return s == Source::Catalog ? L"catalog" : s == Source::Local ? L"local" : L"pdpl"; }

        struct Mod
        {
            Source source = Source::Local;
            std::wstring path, file, ext; // the archive, its file name, ".zip" / ".7z"
            bool inDisabled = false;      // a player's PDPL archive waiting in RoamingState\mods\disabled
            uint64_t size = 0, mtime = 0;
            std::string id, name, version, author, description, online = "match", problem;
            std::string sha, tag; // sha256 of the archive (lobbies compare it), the release tag (catalog mods)
            std::vector<std::string> pluginFiles; // plugins/*.dll|exe (step 8)
            std::string category;                 // mod-info.ini's category= (or pdmod.json's), e.g. Map
            bool isMap = false;                   // a new map (.alr + .dat at the archive's root): a map slot, not copied to PDPL
            std::string warning;                  // shown in the Mods list, doesn't stop the mod (step 8)
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
            if (text.rfind("modinfo 2\n", 0) != 0) return false; // 2: category (mod-info.ini) kept too
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
                else if (k == "sha") m.sha = v;
                else if (k == "category") m.category = v;
            }
            m.read = true;
            return true;
        }

        void SaveCache(const std::wstring& file, const Mod& m)
        {
            std::string t = "modinfo 2\n";
            t += "id=" + Field(m.id) + "\nname=" + Field(m.name) + "\nversion=" + Field(m.version) + "\nauthor=" + Field(m.author) +
                 "\ndescription=" + Field(m.description) + "\nonline=" + m.online + "\nafter=" + Join(m.after, ',') + "\nbefore=" + Join(m.before, ',') + "\nsha=" + m.sha + "\ncategory=" + Field(m.category) + "\n";
            for (const auto& f : m.files) t += "f " + f + "\n";
            WriteText(file, t);
        }

        // Lists the archive and reads its pdmod.json: .zip with Windows' tar.exe (Windows 10 1803 and later), .7z with
        // 7-Zip if it's installed (and a .zip tar can't read). Fills id (from pdmod.json; the caller settles it), name, ...,
        // files. Archives that can't be read aren't cached, so installing 7-Zip later fills them in.
        void Inspect(Mod& m, const std::wstring& cacheDir)
        {
            std::wstring cache = CachePath(cacheDir, m);
            if (LoadCache(cache, m))
            {
                if (m.sha.empty() && g_net) // cached before hashes were kept
                {
                    m.sha = g_net->sha256(ReadBytes(m.path));
                    SaveCache(cache, m);
                }
                return;
            }
            if (g_net) m.sha = g_net->sha256(ReadBytes(m.path)); // lobbies compare mods by it
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
            std::string manifestEntry, iniEntry;
            for (const auto& raw : entries)
            {
                std::string p = GamePath(raw);
                if (p.empty() || p.find("..") != std::string::npos) continue;
                if (p == "pdmod.json") manifestEntry = raw;
                if (p == "mod-info.ini") iniEntry = raw;
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
                    m.category = JsonGetString(json, "category");
                }
                else g_log(L"mods: %s: pdmod.json couldn't be read", m.file.c_str());
            }
            // mod-info.ini (the community's map format: id, author, name, ver, desc, category, preview): its details where
            // pdmod.json gave none. Its id is a random hash, not a catalog id, so it isn't used.
            if (!iniEntry.empty())
            {
                std::string ini;
                if (ReadEntry(m.path, sevenZip, iniEntry, ini))
                    for (const auto& line : Split(ini, '\n'))
                    {
                        size_t eq = line.find('=');
                        if (line.empty() || line[0] == ';' || line[0] == '#' || eq == std::string::npos) continue;
                        std::string k = Lower(line.substr(0, eq)), v = line.substr(eq + 1);
                        while (!k.empty() && k.back() == ' ') k.pop_back();
                        while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.erase(0, 1);
                        if (k == "name" && m.name.empty()) m.name = v;
                        else if (k == "ver" && m.version.empty()) m.version = v;
                        else if (k == "author" && m.author.empty()) m.author = v;
                        else if (k == "desc" && m.description.empty()) m.description = v;
                        else if (k == "category" && m.category.empty()) m.category = v;
                    }
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

        // A whole file, as it is (ReadText drops a UTF-8 byte order mark).
        std::string ReadBytes(const std::wstring& p)
        {
            std::string data;
            if (FILE* f = nullptr; !_wfopen_s(&f, p.c_str(), L"rb") && f)
            {
                char buf[65536];
                for (size_t n; (n = fread(buf, 1, sizeof(buf), f)) > 0;) data.append(buf, n);
                fclose(f);
            }
            return data;
        }

        // One game file out of a mod's archive. The cache keeps paths lowercase, so the archive is listed again for the
        // entry's real name (tar matches names exactly).
        bool ExtractGameFile(const Mod& m, const std::string& gamePath, std::string& out)
        {
            for (bool sevenZip : { m.ext == L".7z", true })
            {
                if (sevenZip && SevenZip().empty()) continue;
                std::vector<std::string> entries;
                if (!ListEntries(m.path, sevenZip, entries)) continue;
                for (const auto& raw : entries)
                    if (GamePath(raw) == gamePath) return ReadEntry(m.path, sevenZip, raw, out) && !out.empty();
            }
            return false;
        }

        // ---- plugins (step 8) ----

        // The DLL names a PE file imports (its import directory), lowercase; empty if it isn't a readable PE.
        std::vector<std::string> PeImports(const std::string& pe)
        {
            std::vector<std::string> out;
            auto u16 = [&](size_t at) -> uint32_t { return at + 2 <= pe.size() ? static_cast<uint8_t>(pe[at]) | static_cast<uint8_t>(pe[at + 1]) << 8 : 0; };
            auto u32 = [&](size_t at) -> uint32_t { return at + 4 <= pe.size() ? u16(at) | u16(at + 2) << 16 : 0; };
            if (pe.size() < 0x40 || u16(0) != 0x5A4D) return out; // "MZ"
            size_t nt = u32(0x3C);
            if (u32(nt) != 0x00004550) return out; // "PE\0\0"
            size_t file = nt + 4, optional = file + 20;
            uint32_t sections = u16(file + 2), optionalSize = u16(file + 16), magic = u16(optional);
            size_t dirs = optional + (magic == 0x20B ? 112 : 96);
            uint32_t importRva = u32(dirs + 8); // directory 1: imports
            size_t table = optional + optionalSize;
            auto offsetOf = [&](uint32_t rva) -> size_t
            {
                for (uint32_t i = 0; i < sections; ++i)
                {
                    size_t s = table + i * 40;
                    uint32_t va = u32(s + 12), vsize = u32(s + 8), raw = u32(s + 16), ptr = u32(s + 20);
                    if (rva >= va && rva < va + (std::max)(vsize, raw)) return ptr + (rva - va);
                }
                return std::string::npos;
            };
            size_t desc = importRva ? offsetOf(importRva) : std::string::npos;
            for (int n = 0; desc != std::string::npos && desc + 20 <= pe.size() && n < 256; ++n, desc += 20)
            {
                uint32_t nameRva = u32(desc + 12);
                if (!nameRva) break;
                size_t name = offsetOf(nameRva);
                if (name == std::string::npos || name >= pe.size()) continue;
                out.push_back(Lower(std::string(pe.c_str() + name, strnlen(pe.c_str() + name, (std::min<size_t>)(pe.size() - name, 260)))));
            }
            return out;
        }

        // ---- the catalog and downloads (step 5) ----

        // A catalog line: "<id> <owner/repo> [plugin] [release=<tag> file=<url> sha256=<hex>]". The release fields are
        // filled in by a GitHub Action on the catalog's branch every 5 minutes (.github\workflows\mod-catalog.yml) and by
        // the Mod Admin app: with them, players' games need no GitHub API lookup at all (raw.githubusercontent.com, where
        // the catalog is read, has no hourly limit). file must be one of that repo's release downloads.
        struct CatalogEntry { std::string id, repo; bool plugin = false; std::string tag, url, sha; };

        std::vector<CatalogEntry> ParseCatalog(const std::string& text)
        {
            std::vector<CatalogEntry> out;
            std::set<std::string> seen;
            for (const auto& line : Split(text, '\n'))
            {
                if (line.empty() || line[0] == '#') continue;
                std::vector<std::string> words;
                std::string word;
                for (char c : line + " ")
                {
                    if (c == ' ' || c == '\t') { if (!word.empty()) words.push_back(word); word.clear(); }
                    else word += c;
                }
                if (words.size() < 2) continue;
                CatalogEntry e{ words[0], words[1] };
                if (CleanId(e.id) != e.id || std::count(e.repo.begin(), e.repo.end(), '/') != 1 || seen.count(e.id))
                {
                    g_log(L"mods: catalog line skipped (id must be a-z 0-9 -, repo owner/name, ids once): %s", Wide(line).c_str());
                    continue;
                }
                for (size_t i = 2; i < words.size(); ++i)
                {
                    const std::string& w = words[i];
                    if (Lower(w) == "plugin") e.plugin = true;
                    else if (w.rfind("release=", 0) == 0) e.tag = w.substr(8);
                    else if (w.rfind("file=", 0) == 0) e.url = w.substr(5);
                    else if (w.rfind("sha256=", 0) == 0) e.sha = Lower(w.substr(7));
                }
                // All three or none, and only a download from the mod's own repo's releases.
                std::string prefix = "https://github.com/" + Lower(e.repo) + "/releases/download/";
                bool hex64 = e.sha.size() == 64 && e.sha.find_first_not_of("0123456789abcdef") == std::string::npos;
                if (!e.tag.empty() || !e.url.empty() || !e.sha.empty())
                    if (e.tag.empty() || !hex64 || Lower(e.url).rfind(prefix, 0) != 0)
                    {
                        g_log(L"mods: catalog: %S's release fields left out (incomplete, or not a download from %S)", e.id.c_str(), e.repo.c_str());
                        e.tag.clear(), e.url.clear(), e.sha.clear();
                    }
                seen.insert(e.id);
                out.push_back(e);
            }
            return out;
        }

        std::vector<CatalogEntry> LoadCatalog(const std::wstring& storeDir, const std::wstring& url)
        {
            std::string text;
            bool fetched = false;
            if (url.rfind(L"https://", 0) == 0) fetched = g_net && g_net->get(url, text, 8000);
            else if (!url.empty()) fetched = !(text = ReadText(url)).empty(); // a local file (testing)
            std::wstring cache = storeDir + L"\\catalog.txt";
            if (fetched) WriteText(cache, text);
            else
            {
                text = ReadText(cache);
                if (!url.empty()) g_log(L"mods: catalog not reached (%s) - %s", url.c_str(), text.empty() ? L"no copy from an earlier start either" : L"using the copy from an earlier start");
            }
            auto catalog = ParseCatalog(text);
            g_log(L"mods: catalog: %zu mod(s)", catalog.size());
            return catalog;
        }

        struct Release { std::string tag, url, asset, sha; uint64_t checked = 0; };

        uint64_t Now()
        {
            FILETIME ft;
            GetSystemTimeAsFileTime(&ft);
            return ((static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime) / 10000000ull - 11644473600ull; // Unix seconds
        }

        // modstore\releases.txt: id, tag, url, asset, sha256, time checked (seconds), tab-separated.
        std::map<std::string, Release> LoadReleases(const std::wstring& file)
        {
            std::map<std::string, Release> out;
            for (const auto& line : Split(ReadText(file), '\n'))
            {
                std::vector<std::string> f;
                std::string cur;
                for (char c : line) { if (c == '\t') { f.push_back(cur); cur.clear(); } else cur += c; }
                f.push_back(cur);
                if (f.size() < 6) continue;
                out[f[0]] = Release{ f[1], f[2], f[3], f[4], _strtoui64(f[5].c_str(), nullptr, 10) };
            }
            return out;
        }

        void SaveReleases(const std::wstring& file, const std::map<std::string, Release>& releases)
        {
            std::string text;
            for (const auto& [id, r] : releases)
                text += id + "\t" + r.tag + "\t" + r.url + "\t" + r.asset + "\t" + r.sha + "\t" + std::to_string(r.checked) + "\n";
            WriteText(file, text);
        }

        std::string PercentDecode(const std::string& s)
        {
            std::string out;
            for (size_t i = 0; i < s.size(); ++i)
            {
                if (s[i] == '%' && i + 2 < s.size() && isxdigit(static_cast<unsigned char>(s[i + 1])) && isxdigit(static_cast<unsigned char>(s[i + 2])))
                {
                    out += static_cast<char>(strtoul(s.substr(i + 1, 2).c_str(), nullptr, 16));
                    i += 2;
                }
                else out += s[i];
            }
            return out;
        }

        // The newest release of a catalog mod: its first .zip / .7z that the notes give a sha256 for.
        // tag empty: the newest release; else that release (a lobby's). answered: GitHub's API answered (false: not
        // reached, rate-limited or no such release), so a failure is that release's own (no usable file) and needn't be
        // looked up again until there's a newer one.
        bool LookUpRelease(const CatalogEntry& e, const std::string& tag, Release& r, std::string& why, bool& answered)
        {
            answered = false;
            std::string json, path = "/releases/latest";
            if (!tag.empty())
            {
                path = "/releases/tags/";
                for (char c : tag) // percent-encode anything but unreserved characters
                {
                    if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '.' || c == '_' || c == '~') path += c;
                    else { char hex[4]; sprintf_s(hex, "%%%02X", static_cast<unsigned char>(c)); path += hex; }
                }
            }
            if (!g_net->get(L"https://api.github.com/repos/" + Wide(e.repo) + Wide(path), json, 8000))
            {
                why = "GitHub didn't answer, or the repo has no release";
                return false;
            }
            answered = true;
            r = Release{};
            r.tag = JsonGetString(json, "tag_name");
            std::string body = JsonGetString(json, "body");
            // The release's first .zip / .7z with a sha256 to check it against: a "sha256 <file> <hex>" line in the notes
            // (the creator's own; it wins), else the "digest" GitHub works out for every file uploaded to a release
            // ("sha256:<hex>", in the asset's entry before its browser_download_url). Creators need do nothing but upload.
            auto isHex64 = [](const std::string& h) { return h.size() == 64 && h.find_first_not_of("0123456789abcdef") == std::string::npos; };
            const std::string key = "\"browser_download_url\"";
            size_t previous = 0; // where the asset before this one ended
            for (size_t at = json.find(key); at != std::string::npos && r.url.empty(); previous = at, at = json.find(key, at + 1))
            {
                size_t i = SkipBlank(json, at + key.size());
                if (i >= json.size() || json[i] != ':') continue;
                i = SkipBlank(json, i + 1);
                std::string url = JsonString(json, i);
                std::string name = PercentDecode(url.substr(url.find_last_of('/') + 1));
                std::wstring ext = Extension(Wide(name));
                if (ext != L".zip" && ext != L".7z") continue;
                std::string hex, from;
                std::string marker = "sha256 " + name + " ";
                if (size_t s = body.find(marker); s != std::string::npos && isHex64(Lower(body.substr(s + marker.size(), 64))))
                {
                    hex = Lower(body.substr(s + marker.size(), 64));
                    from = "the release notes";
                }
                else
                {
                    std::string entry = json.substr(previous, at - previous); // this asset's fields
                    size_t d = entry.rfind("\"digest\"");
                    if (d != std::string::npos)
                    {
                        size_t v = SkipBlank(entry, d + 8);
                        if (v < entry.size() && entry[v] == ':' && (v = SkipBlank(entry, v + 1)) < entry.size() && entry[v] == '"')
                        {
                            std::string digest = Lower(JsonString(entry, v));
                            if (digest.rfind("sha256:", 0) == 0 && isHex64(digest.substr(7))) { hex = digest.substr(7); from = "GitHub"; }
                        }
                    }
                }
                if (hex.empty()) continue;
                r.url = url;
                r.asset = name;
                r.sha = hex;
                if (from == "GitHub") g_log(L"mods: %S %S: %S's sha256 from GitHub (no line in the release notes)", e.id.c_str(), r.tag.c_str(), name.c_str());
            }
            if (r.tag.empty()) { why = "no release found"; return false; }
            if (r.url.empty()) { why = "its newest release has no .zip / .7z (with a sha256: GitHub's, or a line in the notes)"; return false; }
            r.checked = Now();
            return true;
        }

        // The release a catalog line names (release= file= sha256=, CatalogEntry), or an empty one.
        Release CatalogRelease(const CatalogEntry& e)
        {
            if (e.url.empty()) return {};
            return Release{ e.tag, e.url, PercentDecode(e.url.substr(e.url.find_last_of('/') + 1)), e.sha, Now() };
        }

        std::string SafeName(const std::string& tag)
        {
            std::string out;
            for (char c : tag) out += (isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_') ? c : '_';
            return out.empty() ? "release" : out;
        }

        // Downloads the release into modstore\<id>\<tag>.<ext> (if it isn't there yet), checks it, keeps the newest 3.
        bool Download(const CatalogEntry& e, const Release& r, const std::wstring& dir, std::string& why)
        {
            std::wstring file = dir + L"\\" + Wide(SafeName(r.tag)) + Extension(Wide(r.asset));
            if (Exists(file)) return true;
            CreateDirectoryW(dir.c_str(), nullptr);
            std::string data;
            g_log(L"mods: %S: downloading %S (%S)", e.id.c_str(), r.asset.c_str(), r.tag.c_str());
            if (!g_net->get(Wide(r.url), data, 120000)) { why = "the download failed"; return false; }
            if (g_net->sha256(data) != r.sha) { why = "the download doesn't match its sha256"; return false; }
            if (!WriteText(file, data)) { why = "couldn't be saved"; return false; }
            g_log(L"mods: %S %S: downloaded, %zu bytes, sha256 ok", e.id.c_str(), r.tag.c_str(), data.size());
            std::vector<std::pair<uint64_t, std::wstring>> versions;
            for (const auto& f : List(dir))
            {
                uint64_t size, time;
                if (!f.dir && IsArchive(f.name) && FileInfo(dir + L"\\" + f.name, size, time)) versions.push_back({ time, f.name });
            }
            std::sort(versions.rbegin(), versions.rend());
            // The newest 3 by file time, but never the file just downloaded: a PC whose clock went back makes an older
            // download look newer.
            size_t kept = 0;
            for (const auto& [time, name] : versions)
            {
                bool justNow = _wcsicmp((dir + L"\\" + name).c_str(), file.c_str()) == 0;
                if (justNow) continue;              // this release: always kept
                if (kept < 2) { ++kept; continue; } // and the 2 newest others
                DeleteFileW((dir + L"\\" + name).c_str());
            }
            return true;
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

    bool Sync(const std::wstring& localState, LogFn log, const Net* net, const std::wstring& catalogUrl, const std::wstring& gameDir,
              const std::function<bool(const std::string& needsRestart)>& beforeFiles)
    {
        g_log = log;
        g_net = net;
        size_t cut = localState.find_last_of(L'\\');
        if (localState.empty() || cut == std::wstring::npos) { log(L"mods: no LocalState folder - mods not synced"); return false; }
        std::wstring pdnetDir = localState + L"\\pdnet", roaming = localState.substr(0, cut) + L"\\RoamingState";
        std::wstring pdplDir = roaming + L"\\mods", disabledDir = pdplDir + L"\\disabled", localDir = pdnetDir + L"\\mods",
                     storeDir = pdnetDir + L"\\modstore", cacheDir = pdnetDir + L"\\modinfo";
        for (const auto& d : { pdnetDir, roaming, pdplDir, disabledDir, localDir, storeDir, cacheDir }) CreateDirectoryW(d.c_str(), nullptr);
        if (!Exists(Tar())) log(L"mods: %s not found - .zip details can't be read (names and conflicts unknown)", Tar().c_str());
        log(L"mods: .7z details: %s", SevenZip().empty() ? L"7-Zip isn't installed (PDPL still loads .7z mods)" : SevenZip().c_str());

        // ---- order and on / off (settings.ini, written by the Mods menu) ----
        std::string settings = ReadText(pdnetDir + L"\\settings.ini"), orderText, enabledText;
        SettingsValue(settings, "mods_order", orderText);
        bool haveEnabled = SettingsValue(settings, "mods_enabled", enabledText);
        std::vector<std::string> order = Split(orderText, ',');
        std::set<std::string> known(order.begin(), order.end()), enabledSet;
        for (const auto& id : Split(enabledText, ',')) enabledSet.insert(id);

        // ---- one start with a lobby's mods (step 7) ----
        // pdnet writes LocalState\pdnet\lobby-mods.txt when this PC tries to join a lobby whose gameplay mods differ from
        // its own (lines "lobby <id>", "time <unix seconds>", "mod <id> <sha256 start> [<release tag>]", tab-separated).
        // PDPL can't change mods while the game runs, so the next start uses exactly those mods (catalog ones downloaded at
        // the lobby's release), plus this PC's own "online": "local" ones; the player then joins the lobby again. Used once
        // (renamed lobby-mods.used.txt), and only within 2 hours.
        struct LobbyMod { std::string id, sha, tag; };
        std::vector<LobbyMod> lobbyMods;
        bool lobbyStart = false;
        {
            std::wstring request = pdnetDir + L"\\lobby-mods.txt";
            std::string text = ReadText(request);
            if (!text.empty())
            {
                MoveFileExW(request.c_str(), (pdnetDir + L"\\lobby-mods.used.txt").c_str(), MOVEFILE_REPLACE_EXISTING);
                std::string lobbyId;
                uint64_t when = 0;
                for (const auto& line : Split(text, '\n'))
                {
                    auto w = Split(line, '\t');
                    if (w.size() >= 2 && w[0] == "lobby") lobbyId = w[1];
                    else if (w.size() >= 2 && w[0] == "time") when = _strtoui64(w[1].c_str(), nullptr, 10);
                    else if (w.size() >= 3 && w[0] == "mod") lobbyMods.push_back({ w[1], Lower(w[2]), w.size() > 3 ? w[3] : "" });
                }
                lobbyStart = when && Now() - when < 2 * 3600;
                if (lobbyStart) log(L"mods: this start uses lobby %S's mods (%zu), as asked when joining it", lobbyId.c_str(), lobbyMods.size());
                else log(L"mods: lobby-mods.txt is older than 2 hours - not used");
            }
        }
        auto lobbyEntry = [&](const std::string& id) -> const LobbyMod*
        {
            for (const auto& l : lobbyMods) if (l.id == id) return &l;
            return nullptr;
        };

        // ---- the catalog: downloads for the catalog mods that are on ----
        std::vector<CatalogEntry> catalog;
        std::map<std::string, std::string> notDownloaded; // id -> why
        std::map<std::string, Release> releases;
        if (net)
        {
            catalog = LoadCatalog(storeDir, catalogUrl);
            std::wstring releasesFile = storeDir + L"\\releases.txt";
            releases = LoadReleases(releasesFile);
            bool changed = false;
            for (const auto& e : catalog)
            {
                std::string why;
                if (lobbyStart) // the lobby's catalog mods, at the lobby's release
                {
                    const LobbyMod* l = lobbyEntry(e.id);
                    if (!l || l->tag.empty()) continue;
                    Release r;
                    bool answered = false;
                    if (!Exists(storeDir + L"\\" + Wide(e.id) + L"\\" + Wide(SafeName(l->tag)) + L".zip") &&
                        !Exists(storeDir + L"\\" + Wide(e.id) + L"\\" + Wide(SafeName(l->tag)) + L".7z"))
                    {
                        bool catalogs = !e.url.empty() && e.tag == l->tag; // the catalog names that release: no API lookup
                        if (catalogs) r = CatalogRelease(e);
                        if ((!catalogs && !LookUpRelease(e, l->tag, r, why, answered)) || r.sha.rfind(l->sha, 0) != 0 || !Download(e, r, storeDir + L"\\" + Wide(e.id), why))
                        {
                            if (why.empty()) why = "the lobby's file isn't that release's";
                            log(L"mods: %S %S (for the lobby): %S", e.id.c_str(), l->tag.c_str(), why.c_str());
                            notDownloaded[e.id] = why;
                        }
                    }
                    continue;
                }
                if (!enabledSet.count(e.id)) continue;
                // At every start: the newest tag from GitHub's website (free). The same tag as the release known already:
                // that one, without the API. A new tag (or none known): the API, for its file and sha256. The website
                // not reached: the release known already, if any. (A 6-hour cache before 2026-11-21 left new releases
                // unseen for hours; the API's "not modified" answers count against its 60 an hour without a login.)
                // First the catalog's own release fields (kept current by the GitHub Action every 5 minutes): no lookup
                // at all. Lines without them (not filled in yet) use the website / API below.
                Release r;
                auto cached = releases.find(e.id);
                bool fromCatalog = !e.url.empty();
                std::string webTag;
                bool web = !fromCatalog && g_net->latestTag && g_net->latestTag(Wide(e.repo), webTag, 8000);
                if (web) webTag = PercentDecode(webTag);
                bool known = !fromCatalog && cached != releases.end() && (web ? cached->second.tag == webTag : !cached->second.url.empty());
                bool answered = false;
                if (known && cached->second.url.empty()) // that release was looked at: no usable file in it
                {
                    notDownloaded[e.id] = cached->second.asset;
                    continue;
                }
                if (fromCatalog)
                {
                    r = CatalogRelease(e);
                    if (cached == releases.end() || cached->second.tag != r.tag || cached->second.url != r.url)
                    {
                        log(L"mods: %S: newest release %S (%S, from the catalog)%s", e.id.c_str(), r.tag.c_str(), r.asset.c_str(),
                            cached != releases.end() && !cached->second.tag.empty() ? (L", was " + Wide(cached->second.tag)).c_str() : L"");
                        releases[e.id] = r;
                        changed = true;
                    }
                }
                else if (known) r = cached->second;
                else if (LookUpRelease(e, "", r, why, answered))
                {
                    log(L"mods: %S: newest release %S (%S)%s", e.id.c_str(), r.tag.c_str(), r.asset.c_str(),
                        cached != releases.end() && !cached->second.tag.empty() ? (L", was " + Wide(cached->second.tag)).c_str() : L"");
                    releases[e.id] = r;
                    changed = true;
                }
                else if (cached != releases.end() && !cached->second.url.empty())
                {
                    log(L"mods: %S: %S - using the release found earlier (%S)", e.id.c_str(), why.c_str(), cached->second.tag.c_str());
                    r = cached->second;
                }
                else
                {
                    log(L"mods: %S (%S): %S", e.id.c_str(), e.repo.c_str(), why.c_str());
                    notDownloaded[e.id] = why;
                    // No url: the reason. Kept under that release's tag when GitHub answered (looked at again once there's
                    // a newer release); otherwise tried again at the next start.
                    releases[e.id] = Release{ answered ? r.tag : "", "", why, "", Now() };
                    changed = true;
                    continue;
                }
                if (!Download(e, r, storeDir + L"\\" + Wide(e.id), why))
                {
                    log(L"mods: %S %S: %S", e.id.c_str(), r.tag.c_str(), why.c_str());
                    notDownloaded[e.id] = why;
                }
            }
            if (changed) SaveReleases(releasesFile, releases);
        }
        std::map<std::string, const CatalogEntry*> inCatalog;
        for (const auto& e : catalog) inCatalog[e.id] = &e;

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
            // PDPL itself (its release zip, pdpl.zip, put in a mods folder by a player): it comes with the kit, so it isn't a
            // mod. It showed in the Mods list (the user, 2026-11-22). Left out: not listed, not copied into PDPL's folder.
            {
                std::wstring lower = name;
                for (auto& c : lower) c = static_cast<wchar_t>(towlower(c));
                bool loader = lower.rfind(L"pdpl", 0) == 0 || lower.find(L"pd_loader") != std::wstring::npos;
                for (const auto& f : m.files)
                    if (f.size() >= 18 && f.compare(f.size() - 18, 18, "pd_loader_core.dll") == 0) loader = true;
                if (loader)
                {
                    log(L"mods: %s is PDPL itself (the kit has it) - not a mod, left out", name.c_str());
                    return;
                }
            }
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
            std::string id = CleanId(Utf8(d.name));
            const LobbyMod* l = lobbyStart ? lobbyEntry(id) : nullptr;
            // Which download: a lobby start's release; else the newest release GitHub gave (releases.txt), whose file is
            // <tag>.<ext>; else the newest file. (By file time alone, a PC whose clock had gone back kept an older release:
            // Aquarium 0.1 over the 0.2 just downloaded, Experimental 192.)
            auto known = releases.find(id);
            std::wstring releaseStem = known != releases.end() && !known->second.tag.empty() ? Wide(SafeName(known->second.tag)) : L"";
            std::wstring release;
            for (const auto& f : List(storeDir + L"\\" + d.name))
            {
                uint64_t size, time;
                if (f.dir || !IsArchive(f.name) || !FileInfo(storeDir + L"\\" + d.name + L"\\" + f.name, size, time)) continue;
                std::wstring stem = f.name.substr(0, f.name.size() - Extension(f.name).size());
                bool lobbys = l && !l->tag.empty() && _wcsicmp(stem.c_str(), Wide(SafeName(l->tag)).c_str()) == 0;
                if (lobbys) { newest = f.name; release.clear(); break; } // a lobby start: the lobby's release, not the newest
                if (!releaseStem.empty() && _wcsicmp(stem.c_str(), releaseStem.c_str()) == 0) release = f.name;
                if (time >= newestTime) { newest = f.name; newestTime = time; }
            }
            if (!release.empty()) newest = release;
            if (!newest.empty()) add(Source::Catalog, storeDir + L"\\" + d.name, newest, false, id);
        }
        // Catalog mods with nothing downloaded: listed (no path) so the Mods list can turn them on.
        {
            std::set<std::string> have;
            for (const auto& m : mods) have.insert(m.id);
            for (const auto& e : catalog)
            {
                if (have.count(e.id)) continue;
                Mod m;
                m.source = Source::Catalog;
                m.id = e.id;
                m.name = e.id;
                auto why = notDownloaded.find(e.id);
                m.problem = why != notDownloaded.end() ? "not downloaded: " + why->second : "not downloaded";
                mods.push_back(std::move(m));
            }
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
            // A new map: category Map, or a .alr + .dat pair at the archive's root (not at a game path). It gets a map slot
            // (mapslots.cpp) instead of being put in PDPL's folder as it is.
            {
                bool rootAlr = false, rootDat = false;
                for (const auto& p : m.files)
                {
                    if (p.find('/') != std::string::npos) continue;
                    std::wstring ext = Extension(Wide(p));
                    rootAlr = rootAlr || ext == L".alr";
                    rootDat = rootDat || ext == L".dat";
                }
                m.isMap = (Lower(m.category).find("map") != std::string::npos || m.category.empty()) && rootAlr && rootDat;
                if (m.isMap) m.touches = { "maps" };
            }
            // A downloaded mod with DLLs needs the catalog's "plugin" flag (someone read its source): without it, it's never
            // put in PDPL's folder.
            // Plugins (step 8): PDPL loads plugins/*.dll from every archive it mounts, at game start. A plugin that imports
            // functions from pd_loader_core.dll can't load with PDPL 1.2.2, whose DLL exports nothing: warned in the list.
            if (m.plugins && !m.path.empty())
                for (const auto& p : m.files)
                {
                    if (p.rfind("plugins/", 0) != 0) continue;
                    std::wstring ext = Extension(Wide(p));
                    if (ext != L".dll" && ext != L".exe") continue;
                    m.pluginFiles.push_back(p.substr(8));
                    std::string pe;
                    if (!ExtractGameFile(m, p, pe)) continue;
                    auto imports = PeImports(pe);
                    if (std::find(imports.begin(), imports.end(), "pd_loader_core.dll") != imports.end())
                    {
                        m.warning = p.substr(8) + " uses pd_loader_core.dll's functions, which PDPL 1.2.2 doesn't export: it won't load";
                        log(L"mods: %S: %S", m.id.c_str(), m.warning.c_str());
                    }
                }
            if (m.source == Source::Catalog && m.plugins && !(inCatalog.count(m.id) && inCatalog[m.id]->plugin))
                m.problem = "has plugins (needs the catalog's plugin flag)";
            // The release tag of a catalog mod's file (lobbies name it, so a joiner can download the same release).
            if (m.source == Source::Catalog && !m.path.empty())
            {
                std::string stem = Utf8(m.file.substr(0, m.file.size() - m.ext.size()));
                auto r = releases.find(m.id);
                const LobbyMod* l = lobbyEntry(m.id);
                m.tag = r != releases.end() && SafeName(r->second.tag) == stem ? r->second.tag : l && SafeName(l->tag) == stem ? l->tag : stem;
            }
        }

        for (auto& m : mods)
        {
            bool isNew = !known.count(m.id);
            if (m.source == Source::Catalog) m.enabled = enabledSet.count(m.id) > 0; // catalog mods: only once turned on
            else if (m.source == Source::Pdpl && isNew) m.enabled = !m.inDisabled; // as the player left it
            else if (!haveEnabled || isNew) m.enabled = true;
            else m.enabled = enabledSet.count(m.id) > 0;
            if (lobbyStart) // the lobby's mods (the same file), plus this PC's own ones that only change how things look or sound
            {
                const LobbyMod* l = lobbyEntry(m.id);
                bool fromLobby = l && !m.sha.empty() && m.sha.rfind(l->sha, 0) == 0;
                m.enabled = fromLobby || (m.enabled && m.online == "local");
            }
        }
        if (lobbyStart)
            for (const auto& l : lobbyMods)
            {
                bool found = false;
                for (const auto& m : mods) found = found || (m.id == l.id && m.enabled);
                if (!found) log(L"mods: the lobby's %S isn't here (or isn't the same file) - joining will be refused again", l.id.c_str());
            }
        std::vector<std::string> lobbyOrder;
        for (const auto& l : lobbyMods) lobbyOrder.push_back(l.id);
        auto rank = [&](const Mod& m)
        {
            const auto& list = lobbyStart ? lobbyOrder : order;
            auto it = std::find(list.begin(), list.end(), m.id);
            if (it != list.end()) return static_cast<size_t>(it - list.begin());
            if (!lobbyStart) return order.size();
            auto own = std::find(order.begin(), order.end(), m.id); // after the lobby's, in the player's own order
            return list.size() + (own == order.end() ? order.size() : static_cast<size_t>(own - order.begin()));
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

        // ---- while the game runs: can these changes be made now? ----
        // What the game or PDPL reads only at start must stay exactly as the last sync left it (mods-state.txt): the same
        // mods with such files, in the same order. Nothing has been changed yet at this point.
        if (beforeFiles)
        {
            auto bootOnly = [](const std::string& touches)
            {
                for (const auto& t : Split(touches, ','))
                    if (t != "maps" && t != "models" && t != "animation" && t != "audio" && t != "ui") return true;
                return false;
            };
            std::vector<std::string> before, after;
            std::map<std::string, std::string> names;
            std::vector<std::vector<std::string>> rows(1, std::vector<std::string>(1)); // lines of fields, empty ones kept (Split drops them)
            for (char c : ReadText(pdnetDir + L"\\mods-state.txt"))
            {
                if (c == '\n') rows.emplace_back(1);
                else if (c == '\t') rows.back().emplace_back();
                else if (c != '\r') rows.back().back() += c;
            }
            for (auto& f : rows)
            {
                if (f[0] != "mod") continue;
                f.resize((std::max<size_t>)(f.size(), 17));
                if (f[2] != "on" || f[12].empty() || f[11].rfind("has plugins", 0) == 0 || !bootOnly(f[8])) continue;
                before.push_back(f[1] + ":" + f[16]);
                names[before.back()] = f[4].empty() ? f[1] : f[4];
            }
            for (const auto& m : mods)
            {
                if (!m.enabled || m.path.empty() || m.problem.rfind("has plugins", 0) == 0 || !bootOnly(Join(m.touches, ','))) continue;
                after.push_back(m.id + ":" + m.sha.substr(0, 12));
                names[after.back()] = m.name.empty() ? m.id : m.name;
            }
            std::string why;
            if (before != after)
            {
                std::set<std::string> a(before.begin(), before.end()), b(after.begin(), after.end());
                for (const auto& [key, name] : names)
                    if (a.count(key) != b.count(key)) why += (why.empty() ? "" : ", ") + name;
                if (why.empty()) why = "the order of mods that load at game start";
            }
            if (!beforeFiles(why))
            {
                // A lobby's request was for this sync: it's the next start's again.
                if (lobbyStart) MoveFileExW((pdnetDir + L"\\lobby-mods.used.txt").c_str(), (pdnetDir + L"\\lobby-mods.txt").c_str(), MOVEFILE_REPLACE_EXISTING);
                return false;
            }
        }

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
            if (!m.enabled || m.path.empty() || m.isMap || (!m.problem.empty() && m.problem.rfind("has plugins", 0) == 0)) continue;
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

        // ---- new map slots (mapslots.cpp) ----
        // Each enabled map mod (slots by mod id, mapslots.cpp) becomes a new stage in the multiplayer map picker (its .alr / .dat
        // under a free stage number, its preview on the picker's sheet); with none, the generated files are removed.
        if (!gameDir.empty())
        {
            std::vector<mapslots::MapMod> maps;
            for (const auto& m : mods)
            {
                if (!m.isMap || !m.enabled || m.path.empty() || (!m.problem.empty() && m.problem.rfind("has plugins", 0) == 0)) continue;
                mapslots::MapMod map{ m.id, m.name.empty() ? m.id : m.name };
                for (const auto& p : m.files)
                {
                    if (p.find('/') != std::string::npos) continue;
                    std::wstring ext = Extension(Wide(p));
                    if (ext == L".alr" && map.alr.empty()) ExtractGameFile(m, p, map.alr);
                    else if (ext == L".dat" && map.dat.empty()) ExtractGameFile(m, p, map.dat);
                    else if (p == "preview.png" || (map.preview.empty() && (ext == L".png" || ext == L".jpg"))) ExtractGameFile(m, p, map.preview);
                }
                if (map.alr.empty() || map.dat.empty()) { log(L"mods: %S: its map files couldn't be read - no map slot", m.id.c_str()); continue; }
                maps.push_back(std::move(map));
            }
            mapslots::Generate(gameDir, pdplDir, pdnetDir, maps, log);
        }
        else log(L"mods: game folder unknown - map slots not made");

        // ---- skill data (step 6) ----
        // PDPL hands the game a mod's gsdata_<lang>.dat, but the game then copies its stored online copy over that data in
        // memory, so pdnet writes the mod's into memory again (gamepatches.cpp, SkillDataGuard). pdnet can't read archives,
        // so the file the game would get (loose files in PDPL's folder first, then the highest mod that has it) is put in
        // LocalState\pdnet\mod-skills\gsdata_<lang>.dat, with gsdata_<lang>.txt naming where it came from.
        {
            std::wstring skillDir = pdnetDir + L"\\mod-skills";
            CreateDirectoryW(skillDir.c_str(), nullptr);
            for (const auto& f : List(skillDir))
                if (!f.dir) DeleteFileW((skillDir + L"\\" + f.name).c_str());
            for (const char* lang : { "en", "ja" })
            {
                std::string gamePath = std::string("assets/data/gstorage/gsdata_") + lang + ".dat", data, from;
                std::wstring loose = pdplDir + L"\\Assets\\Data\\gstorage\\gsdata_" + Wide(lang) + L".dat";
                if (Exists(loose))
                {
                    data = ReadBytes(loose);
                    from = "loose files in PDPL's mods folder";
                }
                else
                    for (const auto& m : mods)
                    {
                        bool mounted = m.enabled && !m.path.empty() && (m.problem.empty() || m.problem.rfind("has plugins", 0) != 0);
                        if (!mounted || std::find(m.files.begin(), m.files.end(), gamePath) == m.files.end()) continue;
                        if (ExtractGameFile(m, gamePath, data)) from = m.id;
                        else log(L"mods: %S: couldn't read its gsdata_%S.dat", m.id.c_str(), lang);
                        break;
                    }
                if (from.empty()) continue;
                WriteText(skillDir + L"\\gsdata_" + Wide(lang) + L".dat", data);
                WriteText(skillDir + L"\\gsdata_" + Wide(lang) + L".txt", from);
                log(L"mods: skill data (gsdata_%S.dat, %zu bytes) from %S", lang, data.size(), from.c_str());
            }
        }

        // ---- the state file for pdnet's Mods menu ----
        size_t loose = LooseFiles(pdplDir, true);
        SYSTEMTIME t;
        GetLocalTime(&t);
        char when[32];
        sprintf_s(when, "%04u-%02u-%02u %02u:%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute);
        std::string state = std::string("# pdlaunch, ") + when + ": mods in priority order (first wins); tab-separated\n";
        state += "# mod id on|off source name version author online touches plugins files problem path load_after load_before description sha256(12) tag plugin_files warning map\n";
        if (lobbyStart) state += "lobbystart\t1\n"; // this start uses a lobby's mods (the Mods list says so)
        state += "loose\t" + std::to_string(loose) + "\n";
        for (const auto& m : mods)
            state += "mod\t" + m.id + "\t" + (m.enabled ? "on" : "off") + "\t" + Utf8(SourceName(m.source)) + "\t" + Field(m.name) + "\t" +
                     Field(m.version) + "\t" + Field(m.author) + "\t" + m.online + "\t" + Join(m.touches, ',') + "\t" + (m.plugins ? "1" : "0") + "\t" +
                     std::to_string(m.files.size()) + "\t" + Field(m.problem) + "\t" + Field(Utf8(m.path)) + "\t" + Join(m.after, ',') + "\t" + Join(m.before, ',') + "\t" + Field(m.description) + "\t" +
                     m.sha.substr(0, 12) + "\t" + Field(m.tag) + "\t" + Field(Join(m.pluginFiles, ';')) + "\t" + Field(m.warning) + "\t" + (m.isMap ? "map" : "") + "\n";
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
        return true;
    }
}
