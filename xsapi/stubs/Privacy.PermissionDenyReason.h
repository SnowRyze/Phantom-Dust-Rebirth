#pragma once
#include "Privacy/PermissionDenyReason.g.h"


namespace winrt::Microsoft::Xbox::Services::Privacy::implementation
{
    struct PermissionDenyReason : PermissionDenyReasonT<PermissionDenyReason>
    {
        PermissionDenyReason() = default;

        hstring Reason();
        hstring RestrictedSetting();
    };
}
