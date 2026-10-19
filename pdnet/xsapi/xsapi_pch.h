#pragma once
// Precompiled-style include for pdnet's XSAPI stand-in (stubs/ and gen/module.g.cpp).
#include "../src/pch.h"
#include "../src/log.h"
#include <winrt/Windows.System.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Microsoft.Xbox.Services.h>
#include <winrt/Microsoft.Xbox.Services.Achievements.h>
#include <winrt/Microsoft.Xbox.Services.ContextualSearch.h>
#include <winrt/Microsoft.Xbox.Services.Events.h>
#include <winrt/Microsoft.Xbox.Services.GameServerPlatform.h>
#include <winrt/Microsoft.Xbox.Services.Leaderboard.h>
#include <winrt/Microsoft.Xbox.Services.Matchmaking.h>
#include <winrt/Microsoft.Xbox.Services.Multiplayer.h>
#include <winrt/Microsoft.Xbox.Services.Multiplayer.Manager.h>
#include <winrt/Microsoft.Xbox.Services.Presence.h>
#include <winrt/Microsoft.Xbox.Services.Privacy.h>
#include <winrt/Microsoft.Xbox.Services.RealTimeActivity.h>
#include <winrt/Microsoft.Xbox.Services.Social.h>
#include <winrt/Microsoft.Xbox.Services.Social.Manager.h>
#include <winrt/Microsoft.Xbox.Services.Statistics.Manager.h>
#include <winrt/Microsoft.Xbox.Services.System.h>
#include <winrt/Microsoft.Xbox.Services.TitleStorage.h>
#include <winrt/Microsoft.Xbox.Services.Tournaments.h>
#include <winrt/Microsoft.Xbox.Services.UserStatistics.h>

// Unimplemented XSAPI members: log the first few calls of each, then tell the game "not implemented".
namespace pdnet::xsapi
{
    void ReportStub(const char* function);
}
#define PDNET_XSAPI_STUB(fn) do { ::pdnet::xsapi::ReportStub(fn); throw ::winrt::hresult_not_implemented(); } while (0)
#define PDNET_XSAPI_STUB_NOEXCEPT(fn) ::pdnet::xsapi::ReportStub(fn)
