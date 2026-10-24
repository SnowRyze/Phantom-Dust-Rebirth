#pragma once
// Run-time binding to the EOS SDK (EOSSDK-Win64-Shipping.dll next to pdnet), so the game still starts without it.
#ifndef PDNET_NO_EOS
#include <eos_sdk.h>
#include <eos_logging.h>
#include <eos_connect.h>
#include <eos_p2p.h>
#include <eos_lobby.h>
#include <eos_rtc.h>
#include <eos_rtc_audio.h>
#include <eos_stats.h>
#include <eos_leaderboards.h>
#include <Windows/eos_Windows.h>
#include <map>
#include <string>

namespace pdnet::eossdk
{
    struct Api
    {
        HMODULE module = nullptr;
#define PDNET_EOS_FUNCTIONS(X)                                                                                    \
        X(EOS_Initialize) X(EOS_Shutdown) X(EOS_Logging_SetCallback) X(EOS_Logging_SetLogLevel)                   \
        X(EOS_Platform_Create) X(EOS_Platform_Tick) X(EOS_Platform_Release) X(EOS_Platform_GetConnectInterface)   \
        X(EOS_Platform_GetP2PInterface) X(EOS_Platform_GetLobbyInterface)                                         \
        X(EOS_Connect_CreateDeviceId) X(EOS_Connect_Login) X(EOS_Connect_CreateUser) X(EOS_Connect_GetLoginStatus) \
        X(EOS_Connect_AddNotifyAuthExpiration) X(EOS_Connect_RemoveNotifyAuthExpiration)                          \
        X(EOS_Connect_AddNotifyLoginStatusChanged) X(EOS_Connect_RemoveNotifyLoginStatusChanged)                  \
        X(EOS_ProductUserId_ToString) X(EOS_ProductUserId_FromString) X(EOS_P2P_QueryNATType)                     \
        X(EOS_EResult_ToString)                                                                                   \
        PDNET_EOS_LOBBY_FUNCTIONS(X) PDNET_EOS_P2P_FUNCTIONS(X) PDNET_EOS_VOICE_FUNCTIONS(X) PDNET_EOS_SCORE_FUNCTIONS(X)
// Scoreboard: the rating stat and the global leaderboard (see scores.h).
#define PDNET_EOS_SCORE_FUNCTIONS(X)                                                                              \
        X(EOS_Platform_GetStatsInterface) X(EOS_Stats_IngestStat)                                                 \
        X(EOS_Platform_GetLeaderboardsInterface) X(EOS_Leaderboards_QueryLeaderboardRanks)                       \
        X(EOS_Leaderboards_GetLeaderboardRecordCount) X(EOS_Leaderboards_CopyLeaderboardRecordByIndex)           \
        X(EOS_Leaderboards_LeaderboardRecord_Release)
// Voice: each lobby's EOS voice room (see lobbies.h, voice_chat).
#define PDNET_EOS_VOICE_FUNCTIONS(X)                                                                              \
        X(EOS_Platform_GetRTCInterface) X(EOS_RTC_GetAudioInterface)                                              \
        X(EOS_RTC_AddNotifyParticipantStatusChanged) X(EOS_RTC_RemoveNotifyParticipantStatusChanged)             \
        X(EOS_RTCAudio_UpdateSending) X(EOS_RTCAudio_UpdateReceiving)                                             \
        X(EOS_RTCAudio_AddNotifyParticipantUpdated) X(EOS_RTCAudio_RemoveNotifyParticipantUpdated)               \
        X(EOS_RTCAudio_AddNotifyAudioBeforeSend) X(EOS_RTCAudio_RemoveNotifyAudioBeforeSend)                     \
        X(EOS_RTCAudio_AddNotifyAudioInputState) X(EOS_RTCAudio_RemoveNotifyAudioInputState)                     \
        X(EOS_RTCAudio_QueryInputDevicesInformation) X(EOS_RTCAudio_GetInputDevicesCount)                       \
        X(EOS_RTCAudio_CopyInputDeviceInformationByIndex) X(EOS_RTCAudio_InputDeviceInformation_Release)         \
        X(EOS_Lobby_GetRTCRoomName) X(EOS_Lobby_AddNotifyRTCRoomConnectionChanged)
// P2P interface (stage C: match traffic through EOS P2P, see p2p.h).
#define PDNET_EOS_P2P_FUNCTIONS(X)                                                                                \
        X(EOS_P2P_SendPacket) X(EOS_P2P_GetNextReceivedPacketSize) X(EOS_P2P_ReceivePacket)                       \
        X(EOS_P2P_AcceptConnection) X(EOS_P2P_CloseConnection) X(EOS_P2P_SetRelayControl)                         \
        X(EOS_P2P_SetPacketQueueSize) X(EOS_P2P_GetNATType)                                                       \
        X(EOS_P2P_AddNotifyPeerConnectionRequest) X(EOS_P2P_AddNotifyPeerConnectionEstablished)                   \
        X(EOS_P2P_AddNotifyPeerConnectionInterrupted) X(EOS_P2P_AddNotifyPeerConnectionClosed)
// Lobby interface (stage B: multiplayer sessions shared through EOS lobbies, see lobbies.h).
#define PDNET_EOS_LOBBY_FUNCTIONS(X)                                                                              \
        X(EOS_Lobby_CreateLobby) X(EOS_Lobby_DestroyLobby) X(EOS_Lobby_JoinLobbyById) X(EOS_Lobby_LeaveLobby)     \
        X(EOS_Lobby_UpdateLobbyModification) X(EOS_Lobby_UpdateLobby) X(EOS_Lobby_PromoteMember)                  \
        X(EOS_Lobby_CopyLobbyDetailsHandle) X(EOS_Lobby_CreateLobbySearch)                                        \
        X(EOS_Lobby_AddNotifyLobbyUpdateReceived) X(EOS_Lobby_RemoveNotifyLobbyUpdateReceived)                    \
        X(EOS_Lobby_AddNotifyLobbyMemberUpdateReceived) X(EOS_Lobby_RemoveNotifyLobbyMemberUpdateReceived)        \
        X(EOS_Lobby_AddNotifyLobbyMemberStatusReceived) X(EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived)        \
        X(EOS_LobbyModification_SetPermissionLevel) X(EOS_LobbyModification_SetMaxMembers)                        \
        X(EOS_LobbyModification_AddAttribute) X(EOS_LobbyModification_RemoveAttribute)                           \
        X(EOS_LobbyModification_AddMemberAttribute) X(EOS_LobbyModification_RemoveMemberAttribute)               \
        X(EOS_LobbyModification_Release)                                                                          \
        X(EOS_LobbyDetails_GetLobbyOwner) X(EOS_LobbyDetails_CopyInfo) X(EOS_LobbyDetails_Info_Release)           \
        X(EOS_LobbyDetails_GetAttributeCount) X(EOS_LobbyDetails_CopyAttributeByIndex)                            \
        X(EOS_LobbyDetails_GetMemberCount) X(EOS_LobbyDetails_GetMemberByIndex)                                   \
        X(EOS_LobbyDetails_GetMemberAttributeCount) X(EOS_LobbyDetails_CopyMemberAttributeByIndex)                \
        X(EOS_LobbyDetails_Release) X(EOS_Lobby_Attribute_Release)                                                \
        X(EOS_LobbySearch_SetParameter) X(EOS_LobbySearch_SetLobbyId) X(EOS_LobbySearch_SetMaxResults)            \
        X(EOS_LobbySearch_Find) X(EOS_LobbySearch_GetSearchResultCount) X(EOS_LobbySearch_CopySearchResultByIndex) \
        X(EOS_LobbySearch_Release)
#define PDNET_EOS_MEMBER(name) decltype(&::name) name = nullptr;
        PDNET_EOS_FUNCTIONS(PDNET_EOS_MEMBER)
#undef PDNET_EOS_MEMBER
    };

