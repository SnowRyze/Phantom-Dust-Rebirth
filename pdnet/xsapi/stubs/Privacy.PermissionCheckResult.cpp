#include "xsapi_pch.h"
#include "Privacy.PermissionCheckResult.h"


namespace winrt::Microsoft::Xbox::Services::Privacy::implementation
{
    bool PermissionCheckResult::IsAllowed()
    {
        PDNET_XSAPI_STUB("PermissionCheckResult::IsAllowed");
    }
    hstring PermissionCheckResult::PermissionRequested()
    {
        PDNET_XSAPI_STUB("PermissionCheckResult::PermissionRequested");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Privacy::PermissionDenyReason> PermissionCheckResult::DenyReasons()
    {
        PDNET_XSAPI_STUB("PermissionCheckResult::DenyReasons");
    }
}
