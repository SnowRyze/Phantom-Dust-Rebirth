#include "xsapi_pch.h"
#include "Presence.PresenceDeviceRecord.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    Microsoft::Xbox::Services::Presence::PresenceDeviceType PresenceDeviceRecord::DeviceType()
    {
        PDNET_XSAPI_STUB("PresenceDeviceRecord::DeviceType");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceTitleRecord> PresenceDeviceRecord::PresenceTitleRecords()
    {
        PDNET_XSAPI_STUB("PresenceDeviceRecord::PresenceTitleRecords");
    }
}
