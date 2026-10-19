#pragma once
#include "Social/Manager/SocialEvent.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::Manager::implementation
{
    struct SocialEvent : SocialEventT<SocialEvent>
    {
        SocialEvent() = default;

        Microsoft::Xbox::Services::System::XboxLiveUser User();
        Microsoft::Xbox::Services::Social::Manager::SocialEventType EventType();
        Windows::Foundation::Collections::IVectorView<hstring> UsersAffected();
        int32_t ErrorCode();
        hstring ErrorMessage();
        Microsoft::Xbox::Services::Social::Manager::SocialEventArgs EventArgs();
    };
}
