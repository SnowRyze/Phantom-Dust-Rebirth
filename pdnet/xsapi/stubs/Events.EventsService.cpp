// pdnet-hand: the game's Hero_MP_Ranking event (its multiplayer rating, measurement "Count") goes to the EOS
// scoreboard (src/scores.h); every other event is dropped, as before.
#include "xsapi_pch.h"
#include "Events.EventsService.h"
#include "../../src/scores.h"
#include "../../src/gamepatches.h"

namespace winrt::Microsoft::Xbox::Services::Events::implementation
{
    void EventsService::WriteInGameEvent(hstring const& eventName)
    {
        PDNET_XSAPI_STUB_DEFAULT("EventsService::WriteInGameEvent");
    }
    void EventsService::WriteInGameEvent(hstring const& eventName, Windows::Foundation::Collections::PropertySet const& dimensions, Windows::Foundation::Collections::PropertySet const& measurements)
    {
        if (eventName == L"Hero_MP_Ranking")
        {
            int rating = -1;
            try
            {
                if (measurements && measurements.HasKey(L"Count"))
                {
                    auto v = measurements.Lookup(L"Count").as<Windows::Foundation::IPropertyValue>();
                    switch (v.Type())
                    {
                    case Windows::Foundation::PropertyType::Int32: rating = v.GetInt32(); break;
                    case Windows::Foundation::PropertyType::UInt32: rating = static_cast<int>(v.GetUInt32()); break;
                    case Windows::Foundation::PropertyType::Int64: rating = static_cast<int>(v.GetInt64()); break;
                    case Windows::Foundation::PropertyType::UInt64: rating = static_cast<int>(v.GetUInt64()); break;
                    case Windows::Foundation::PropertyType::Double: rating = static_cast<int>(v.GetDouble()); break;
                    case Windows::Foundation::PropertyType::Single: rating = static_cast<int>(v.GetSingle()); break;
                    default: break;
                    }
                }
            }
            catch (...) {}
            if (rating < 0) rating = ::pdnet::gamepatches::SavedRating();
            ::pdnet::Log(L"xsapi-pdnet: rating event %d", rating);
            ::pdnet::scores::SubmitRating(rating);
            return;
        }
        PDNET_XSAPI_STUB_DEFAULT("EventsService::WriteInGameEvent");
    }
}
