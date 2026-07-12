// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code

#include <iostream>

#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>

using namespace winrt::Windows::Devices::Midi2;                  // SDK Core
using namespace winrt::Windows::Devices::Midi2::Enumeration;     // Devices and metadata

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
    // initialize the thread before calling the bootstrapper or any WinRT code. You may also
    // be able to leave this out and call RoInitialize() or CoInitializeEx() before creating
    // the initializer.
    //winrt::init_apartment(winrt::apartment_type::single_threaded);

    // MTA by default
    winrt::init_apartment();

    if (!MidiApi::EnsureServiceAvailable())
    {
        std::cout << "Could not demand-start the MIDI service" << std::endl;
        return 1;
    }

    std::cout << "Enumerating endpoints..." << std::endl;


    auto enumerationStartTime = MidiClock::Now();

    // you can change the MidiEndpointDeviceInformationFilters here depending upon the types
    // of endpoints you want returned. "AllStandardEndpoints" contains what MIDI application
    // users (in a DAW, for example) will typically need to see.
    auto endpoints = MidiEndpointDeviceInformation::FindAll(
        MidiEndpointDeviceInformationSortOrder::Name,
        MidiEndpointDeviceInformationFilters::AllStandardEndpoints |
        MidiEndpointDeviceInformationFilters::DiagnosticLoopback
    );

    auto enumerationEndTime = MidiClock::Now();

    auto enumerationDuration = enumerationEndTime - enumerationStartTime;

    std::cout << "Enumeration took " << MidiClock::ConvertTimestampTicksToMilliseconds(enumerationDuration) << " milliseconds" << std::endl;

    std::cout << endpoints.Size() << " endpoints returned" << std::endl << std::endl;

    for (auto const& endpoint : endpoints)
    {
        std::cout << "Identification" << std::endl;
        std::cout << "- Name:    " << winrt::to_string(endpoint.Name()) << std::endl;
        std::cout << "- Id:      " << winrt::to_string(endpoint.EndpointDeviceId()) << std::endl;

        auto parent = endpoint.GetParentDeviceInformation();

        if (parent != nullptr)
        {
            std::cout << "- Parent:  " << winrt::to_string(parent.Id()) << std::endl;
        }
        else
        {
            std::cout << "- Parent:  Unknown" <<std::endl;
        }

        if (endpoint.EndpointPurpose() == MidiEndpointDevicePurpose::NormalMessageEndpoint)
        {
            std::cout << "- Purpose: Application MIDI Traffic" << std::endl;
        }
        else if (endpoint.EndpointPurpose() == MidiEndpointDevicePurpose::VirtualDeviceResponder)
        {
            std::cout << "- Purpose: Virtual Device Responder" << std::endl;
        }
        else if (endpoint.EndpointPurpose() == MidiEndpointDevicePurpose::DiagnosticLoopback)
        {
            std::cout << "- Purpose: Diagnostics use" << std::endl;
        }
        else if (endpoint.EndpointPurpose() == MidiEndpointDevicePurpose::DiagnosticPing)
        {
            std::cout << "- Purpose: Internal Diagnostics Ping" << std::endl;
        }
        else
        {
            std::cout << "- Purpose: Unknown" << std::endl;
        }

        // Note: Most of these std::cout calls should really be std::wcout due to the format
        // of the string data. Similarly, conversions to std::string should be std::wstring
        
        // info gathered through endpoint discovery
        auto declaredEndpointInfo = endpoint.GetDeclaredEndpointInfo();

        if (declaredEndpointInfo != nullptr)
        {
            std::cout << std::endl << "Endpoint Metadata" << std::endl;
            std::cout << "- Product Instance Id:    " << winrt::to_string(declaredEndpointInfo.ProductInstanceId()) << std::endl;
            std::cout << "- Endpoint-supplied Name: " << winrt::to_string(declaredEndpointInfo.Name()) << std::endl;

            std::cout << std::endl << "Endpoint Supported Capabilities" << std::endl;
            std::cout << "- UMP Major.Minor:   " << declaredEndpointInfo.SpecificationVersionMajor() << "." << declaredEndpointInfo.SpecificationVersionMinor() << std::endl;
            std::cout << "- MIDI 1.0 Protocol: " << BooleanToString(declaredEndpointInfo.SupportsMidi10Protocol()) << std::endl;
            std::cout << "- MIDI 2.0 Protocol: " << BooleanToString(declaredEndpointInfo.SupportsMidi20Protocol()) << std::endl;
            std::cout << "- Sending JR Time:   " << BooleanToString(declaredEndpointInfo.SupportsSendingJitterReductionTimestamps()) << std::endl;
            std::cout << "- Receiving JR Time: " << BooleanToString(declaredEndpointInfo.SupportsReceivingJitterReductionTimestamps()) << std::endl;
            //std::cout << "- Multi-client:      " << BooleanToString(declaredEndpointInfo.SupportsMultiClient) << std::endl;
        }
        else
        {
            std::cout << "No declared endpoint info available" << std::endl;
        }

        // Device Identity
        auto declaredDeviceIdentity = endpoint.GetDeclaredDeviceIdentity();

        if (declaredDeviceIdentity != nullptr)
        {
            std::cout << std::endl << "Device Identity" << std::endl;
            std::cout << "- Device Family:          "
                << declaredDeviceIdentity.DeviceFamilyMsb() << " "
                << declaredDeviceIdentity.DeviceFamilyLsb()
                << std::endl;

            std::cout << "- Device Family Model:    "
                << declaredDeviceIdentity.DeviceFamilyModelNumberMsb() << " "
                << declaredDeviceIdentity.DeviceFamilyModelNumberLsb()
                << std::endl;

            std::cout << "- System Exclusive Id:    "
                << declaredDeviceIdentity.SystemExclusiveId()[0] << " " 
                << declaredDeviceIdentity.SystemExclusiveId()[1] << " "
                << declaredDeviceIdentity.SystemExclusiveId()[2]
                << std::endl;

            std::cout << "- Software Revision Lvel: "
                << declaredDeviceIdentity.SoftwareRevisionLevel()[0] << " "
                << declaredDeviceIdentity.SoftwareRevisionLevel()[1] << " "
                << declaredDeviceIdentity.SoftwareRevisionLevel()[2] << " "
                << declaredDeviceIdentity.SoftwareRevisionLevel()[3]
                << std::endl;
        }
        else
        {
            std::cout << "No declared device identity." << std::endl;
        }


        // user-supplied info
        auto userInfo = endpoint.GetUserSuppliedInfo();

        if (userInfo != nullptr)
        {
            std::cout << std::endl << "User-supplied Metadata" << std::endl;
            std::cout << "- User-supplied Name: " << winrt::to_string(userInfo.Name()) << std::endl;
            std::cout << "- User Description:   " << winrt::to_string(userInfo.Description()) << std::endl;
            std::cout << "- Image Path:   " << winrt::to_string(userInfo.ImageFileName()) << std::endl;
        }
        else
        {
            std::cout << "No user-supplied information." << std::endl;
        }

        // TODO: Configured protocol

        auto transportInfo = endpoint.GetTransportSuppliedInfo();

        if (transportInfo != nullptr)
        {
            std::cout << std::endl << "Transport Information" << std::endl;
            std::cout << "- Transport-supplied Name: " << winrt::to_string(transportInfo.Name()) << std::endl;
            std::cout << "- Description:             " << winrt::to_string(transportInfo.Description()) << std::endl;
            std::cout << "- Transport Id:            " << winrt::to_string(winrt::to_hstring(transportInfo.TransportId())) << std::endl;
            std::cout << "- Transport Code:          " << winrt::to_string(transportInfo.TransportCode()) << std::endl;

            if (transportInfo.NativeDataFormat() == MidiEndpointNativeDataFormat::Midi1ByteFormat)
            {
                std::cout << "- Native Data Format:      MIDI 1.0 Byte Stream" << std::endl;
            }
            else if (transportInfo.NativeDataFormat() == MidiEndpointNativeDataFormat::UniversalMidiPacketFormat)
            {
                std::cout << "- Native Data Format:      MIDI 2.0 UMP" << std::endl;
            }
            else 
            {
                std::cout << "- Native Data Format:      Unknown" << std::endl;
            }
        }
        else
        {
            std::cout << "No transport-supplied information." << std::endl;
        }

        std::cout << std::endl << "Function Block Information" << std::endl;

        // Function Blocks

        auto functionBlocks = endpoint.GetDeclaredFunctionBlocks();
        std::cout << "- Static Blocks?:  " << BooleanToString(declaredEndpointInfo.HasStaticFunctionBlocks()) << std::endl;
        std::cout << "- Block Count:     " << functionBlocks.Size() << std::endl;

        for (auto const& functionBlock : functionBlocks)
        {
            std::cout << "  - " << int(functionBlock.Number()) << " : " << winrt::to_string(functionBlock.Name()) << std::endl;
        }
        
        // Group Terminal Blocks

        auto groupTerminalBlocks = endpoint.GetGroupTerminalBlocks();
        std::cout << std::endl << "Group Terminal Blocks" << std::endl;
        std::cout << "- Block Count:     " << groupTerminalBlocks.Size() << std::endl;

        for (auto const& groupTerminalBlock : groupTerminalBlocks)
        {
            std::cout << "  - " << int(groupTerminalBlock.Number()) << " : " << winrt::to_string(groupTerminalBlock.Name()) << std::endl;
        }

        std::cout << "--------------------------------------------------------------------------" << std::endl << std::endl;
    }

    std::cout << "Cleaning up WinRT / COM apartment..." << std::endl;
    winrt::uninit_apartment();

}
