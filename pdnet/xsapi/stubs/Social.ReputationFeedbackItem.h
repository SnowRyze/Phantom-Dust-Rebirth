#pragma once
#include "Social/ReputationFeedbackItem.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    struct ReputationFeedbackItem : ReputationFeedbackItemT<ReputationFeedbackItem>
    {
        ReputationFeedbackItem() = default;

        ReputationFeedbackItem(hstring const& xboxUserId, Microsoft::Xbox::Services::Social::ReputationFeedbackType const& reputationFeedbackType, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference const& sessionReference, hstring const& reasonMessage, hstring const& evidenceResourceId);
        hstring XboxUserId();
        Microsoft::Xbox::Services::Social::ReputationFeedbackType FeedbackType();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference SessionReference();
        hstring ReasonMessage();
        hstring EvidenceResourceId();
    };
}
namespace winrt::Microsoft::Xbox::Services::Social::factory_implementation
{
    struct ReputationFeedbackItem : ReputationFeedbackItemT<ReputationFeedbackItem, implementation::ReputationFeedbackItem>
    {
    };
}
