// lobbytool: a second EOS client on the same PC, for testing stage B without a second machine.
// It is built from pdnet's own lobbies.cpp / eos.cpp, but runs as a normal desktop program, so the EOS SDK keeps its
// device ID in Windows Credential Manager instead of the game's LocalState: it signs in as a different product user.
//
//   lobbytool search [template]          list the game's lobbies (bucket pd1|<scid>|<template>, default SearchableGame)
//   lobbytool show <lobby id>            one lobby with all attributes and members
//   lobbytool join <lobby id> [seconds]  join as a fake player "lobbytool", print change notifications, then leave
//   lobbytool host <lobby id> [seconds]  self-test of the lobby layer in a separate bucket (create, search, update, destroy)
//
// Needs EOSSDK-Win64-Shipping.dll and eos.ini next to lobbytool.exe (build-lobbytool.cmd copies them).
#include "pch.h"
#include "config.h"
#include "eos.h"
#include "lobbies.h"
#include "log.h"
#include <cstdarg>
#include <cstdio>

namespace pdnet
{
    // pdnet support functions, desktop versions.
    void Log(const wchar_t* fmt, ...)
    {
        SYSTEMTIME t;
        GetLocalTime(&t);
        wchar_t line[4096];
        va_list args;
        va_start(args, fmt);
        _vsnwprintf_s(line, _TRUNCATE, fmt, args);
        va_end(args);
        fwprintf(stdout, L"%02d:%02d:%02d.%03d %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, line);
        fflush(stdout);
    }

    const std::wstring& LocalStatePath()
    {
        static std::wstring path = []
        {
            wchar_t exe[MAX_PATH];
            GetModuleFileNameW(nullptr, exe, MAX_PATH);
            std::wstring dir = exe;
            dir = dir.substr(0, dir.find_last_of(L'\\')) + L"\\state";
            CreateDirectoryW(dir.c_str(), nullptr);
            return dir;
        }();
        return path;
    }

    const Config& GetConfig()
    {
        static Config config = []
        {
            Config c;
            c.eos = true;
            c.displayName = L"lobbytool";
            return c;
        }();
        return config;
    }

    // Desktop programs may use Credential Manager, so nothing is redirected (and the device ID differs from the game's).
    int RedirectCredentialStore(HMODULE, const std::wstring&) { return 0; }
}

namespace
{
    using namespace pdnet;
    const char* kScid = "164d0100-a5f0-44b8-90ce-bf3a0fede427"; // xboxservices.config PrimaryServiceConfigId

    void Print(const lobbies::Lobby& l)
    {
        wprintf(L"lobby %S  owner %S  bucket %S  members %u/%u%s\n", l.id.c_str(), l.ownerId.c_str(), l.bucket.c_str(), l.memberCount, l.maxMembers,
            l.joined ? L"  (joined)" : L"  (search result: member details hidden)");
        for (auto& [k, v] : l.attributes) wprintf(L"    %-14S %S\n", k.c_str(), v.c_str());
        for (auto& m : l.members)
        {
            wprintf(L"  member %S%s\n", m.userId.c_str(), m.userId == l.ownerId ? L" (owner)" : L"");
            for (auto& [k, v] : m.attributes) wprintf(L"      %-12S %S\n", k.c_str(), v.c_str());
        }
    }

    bool WaitForSignIn()
    {
        for (int i = 0; i < 300 && !lobbies::Available(); ++i) Sleep(100);
        if (!lobbies::Available()) Log(L"lobbytool: EOS sign-in did not finish in 30 s");
        return lobbies::Available();
    }
}

int wmain(int argc, wchar_t** argv)
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    std::wstring command = argc > 1 ? argv[1] : L"";
    if (command != L"search" && command != L"show" && command != L"join" && command != L"host")
    {
        wprintf(L"usage: lobbytool search [template] | show <lobby id> | join <lobby id> [seconds] | host <lobby id> [seconds]\n");
        return 2;
    }
    eos::Start();
    if (!WaitForSignIn()) return 1;
    Log(L"lobbytool: signed in as product user %s", eos::LocalUserId().c_str());

