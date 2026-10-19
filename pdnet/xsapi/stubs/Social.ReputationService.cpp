#include "xsapi_pch.h"
#include "Social.ReputationService.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    Windows::Foundation::IAsyncAction ReputationService::SubmitReputationFeedbackAsync(hstring xboxUserId, Microsoft::Xbox::Services::Social::ReputationFeedbackType reputationFeedbackType, hstring sessionName, hstring reasonMessage, hstring evidenceResourceId)
    {
        PDNET_XSAPI_STUB("ReputationService::SubmitReputationFeedbackAsync");
    }
    Windows::Foundation::IAsyncAction ReputationService::SubmitBatchReputationFeedbackAsync(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::ReputationFeedbackItem> feedbackItems)
    {
        PDNET_XSAPI_STUB("ReputationService::SubmitBatchReputationFeedbackAsync");
    }
}
