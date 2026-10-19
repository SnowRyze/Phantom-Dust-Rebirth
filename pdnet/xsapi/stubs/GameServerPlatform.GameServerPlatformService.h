#pragma once
#include "GameServerPlatform/GameServerPlatformService.g.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    struct GameServerPlatformService : GameServerPlatformServiceT<GameServerPlatformService>
    {
        GameServerPlatformService() = default;

        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::GameServerPlatform::ClusterResult> AllocateClusterAsync(uint32_t gameServerTitleId, hstring serviceConfigurationId, hstring sessionTemplateName, hstring sessionName, bool abortIfQueued);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::GameServerPlatform::ClusterResult> AllocateClusterInlineAsync(uint32_t gameServerTitleId, hstring serviceConfigurationId, hstring sandboxId, hstring ticketId, hstring gsiSetId, hstring gameVariantId, uint64_t maxAllowedPlayers, hstring location, bool abortIfQueued);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::GameServerPlatform::GameServerTicketStatus> GetTicketStatusAsync(uint32_t gameServerTitleId, hstring ticketId);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::GameServerPlatform::GameServerMetadataResult> GetGameServerMetadataAsync(uint32_t titleId, uint32_t maxAllowedPlayers, bool publisherOnly, uint32_t maxVariants, hstring locale);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::GameServerPlatform::GameServerMetadataResult> GetGameServerMetadataAsync(uint32_t titleId, uint32_t maxAllowedPlayers, bool publisherOnly, uint32_t maxVariants, hstring locale, Windows::Foundation::Collections::IMapView<hstring, hstring> filterTags);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::GameServerPlatform::QualityOfServiceServer>> GetQualityOfServiceServersAsync();
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::GameServerPlatform::AllocationResult> AllocateSessionHost(uint32_t gameServerTitleId, Windows::Foundation::Collections::IVectorView<hstring> locations, hstring sessionId, hstring cloudGameId, hstring gameModeId, hstring sessionCookie);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::GameServerPlatform::AllocationResult> GetSessionHostAllocationStatus(uint32_t gameServerTitleId, hstring sessionId);
    };
}