    if (command == L"search")
    {
        std::string templ = argc > 2 ? winrt::to_string(argv[2]) : "SearchableGame";
        std::string bucket = std::string("pd1|") + kScid + "|" + templ;
        auto found = lobbies::Search(bucket);
        Log(L"lobbytool: %zu lobby(s) in bucket %S", found.size(), bucket.c_str());
        for (auto& l : found) Print(l);
        return 0;
    }

    if (argc < 3)
    {
        wprintf(L"lobby id missing\n");
        return 2;
    }
    std::string id = winrt::to_string(argv[2]);
    if (command == L"show")
    {
        auto l = lobbies::Read(id);
        if (!l)
        {
            Log(L"lobbytool: lobby %S not found", id.c_str());
            return 1;
        }
        Print(*l);
        return 0;
    }

    // join: look like a pdnet player (see xsapi\mpsd.h for the member attributes).
    int seconds = argc > 3 ? _wtoi(argv[3]) : 60;
    if (command == L"host")
    {
        // Self-test of the lobby layer: create with a long (split) attribute, find it by bucket and by id, update, destroy.
        std::string bucket = std::string("pd1|") + kScid + "|lobbytool-test";
        std::string longValue;
        for (int i = 0; i < 130; ++i) longValue += "0123456789"; // 1300 bytes -> 3 pieces
        lobbies::Attributes attrs = { { "PD_VERSION", "1" }, { "PD_NAME", "selftest" } };
        lobbies::Split(attrs, "PD_PROPS", longValue);
        auto created = lobbies::Create(id, bucket, 4, attrs, { { "PD_XUID", "1" }, { "PD_NAME", "lobbytool" } });
        if (!created) return 1;
        Print(*created);
        bool roundTrip = lobbies::Joined(created->attributes, "PD_PROPS") == longValue;
        auto found = lobbies::Search(bucket);
        auto byId = lobbies::Read(id);
        bool updated = lobbies::Update(id, lobbies::Changes{ { { "PD_CLOSED", "1" } }, { "PD_NAME" } }, lobbies::Changes{ { { "PD_ADDR", "abc" } }, {} });
        auto after = lobbies::Read(id);
        bool updateSeen = after && after->attributes.count("PD_CLOSED") && !after->attributes.count("PD_NAME") && !after->members.empty() && after->members[0].attributes.count("PD_ADDR");
        Log(L"selftest: split round trip %s, search found %zu, read by id %s, update %s / visible %s", roundTrip ? L"ok" : L"FAIL", found.size(), byId ? L"ok" : L"FAIL",
            updated ? L"ok" : L"FAIL", updateSeen ? L"ok" : L"FAIL");
        if (seconds > 0)
        {
            Log(L"lobbytool: lobby stays up for %d s", seconds);
            Sleep(seconds * 1000);
        }
        Log(L"selftest: destroy %s", lobbies::Destroy(id) ? L"ok" : L"FAIL");
        return 0;
    }
    lobbies::SetListener([](const std::string& lobbyId, lobbies::Event e, const std::string& user)
    {
        static const wchar_t* names[] = { L"lobby updated", L"member updated", L"member joined", L"member left", L"promoted", L"closed", L"kicked" };
        Log(L"lobbytool: event %s (lobby %S, user %S)", names[static_cast<int>(e)], lobbyId.c_str(), user.c_str());
    });
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    int64_t ticks = (static_cast<int64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
    lobbies::Attributes me = {
        { "PD_XUID", "2533274790395903" }, { "PD_NAME", "lobbytool" }, { "PD_STATUS", "3" /* Active */ }, { "PD_INIT", "0" }, { "PD_JOIN", std::to_string(ticks) },
        // A well-formed pdnet snapshot (game 43213, control 43212) at 192.0.2.1, a TEST-NET address nobody answers on,
        // so the game can parse it and its connection attempt simply times out.
        { "PD_ADDR", "UEROMQEAqM2ozAAAAAAAAAAAAAD//8AAAgEAAAAB2x7nAQ==" },
    };
    auto l = lobbies::Join(id, me);
    if (!l)
    {
        Log(L"lobbytool: could not join %S", id.c_str());
        return 1;
    }
    Print(*l);
    Log(L"lobbytool: staying for %d s", seconds);
    for (int i = 0; i < seconds; ++i) Sleep(1000);
    if (auto after = lobbies::Read(id)) Print(*after);
    lobbies::Leave(id);
    Log(L"lobbytool: left");
    return 0;
}
