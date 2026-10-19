#pragma once
#include "Social/XboxSocialRelationshipResult.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    struct XboxSocialRelationshipResult : XboxSocialRelationshipResultT<XboxSocialRelationshipResult>
    {
        XboxSocialRelationshipResult() = default;

        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxSocialRelationship> Items();
        uint32_t TotalCount();
        bool HasNext();
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Social::XboxSocialRelationshipResult> GetNextAsync(uint32_t maxItems);
    };
}
