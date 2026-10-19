#pragma once
#include "Privacy/MultiplePermissionsCheckResult.g.h"


namespace winrt::Microsoft::Xbox::Services::Privacy::implementation
{
    struct MultiplePermissionsCheckResult : MultiplePermissionsCheckResultT<MultiplePermissionsCheckResult>
    {
        MultiplePermissionsCheckResult() = default;

        hstring XboxUserId();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Privacy::PermissionCheckResult> Items();
    };
}
