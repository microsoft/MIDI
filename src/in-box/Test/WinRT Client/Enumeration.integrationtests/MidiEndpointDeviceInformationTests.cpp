// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



#include "stdafx.h"


#pragma warning(disable : 4702)
void MidiEndpointDeviceInformationTests::TestCreateFromId()
{
    // use basic WinRT device support to find a MIDI Device id

    auto devices = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(MidiEndpointConnection::GetDeviceSelector()).get();

    for (const auto& device : devices)
    {
        std::wcout << L"----- Checking device: " << device.Name().c_str() << std::endl;

        auto endpointInfo = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(device.Id());
        VERIFY_IS_NOT_NULL(endpointInfo);

        std::wcout << L"Name:        " << endpointInfo.Name().c_str() << std::endl;
        std::wcout << L"Id:          " << endpointInfo.EndpointDeviceId().c_str() << std::endl;
        std::wcout << L"Instance Id: " << endpointInfo.DeviceInstanceId().c_str() << std::endl;
        std::wcout << L"Parent:      " << endpointInfo.ParentDeviceInstanceId().c_str() << std::endl;
        std::wcout << L"ContainerId: " << winrt::to_hstring(endpointInfo.ContainerId()) << std::endl;

        VERIFY_IS_FALSE(endpointInfo.Name().empty());
        VERIFY_IS_FALSE(endpointInfo.EndpointDeviceId().empty());
        //VERIFY_IS_FALSE(endpointInfo.ParentDeviceInstanceId().empty());

        // TODO: ensure we can get the other related info. Much of it is optional, however
    }
}

void MidiEndpointDeviceInformationTests::TestWalkUpToParent()
{
    auto devices = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(MidiEndpointConnection::GetDeviceSelector()).get();

    for (const auto& device : devices)
    {
        std::wcout << std::endl << L"----- Checking device: " << device.Name().c_str() << std::endl;

        auto endpointInfo = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(device.Id());
        VERIFY_IS_NOT_NULL(endpointInfo);

        std::wcout << L"Name:        " << endpointInfo.Name().c_str() << std::endl;
        std::wcout << L"Id:          " << endpointInfo.EndpointDeviceId().c_str() << std::endl;
        std::wcout << L"Parent:      " << endpointInfo.ParentDeviceInstanceId().c_str() << std::endl;

        if (!endpointInfo.ParentDeviceInstanceId().empty())
        {
            auto parentDevice = endpointInfo.GetParentDeviceInformation();
            VERIFY_IS_NOT_NULL(parentDevice);

            if (parentDevice)
            {
                std::wcout << L" - Parent Device:      " << parentDevice.Name().c_str() << std::endl;
                std::wcout << L" - Container:          " << winrt::to_hstring(parentDevice.ContainerId()).c_str() << std::endl;
                std::wcout << L" - Driver Inf Path:    " << parentDevice.DriverInfPath().c_str() << std::endl;
                std::wcout << L" - Driver Provider:    " << parentDevice.DriverProvider().c_str() << std::endl;
                std::wcout << L" - Driver Version:     " << parentDevice.DriverVersion().c_str() << std::endl;
                std::wcout << L" - Enumerator Name:    " << parentDevice.EnumeratorName().c_str() << std::endl;
                std::wcout << L" - Service Name:       " << parentDevice.ServiceName().c_str() << std::endl;
            }
        }
        else
        {
            auto transportInfo = endpointInfo.GetTransportSuppliedInfo();
            VERIFY_IS_NOT_NULL(transportInfo);

            if (transportInfo.TransportCode() == L"DIAG")
            {
                std::wcout << L"** Diagnostics endpoint. NO PARENT." << std::endl;
            }
            else
            {
                VERIFY_FAIL();
            }
        }

    }

}

void MidiEndpointDeviceInformationTests::TestFindAll()
{
    auto start = std::chrono::high_resolution_clock::now();

    auto endpoints = MidiEndpointDeviceInformation::FindAll();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::wcout << L"Found " << endpoints.Size() << L" endpoints. (Time taken: " << duration << L" ms)" << std::endl;

    
    // at a bare minimum, we'll have the GS synth
    VERIFY_IS_TRUE(endpoints.Size() > 0);

    for (auto const& ep : endpoints)
    {
        std::wcout << L"Endpoint: " << ep.Name().c_str() << std::endl;
    }
}


namespace
{
    // the rule an endpoint has to meet to be listed under a single filter flag
    bool IsEndpointOfKind(
        _In_ MidiEndpointDeviceInformation const& endpoint,
        _In_ MidiEndpointDeviceInformationFilters const filter)
    {
        auto const purpose = endpoint.EndpointPurpose();

        switch (filter)
        {
        case MidiEndpointDeviceInformationFilters::DiagnosticLoopback:
            return purpose == MidiEndpointDevicePurpose::DiagnosticLoopback;
        case MidiEndpointDeviceInformationFilters::DiagnosticPing:
            return purpose == MidiEndpointDevicePurpose::DiagnosticPing;
        case MidiEndpointDeviceInformationFilters::VirtualDeviceResponder:
            return purpose == MidiEndpointDevicePurpose::VirtualDeviceResponder;
        default:
            break;
        }

        if (purpose == MidiEndpointDevicePurpose::DiagnosticLoopback ||
            purpose == MidiEndpointDevicePurpose::DiagnosticPing ||
            purpose == MidiEndpointDevicePurpose::VirtualDeviceResponder)
        {
            return false;
        }

        auto const isByteFormat =
            endpoint.GetTransportSuppliedInfo().NativeDataFormat() == MidiEndpointNativeDataFormat::Midi1ByteFormat;

        return (filter == MidiEndpointDeviceInformationFilters::StandardNativeMidi1ByteFormat) ? isByteFormat : !isByteFormat;
    }
}

