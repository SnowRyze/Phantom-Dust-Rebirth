#pragma once
#include "Social/ReputationService.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    struct ReputationService : ReputationServiceT<ReputationService>
    {
        ReputationService() = default;

        Windows::Foundation::IAsyncAction SubmitReputationFeedbackAsync(hstring xboxUserId, Microsoft::Xbox::Services::Social::ReputationFeedbackType reputationFeedbackType, hstring sessionName, hstring reasonMessage, hstring evidenceResourceId);
        Windows::Foundation::IAsyncAction SubmitBatchReputationFeedbackAsync(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::ReputationFeedbackItem> feedbackItems);
    };
}
