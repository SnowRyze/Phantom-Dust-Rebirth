#include "xsapi_pch.h"
#include "Social.XboxSocialRelationshipResult.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxSocialRelationship> XboxSocialRelationshipResult::Items()
    {
        PDNET_XSAPI_STUB("XboxSocialRelationshipResult::Items");
    }
    uint32_t XboxSocialRelationshipResult::TotalCount()
    {
        PDNET_XSAPI_STUB("XboxSocialRelationshipResult::TotalCount");
    }
    bool XboxSocialRelationshipResult::HasNext()
    {
        PDNET_XSAPI_STUB("XboxSocialRelationshipResult::HasNext");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Social::XboxSocialRelationshipResult> XboxSocialRelationshipResult::GetNextAsync(uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("XboxSocialRelationshipResult::GetNextAsync");
    }
}
