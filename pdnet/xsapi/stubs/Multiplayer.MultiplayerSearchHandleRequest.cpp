#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerSearchHandleRequest.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    MultiplayerSearchHandleRequest::MultiplayerSearchHandleRequest(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference const& sessionReference)
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleRequest::MultiplayerSearchHandleRequest");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MultiplayerSearchHandleRequest::SessionReference()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleRequest::SessionReference");
    }
    Windows::Foundation::Collections::IVectorView<hstring> MultiplayerSearchHandleRequest::Tags()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleRequest::Tags");
    }
    void MultiplayerSearchHandleRequest::Tags(Windows::Foundation::Collections::IVectorView<hstring> const& tags)
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleRequest::Tags");
    }
    Windows::Foundation::Collections::IMapView<hstring, double> MultiplayerSearchHandleRequest::NumbersMetadata()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleRequest::NumbersMetadata");
    }
    void MultiplayerSearchHandleRequest::NumbersMetadata(Windows::Foundation::Collections::IMapView<hstring, double> const& metadata)
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleRequest::NumbersMetadata");
    }
    Windows::Foundation::Collections::IMapView<hstring, hstring> MultiplayerSearchHandleRequest::StringsMetadata()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleRequest::StringsMetadata");
    }
    void MultiplayerSearchHandleRequest::StringsMetadata(Windows::Foundation::Collections::IMapView<hstring, hstring> const& metadata)
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleRequest::StringsMetadata");
    }
}
