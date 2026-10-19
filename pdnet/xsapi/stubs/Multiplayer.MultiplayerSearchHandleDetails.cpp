// pdnet-hand: lobby browser entry built from a session snapshot (stage B).
#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerSearchHandleDetails.h"
#include "Multiplayer.MultiplayerSessionReference.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MultiplayerSearchHandleDetails::SessionReference()
    {
        return winrt::make<implementation::MultiplayerSessionReference>(hstring{ m_doc->scid }, hstring{ m_doc->templateName }, hstring{ m_doc->sessionName });
    }
    Windows::Foundation::Collections::IVectorView<hstring> MultiplayerSearchHandleDetails::SessionOwnerXboxUserIds()
    {
        std::vector<hstring> owners;
        if (!m_doc->ownerXuid.empty()) owners.emplace_back(m_doc->ownerXuid);
        else if (!m_doc->hostDeviceToken.empty()) owners.emplace_back(m_doc->hostDeviceToken);
        return winrt::single_threaded_vector(std::move(owners)).GetView();
    }
    Windows::Foundation::Collections::IVectorView<hstring> MultiplayerSearchHandleDetails::Tags()
    {
        std::vector<hstring> tags;
        for (auto& t : m_doc->search->tags) tags.emplace_back(t);
        return winrt::single_threaded_vector(std::move(tags)).GetView();
    }
    Windows::Foundation::Collections::IMapView<hstring, double> MultiplayerSearchHandleDetails::NumbersMetadata()
    {
        std::map<hstring, double> values;
        for (auto& [k, v] : m_doc->search->numbers) values.emplace(hstring{ k }, v);
        return winrt::single_threaded_map(std::move(values)).GetView();
    }
    Windows::Foundation::Collections::IMapView<hstring, hstring> MultiplayerSearchHandleDetails::StringsMetadata()
    {
        std::map<hstring, hstring> values;
        for (auto& [k, v] : m_doc->search->strings) values.emplace(hstring{ k }, hstring{ v });
        return winrt::single_threaded_map(std::move(values)).GetView();
    }
    Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerRoleType> MultiplayerSearchHandleDetails::RoleTypes()
    {
        return winrt::single_threaded_map<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerRoleType>().GetView();
    }
    uint32_t MultiplayerSearchHandleDetails::MembersCount()
    {
        uint32_t n = 0;
        for (auto& m : m_doc->members)
            if (!m.reservation && !m.leaving) ++n;
        return (std::max)(n, m_doc->lobbyMemberCount);
    }
}
