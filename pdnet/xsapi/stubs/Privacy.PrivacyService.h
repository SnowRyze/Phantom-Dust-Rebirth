#pragma once
#include "Privacy/PrivacyService.g.h"


namespace winrt::Microsoft::Xbox::Services::Privacy::implementation
{
    struct PrivacyService : PrivacyServiceT<PrivacyService>
    {
        PrivacyService() = default;

        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<hstring>> GetAvoidListAsync();
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Privacy::PermissionCheckResult> CheckPermissionWithTargetUserAsync(hstring permissionId, hstring targetXboxUserId);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Privacy::MultiplePermissionsCheckResult>> CheckMultiplePermissionsWithMultipleTargetUsersAsync(Windows::Foundation::Collections::IVectorView<hstring> permissionIds, Windows::Foundation::Collections::IVectorView<hstring> targetXboxUserIds);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<hstring>> GetMuteListAsync();
    };
}