void MidiEndpointDeviceInformationTests::TestFindAllReturnsOnlyTheRequestedKind()
{
    MidiEndpointDeviceInformationFilters const filters[]
    {
        MidiEndpointDeviceInformationFilters::StandardNativeUniversalMidiPacketFormat,
        MidiEndpointDeviceInformationFilters::StandardNativeMidi1ByteFormat,
        MidiEndpointDeviceInformationFilters::VirtualDeviceResponder,
        MidiEndpointDeviceInformationFilters::DiagnosticLoopback,
        MidiEndpointDeviceInformationFilters::DiagnosticPing,
    };

    for (auto const filter : filters)
    {
        for (auto const& endpoint : MidiEndpointDeviceInformation::FindAll(MidiEndpointDeviceInformationSortOrder::Name, filter))
        {
            if (!IsEndpointOfKind(endpoint, filter))
            {
                LOG_OUTPUT(L"Filter 0x%08x listed '%s', purpose %d",
                    static_cast<uint32_t>(filter), endpoint.Name().c_str(), static_cast<int>(endpoint.EndpointPurpose()));
            }

            VERIFY_IS_TRUE(IsEndpointOfKind(endpoint, filter));
        }
    }
}

void MidiEndpointDeviceInformationTests::TestGeneralMidiSynthIsANormalEndpoint()
{
    auto const everything = MidiEndpointDeviceInformation::FindAll(
        MidiEndpointDeviceInformationSortOrder::Name,
        MidiEndpointDeviceInformationFilters::AllStandardEndpoints |
        MidiEndpointDeviceInformationFilters::VirtualDeviceResponder |
        MidiEndpointDeviceInformationFilters::DiagnosticLoopback |
        MidiEndpointDeviceInformationFilters::DiagnosticPing);

    winrt::hstring synthId{};

    for (auto const& endpoint : everything)
    {
        if (endpoint.EndpointPurpose() == MidiEndpointDevicePurpose::InBoxGeneralMidiSynth)
        {
            synthId = endpoint.EndpointDeviceId();
            break;
        }
    }

    if (synthId.empty())
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"The General MIDI Synth endpoint is not present.");
        return;
    }

    auto const listed = [&](MidiEndpointDeviceInformationFilters const filter)
        {
            for (auto const& endpoint : MidiEndpointDeviceInformation::FindAll(MidiEndpointDeviceInformationSortOrder::Name, filter))
            {
                if (endpoint.EndpointDeviceId() == synthId)
                {
                    return true;
                }
            }

            return false;
        };

    VERIFY_IS_TRUE(listed(MidiEndpointDeviceInformationFilters::AllStandardEndpoints), L"The synth is in the standard list");
    VERIFY_IS_FALSE(listed(MidiEndpointDeviceInformationFilters::DiagnosticLoopback | MidiEndpointDeviceInformationFilters::DiagnosticPing), L"and not in the diagnostics list");
    VERIFY_IS_FALSE(listed(MidiEndpointDeviceInformationFilters::VirtualDeviceResponder), L"and not in the virtual device responder list");
}


void MidiEndpointDeviceInformationTests::ValidateAssociatedMidi1Ports()
{
    auto endpoints = MidiEndpointDeviceInformation::FindAll();

    VERIFY_IS_TRUE(endpoints.Size() > 0);

    for (auto const& ep : endpoints)
    {
        std::wcout << L"Endpoint: " << ep.Name().c_str() << std::endl;

        auto ports = winrt::Windows::Devices::Midi2::Enumeration::Legacy::MidiLegacyPortDeviceInformation::FindAllForAssociatedEndpoint(ep.EndpointDeviceId());

        std::map<std::tuple<uint32_t, uint8_t>, bool> groupMap;

        for (auto const& port : ports)
        {
            // check to ensure we don't have duplicate group/flow combos. This is to verify we don't have another github issue #1060
            if (groupMap.find(std::make_tuple(static_cast<uint32_t>(port.Flow()), port.Group().DisplayValue())) != groupMap.end())
            {
                // duplicate group and flow
                VERIFY_FAIL();
            }
            else
            {
                groupMap[std::make_tuple(static_cast<uint32_t>(port.Flow()), port.Group().DisplayValue())] = true;
            }

            std::wcout << L" - Port: " << port.Name().c_str() << std::endl;
            std::wcout << L"   - Id: " << port.PortDeviceId().c_str() << std::endl;
            std::wcout << L"   - Group: " << port.Group().ToString() << std::endl;
            std::wcout << L"   - Flow: " << static_cast<uint32_t>(port.Flow()) << std::endl;
        }

    }
}