#include "pch.h"
#include "IMidiUmpEndpoint.h"
#include "IMidiUmpEndpointFactory.h"

namespace winrt::Windows::Devices::Midi::ServiceContracts::implementation
{
    winrt::Windows::Devices::Midi::ServiceContracts::IMidiUmpEndpoint IMidiUmpEndpointFactory::GetExistingUmpEndpoint(hstring const& id)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::ServiceContracts::IMidiUmpEndpoint IMidiUmpEndpointFactory::CreateNewUmpEndpoint(hstring const& id, hstring const& configurationJson)
    {
        throw hresult_not_implemented();
    }
}