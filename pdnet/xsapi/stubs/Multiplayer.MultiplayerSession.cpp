// pdnet-hand: core session members implemented (stage B: setters record edits, see mpsd.h); the rest are still generated defaults.
#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerSession.h"
#include "Multiplayer.MultiplayerSessionTournamentsServer.h"
#include "Multiplayer.MultiplayerSessionRoleTypes.h"
#include "Multiplayer.MultiplayerSessionReference.h"
#include "Multiplayer.MultiplayerSessionProperties.h"
#include "Multiplayer.MultiplayerSessionMember.h"
#include "Multiplayer.MultiplayerSessionMatchmakingServer.h"
#include "Multiplayer.MultiplayerSessionConstants.h"
#include "Multiplayer.MultiplayerSessionArbitrationServer.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    ::pdnet::mpsd::MemberDoc& MultiplayerSession::Me()
    {
        auto& xuid = ::pdnet::xsapi::LocalXuid();
        if (auto* me = m_doc->FindMember(xuid)) return *me;
        ::pdnet::mpsd::MemberDoc m;
        m.xuid = xuid;
        m.gamertag = ::pdnet::GetConfig().displayName;
        m_doc->members.push_back(m);
        return m_doc->members.back();
    }

    MultiplayerSession::MultiplayerSession(Microsoft::Xbox::Services::XboxLiveContext const& xboxLiveContext)
    {
        m_context = xboxLiveContext; m_doc = std::make_shared<::pdnet::mpsd::SessionDoc>();
    }
    MultiplayerSession::MultiplayerSession(Microsoft::Xbox::Services::XboxLiveContext const& xboxLiveContext, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference const& multiplayerSessionReference, uint32_t maxMembersInSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility const& multiplayerSessionVisibility, Windows::Foundation::Collections::IVectorView<hstring> const& initiatorXboxUserIds, hstring const& sessionCustomConstantsJson)
    {
        _Init(xboxLiveContext, multiplayerSessionReference, maxMembersInSession, multiplayerSessionVisibility, initiatorXboxUserIds, sessionCustomConstantsJson);
    }
    MultiplayerSession::MultiplayerSession(Microsoft::Xbox::Services::XboxLiveContext const& xboxLiveContext, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference const& multiplayerSessionReference)
    {
        _Init(xboxLiveContext, multiplayerSessionReference, 0, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility::Unknown, nullptr, L"");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes MultiplayerSession::CompareMultiplayerSessions(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession const& currentSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession const& oldSession)
    {
        if (!currentSession || !oldSession) return Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes::Everything;
        return ::pdnet::mpsd::Compare(winrt::get_self<MultiplayerSession>(currentSession)->Doc(), winrt::get_self<MultiplayerSession>(oldSession)->Doc());
    }
    Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus MultiplayerSession::ConvertHttpStatusToWriteSessionStatus(int32_t httpStatusCode)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::ConvertHttpStatusToWriteSessionStatus"); return Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus{};
    }
    hstring MultiplayerSession::MultiplayerCorrelationId()
    {
        return hstring{ m_doc->correlationId };
    }
    hstring MultiplayerSession::SearchHandleId()
    {
        return m_doc->search ? hstring{ m_doc->search->id } : hstring{};
    }
    Microsoft::Xbox::Services::Tournaments::TournamentArbitrationStatus MultiplayerSession::ArbitrationStatus()
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::ArbitrationStatus"); return Microsoft::Xbox::Services::Tournaments::TournamentArbitrationStatus{};
    }
    Windows::Foundation::DateTime MultiplayerSession::StartTime()
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::StartTime"); return Windows::Foundation::DateTime{};
    }
    Windows::Foundation::DateTime MultiplayerSession::DateOfNextTimer()
    {
        return Windows::Foundation::DateTime{}; // no session timers
    }
    Windows::Foundation::DateTime MultiplayerSession::DateOfSession()
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::DateOfSession"); return Windows::Foundation::DateTime{};
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerInitializationStage MultiplayerSession::InitializationStage()
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::InitializationStage"); return Microsoft::Xbox::Services::Multiplayer::MultiplayerInitializationStage{};
    }
    Windows::Foundation::DateTime MultiplayerSession::InitializingStageStartTime()
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::InitializingStageStartTime"); return Windows::Foundation::DateTime{};
    }
    uint32_t MultiplayerSession::InitializingEpisode()
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::InitializingEpisode"); return uint32_t{};
    }
    Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus MultiplayerSession::WriteStatus()
    {
        return m_doc->writeStatus;
    }
    Windows::Foundation::Collections::IVectorView<hstring> MultiplayerSession::HostCandidates()
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::HostCandidates"); return winrt::single_threaded_vector<hstring>().GetView();
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MultiplayerSession::SessionReference()
    {
        return winrt::make<implementation::MultiplayerSessionReference>(hstring{ m_doc->scid }, hstring{ m_doc->templateName }, hstring{ m_doc->sessionName });
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionConstants MultiplayerSession::SessionConstants()
    {
        return winrt::make<implementation::MultiplayerSessionConstants>(m_doc);
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionProperties MultiplayerSession::SessionProperties()
    {
        return winrt::make<implementation::MultiplayerSessionProperties>(m_doc);
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRoleTypes MultiplayerSession::SessionRoleTypes()
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SessionRoleTypes"); return winrt::make<winrt::Microsoft::Xbox::Services::Multiplayer::implementation::MultiplayerSessionRoleTypes>();
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> MultiplayerSession::Members()
    {
        std::vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> list;
        for (auto& m : m_doc->members)
            if (!m.leaving) list.push_back(winrt::make<implementation::MultiplayerSessionMember>(m));
        return winrt::single_threaded_vector(std::move(list)).GetView();
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMatchmakingServer MultiplayerSession::MatchmakingServer()
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::MatchmakingServer"); return winrt::make<winrt::Microsoft::Xbox::Services::Multiplayer::implementation::MultiplayerSessionMatchmakingServer>();
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionTournamentsServer MultiplayerSession::TournamentsServer()
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::TournamentsServer"); return winrt::make<winrt::Microsoft::Xbox::Services::Multiplayer::implementation::MultiplayerSessionTournamentsServer>();
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionArbitrationServer MultiplayerSession::ArbitrationServer()
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::ArbitrationServer"); return winrt::make<winrt::Microsoft::Xbox::Services::Multiplayer::implementation::MultiplayerSessionArbitrationServer>();
    }
    uint32_t MultiplayerSession::MembersAccepted()
    {
        return static_cast<uint32_t>(m_doc->members.size());
    }
    hstring MultiplayerSession::ServersJson()
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::ServersJson"); return hstring{};
    }
    void MultiplayerSession::ServersJson(hstring const& value)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::ServersJson");
    }
    hstring MultiplayerSession::ETag()
    {
        return hstring{ std::to_wstring(m_doc->changeNumber) };
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes MultiplayerSession::SubscribedChangeTypes()
    {
        return m_doc->subscribedChanges;
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember MultiplayerSession::CurrentUser()
    {
        auto* me = m_doc->FindMember(::pdnet::xsapi::LocalXuid());
        return me && !me->leaving ? winrt::make<implementation::MultiplayerSessionMember>(*me) : nullptr;
    }
    hstring MultiplayerSession::Branch()
    {
        return hstring{ m_doc->correlationId };
    }
    uint64_t MultiplayerSession::ChangeNumber()
    {
        return m_doc->changeNumber;
    }
    void MultiplayerSession::AddMemberReservation(hstring const& xboxUserId, hstring const& memberCustomConstantsJson)
    {
        AddMemberReservation(xboxUserId, memberCustomConstantsJson, false);
    }
    void MultiplayerSession::AddMemberReservation(hstring const& xboxUserId, hstring const& memberCustomConstantsJson, bool initializeRequested)
    {
        ::pdnet::mpsd::MemberDoc m;
        m.xuid = xboxUserId;
        m.customConstantsJson = memberCustomConstantsJson;
        m.initializeRequested = initializeRequested;
        m.status = Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMemberStatus::Reserved;
        m.reservation = true;
        m_doc->members.push_back(m);
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember MultiplayerSession::Join()
    {
        return Join(L"", false, true);
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember MultiplayerSession::Join(hstring const& memberCustomConstantsJson)
    {
        return Join(memberCustomConstantsJson, false, true);
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember MultiplayerSession::Join(hstring const& memberCustomConstantsJson, bool initializeRequested)
    {
        return Join(memberCustomConstantsJson, initializeRequested, true);
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember MultiplayerSession::Join(hstring const& memberCustomConstantsJson, bool initializeRequested, bool joinWithActiveStatus)
    {
        auto& me = Me();
        me.leaving = false;
        me.customConstantsJson = memberCustomConstantsJson;
        me.initializeRequested = initializeRequested;
        me.status = joinWithActiveStatus ? Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMemberStatus::Active : Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMemberStatus::Inactive;
        ::pdnet::Log(L"xsapi-pdnet: join %s/%s as '%s'", m_doc->templateName.c_str(), m_doc->sessionName.c_str(), me.gamertag.c_str());
        return winrt::make<implementation::MultiplayerSessionMember>(me);
    }
    void MultiplayerSession::SetVisibility(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility const& visibility)
    {
        m_doc->visibility = visibility;
        m_doc->edits.visibility = true;
    }
    void MultiplayerSession::SetMaxMembersInSession(uint32_t maxMembersInSession)
    {
        m_doc->maxMembers = maxMembersInSession;
        m_doc->edits.maxMembers = true;
    }
    void MultiplayerSession::SetMutableRoleSettings(Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerRoleType> const& roleTypes)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetMutableRoleSettings");
    }
    void MultiplayerSession::SetTimeouts(Windows::Foundation::TimeSpan const& memberReservedTimeout, Windows::Foundation::TimeSpan const& memberInactiveTimeout, Windows::Foundation::TimeSpan const& memberReadyTimeout, Windows::Foundation::TimeSpan const& sessionEmptyTimeout)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetTimeouts");
    }
    void MultiplayerSession::SetArbitrationTimeouts(Windows::Foundation::TimeSpan const& arbitrationTimeout, Windows::Foundation::TimeSpan const& forfeitTimeout)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetArbitrationTimeouts");
    }
    void MultiplayerSession::SetQualityOfServiceConnectivityMetrics(bool enableLatencyMetric, bool enableBandwidthDownMetric, bool enableBandwidthUpMetric, bool enableCustomMetric)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetQualityOfServiceConnectivityMetrics");
    }
    void MultiplayerSession::SetManagedInitialization(Windows::Foundation::TimeSpan const& joinTimeout, Windows::Foundation::TimeSpan const& measurementTimeout, Windows::Foundation::TimeSpan const& evaluationTimeout, bool autoEvalute, uint32_t membersNeededToStart)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetManagedInitialization");
    }
    void MultiplayerSession::SetMemberInitialization(Windows::Foundation::TimeSpan const& joinTimeout, Windows::Foundation::TimeSpan const& measurementTimeout, Windows::Foundation::TimeSpan const& evaluationTimeout, bool autoEvalute, uint32_t membersNeededToStart)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetMemberInitialization");
    }
    void MultiplayerSession::SetPeerToPeerRequirements(Windows::Foundation::TimeSpan const& latencyMaximum, uint32_t bandwidthMinimumInKilobitsPerSecond)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetPeerToPeerRequirements");
    }
    void MultiplayerSession::SetPeerToHostRequirements(Windows::Foundation::TimeSpan const& latencyMaximum, uint32_t bandwidthDownMinimumInKilobitsPerSecond, uint32_t bandwidthUpMinimumInKilobitsPerSecond, Microsoft::Xbox::Services::Multiplayer::MultiplayMetrics const& hostSelectionMetric)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetPeerToHostRequirements");
    }
    void MultiplayerSession::SetMeasurementServerAddresses(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::GameServerPlatform::QualityOfServiceServer> const& measurementServerAddresses)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetMeasurementServerAddresses");
    }
    void MultiplayerSession::SetCloudComputePackageJson(hstring const& sessionCloudComputePackageConstantsJson)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetCloudComputePackageJson");
    }
    void MultiplayerSession::SetSessionCapabilities(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionCapabilities const& capabilities)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetSessionCapabilities");
    }
    void MultiplayerSession::SetInitializationStatus(bool initializationSucceeded)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetInitializationStatus");
    }
    void MultiplayerSession::SetHostDeviceToken(hstring const& hostDeviceToken)
    {
        m_doc->hostDeviceToken = hostDeviceToken;
        m_doc->edits.host = true;
    }
    void MultiplayerSession::SetMatchmakingServerConnectionPath(hstring const& serverConnectionPath)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetMatchmakingServerConnectionPath");
    }
    void MultiplayerSession::SetClosed(bool closed)
    {
        m_doc->closed = closed;
        m_doc->edits.closed = true;
    }
    void MultiplayerSession::SetAllocateCloudCompute(bool allocateCloudCompute)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetAllocateCloudCompute");
    }
    void MultiplayerSession::SetMatchmakingResubmit(bool matchResubmit)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetMatchmakingResubmit");
    }
    void MultiplayerSession::SetServerConnectionStringCandidates(Windows::Foundation::Collections::IVectorView<hstring> const& serverConnectionStringCandidates)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetServerConnectionStringCandidates");
    }
    void MultiplayerSession::SetSessionChangeSubscription(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes const& changeTypes)
    {
        m_doc->subscribedChanges = changeTypes;
    }
    void MultiplayerSession::Leave()
    {
        Me().leaving = true; ::pdnet::Log(L"xsapi-pdnet: leave %s/%s", m_doc->templateName.c_str(), m_doc->sessionName.c_str());
    }
    void MultiplayerSession::SetCurrentUserStatus(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMemberStatus const& status)
    {
        Me().status = status;
    }
    void MultiplayerSession::SetCurrentUserSecureDeviceAddressBase64(hstring const& value)
    {
        Me().deviceAddressBase64 = value;
    }
    void MultiplayerSession::SetCurrentUserRoleInfo(Windows::Foundation::Collections::IMapView<hstring, hstring> const& roles)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetCurrentUserRoleInfo");
    }
    void MultiplayerSession::SetCurrentUserMembersInGroup(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> const& membersInGroup)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetCurrentUserMembersInGroup");
    }
    void MultiplayerSession::SetCurrentUserQualityOfServiceMeasurements(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerQualityOfServiceMeasurements> const& measurements)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetCurrentUserQualityOfServiceMeasurements");
    }
    void MultiplayerSession::SetCurrentUserQualityOfServiceServerMeasurementsJson(hstring const& valueJson)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetCurrentUserQualityOfServiceServerMeasurementsJson");
    }
    void MultiplayerSession::SetCurrentUserArbitrationResults(Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Tournaments::TournamentTeamResult> const& results)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetCurrentUserArbitrationResults");
    }
    void MultiplayerSession::SetCurrentUserMemberCustomPropertyJson(hstring const& name, hstring const& valueJson)
    {
        Me().customProperties[std::wstring{ name }] = valueJson;
    }
    void MultiplayerSession::DeleteCurrentUserMemberCustomPropertyJson(hstring const& name)
    {
        Me().customProperties.erase(std::wstring{ name });
    }
    void MultiplayerSession::SetMatchmakingTargetSessionConstantsJson(hstring const& matchmakingTargetSessionConstants)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerSession::SetMatchmakingTargetSessionConstantsJson");
    }
    void MultiplayerSession::SetSessionCustomPropertyJson(hstring const& name, hstring const& valueJson)
    {
        m_doc->customProperties[std::wstring{ name }] = valueJson;
        m_doc->edits.properties.insert(std::wstring{ name });
    }
    void MultiplayerSession::DeleteSessionCustomPropertyJson(hstring const& name)
    {
        m_doc->customProperties.erase(std::wstring{ name });
        m_doc->edits.properties.insert(std::wstring{ name });
    }
    void MultiplayerSession::_Init(Microsoft::Xbox::Services::XboxLiveContext const& xboxLiveContext, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference const& multiplayerSessionReference, uint32_t maxMembersInSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility const& multiplayerSessionVisibility, Windows::Foundation::Collections::IVectorView<hstring> const& initiatorXboxUserIds, hstring const& sessionCustomConstantsJson)
    {
        m_context = xboxLiveContext;
        m_doc = std::make_shared<::pdnet::mpsd::SessionDoc>();
        if (multiplayerSessionReference)
        {
            m_doc->scid = multiplayerSessionReference.ServiceConfigurationId();
            m_doc->templateName = multiplayerSessionReference.SessionTemplateName();
            m_doc->sessionName = multiplayerSessionReference.SessionName();
        }
        if (maxMembersInSession) m_doc->maxMembers = maxMembersInSession;
        if (multiplayerSessionVisibility != Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility::Unknown) m_doc->visibility = multiplayerSessionVisibility;
        if (initiatorXboxUserIds) for (auto const& x : initiatorXboxUserIds) m_doc->initiators.emplace_back(x);
        m_doc->customConstantsJson = sessionCustomConstantsJson;
        ::pdnet::Log(L"xsapi-pdnet: new session %s/%s (max %u)", m_doc->templateName.c_str(), m_doc->sessionName.c_str(), m_doc->maxMembers);
    }
}
