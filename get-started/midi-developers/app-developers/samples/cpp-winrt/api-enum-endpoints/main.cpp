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

std::string BooleanToString(bool value)
{
    if (value)
        return "true";
    else
        return "false";
}

int main()
{
    winrt::init_apartment();

    bool includeDiagnosticsEndpoints = true;

    std::cout << "Enumerating endpoints..." << std::endl;

    auto endpoints = MidiEndpointDeviceInformation::FindAll(includeDiagnosticsEndpoints);

    std::cout << endpoints.Size() << " endpoints returned" << std::endl << std::endl;

    for (auto const& endpoint : endpoints)
    {
        std::cout << "Identification" << std::endl;
        std::cout << "- Name:    " << winrt::to_string(endpoint.Name()) << std::endl;
        std::cout << "- Id:      " << winrt::to_string(endpoint.Id()) << std::endl;
        std::cout << "- Parent:  " << winrt::to_string(endpoint.ParentDeviceId()) << std::endl;


        if (endpoint.EndpointPurpose() == MidiEndpointDevicePurpose::NormalMessageEndpoint)
        {
            std::cout << "- Purpose: Application MIDI Traffic" << std::endl;
        }
        else if (endpoint.EndpointPurpose() == MidiEndpointDevicePurpose::DiagnosticLoopback)
        {
            std::cout << "- Purpose: Diagnostics use" << std::endl;
        }
        else if (endpoint.EndpointPurpose() == MidiEndpointDevicePurpose::DiagnosticPing)
        {
            std::cout << "- Purpose: Internal Diagnostics Ping" << std::endl;
        }

        std::cout << std::endl << "Endpoint Metadata" << std::endl;
        std::cout << "- Product Instance Id:    " << winrt::to_string(endpoint.ProductInstanceId()) << std::endl;
        std::cout << "- Endpoint-supplied Name: " << winrt::to_string(endpoint.EndpointSuppliedName()) << std::endl;
    
        // todo: Sysex etc.

        std::cout << std::endl << "User-supplied Metadata" << std::endl;
        std::cout << "- User-supplied Name: " << winrt::to_string(endpoint.UserSuppliedName()) << std::endl;
        std::cout << "- Description:        " << winrt::to_string(endpoint.Description()) << std::endl;
        std::cout << "- Image Path:         " << winrt::to_string(endpoint.ImagePath()) << std::endl;

        std::cout << std::endl << "Endpoint Supported Capabilities" << std::endl;
        std::cout << "- UMP Major.Minor:   " << endpoint.SpecificationVersionMajor() << "." << endpoint.SpecificationVersionMinor() << std::endl;
        std::cout << "- MIDI 1.0 Protocol: " << BooleanToString(endpoint.SupportsMidi10Protocol()) << std::endl;
        std::cout << "- MIDI 2.0 Protocol: " << BooleanToString(endpoint.SupportsMidi20Protocol()) << std::endl;
        std::cout << "- Sending JR Time:   " << BooleanToString(endpoint.SupportsSendingJRTimestamps()) << std::endl;
        std::cout << "- Receiving JR Time: " << BooleanToString(endpoint.SupportsReceivingJRTimestamps()) << std::endl;
        std::cout << "- Multi-client:      " << BooleanToString(endpoint.SupportsMultiClient()) << std::endl;

        // TODO: Configured protocol

        std::cout << std::endl << "Transport Information" << std::endl;
        std::cout << "- Transport-supplied Name: " << winrt::to_string(endpoint.TransportSuppliedName()) << std::endl;
        std::cout << "- Transport Id:            " << winrt::to_string(endpoint.TransportId()) << std::endl;
        std::cout << "- Transport Mnemonic:      " << winrt::to_string(endpoint.TransportMnemonic()) << std::endl;

        if (endpoint.NativeDataFormat() == MidiEndpointNativeDataFormat::ByteStream)
        {
            std::cout << "- Native Data Format:      MIDI 1.0 Byte Stream" << std::endl;
        }
        else if (endpoint.NativeDataFormat() == MidiEndpointNativeDataFormat::UniversalMidiPacket)
        {
            std::cout << "- Native Data Format:      MIDI 2.0 UMP" << std::endl;
        }
        else 
        {
            std::cout << "- Native Data Format:      Unknown" << std::endl;
        }

        std::cout << std::endl << "Function Block Information" << std::endl;

        // Function Blocks
        std::cout << "- Static Blocks?:  " << BooleanToString(endpoint.HasStaticFunctionBlocks()) << std::endl;
        std::cout << "- Block Count:     " << endpoint.FunctionBlocks().Size() << std::endl;
        // TODO

        // Group Terminal Blocks
        std::cout << std::endl << "Group Terminal Blocks" << std::endl;
        std::cout << "- Block Count:     " << endpoint.GroupTerminalBlocks().Size() << std::endl;
        // TODO


        std::cout << "--------------------------------------------------------------------------" << std::endl << std::endl;


    }




}
