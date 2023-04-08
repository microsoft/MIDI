#include "IMidiUmpEndpointFactory.g.h"

namespace winrt::Windows::Devices::Midi::ServiceContracts::implementation
{
    struct IMidiUmpEndpointFactory : IMidiUmpEndpointFactoryT<IMidiUmpEndpointFactory>
    {
        IMidiUmpEndpointFactory() = default;

        winrt::Windows::Devices::Midi::ServiceContracts::IMidiUmpEndpoint GetExistingUmpEndpoint(hstring const& id);
        winrt::Windows::Devices::Midi::ServiceContracts::IMidiUmpEndpoint CreateNewUmpEndpoint(hstring const& id, hstring const& configurationJson);
    };
}


