#include "xsapi_pch.h"
#include "Privacy.MultiplePermissionsCheckResult.h"


namespace winrt::Microsoft::Xbox::Services::Privacy::implementation
{
    hstring MultiplePermissionsCheckResult::XboxUserId()
    {
        PDNET_XSAPI_STUB("MultiplePermissionsCheckResult::XboxUserId");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Privacy::PermissionCheckResult> MultiplePermissionsCheckResult::Items()
    {
        PDNET_XSAPI_STUB("MultiplePermissionsCheckResult::Items");
    }
}
