#include "xsapi_pch.h"
#include "Privacy.PermissionDenyReason.h"


namespace winrt::Microsoft::Xbox::Services::Privacy::implementation
{
    hstring PermissionDenyReason::Reason()
    {
        PDNET_XSAPI_STUB("PermissionDenyReason::Reason");
    }
    hstring PermissionDenyReason::RestrictedSetting()
    {
        PDNET_XSAPI_STUB("PermissionDenyReason::RestrictedSetting");
    }
}
