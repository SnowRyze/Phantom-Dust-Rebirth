#pragma once
#include "Multiplayer/Manager/PerformQosMeasurementsEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    struct PerformQosMeasurementsEventArgs : PerformQosMeasurementsEventArgsT<PerformQosMeasurementsEventArgs>
    {
        PerformQosMeasurementsEventArgs() = default;

        Windows::Foundation::Collections::IMapView<hstring, hstring> AddressToDeviceTokens();
    };
}
