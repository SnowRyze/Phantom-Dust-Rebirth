#include "xsapi_pch.h"
#include "GameServerPlatform.GameServerPlatformService.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::GameServerPlatform::ClusterResult> GameServerPlatformService::AllocateClusterAsync(uint32_t gameServerTitleId, hstring serviceConfigurationId, hstring sessionTemplateName, hstring sessionName, bool abortIfQueued)
    {
        PDNET_XSAPI_STUB("GameServerPlatformService::AllocateClusterAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::GameServerPlatform::ClusterResult> GameServerPlatformService::AllocateClusterInlineAsync(uint32_t gameServerTitleId, hstring serviceConfigurationId, hstring sandboxId, hstring ticketId, hstring gsiSetId, hstring gameVariantId, uint64_t maxAllowedPlayers, hstring location, bool abortIfQueued)
    {
        PDNET_XSAPI_STUB("GameServerPlatformService::AllocateClusterInlineAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::GameServerPlatform::GameServerTicketStatus> GameServerPlatformService::GetTicketStatusAsync(uint32_t gameServerTitleId, hstring ticketId)
    {
        PDNET_XSAPI_STUB("GameServerPlatformService::GetTicketStatusAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::GameServerPlatform::GameServerMetadataResult> GameServerPlatformService::GetGameServerMetadataAsync(uint32_t titleId, uint32_t maxAllowedPlayers, bool publisherOnly, uint32_t maxVariants, hstring locale)
    {
        PDNET_XSAPI_STUB("GameServerPlatformService::GetGameServerMetadataAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::GameServerPlatform::GameServerMetadataResult> GameServerPlatformService::GetGameServerMetadataAsync(uint32_t titleId, uint32_t maxAllowedPlayers, bool publisherOnly, uint32_t maxVariants, hstring locale, Windows::Foundation::Collections::IMapView<hstring, hstring> filterTags)
    {
        PDNET_XSAPI_STUB("GameServerPlatformService::GetGameServerMetadataAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::GameServerPlatform::QualityOfServiceServer>> GameServerPlatformService::GetQualityOfServiceServersAsync()
    {
        PDNET_XSAPI_STUB("GameServerPlatformService::GetQualityOfServiceServersAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::GameServerPlatform::AllocationResult> GameServerPlatformService::AllocateSessionHost(uint32_t gameServerTitleId, Windows::Foundation::Collections::IVectorView<hstring> locations, hstring sessionId, hstring cloudGameId, hstring gameModeId, hstring sessionCookie)
    {
        PDNET_XSAPI_STUB("GameServerPlatformService::AllocateSessionHost");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::GameServerPlatform::AllocationResult> GameServerPlatformService::GetSessionHostAllocationStatus(uint32_t gameServerTitleId, hstring sessionId)
    {
        PDNET_XSAPI_STUB("GameServerPlatformService::GetSessionHostAllocationStatus");
    }
}
