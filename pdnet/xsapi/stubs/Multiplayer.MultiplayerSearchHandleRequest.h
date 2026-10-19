#pragma once
#include "Multiplayer/MultiplayerSearchHandleRequest.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSearchHandleRequest : MultiplayerSearchHandleRequestT<MultiplayerSearchHandleRequest>
    {
        MultiplayerSearchHandleRequest() = default;

        MultiplayerSearchHandleRequest(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference const& sessionReference);
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference SessionReference();
        Windows::Foundation::Collections::IVectorView<hstring> Tags();
        void Tags(Windows::Foundation::Collections::IVectorView<hstring> const& tags);
        Windows::Foundation::Collections::IMapView<hstring, double> NumbersMetadata();
        void NumbersMetadata(Windows::Foundation::Collections::IMapView<hstring, double> const& metadata);
        Windows::Foundation::Collections::IMapView<hstring, hstring> StringsMetadata();
        void StringsMetadata(Windows::Foundation::Collections::IMapView<hstring, hstring> const& metadata);
    };
}
namespace winrt::Microsoft::Xbox::Services::Multiplayer::factory_implementation
{
    struct MultiplayerSearchHandleRequest : MultiplayerSearchHandleRequestT<MultiplayerSearchHandleRequest, implementation::MultiplayerSearchHandleRequest>
    {
    };
}
