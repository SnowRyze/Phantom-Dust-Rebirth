#include "xsapi_pch.h"
#include "Multiplayer.Manager.MultiplayerManager.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerManager MultiplayerManager::SingletonInstance()
    {
        PDNET_XSAPI_STUB("MultiplayerManager::SingletonInstance");
    }
    void MultiplayerManager::Initialize(hstring const& lobbySessionTemplateName)
    {
        PDNET_XSAPI_STUB("MultiplayerManager::Initialize");
    }
    void MultiplayerManager::FindMatch(hstring const& hopperName, hstring const& attributes, Windows::Foundation::TimeSpan const& timeout)
    {
        PDNET_XSAPI_STUB("MultiplayerManager::FindMatch");
    }
    void MultiplayerManager::CancelMatch()
    {
        PDNET_XSAPI_STUB("MultiplayerManager::CancelMatch");
    }
    void MultiplayerManager::JoinLobby(hstring const& handleId, Microsoft::Xbox::Services::System::XboxLiveUser const& user)
    {
        PDNET_XSAPI_STUB("MultiplayerManager::JoinLobby");
    }
    void MultiplayerManager::JoinLobby(Windows::ApplicationModel::Activation::IProtocolActivatedEventArgs const& eventArgs, Microsoft::Xbox::Services::System::XboxLiveUser const& user)
    {
        PDNET_XSAPI_STUB("MultiplayerManager::JoinLobby");
    }
    void MultiplayerManager::JoinGameFromLobby(hstring const& sessionTemplateName)
    {
        PDNET_XSAPI_STUB("MultiplayerManager::JoinGameFromLobby");
    }
    void MultiplayerManager::JoinGame(hstring const& sessionName, hstring const& sessionTemplateName)
    {
        PDNET_XSAPI_STUB("MultiplayerManager::JoinGame");
    }
    void MultiplayerManager::JoinGame(hstring const& sessionName, hstring const& sessionTemplateName, Windows::Foundation::Collections::IVectorView<hstring> const& xboxUserIds)
    {
        PDNET_XSAPI_STUB("MultiplayerManager::JoinGame");
    }
    void MultiplayerManager::LeaveGame()
    {
        PDNET_XSAPI_STUB("MultiplayerManager::LeaveGame");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerEvent> MultiplayerManager::DoWork()
    {
        PDNET_XSAPI_STUB("MultiplayerManager::DoWork");
    }
    Microsoft::Xbox::Services::Multiplayer::Manager::MatchStatus MultiplayerManager::MatchStatus()
    {
        PDNET_XSAPI_STUB("MultiplayerManager::MatchStatus");
    }
    Windows::Foundation::TimeSpan MultiplayerManager::EstimatedMatchWaitTime()
    {
        PDNET_XSAPI_STUB("MultiplayerManager::EstimatedMatchWaitTime");
    }
    Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerLobbySession MultiplayerManager::LobbySession()
    {
        PDNET_XSAPI_STUB("MultiplayerManager::LobbySession");
    }
    Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerGameSession MultiplayerManager::GameSession()
    {
        PDNET_XSAPI_STUB("MultiplayerManager::GameSession");
    }
    Microsoft::Xbox::Services::Multiplayer::Manager::Joinability MultiplayerManager::Joinability()
    {
        PDNET_XSAPI_STUB("MultiplayerManager::Joinability");
    }
    void MultiplayerManager::SetJoinInProgress(Microsoft::Xbox::Services::Multiplayer::Manager::Joinability const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerManager::SetJoinInProgress");
    }
    bool MultiplayerManager::AutoFillMembersDuringMatchmaking()
    {
        PDNET_XSAPI_STUB("MultiplayerManager::AutoFillMembersDuringMatchmaking");
    }
    void MultiplayerManager::AutoFillMembersDuringMatchmaking(bool value)
    {
        PDNET_XSAPI_STUB("MultiplayerManager::AutoFillMembersDuringMatchmaking");
    }
    void MultiplayerManager::SetQualityOfServiceMeasurements(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerQualityOfServiceMeasurements> const& measurements)
    {
        PDNET_XSAPI_STUB("MultiplayerManager::SetQualityOfServiceMeasurements");
    }
}
