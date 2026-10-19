#pragma once
#include "Multiplayer/MultiplayerSession.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSession : MultiplayerSessionT<MultiplayerSession>
    {
        MultiplayerSession() = default;

        MultiplayerSession(Microsoft::Xbox::Services::XboxLiveContext const& xboxLiveContext);
        MultiplayerSession(Microsoft::Xbox::Services::XboxLiveContext const& xboxLiveContext, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference const& multiplayerSessionReference, uint32_t maxMembersInSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility const& multiplayerSessionVisibility, Windows::Foundation::Collections::IVectorView<hstring> const& initiatorXboxUserIds, hstring const& sessionCustomConstantsJson);
        MultiplayerSession(Microsoft::Xbox::Services::XboxLiveContext const& xboxLiveContext, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference const& multiplayerSessionReference);
        static Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes CompareMultiplayerSessions(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession const& currentSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession const& oldSession);
        static Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus ConvertHttpStatusToWriteSessionStatus(int32_t httpStatusCode);
        hstring MultiplayerCorrelationId();
        hstring SearchHandleId();
        Microsoft::Xbox::Services::Tournaments::TournamentArbitrationStatus ArbitrationStatus();
        Windows::Foundation::DateTime StartTime();
        Windows::Foundation::DateTime DateOfNextTimer();
        Windows::Foundation::DateTime DateOfSession();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerInitializationStage InitializationStage();
        Windows::Foundation::DateTime InitializingStageStartTime();
        uint32_t InitializingEpisode();
        Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus WriteStatus();
        Windows::Foundation::Collections::IVectorView<hstring> HostCandidates();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference SessionReference();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionConstants SessionConstants();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionProperties SessionProperties();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRoleTypes SessionRoleTypes();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> Members();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMatchmakingServer MatchmakingServer();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionTournamentsServer TournamentsServer();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionArbitrationServer ArbitrationServer();
        uint32_t MembersAccepted();
        hstring ServersJson();
        void ServersJson(hstring const& value);
        hstring ETag();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes SubscribedChangeTypes();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember CurrentUser();
        hstring Branch();
        uint64_t ChangeNumber();
        void AddMemberReservation(hstring const& xboxUserId, hstring const& memberCustomConstantsJson);
        void AddMemberReservation(hstring const& xboxUserId, hstring const& memberCustomConstantsJson, bool initializeRequested);
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember Join();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember Join(hstring const& memberCustomConstantsJson);
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember Join(hstring const& memberCustomConstantsJson, bool initializeRequested);
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember Join(hstring const& memberCustomConstantsJson, bool initializeRequested, bool joinWithActiveStatus);
        void SetVisibility(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility const& visibility);
        void SetMaxMembersInSession(uint32_t maxMembersInSession);
        void SetMutableRoleSettings(Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerRoleType> const& roleTypes);
        void SetTimeouts(Windows::Foundation::TimeSpan const& memberReservedTimeout, Windows::Foundation::TimeSpan const& memberInactiveTimeout, Windows::Foundation::TimeSpan const& memberReadyTimeout, Windows::Foundation::TimeSpan const& sessionEmptyTimeout);
        void SetArbitrationTimeouts(Windows::Foundation::TimeSpan const& arbitrationTimeout, Windows::Foundation::TimeSpan const& forfeitTimeout);
        void SetQualityOfServiceConnectivityMetrics(bool enableLatencyMetric, bool enableBandwidthDownMetric, bool enableBandwidthUpMetric, bool enableCustomMetric);
        void SetManagedInitialization(Windows::Foundation::TimeSpan const& joinTimeout, Windows::Foundation::TimeSpan const& measurementTimeout, Windows::Foundation::TimeSpan const& evaluationTimeout, bool autoEvalute, uint32_t membersNeededToStart);
        void SetMemberInitialization(Windows::Foundation::TimeSpan const& joinTimeout, Windows::Foundation::TimeSpan const& measurementTimeout, Windows::Foundation::TimeSpan const& evaluationTimeout, bool autoEvalute, uint32_t membersNeededToStart);
        void SetPeerToPeerRequirements(Windows::Foundation::TimeSpan const& latencyMaximum, uint32_t bandwidthMinimumInKilobitsPerSecond);
        void SetPeerToHostRequirements(Windows::Foundation::TimeSpan const& latencyMaximum, uint32_t bandwidthDownMinimumInKilobitsPerSecond, uint32_t bandwidthUpMinimumInKilobitsPerSecond, Microsoft::Xbox::Services::Multiplayer::MultiplayMetrics const& hostSelectionMetric);
        void SetMeasurementServerAddresses(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::GameServerPlatform::QualityOfServiceServer> const& measurementServerAddresses);
        void SetCloudComputePackageJson(hstring const& sessionCloudComputePackageConstantsJson);
        void SetSessionCapabilities(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionCapabilities const& capabilities);
        void SetInitializationStatus(bool initializationSucceeded);
        void SetHostDeviceToken(hstring const& hostDeviceToken);
        void SetMatchmakingServerConnectionPath(hstring const& serverConnectionPath);
        void SetClosed(bool closed);
        void SetAllocateCloudCompute(bool allocateCloudCompute);
        void SetMatchmakingResubmit(bool matchResubmit);
        void SetServerConnectionStringCandidates(Windows::Foundation::Collections::IVectorView<hstring> const& serverConnectionStringCandidates);
        void SetSessionChangeSubscription(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes const& changeTypes);
        void Leave();
        void SetCurrentUserStatus(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMemberStatus const& status);
        void SetCurrentUserSecureDeviceAddressBase64(hstring const& value);
        void SetCurrentUserRoleInfo(Windows::Foundation::Collections::IMapView<hstring, hstring> const& roles);
        void SetCurrentUserMembersInGroup(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> const& membersInGroup);
        void SetCurrentUserQualityOfServiceMeasurements(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerQualityOfServiceMeasurements> const& measurements);
        void SetCurrentUserQualityOfServiceServerMeasurementsJson(hstring const& valueJson);
        void SetCurrentUserArbitrationResults(Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Tournaments::TournamentTeamResult> const& results);
        void SetCurrentUserMemberCustomPropertyJson(hstring const& name, hstring const& valueJson);
        void DeleteCurrentUserMemberCustomPropertyJson(hstring const& name);
        void SetMatchmakingTargetSessionConstantsJson(hstring const& matchmakingTargetSessionConstants);
        void SetSessionCustomPropertyJson(hstring const& name, hstring const& valueJson);
        void DeleteSessionCustomPropertyJson(hstring const& name);
        void _Init(Microsoft::Xbox::Services::XboxLiveContext const& xboxLiveContext, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference const& multiplayerSessionReference, uint32_t maxMembersInSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility const& multiplayerSessionVisibility, Windows::Foundation::Collections::IVectorView<hstring> const& initiatorXboxUserIds, hstring const& sessionCustomConstantsJson);
    };
}
namespace winrt::Microsoft::Xbox::Services::Multiplayer::factory_implementation
{
    struct MultiplayerSession : MultiplayerSessionT<MultiplayerSession, implementation::MultiplayerSession>
    {
    };
}
