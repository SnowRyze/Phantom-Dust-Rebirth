#include "xsapi_pch.h"
#include "Social.Manager.SocialEvent.h"


namespace winrt::Microsoft::Xbox::Services::Social::Manager::implementation
{
    Microsoft::Xbox::Services::System::XboxLiveUser SocialEvent::User()
    {
        PDNET_XSAPI_STUB("SocialEvent::User");
    }
    Microsoft::Xbox::Services::Social::Manager::SocialEventType SocialEvent::EventType()
    {
        PDNET_XSAPI_STUB("SocialEvent::EventType");
    }
    Windows::Foundation::Collections::IVectorView<hstring> SocialEvent::UsersAffected()
    {
        PDNET_XSAPI_STUB("SocialEvent::UsersAffected");
    }
    int32_t SocialEvent::ErrorCode()
    {
        PDNET_XSAPI_STUB("SocialEvent::ErrorCode");
    }
    hstring SocialEvent::ErrorMessage()
    {
        PDNET_XSAPI_STUB("SocialEvent::ErrorMessage");
    }
    Microsoft::Xbox::Services::Social::Manager::SocialEventArgs SocialEvent::EventArgs()
    {
        PDNET_XSAPI_STUB("SocialEvent::EventArgs");
    }
}
