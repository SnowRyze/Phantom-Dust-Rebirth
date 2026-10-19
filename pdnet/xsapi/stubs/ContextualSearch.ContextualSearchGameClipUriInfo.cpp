#include "xsapi_pch.h"
#include "ContextualSearch.ContextualSearchGameClipUriInfo.h"


namespace winrt::Microsoft::Xbox::Services::ContextualSearch::implementation
{
    Windows::Foundation::Uri ContextualSearchGameClipUriInfo::Url()
    {
        PDNET_XSAPI_STUB("ContextualSearchGameClipUriInfo::Url");
    }
    uint64_t ContextualSearchGameClipUriInfo::FileSize()
    {
        PDNET_XSAPI_STUB("ContextualSearchGameClipUriInfo::FileSize");
    }
    Microsoft::Xbox::Services::ContextualSearch::ContextualSearchGameClipUriType ContextualSearchGameClipUriInfo::UriType()
    {
        PDNET_XSAPI_STUB("ContextualSearchGameClipUriInfo::UriType");
    }
    Windows::Foundation::DateTime ContextualSearchGameClipUriInfo::Expiration()
    {
        PDNET_XSAPI_STUB("ContextualSearchGameClipUriInfo::Expiration");
    }
}
