#pragma once
#include <windows.h>
#include <activation.h>
#include <winstring.h>

namespace pdnet::xsapi
{
    // xbox=off: answers activation of Microsoft.Xbox.Services.* classes with pdnet's own implementations
    // (stubs/, generated from the XSAPI metadata by generate.py).
    HRESULT GetActivationFactory(HSTRING classId, IActivationFactory** factory);

    // Sets the player limit of every shared session this PC hosts (the EOS lobby's max members), e.g. after the host
    // changes the game type in the lobby (see gamepatches.cpp). Returns how many sessions were changed.
    int SetHostedSessionsMaxMembers(uint32_t maxMembers);
}
