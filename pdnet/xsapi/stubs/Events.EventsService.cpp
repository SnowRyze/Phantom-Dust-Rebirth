#include "xsapi_pch.h"
#include "Events.EventsService.h"


namespace winrt::Microsoft::Xbox::Services::Events::implementation
{
    void EventsService::WriteInGameEvent(hstring const& eventName)
    {
        PDNET_XSAPI_STUB("EventsService::WriteInGameEvent");
    }
    void EventsService::WriteInGameEvent(hstring const& eventName, Windows::Foundation::Collections::PropertySet const& dimensions, Windows::Foundation::Collections::PropertySet const& measurements)
    {
        PDNET_XSAPI_STUB("EventsService::WriteInGameEvent");
    }
}
