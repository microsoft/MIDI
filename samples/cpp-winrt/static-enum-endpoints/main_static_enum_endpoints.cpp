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

std::wstring BooleanToString(bool value)
{
    if (value)
        return L"true";
    else
        return L"false";
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
        std::wcout << L"Could not demand-start the MIDI service" << std::endl;
        return 1;
    }

    std::wcout << L"Enumerating endpoints..." << std::endl;


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

    std::wcout << L"Enumeration took " << MidiClock::ConvertTimestampTicksToMilliseconds(enumerationDuration) << L" milliseconds" << std::endl;

    std::wcout << endpoints.Size() << L" endpoints returned" << std::endl << std::endl;

    for (auto const& endpoint : endpoints)
    {
        std::wcout << L"Identification" << std::endl;
        std::wcout << L"- Name:    " << endpoint.Name().c_str() << std::endl;
        std::wcout << L"- Id:      " << endpoint.EndpointDeviceId().c_str() << std::endl;

        auto parent = endpoint.GetParentDeviceInformation();

        if (parent != nullptr)
        {
            std::wcout << L"- Parent:  " << parent.Id().c_str() << std::endl;
        }
        else
        {
            std::wcout << L"- Parent:  Unknown" <<std::endl;
        }

        if (endpoint.EndpointPurpose() == MidiEndpointDevicePurpose::NormalMessageEndpoint)
        {
            std::wcout << L"- Purpose: Application MIDI Traffic" << std::endl;
        }
        else if (endpoint.EndpointPurpose() == MidiEndpointDevicePurpose::VirtualDeviceResponder)
        {
            std::wcout << L"- Purpose: Virtual Device Responder" << std::endl;
        }
        else if (endpoint.EndpointPurpose() == MidiEndpointDevicePurpose::DiagnosticLoopback)
        {
            std::wcout << L"- Purpose: Diagnostics use" << std::endl;
        }
        else if (endpoint.EndpointPurpose() == MidiEndpointDevicePurpose::DiagnosticPing)
        {
            std::wcout << L"- Purpose: Internal Diagnostics Ping" << std::endl;
        }
        else
        {
            std::wcout << L"- Purpose: Unknown" << std::endl;
        }

        // Note: Most of these std::cout calls should really be std::wcout due to the format
        // of the string data. Similarly, conversions to std::string should be std::wstring
        
        // info gathered through endpoint discovery
        auto declaredEndpointInfo = endpoint.GetDeclaredEndpointInfo();

        if (declaredEndpointInfo != nullptr)
        {
            std::wcout << std::endl << L"Endpoint Metadata" << std::endl;
            std::wcout << L"- Product Instance Id:    " << declaredEndpointInfo.ProductInstanceId().c_str() << std::endl;
            std::wcout << L"- Endpoint-supplied Name: " << declaredEndpointInfo.Name().c_str() << std::endl;

            std::wcout << std::endl << "Endpoint Supported Capabilities" << std::endl;
            std::wcout << L"- UMP Major.Minor:   " << declaredEndpointInfo.SpecificationVersionMajor() << L"." << declaredEndpointInfo.SpecificationVersionMinor() << std::endl;
            std::wcout << L"- MIDI 1.0 Protocol: " << BooleanToString(declaredEndpointInfo.SupportsMidi10Protocol()) << std::endl;
            std::wcout << L"- MIDI 2.0 Protocol: " << BooleanToString(declaredEndpointInfo.SupportsMidi20Protocol()) << std::endl;
            std::wcout << L"- Sending JR Time:   " << BooleanToString(declaredEndpointInfo.SupportsSendingJitterReductionTimestamps()) << std::endl;
            std::wcout << L"- Receiving JR Time: " << BooleanToString(declaredEndpointInfo.SupportsReceivingJitterReductionTimestamps()) << std::endl;
            //std::cout << L"- Multi-client:      " << BooleanToString(declaredEndpointInfo.SupportsMultiClient) << std::endl;
        }
        else
        {
            std::wcout << L"No declared endpoint info available" << std::endl;
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
        std::wcout << L"- Static Blocks?:  " << BooleanToString(declaredEndpointInfo.HasStaticFunctionBlocks()) << std::endl;
        std::wcout << L"- Block Count:     " << functionBlocks.Size() << std::endl;

        for (auto const& functionBlock : functionBlocks)
        {
            std::cout << "  - " << int(functionBlock.Number()) << " : " << winrt::to_string(functionBlock.Name()) << std::endl;
        }
        
        // Group Terminal Blocks

        auto groupTerminalBlocks = endpoint.GetGroupTerminalBlocks();
        std::wcout << std::endl << L"Group Terminal Blocks" << std::endl;
        std::wcout << L"- Block Count:     " << groupTerminalBlocks.Size() << std::endl;

        for (auto const& groupTerminalBlock : groupTerminalBlocks)
        {
            std::wcout << "  - " << int(groupTerminalBlock.Number()) << L" : " << groupTerminalBlock.Name().c_str() << std::endl;
        }

        std::wcout << L"--------------------------------------------------------------------------" << std::endl << std::endl;
    }

    std::wcout << L"Cleaning up WinRT / COM apartment..." << std::endl;
    winrt::uninit_apartment();

}
