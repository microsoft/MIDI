// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: working out what kind of thing an endpoint actually is.
//
// Under WinMM there was no answer to this. midiInGetDevCaps gave you a name and
// a technology field which said almost nothing, so applications guessed from
// the name, which is why so many of them break when a user renames a device.
//
// Here, every endpoint records which transport created it. Ask the endpoint for
// its transport id, then match that against the list of installed transports to
// get a name and description you can show a user.
//
// The short TransportCode ("KS", "BLE", "NET", "DIAG", and so on) is the stable
// thing to branch on in code. The Name and Description are display strings.

#include <iostream>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.Reporting.h>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::Enumeration;
using namespace winrt::Windows::Devices::Midi2::Reporting;

namespace collections = winrt::Windows::Foundation::Collections;


int main()
{
    winrt::init_apartment();

    if (!MidiApi::EnsureServiceAvailable())
    {
        std::wcout << L"Could not demand-start the MIDI service." << std::endl;
        return 1;
    }

    // The set of installed transports. This is small and changes only when the
    // service configuration changes, so reading it once is fine.
    collections::IVectorView<MidiServiceTransportPluginInfo> transports =
        MidiReporting::GetInstalledTransportPlugins();

    std::wcout << transports.Size() << L" transport(s) installed:" << std::endl;

    for (MidiServiceTransportPluginInfo const& transport : transports)
    {
        std::wcout << L"  " << transport.TransportCode().c_str()
            << L" - " << transport.Name().c_str() << std::endl;
    }

    std::wcout << std::endl;

    collections::IVectorView<MidiEndpointDeviceInformation> endpoints =
        MidiEndpointDeviceInformation::FindAll(
            MidiEndpointDeviceInformationSortOrder::Name,
            MidiEndpointDeviceInformationFilters::AllStandardEndpoints);

    std::wcout << endpoints.Size() << L" endpoint(s):" << std::endl << std::endl;

    for (MidiEndpointDeviceInformation const& endpoint : endpoints)
    {
        MidiEndpointTransportSuppliedInfo transportInfo = endpoint.GetTransportSuppliedInfo();

        std::wcout << endpoint.Name().c_str() << std::endl;

        // The transport list is short, so a straight search is clearer here than
        // building a lookup keyed on the GUID.
        MidiServiceTransportPluginInfo matchingTransport{ nullptr };

        for (MidiServiceTransportPluginInfo const& transport : transports)
        {
            if (transport.TransportId() == transportInfo.TransportId())
            {
                matchingTransport = transport;
                break;
            }
        }

        if (matchingTransport != nullptr)
        {
            std::wcout << L"  Transport:   " << matchingTransport.Name().c_str()
                << L" (" << matchingTransport.TransportCode().c_str() << L")" << std::endl;
            std::wcout << L"  Description: " << matchingTransport.Description().c_str() << std::endl;
        }
        else
        {
            // An endpoint can outlive the transport which created it, for example
            // if a transport was uninstalled while the endpoint record remains.
            std::wcout << L"  Transport:   " << transportInfo.TransportCode().c_str()
                << L" (not currently installed)" << std::endl;
        }

        // Worth showing alongside the transport, because these two are what most
        // applications actually want to branch on.
        std::wcout << L"  Native format: "
            << (transportInfo.NativeDataFormat() == MidiEndpointNativeDataFormat::UniversalMidiPacketFormat
                ? L"UMP" : L"MIDI 1.0 bytestream")
            << std::endl;

        std::wcout << L"  Multi-client:  "
            << (transportInfo.SupportsMultiClient() ? L"yes" : L"no")
            << std::endl << std::endl;
    }

    return 0;
}