    // Loads the SDK from `dir` once, resolves every function above, and gives EOS a private credential store
    // (the sandbox can't use Windows Credential Manager). Returns null and logs why if anything is missing.
    const Api* Load(const std::wstring& dir);

    // EOS_Initialize + SDK log forwarding (warnings and up). Safe to call more than once.
    bool Initialize(const Api& api, const char* productVersion);

    // Credentials from eos.ini in `dir`: ProductId, SandboxId, DeploymentId, ClientId, ClientSecret.
    // Returns the name of the first missing key, or empty when all are present.
    std::string ReadCredentials(const std::wstring& dir, std::map<std::string, std::string>& out);

    // EOS_Platform_Create with those credentials, the overlay disabled and the cache in LocalState\eos\cache.
    EOS_HPlatform CreatePlatform(const Api& api, const std::map<std::string, std::string>& creds);

    std::wstring Result(const Api& api, EOS_EResult r);
    std::wstring UserId(const Api& api, EOS_ProductUserId id);
    std::wstring Wide(const char* s);

    // Directory of pdnet's DLL (with trailing backslash).
    std::wstring ModuleDir();

    // EOS completes calls through callbacks during EOS_Platform_Tick. Completion<Info> captures one result;
    // TickUntil ticks on the calling thread (which must be the EOS thread) until it arrives or time runs out.
    template <typename Info>
    struct Completion
    {
        bool done = false;
        Info info{};
        std::string lobbyId; // EOS strings are only valid inside the callback, so this one is copied

        static void EOS_CALL Callback(const Info* data)
        {
            auto self = static_cast<Completion*>(data->ClientData);
            self->info = *data;
            if constexpr (requires { data->LobbyId; })
                if (data->LobbyId) self->lobbyId = data->LobbyId;
            self->done = true;
        }
    };

    template <typename Info>
    bool TickUntil(const Api& api, EOS_HPlatform platform, const Completion<Info>& c, uint32_t timeoutMs)
    {
        auto end = GetTickCount64() + timeoutMs;
        while (!c.done && GetTickCount64() < end)
        {
            api.EOS_Platform_Tick(platform);
            Sleep(20);
        }
        return c.done;
    }
}
#endif
