#pragma once
#include <windows.h>
#include <activation.h>
#include <winstring.h>

namespace pdnet::xsapi
{
    // xbox=off: answers activation of Microsoft.Xbox.Services.* classes with pdnet's own implementations
    // (stubs/, generated from the XSAPI metadata by generate.py).
    HRESULT GetActivationFactory(HSTRING classId, IActivationFactory** factory);
}
