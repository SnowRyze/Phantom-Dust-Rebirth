#include "xsapi_pch.h"
#include "GameServerPlatform.QualityOfServiceServer.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    hstring QualityOfServiceServer::ServerFullQualifiedDomainName()
    {
        PDNET_XSAPI_STUB("QualityOfServiceServer::ServerFullQualifiedDomainName");
    }
    hstring QualityOfServiceServer::SecureDeviceAddressBase64()
    {
        PDNET_XSAPI_STUB("QualityOfServiceServer::SecureDeviceAddressBase64");
    }
    hstring QualityOfServiceServer::TargetLocation()
    {
        PDNET_XSAPI_STUB("QualityOfServiceServer::TargetLocation");
    }
}
