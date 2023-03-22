#include "pch.h"
#include "IMidiUmpEndpoint.h"
#include "IMidiUmpEndpoint.g.cpp"

namespace winrt::Windows::Devices::Midi::ServiceContracts::implementation
{
    winrt::Windows::Foundation::Collections::IVector<uint32_t> IMidiUmpEndpoint::IncomingMessages()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVector<uint32_t> IMidiUmpEndpoint::OutgoingMessages()
    {
        throw hresult_not_implemented();
    }
    hstring IMidiUmpEndpoint::Id()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IMapView<hstring, winrt::Windows::Foundation::IInspectable> IMidiUmpEndpoint::Properties()
    {
        throw hresult_not_implemented();
    }
}
