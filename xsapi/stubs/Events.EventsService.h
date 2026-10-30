#pragma once
#include "Events/EventsService.g.h"


namespace winrt::Microsoft::Xbox::Services::Events::implementation
{
    struct EventsService : EventsServiceT<EventsService>
    {
        EventsService() = default;

        void WriteInGameEvent(hstring const& eventName);
        void WriteInGameEvent(hstring const& eventName, Windows::Foundation::Collections::PropertySet const& dimensions, Windows::Foundation::Collections::PropertySet const& measurements);
    };
}
