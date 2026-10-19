#include "xsapi_pch.h"
#include "Multiplayer.Manager.PerformQosMeasurementsEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    Windows::Foundation::Collections::IMapView<hstring, hstring> PerformQosMeasurementsEventArgs::AddressToDeviceTokens()
    {
        PDNET_XSAPI_STUB("PerformQosMeasurementsEventArgs::AddressToDeviceTokens");
    }
}
