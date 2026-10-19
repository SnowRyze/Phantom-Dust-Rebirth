#include "xsapi_pch.h"
#include "Privacy.PrivacyService.h"


namespace winrt::Microsoft::Xbox::Services::Privacy::implementation
{
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<hstring>> PrivacyService::GetAvoidListAsync()
    {
        PDNET_XSAPI_STUB("PrivacyService::GetAvoidListAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Privacy::PermissionCheckResult> PrivacyService::CheckPermissionWithTargetUserAsync(hstring permissionId, hstring targetXboxUserId)
    {
        PDNET_XSAPI_STUB("PrivacyService::CheckPermissionWithTargetUserAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Privacy::MultiplePermissionsCheckResult>> PrivacyService::CheckMultiplePermissionsWithMultipleTargetUsersAsync(Windows::Foundation::Collections::IVectorView<hstring> permissionIds, Windows::Foundation::Collections::IVectorView<hstring> targetXboxUserIds)
    {
        PDNET_XSAPI_STUB("PrivacyService::CheckMultiplePermissionsWithMultipleTargetUsersAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<hstring>> PrivacyService::GetMuteListAsync()
    {
        PDNET_XSAPI_STUB("PrivacyService::GetMuteListAsync");
    }
}
