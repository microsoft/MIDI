// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code

#include "pch.h"
#include <iostream>

using namespace winrt::Windows::Devices::Midi2;        // API


// where you find types like IAsyncOperation, IInspectable, etc.
namespace foundation = winrt::Windows::Foundation;


int main()
{
    winrt::init_apartment();

    bool includeDiagnosticsEndpoints = true;

    std::cout << "Enumerating endpoints..." << std::endl;

    auto endpoints = MidiEndpointDeviceInformation::FindAll(includeDiagnosticsEndpoints);

    std::cout << endpoints.Size() << " endpoints returned" << std::endl;

    for (auto const& endpoint : endpoints)
    {
        std::cout << "Id:   " << winrt::to_string(endpoint.Id()) << std::endl;
        std::cout << "Name: " << winrt::to_string(endpoint.Name()) << std::endl;
        std::cout << "  User-supplied Name:      " << winrt::to_string(endpoint.UserSuppliedName()) << std::endl;
        std::cout << "  Endpoint-supplied Name:  " << winrt::to_string(endpoint.EndpointSuppliedName()) << std::endl;
        std::cout << "  Transport-supplied Name: " << winrt::to_string(endpoint.TransportSuppliedName()) << std::endl;

        // Function Blocks
        std::cout << "Static Function Blocks:   " << endpoint.HasStaticFunctionBlocks() << std::endl;
        // TODO

        // Group Terminal Blocks





    }




}
