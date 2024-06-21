// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code

#include <iostream>

#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Microsoft.Windows.Devices.Midi2.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Initialization.h>

using namespace winrt::Microsoft::Windows::Devices::Midi2;                  // SDK Core
using namespace winrt::Microsoft::Windows::Devices::Midi2::Initialization;  // for code to check if the service is installed/running

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

    // Check to see if Windows MIDI Services is installed and running on this PC
    if (!MidiServicesInitializer::EnsureServiceAvailable())
    {
        // you may wish to fallback to an older MIDI API if it suits your application's workflow
        std::cout << std::endl << "** Windows MIDI Services is not running on this PC **" << std::endl;

        return 1;
    }
    else
    {
        std::cout << std::endl << "Verified that the MIDI Service is available and started" << std::endl;

        // bootstrap the SDK runtime
        MidiServicesInitializer::InitializeSdkRuntime();
    }

    std::cout << "Enumerating endpoints..." << std::endl;

    auto endpoints = MidiEndpointDeviceInformation::FindAll(
        MidiEndpointDeviceInformationSortOrder::Name,
        MidiEndpointDeviceInformationFilters::StandardNativeMidi1ByteFormat |
        MidiEndpointDeviceInformationFilters::StandardNativeUniversalMidiPacketFormat |
        MidiEndpointDeviceInformationFilters::DiagnosticLoopback |
        MidiEndpointDeviceInformationFilters::VirtualDeviceResponder
    );

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

        std::cout << std::endl << "Endpoint Metadata" << std::endl;
        std::cout << "- Product Instance Id:    " << winrt::to_string(declaredEndpointInfo.ProductInstanceId) << std::endl;
        std::cout << "- Endpoint-supplied Name: " << winrt::to_string(declaredEndpointInfo.Name) << std::endl;
    
        // Device Identity
        auto declaredDeviceIdentity = endpoint.GetDeclaredDeviceIdentity();

        std::cout << std::endl << "Device Identity" << std::endl;
        std::cout << "- System Exclusive Id:    "
            << declaredDeviceIdentity.SystemExclusiveIdByte1 << " " 
            << declaredDeviceIdentity.SystemExclusiveIdByte2 << " "
            << declaredDeviceIdentity.SystemExclusiveIdByte3
            << std::endl;

        std::cout << "- Device Family:          "
            << declaredDeviceIdentity.DeviceFamilyMsb << " "
            << declaredDeviceIdentity.DeviceFamilyLsb
            << std::endl;

        std::cout << "- Device Family Model:    "
            << declaredDeviceIdentity.DeviceFamilyModelNumberMsb << " "
            << declaredDeviceIdentity.DeviceFamilyModelNumberLsb
            << std::endl;

        std::cout << "- Software Revision Lvel: "
            << declaredDeviceIdentity.SoftwareRevisionLevelByte1 << " "
            << declaredDeviceIdentity.SoftwareRevisionLevelByte2 << " "
            << declaredDeviceIdentity.SoftwareRevisionLevelByte3 << " "
            << declaredDeviceIdentity.SoftwareRevisionLevelByte4
            << std::endl;


        // user-supplied info
        auto userInfo = endpoint.GetUserSuppliedInfo();
        std::cout << std::endl << "User-supplied Metadata" << std::endl;
        std::cout << "- User-supplied Name: " << winrt::to_string(userInfo.Name) << std::endl;
        std::cout << "- User Description:   " << winrt::to_string(userInfo.Description) << std::endl;
        std::cout << "- Small Image Path:   " << winrt::to_string(userInfo.SmallImagePath) << std::endl;
        std::cout << "- Large Image Path:   " << winrt::to_string(userInfo.LargeImagePath) << std::endl;

        std::cout << std::endl << "Endpoint Supported Capabilities" << std::endl;
        std::cout << "- UMP Major.Minor:   " << declaredEndpointInfo.SpecificationVersionMajor << "." << declaredEndpointInfo.SpecificationVersionMinor << std::endl;
        std::cout << "- MIDI 1.0 Protocol: " << BooleanToString(declaredEndpointInfo.SupportsMidi10Protocol) << std::endl;
        std::cout << "- MIDI 2.0 Protocol: " << BooleanToString(declaredEndpointInfo.SupportsMidi20Protocol) << std::endl;
        std::cout << "- Sending JR Time:   " << BooleanToString(declaredEndpointInfo.SupportsSendingJitterReductionTimestamps) << std::endl;
        std::cout << "- Receiving JR Time: " << BooleanToString(declaredEndpointInfo.SupportsReceivingJitterReductionTimestamps) << std::endl;
        //std::cout << "- Multi-client:      " << BooleanToString(declaredEndpointInfo.SupportsMultiClient) << std::endl;

        // TODO: Configured protocol

        auto transportInfo = endpoint.GetTransportSuppliedInfo();
        std::cout << std::endl << "Transport Information" << std::endl;
        std::cout << "- Transport-supplied Name: " << winrt::to_string(transportInfo.Name) << std::endl;
        std::cout << "- Description:             " << winrt::to_string(transportInfo.Description) << std::endl;
        std::cout << "- Transport Id:            " << winrt::to_string(winrt::to_hstring(transportInfo.TransportId)) << std::endl;
        std::cout << "- Transport Mnemonic:      " << winrt::to_string(transportInfo.TransportAbbreviation) << std::endl;

        if (transportInfo.NativeDataFormat == MidiEndpointNativeDataFormat::Midi1ByteFormat)
        {
            std::cout << "- Native Data Format:      MIDI 1.0 Byte Stream" << std::endl;
        }
        else if (transportInfo.NativeDataFormat == MidiEndpointNativeDataFormat::UniversalMidiPacketFormat)
        {
            std::cout << "- Native Data Format:      MIDI 2.0 UMP" << std::endl;
        }
        else 
        {
            std::cout << "- Native Data Format:      Unknown" << std::endl;
        }

        std::cout << std::endl << "Function Block Information" << std::endl;

        // Function Blocks

        auto functionBlocks = endpoint.GetDeclaredFunctionBlocks();
        std::cout << "- Static Blocks?:  " << BooleanToString(declaredEndpointInfo.HasStaticFunctionBlocks) << std::endl;
        std::cout << "- Block Count:     " << functionBlocks.Size() << std::endl;

        for (auto const& functionBlock : functionBlocks)
        {
            std::cout << "  - " << functionBlock.Number() << " : " << winrt::to_string(functionBlock.Name()) << std::endl;
        }
        

        // Group Terminal Blocks
        auto groupTerminalBlocks = endpoint.GetGroupTerminalBlocks();
        std::cout << std::endl << "Group Terminal Blocks" << std::endl;
        std::cout << "- Block Count:     " << groupTerminalBlocks.Size() << std::endl;

        for (auto const& groupTerminalBlock : groupTerminalBlocks)
        {
            std::cout << "  - " << groupTerminalBlock.Number() << " : " << winrt::to_string(groupTerminalBlock.Name()) << std::endl;
        }



        std::cout << "--------------------------------------------------------------------------" << std::endl << std::endl;


    }




}
