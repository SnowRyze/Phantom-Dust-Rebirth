#include "xsapi_pch.h"
#include "Statistics.Manager.StatisticValue.h"


namespace winrt::Microsoft::Xbox::Services::Statistics::Manager::implementation
{
    hstring StatisticValue::Name()
    {
        PDNET_XSAPI_STUB("StatisticValue::Name");
    }
    double StatisticValue::AsNumber()
    {
        PDNET_XSAPI_STUB("StatisticValue::AsNumber");
    }
    int64_t StatisticValue::AsInteger()
    {
        PDNET_XSAPI_STUB("StatisticValue::AsInteger");
    }
    hstring StatisticValue::AsString()
    {
        PDNET_XSAPI_STUB("StatisticValue::AsString");
    }
    Microsoft::Xbox::Services::Statistics::Manager::StatisticDataType StatisticValue::DataType()
    {
        PDNET_XSAPI_STUB("StatisticValue::DataType");
    }
}
