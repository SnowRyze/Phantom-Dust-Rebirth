#include "xsapi_pch.h"
#include "Social.ReputationFeedbackItem.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    ReputationFeedbackItem::ReputationFeedbackItem(hstring const& xboxUserId, Microsoft::Xbox::Services::Social::ReputationFeedbackType const& reputationFeedbackType, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference const& sessionReference, hstring const& reasonMessage, hstring const& evidenceResourceId)
    {
        PDNET_XSAPI_STUB("ReputationFeedbackItem::ReputationFeedbackItem");
    }
    hstring ReputationFeedbackItem::XboxUserId()
    {
        PDNET_XSAPI_STUB("ReputationFeedbackItem::XboxUserId");
    }
    Microsoft::Xbox::Services::Social::ReputationFeedbackType ReputationFeedbackItem::FeedbackType()
    {
        PDNET_XSAPI_STUB("ReputationFeedbackItem::FeedbackType");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference ReputationFeedbackItem::SessionReference()
    {
        PDNET_XSAPI_STUB("ReputationFeedbackItem::SessionReference");
    }
    hstring ReputationFeedbackItem::ReasonMessage()
    {
        PDNET_XSAPI_STUB("ReputationFeedbackItem::ReasonMessage");
    }
    hstring ReputationFeedbackItem::EvidenceResourceId()
    {
        PDNET_XSAPI_STUB("ReputationFeedbackItem::EvidenceResourceId");
    }
}
