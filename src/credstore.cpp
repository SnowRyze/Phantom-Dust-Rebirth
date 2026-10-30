#include "pch.h"
#include "credstore.h"
#include "log.h"
#include <wincred.h>
#include <dpapi.h>
#include <fstream>
#include <set>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")

namespace pdnet
{
    namespace
    {
        std::mutex g_lock;
        std::wstring g_dir;
        std::set<void*> g_ours; // CREDENTIALW blocks we allocated (freed by our CredFree)
        decltype(&CredFree) g_realCredFree = nullptr;

        std::wstring FileFor(LPCWSTR target, DWORD type)
        {
            std::wstring name;
            for (const wchar_t* p = target ? target : L""; *p; ++p)
            {
                wchar_t c = *p;
                if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'-' || c == L'.')
                    name += c;
                else
                {
                    wchar_t hex[8];
                    swprintf_s(hex, L"_%04X", c);
                    name += hex;
                }
            }
            return g_dir + L"\\" + name + L"~" + std::to_wstring(type) + L".cred";
        }

        // File: 'D' + DPAPI-protected payload, or 'P' + payload if DPAPI is unavailable.
        // Payload: persist u32 | userName chars u32 | userName UTF-16 | blob size u32 | blob
        bool Save(const std::wstring& path, const std::vector<uint8_t>& payload)
        {
            DATA_BLOB in{ static_cast<DWORD>(payload.size()), const_cast<BYTE*>(payload.data()) }, out{};
            std::vector<uint8_t> file;
            if (CryptProtectData(&in, L"pdnet-eos", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
            {
                file.push_back('D');
                file.insert(file.end(), out.pbData, out.pbData + out.cbData);
                LocalFree(out.pbData);
            }
            else
            {
                Log(L"credstore: DPAPI unavailable (%lu), storing unprotected in the app's private folder", GetLastError());
                file.push_back('P');
                file.insert(file.end(), payload.begin(), payload.end());
            }
            std::wstring tmp = path + L".tmp";
            {
                std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
                if (!f.write(reinterpret_cast<const char*>(file.data()), file.size())) return false;
            }
            return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
        }

        bool Load(const std::wstring& path, std::vector<uint8_t>& payload)
        {
            std::ifstream f(path, std::ios::binary);
            if (!f) return false;
            std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            if (file.empty()) return false;
            if (file[0] == 'P')
            {
                payload.assign(file.begin() + 1, file.end());
                return true;
            }
            DATA_BLOB in{ static_cast<DWORD>(file.size() - 1), file.data() + 1 }, out{};
            if (file[0] != 'D' || !CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return false;
            payload.assign(out.pbData, out.pbData + out.cbData);
            LocalFree(out.pbData);
            return true;
        }

        void Put32(std::vector<uint8_t>& b, uint32_t v) { b.insert(b.end(), reinterpret_cast<uint8_t*>(&v), reinterpret_cast<uint8_t*>(&v) + 4); }
        bool Get32(const std::vector<uint8_t>& b, size_t& at, uint32_t& v)
        {
            if (at + 4 > b.size()) return false;
            memcpy(&v, &b[at], 4);
            at += 4;
            return true;
        }

        BOOL WINAPI StoreCredWriteW(PCREDENTIALW cred, DWORD)
        {
            if (!cred || !cred->TargetName) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
            std::vector<uint8_t> payload;
            Put32(payload, cred->Persist);
            std::wstring user = cred->UserName ? cred->UserName : L"";
            Put32(payload, static_cast<uint32_t>(user.size()));
            payload.insert(payload.end(), reinterpret_cast<const uint8_t*>(user.data()), reinterpret_cast<const uint8_t*>(user.data() + user.size()));
            Put32(payload, cred->CredentialBlobSize);
            if (cred->CredentialBlobSize) payload.insert(payload.end(), cred->CredentialBlob, cred->CredentialBlob + cred->CredentialBlobSize);

            std::lock_guard guard(g_lock);
            CreateDirectoryW(g_dir.c_str(), nullptr);
            bool ok = Save(FileFor(cred->TargetName, cred->Type), payload);
            Log(L"credstore: write '%s' (type %lu, %lu bytes) -> %s", cred->TargetName, cred->Type, cred->CredentialBlobSize, ok ? L"ok" : L"FAILED");
            if (!ok) { SetLastError(ERROR_WRITE_FAULT); return FALSE; }
            return TRUE;
        }

        BOOL WINAPI StoreCredReadW(LPCWSTR target, DWORD type, DWORD, PCREDENTIALW* result)
        {
            if (!target || !result) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
            *result = nullptr;
            std::vector<uint8_t> payload;
            std::lock_guard guard(g_lock);
            if (!Load(FileFor(target, type), payload))
            {
                Log(L"credstore: read '%s' (type %lu) -> not found", target, type);
                SetLastError(ERROR_NOT_FOUND);
                return FALSE;
            }
            size_t at = 0;
            uint32_t persist = 0, userChars = 0, blobSize = 0;
            if (!Get32(payload, at, persist) || !Get32(payload, at, userChars) || at + userChars * 2 > payload.size())
            {
                SetLastError(ERROR_NOT_FOUND);
                return FALSE;
            }
            std::wstring user(reinterpret_cast<const wchar_t*>(&payload[at]), userChars);
            at += userChars * 2;
            if (!Get32(payload, at, blobSize) || at + blobSize > payload.size())
            {
                SetLastError(ERROR_NOT_FOUND);
                return FALSE;
            }

            // One allocation, like the real API: struct, then target, user name and blob.
            size_t targetBytes = (wcslen(target) + 1) * sizeof(wchar_t), userBytes = (user.size() + 1) * sizeof(wchar_t);
            auto block = static_cast<uint8_t*>(calloc(1, sizeof(CREDENTIALW) + targetBytes + userBytes + blobSize));
            if (!block) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
            auto cred = reinterpret_cast<CREDENTIALW*>(block);
            uint8_t* p = block + sizeof(CREDENTIALW);
            cred->TargetName = reinterpret_cast<LPWSTR>(p);
            memcpy(p, target, targetBytes);
            p += targetBytes;
            cred->UserName = reinterpret_cast<LPWSTR>(p);
            memcpy(p, user.c_str(), userBytes);
            p += userBytes;
            cred->CredentialBlob = blobSize ? p : nullptr;
            if (blobSize) memcpy(p, &payload[at], blobSize);
            cred->CredentialBlobSize = blobSize;
            cred->Type = type;
            cred->Persist = persist;
            g_ours.insert(block);
            *result = cred;
            Log(L"credstore: read '%s' (type %lu) -> ok", target, type);
            return TRUE;
        }

        BOOL WINAPI StoreCredDeleteW(LPCWSTR target, DWORD type, DWORD)
        {
            std::lock_guard guard(g_lock);
            bool ok = DeleteFileW(FileFor(target, type).c_str()) != 0;
            Log(L"credstore: delete '%s' (type %lu) -> %s", target ? target : L"", type, ok ? L"ok" : L"not found");
            if (!ok) { SetLastError(ERROR_NOT_FOUND); return FALSE; }
            return TRUE;
        }

        VOID WINAPI StoreCredFree(PVOID buffer)
        {
            {
                std::lock_guard guard(g_lock);
                if (g_ours.erase(buffer))
                {
                    free(buffer);
                    return;
                }
            }
            if (g_realCredFree) g_realCredFree(buffer);
        }

        int PatchImports(HMODULE module, const std::map<std::string, void*>& replacements)
        {
            auto base = reinterpret_cast<uint8_t*>(module);
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
            auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            int patched = 0;
            for (auto desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); dir.VirtualAddress && desc->Name; ++desc)
            {
                auto dll = reinterpret_cast<const char*>(base + desc->Name);
                if (_stricmp(dll, "advapi32.dll") != 0 && _strnicmp(dll, "api-ms-win-security-credentials-", 32) != 0) continue;
                auto names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk));
                auto slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);
                for (; names->u1.AddressOfData; ++names, ++slots)
                {
                    if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
                    auto byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
                    auto it = replacements.find(reinterpret_cast<const char*>(byName->Name));
                    if (it == replacements.end()) continue;
                    DWORD old;
                    VirtualProtect(&slots->u1.Function, sizeof(ULONG_PTR), PAGE_READWRITE, &old);
                    slots->u1.Function = reinterpret_cast<ULONG_PTR>(it->second);
                    VirtualProtect(&slots->u1.Function, sizeof(ULONG_PTR), old, &old);
                    ++patched;
                }
            }
            return patched;
        }
    }

    int RedirectCredentialStore(HMODULE eosModule, const std::wstring& directory)
    {
        g_dir = directory;
        g_realCredFree = reinterpret_cast<decltype(&CredFree)>(GetProcAddress(GetModuleHandleW(L"advapi32.dll"), "CredFree"));
        int n = PatchImports(eosModule, {
            { "CredReadW", reinterpret_cast<void*>(&StoreCredReadW) },
            { "CredWriteW", reinterpret_cast<void*>(&StoreCredWriteW) },
            { "CredDeleteW", reinterpret_cast<void*>(&StoreCredDeleteW) },
            { "CredFree", reinterpret_cast<void*>(&StoreCredFree) },
        });
        Log(L"credstore: redirected %d EOS credential imports to %s", n, directory.c_str());
        return n;
    }
}
