#pragma once
#include "Tournaments/Tournament.g.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    struct Tournament : TournamentT<Tournament>
    {
        Tournament() = default;

        hstring Id();
        hstring OrganizerId();
        hstring OrganizerName();
        hstring Name();
        hstring Description();
        hstring GameMode();
        hstring TournamentStyle();
        bool IsRegistrationOpen();
        bool IsCheckinOpen();
        bool IsPlayingOpen();
        bool HasPrize();
        bool IsPaused();
        uint32_t MinTeamSize();
        uint32_t MaxTeamSize();
        uint32_t TeamsRegisteredCount();
        uint32_t MinTeamsRegistered();
        uint32_t MaxTeamsRegistered();
        Microsoft::Xbox::Services::Tournaments::TournamentState TournamentState();
        Windows::Foundation::DateTime RegistrationStartTime();
        Windows::Foundation::DateTime RegistrationEndTime();
        Windows::Foundation::DateTime CheckinStartTime();
        Windows::Foundation::DateTime CheckinEndTime();
        Windows::Foundation::DateTime PlayingStartTime();
        Windows::Foundation::DateTime PlayingEndTime();
        Windows::Foundation::DateTime EndTime();
        Microsoft::Xbox::Services::Tournaments::TeamSummary TeamSummary();
    };
}
