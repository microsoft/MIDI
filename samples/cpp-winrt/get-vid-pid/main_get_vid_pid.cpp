// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: getting the USB VID and PID for an endpoint.
//
// Under WinMM you would have sent a DRV_QUERYDEVICEINTERFACE message to the
// driver and then parsed the device interface string yourself. Here, the
// identifiers are already parsed and waiting for you as properties.
//
// There are two different places to look, and they do not mean the same thing:
//
//   MidiParentDeviceInformation   - the USB VID/PID of the physical parent
//                                   device, parsed from the device instance
//                                   id. This is what WinMM developers are
//                                   usually looking for.
//
//   MidiEndpointTransportSuppliedInfo - VendorId/ProductId as supplied by the
//                                   transport. Populated for endpoints on the
//                                   UMP USB driver, or when we can otherwise
//                                   obtain it.
//
// Endpoints which are not USB devices (network, virtual, loopback, and so on)
// have no VID/PID at all. Those properties are UInt16, not nullable, so a
// value of zero means "not applicable" rather than "vendor zero".

#include <iostream>
#include <iomanip>
#include <sstream>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>

using namespace winrt::Windows::Devices::Midi2;                  // SDK core
using namespace winrt::Windows::Devices::Midi2::Enumeration;     // devices and metadata

namespace collections = winrt::Windows::Foundation::Collections;

// VID and PID are conventionally displayed as four hex digits.
std::wstring FormatUsbId(uint16_t id)
{
    if (id == 0)
    {
        return L"(not reported)";
    }

    std::wstringstream stream;
    stream << L"0x" << std::hex << std::uppercase << std::setw(4) << std::setfill(L'0') << id;

    return stream.str();
}

int main()
{
    winrt::init_apartment();

    if (!MidiApi::EnsureServiceAvailable())
    {
        std::wcout << L"Could not demand-start the MIDI service." << std::endl;
        return 1;
    }

    collections::IVectorView<MidiEndpointDeviceInformation> endpoints =
        MidiEndpointDeviceInformation::FindAll(
            MidiEndpointDeviceInformationSortOrder::Name,
            MidiEndpointDeviceInformationFilters::AllStandardEndpoints);

    std::wcout << endpoints.Size() << L" endpoint(s) found." << std::endl << std::endl;

    for (MidiEndpointDeviceInformation const& endpoint : endpoints)
    {
        std::wcout << endpoint.Name().c_str() << std::endl;

        // The parent is the physical device the endpoint belongs to. It is null
        // for endpoints which have no parent device, such as app-to-app MIDI.
        MidiParentDeviceInformation parent = endpoint.GetParentDeviceInformation();

        if (parent != nullptr)
        {
            // EnumeratorName tells you what kind of bus this is. "USB" means the
            // VID and PID below are meaningful.
            std::wcout << L"  Enumerator:    " << parent.EnumeratorName().c_str() << std::endl;
            std::wcout << L"  Parent name:   " << parent.Name().c_str() << std::endl;
            std::wcout << L"  USB VID:       " << FormatUsbId(parent.UsbVendorId()) << std::endl;
            std::wcout << L"  USB PID:       " << FormatUsbId(parent.UsbProductId()) << std::endl;

            winrt::hstring serialNumber = parent.UsbSerialNumber();

            if (!serialNumber.empty())
            {
                std::wcout << L"  USB serial:    " << serialNumber.c_str() << std::endl;
            }
        }
        else
        {
            std::wcout << L"  No parent device. This endpoint is not backed by physical hardware." << std::endl;
        }

        // The transport may also report the identifiers. For a device on the UMP
        // USB driver these come from the USB descriptors rather than from the
        // device instance id, so they are worth checking when the parent has none.
        MidiEndpointTransportSuppliedInfo transportInfo = endpoint.GetTransportSuppliedInfo();

        std::wcout << L"  Transport:     " << transportInfo.TransportCode().c_str() << std::endl;
        std::wcout << L"  Transport VID: " << FormatUsbId(transportInfo.VendorId()) << std::endl;
        std::wcout << L"  Transport PID: " << FormatUsbId(transportInfo.ProductId()) << std::endl;

        std::wcout << std::endl;
    }

    return 0;
}
