#include "xsapi_pch.h"
#include "Tournaments.Tournament.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    hstring Tournament::Id()
    {
        PDNET_XSAPI_STUB("Tournament::Id");
    }
    hstring Tournament::OrganizerId()
    {
        PDNET_XSAPI_STUB("Tournament::OrganizerId");
    }
    hstring Tournament::OrganizerName()
    {
        PDNET_XSAPI_STUB("Tournament::OrganizerName");
    }
    hstring Tournament::Name()
    {
        PDNET_XSAPI_STUB("Tournament::Name");
    }
    hstring Tournament::Description()
    {
        PDNET_XSAPI_STUB("Tournament::Description");
    }
    hstring Tournament::GameMode()
    {
        PDNET_XSAPI_STUB("Tournament::GameMode");
    }
    hstring Tournament::TournamentStyle()
    {
        PDNET_XSAPI_STUB("Tournament::TournamentStyle");
    }
    bool Tournament::IsRegistrationOpen()
    {
        PDNET_XSAPI_STUB("Tournament::IsRegistrationOpen");
    }
    bool Tournament::IsCheckinOpen()
    {
        PDNET_XSAPI_STUB("Tournament::IsCheckinOpen");
    }
    bool Tournament::IsPlayingOpen()
    {
        PDNET_XSAPI_STUB("Tournament::IsPlayingOpen");
    }
    bool Tournament::HasPrize()
    {
        PDNET_XSAPI_STUB("Tournament::HasPrize");
    }
    bool Tournament::IsPaused()
    {
        PDNET_XSAPI_STUB("Tournament::IsPaused");
    }
    uint32_t Tournament::MinTeamSize()
    {
        PDNET_XSAPI_STUB("Tournament::MinTeamSize");
    }
    uint32_t Tournament::MaxTeamSize()
    {
        PDNET_XSAPI_STUB("Tournament::MaxTeamSize");
    }
    uint32_t Tournament::TeamsRegisteredCount()
    {
        PDNET_XSAPI_STUB("Tournament::TeamsRegisteredCount");
    }
    uint32_t Tournament::MinTeamsRegistered()
    {
        PDNET_XSAPI_STUB("Tournament::MinTeamsRegistered");
    }
    uint32_t Tournament::MaxTeamsRegistered()
    {
        PDNET_XSAPI_STUB("Tournament::MaxTeamsRegistered");
    }
    Microsoft::Xbox::Services::Tournaments::TournamentState Tournament::TournamentState()
    {
        PDNET_XSAPI_STUB("Tournament::TournamentState");
    }
    Windows::Foundation::DateTime Tournament::RegistrationStartTime()
    {
        PDNET_XSAPI_STUB("Tournament::RegistrationStartTime");
    }
    Windows::Foundation::DateTime Tournament::RegistrationEndTime()
    {
        PDNET_XSAPI_STUB("Tournament::RegistrationEndTime");
    }
    Windows::Foundation::DateTime Tournament::CheckinStartTime()
    {
        PDNET_XSAPI_STUB("Tournament::CheckinStartTime");
    }
    Windows::Foundation::DateTime Tournament::CheckinEndTime()
    {
        PDNET_XSAPI_STUB("Tournament::CheckinEndTime");
    }
    Windows::Foundation::DateTime Tournament::PlayingStartTime()
    {
        PDNET_XSAPI_STUB("Tournament::PlayingStartTime");
    }
    Windows::Foundation::DateTime Tournament::PlayingEndTime()
    {
        PDNET_XSAPI_STUB("Tournament::PlayingEndTime");
    }
    Windows::Foundation::DateTime Tournament::EndTime()
    {
        PDNET_XSAPI_STUB("Tournament::EndTime");
    }
    Microsoft::Xbox::Services::Tournaments::TeamSummary Tournament::TeamSummary()
    {
        PDNET_XSAPI_STUB("Tournament::TeamSummary");
    }
}
