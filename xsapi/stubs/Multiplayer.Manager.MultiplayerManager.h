#pragma once
#include "Multiplayer/Manager/MultiplayerManager.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    struct MultiplayerManager : MultiplayerManagerT<MultiplayerManager>
    {
        MultiplayerManager() = default;

        static Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerManager SingletonInstance();
        void Initialize(hstring const& lobbySessionTemplateName);
        void FindMatch(hstring const& hopperName, hstring const& attributes, Windows::Foundation::TimeSpan const& timeout);
        void CancelMatch();
        void JoinLobby(hstring const& handleId, Microsoft::Xbox::Services::System::XboxLiveUser const& user);
        void JoinLobby(Windows::ApplicationModel::Activation::IProtocolActivatedEventArgs const& eventArgs, Microsoft::Xbox::Services::System::XboxLiveUser const& user);
        void JoinGameFromLobby(hstring const& sessionTemplateName);
        void JoinGame(hstring const& sessionName, hstring const& sessionTemplateName);
        void JoinGame(hstring const& sessionName, hstring const& sessionTemplateName, Windows::Foundation::Collections::IVectorView<hstring> const& xboxUserIds);
        void LeaveGame();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerEvent> DoWork();
        Microsoft::Xbox::Services::Multiplayer::Manager::MatchStatus MatchStatus();
        Windows::Foundation::TimeSpan EstimatedMatchWaitTime();
        Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerLobbySession LobbySession();
        Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerGameSession GameSession();
        Microsoft::Xbox::Services::Multiplayer::Manager::Joinability Joinability();
        void SetJoinInProgress(Microsoft::Xbox::Services::Multiplayer::Manager::Joinability const& value);
        bool AutoFillMembersDuringMatchmaking();
        void AutoFillMembersDuringMatchmaking(bool value);
        void SetQualityOfServiceMeasurements(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerQualityOfServiceMeasurements> const& measurements);
    };
}
namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::factory_implementation
{
    struct MultiplayerManager : MultiplayerManagerT<MultiplayerManager, implementation::MultiplayerManager>
    {
    };
}
