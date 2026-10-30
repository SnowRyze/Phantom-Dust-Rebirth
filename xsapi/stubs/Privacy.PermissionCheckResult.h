#pragma once
#include "Privacy/PermissionCheckResult.g.h"


namespace winrt::Microsoft::Xbox::Services::Privacy::implementation
{
    struct PermissionCheckResult : PermissionCheckResultT<PermissionCheckResult>
    {
        PermissionCheckResult() = default;

        bool IsAllowed();
        hstring PermissionRequested();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Privacy::PermissionDenyReason> DenyReasons();
    };
}
