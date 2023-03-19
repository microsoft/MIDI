#pragma once

#include "IMidiUmpEndpoint.g.h"

namespace winrt::Windows::Devices::Midi::ServiceContracts::implementation
{
    struct IMidiUmpEndpoint : IMidiUmpEndpointT<IMidiUmpEndpoint>
    {
        IMidiUmpEndpoint() = default;

        winrt::Windows::Foundation::Collections::IVector<uint32_t> IncomingMessages();
        winrt::Windows::Foundation::Collections::IVector<uint32_t> OutgoingMessages();
        hstring Id();
        winrt::Windows::Foundation::Collections::IMapView<hstring, winrt::Windows::Foundation::IInspectable> Properties();
    };
}

