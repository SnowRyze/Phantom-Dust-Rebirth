#pragma once
#include "Multiplayer/MultiplayerSearchHandleRequest.g.h"

// pdnet-hand: plain data holder (the game fills it, SetSearchHandleAsync reads it).
namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSearchHandleRequest : MultiplayerSearchHandleRequestT<MultiplayerSearchHandleRequest>
    {
        MultiplayerSearchHandleRequest() = default;

        MultiplayerSearchHandleRequest(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference const& sessionReference) : m_reference(sessionReference) {}
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference SessionReference() { return m_reference; }
        Windows::Foundation::Collections::IVectorView<hstring> Tags() { return m_tags; }
        void Tags(Windows::Foundation::Collections::IVectorView<hstring> const& tags) { m_tags = tags ? tags : winrt::single_threaded_vector<hstring>().GetView(); }
        Windows::Foundation::Collections::IMapView<hstring, double> NumbersMetadata() { return m_numbers; }
        void NumbersMetadata(Windows::Foundation::Collections::IMapView<hstring, double> const& metadata) { m_numbers = metadata ? metadata : winrt::single_threaded_map<hstring, double>().GetView(); }
        Windows::Foundation::Collections::IMapView<hstring, hstring> StringsMetadata() { return m_strings; }
        void StringsMetadata(Windows::Foundation::Collections::IMapView<hstring, hstring> const& metadata) { m_strings = metadata ? metadata : winrt::single_threaded_map<hstring, hstring>().GetView(); }

    private:
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference m_reference{ nullptr };
        Windows::Foundation::Collections::IVectorView<hstring> m_tags = winrt::single_threaded_vector<hstring>().GetView();
        Windows::Foundation::Collections::IMapView<hstring, double> m_numbers = winrt::single_threaded_map<hstring, double>().GetView();
        Windows::Foundation::Collections::IMapView<hstring, hstring> m_strings = winrt::single_threaded_map<hstring, hstring>().GetView();
    };
}
namespace winrt::Microsoft::Xbox::Services::Multiplayer::factory_implementation
{
    struct MultiplayerSearchHandleRequest : MultiplayerSearchHandleRequestT<MultiplayerSearchHandleRequest, implementation::MultiplayerSearchHandleRequest>
    {
    };
}
