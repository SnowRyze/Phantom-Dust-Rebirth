#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerSession.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    MultiplayerSession::MultiplayerSession(Microsoft::Xbox::Services::XboxLiveContext const& xboxLiveContext)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::MultiplayerSession");
    }
    MultiplayerSession::MultiplayerSession(Microsoft::Xbox::Services::XboxLiveContext const& xboxLiveContext, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference const& multiplayerSessionReference, uint32_t maxMembersInSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility const& multiplayerSessionVisibility, Windows::Foundation::Collections::IVectorView<hstring> const& initiatorXboxUserIds, hstring const& sessionCustomConstantsJson)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::MultiplayerSession");
    }
    MultiplayerSession::MultiplayerSession(Microsoft::Xbox::Services::XboxLiveContext const& xboxLiveContext, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference const& multiplayerSessionReference)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::MultiplayerSession");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes MultiplayerSession::CompareMultiplayerSessions(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession const& currentSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession const& oldSession)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::CompareMultiplayerSessions");
    }
    Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus MultiplayerSession::ConvertHttpStatusToWriteSessionStatus(int32_t httpStatusCode)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::ConvertHttpStatusToWriteSessionStatus");
    }
    hstring MultiplayerSession::MultiplayerCorrelationId()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::MultiplayerCorrelationId");
    }
    hstring MultiplayerSession::SearchHandleId()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SearchHandleId");
    }
    Microsoft::Xbox::Services::Tournaments::TournamentArbitrationStatus MultiplayerSession::ArbitrationStatus()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::ArbitrationStatus");
    }
    Windows::Foundation::DateTime MultiplayerSession::StartTime()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::StartTime");
    }
    Windows::Foundation::DateTime MultiplayerSession::DateOfNextTimer()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::DateOfNextTimer");
    }
    Windows::Foundation::DateTime MultiplayerSession::DateOfSession()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::DateOfSession");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerInitializationStage MultiplayerSession::InitializationStage()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::InitializationStage");
    }
    Windows::Foundation::DateTime MultiplayerSession::InitializingStageStartTime()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::InitializingStageStartTime");
    }
    uint32_t MultiplayerSession::InitializingEpisode()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::InitializingEpisode");
    }
    Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus MultiplayerSession::WriteStatus()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::WriteStatus");
    }
    Windows::Foundation::Collections::IVectorView<hstring> MultiplayerSession::HostCandidates()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::HostCandidates");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MultiplayerSession::SessionReference()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SessionReference");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionConstants MultiplayerSession::SessionConstants()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SessionConstants");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionProperties MultiplayerSession::SessionProperties()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SessionProperties");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRoleTypes MultiplayerSession::SessionRoleTypes()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SessionRoleTypes");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> MultiplayerSession::Members()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::Members");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMatchmakingServer MultiplayerSession::MatchmakingServer()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::MatchmakingServer");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionTournamentsServer MultiplayerSession::TournamentsServer()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::TournamentsServer");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionArbitrationServer MultiplayerSession::ArbitrationServer()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::ArbitrationServer");
    }
    uint32_t MultiplayerSession::MembersAccepted()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::MembersAccepted");
    }
    hstring MultiplayerSession::ServersJson()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::ServersJson");
    }
    void MultiplayerSession::ServersJson(hstring const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::ServersJson");
    }
    hstring MultiplayerSession::ETag()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::ETag");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes MultiplayerSession::SubscribedChangeTypes()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SubscribedChangeTypes");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember MultiplayerSession::CurrentUser()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::CurrentUser");
    }
    hstring MultiplayerSession::Branch()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::Branch");
    }
    uint64_t MultiplayerSession::ChangeNumber()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::ChangeNumber");
    }
    void MultiplayerSession::AddMemberReservation(hstring const& xboxUserId, hstring const& memberCustomConstantsJson)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::AddMemberReservation");
    }
    void MultiplayerSession::AddMemberReservation(hstring const& xboxUserId, hstring const& memberCustomConstantsJson, bool initializeRequested)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::AddMemberReservation");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember MultiplayerSession::Join()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::Join");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember MultiplayerSession::Join(hstring const& memberCustomConstantsJson)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::Join");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember MultiplayerSession::Join(hstring const& memberCustomConstantsJson, bool initializeRequested)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::Join");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember MultiplayerSession::Join(hstring const& memberCustomConstantsJson, bool initializeRequested, bool joinWithActiveStatus)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::Join");
    }
    void MultiplayerSession::SetVisibility(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility const& visibility)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetVisibility");
    }
    void MultiplayerSession::SetMaxMembersInSession(uint32_t maxMembersInSession)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetMaxMembersInSession");
    }
    void MultiplayerSession::SetMutableRoleSettings(Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerRoleType> const& roleTypes)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetMutableRoleSettings");
    }
    void MultiplayerSession::SetTimeouts(Windows::Foundation::TimeSpan const& memberReservedTimeout, Windows::Foundation::TimeSpan const& memberInactiveTimeout, Windows::Foundation::TimeSpan const& memberReadyTimeout, Windows::Foundation::TimeSpan const& sessionEmptyTimeout)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetTimeouts");
    }
    void MultiplayerSession::SetArbitrationTimeouts(Windows::Foundation::TimeSpan const& arbitrationTimeout, Windows::Foundation::TimeSpan const& forfeitTimeout)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetArbitrationTimeouts");
    }
    void MultiplayerSession::SetQualityOfServiceConnectivityMetrics(bool enableLatencyMetric, bool enableBandwidthDownMetric, bool enableBandwidthUpMetric, bool enableCustomMetric)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetQualityOfServiceConnectivityMetrics");
    }
    void MultiplayerSession::SetManagedInitialization(Windows::Foundation::TimeSpan const& joinTimeout, Windows::Foundation::TimeSpan const& measurementTimeout, Windows::Foundation::TimeSpan const& evaluationTimeout, bool autoEvalute, uint32_t membersNeededToStart)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetManagedInitialization");
    }
    void MultiplayerSession::SetMemberInitialization(Windows::Foundation::TimeSpan const& joinTimeout, Windows::Foundation::TimeSpan const& measurementTimeout, Windows::Foundation::TimeSpan const& evaluationTimeout, bool autoEvalute, uint32_t membersNeededToStart)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetMemberInitialization");
    }
    void MultiplayerSession::SetPeerToPeerRequirements(Windows::Foundation::TimeSpan const& latencyMaximum, uint32_t bandwidthMinimumInKilobitsPerSecond)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetPeerToPeerRequirements");
    }
    void MultiplayerSession::SetPeerToHostRequirements(Windows::Foundation::TimeSpan const& latencyMaximum, uint32_t bandwidthDownMinimumInKilobitsPerSecond, uint32_t bandwidthUpMinimumInKilobitsPerSecond, Microsoft::Xbox::Services::Multiplayer::MultiplayMetrics const& hostSelectionMetric)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetPeerToHostRequirements");
    }
    void MultiplayerSession::SetMeasurementServerAddresses(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::GameServerPlatform::QualityOfServiceServer> const& measurementServerAddresses)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetMeasurementServerAddresses");
    }
    void MultiplayerSession::SetCloudComputePackageJson(hstring const& sessionCloudComputePackageConstantsJson)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetCloudComputePackageJson");
    }
    void MultiplayerSession::SetSessionCapabilities(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionCapabilities const& capabilities)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetSessionCapabilities");
    }
    void MultiplayerSession::SetInitializationStatus(bool initializationSucceeded)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetInitializationStatus");
    }
    void MultiplayerSession::SetHostDeviceToken(hstring const& hostDeviceToken)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetHostDeviceToken");
    }
    void MultiplayerSession::SetMatchmakingServerConnectionPath(hstring const& serverConnectionPath)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetMatchmakingServerConnectionPath");
    }
    void MultiplayerSession::SetClosed(bool closed)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetClosed");
    }
    void MultiplayerSession::SetAllocateCloudCompute(bool allocateCloudCompute)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetAllocateCloudCompute");
    }
    void MultiplayerSession::SetMatchmakingResubmit(bool matchResubmit)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetMatchmakingResubmit");
    }
    void MultiplayerSession::SetServerConnectionStringCandidates(Windows::Foundation::Collections::IVectorView<hstring> const& serverConnectionStringCandidates)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetServerConnectionStringCandidates");
    }
    void MultiplayerSession::SetSessionChangeSubscription(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes const& changeTypes)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetSessionChangeSubscription");
    }
    void MultiplayerSession::Leave()
    {
        PDNET_XSAPI_STUB("MultiplayerSession::Leave");
    }
    void MultiplayerSession::SetCurrentUserStatus(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMemberStatus const& status)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetCurrentUserStatus");
    }
    void MultiplayerSession::SetCurrentUserSecureDeviceAddressBase64(hstring const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetCurrentUserSecureDeviceAddressBase64");
    }
    void MultiplayerSession::SetCurrentUserRoleInfo(Windows::Foundation::Collections::IMapView<hstring, hstring> const& roles)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetCurrentUserRoleInfo");
    }
    void MultiplayerSession::SetCurrentUserMembersInGroup(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> const& membersInGroup)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetCurrentUserMembersInGroup");
    }
    void MultiplayerSession::SetCurrentUserQualityOfServiceMeasurements(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerQualityOfServiceMeasurements> const& measurements)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetCurrentUserQualityOfServiceMeasurements");
    }
    void MultiplayerSession::SetCurrentUserQualityOfServiceServerMeasurementsJson(hstring const& valueJson)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetCurrentUserQualityOfServiceServerMeasurementsJson");
    }
    void MultiplayerSession::SetCurrentUserArbitrationResults(Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Tournaments::TournamentTeamResult> const& results)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetCurrentUserArbitrationResults");
    }
    void MultiplayerSession::SetCurrentUserMemberCustomPropertyJson(hstring const& name, hstring const& valueJson)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetCurrentUserMemberCustomPropertyJson");
    }
    void MultiplayerSession::DeleteCurrentUserMemberCustomPropertyJson(hstring const& name)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::DeleteCurrentUserMemberCustomPropertyJson");
    }
    void MultiplayerSession::SetMatchmakingTargetSessionConstantsJson(hstring const& matchmakingTargetSessionConstants)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetMatchmakingTargetSessionConstantsJson");
    }
    void MultiplayerSession::SetSessionCustomPropertyJson(hstring const& name, hstring const& valueJson)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::SetSessionCustomPropertyJson");
    }
    void MultiplayerSession::DeleteSessionCustomPropertyJson(hstring const& name)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::DeleteSessionCustomPropertyJson");
    }
    void MultiplayerSession::_Init(Microsoft::Xbox::Services::XboxLiveContext const& xboxLiveContext, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference const& multiplayerSessionReference, uint32_t maxMembersInSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility const& multiplayerSessionVisibility, Windows::Foundation::Collections::IVectorView<hstring> const& initiatorXboxUserIds, hstring const& sessionCustomConstantsJson)
    {
        PDNET_XSAPI_STUB("MultiplayerSession::_Init");
    }
}
