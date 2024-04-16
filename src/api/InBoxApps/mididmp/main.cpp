// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <winrt/Windows.Devices.Midi2.h>

#include <iostream>

#include <chrono>
#include <format>


#include "pch.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;

const std::wstring fieldSeparator = L" : ";

void OutputBlankLine()
{
    std::wcout
        << std::endl;
}

void OutputSectionHeader(_In_ std::wstring headerText)
{
    const std::wstring sectionHeaderSeparator = std::wstring(79, '=');

    std::wcout
        << std::endl
        << sectionHeaderSeparator
        << std::endl
        << headerText
        << std::endl
        << sectionHeaderSeparator
        << std::endl
        << std::endl;
}

void OutputItemSeparator()
{
    const std::wstring itemSeparator = std::wstring(79, '-');

    std::wcout
        << itemSeparator
        << std::endl;
}

void OutputHeader(_In_ std::wstring headerText)
{
    std::wcout
        << headerText
        << std::endl;
}

void OutputStringField(_In_ std::wstring fieldName, _In_ winrt::hstring value)
{
    std::wcout
        << fieldName
        << fieldSeparator
        << value.c_str()
        << std::endl;
}

void OutputStringField(_In_ std::wstring fieldName, _In_ std::wstring value)
{
    std::wcout
        << fieldName
        << fieldSeparator
        << value
        << std::endl;
}

void OutputGuidField(_In_ std::wstring fieldName, _In_ winrt::guid value)
{
    OutputStringField(fieldName, internal::GuidToString(value));
}



void OutputCurrentTime()
{
    auto const time = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());

    OutputStringField(L"current_time", std::format(L"{:%Y-%m-%d %X}", time));

}



void OutputTimestampField(_In_ std::wstring fieldName, _In_ uint64_t value)
{
    std::wcout
        << fieldName
        << fieldSeparator
        << value
        << std::endl;
}

void OutputNumericField(_In_ std::wstring fieldName, _In_ uint32_t value)
{
    std::wcout
        << fieldName
        << fieldSeparator
        << value
        << std::endl;
}


void OutputError(_In_ std::wstring errorMessage)
{
    const std::wstring errorLabel = L"ERROR";

    std::wcout 
        << errorLabel
        << fieldSeparator
        << errorMessage
        << std::endl;
}

#define RETURN_SUCCESS return 0
#define RETURN_FAIL return 1


bool DoSectionTransports(_In_ bool verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    try
    {
        OutputSectionHeader(L"enum_transports");

        auto transports = midi2::MidiService::GetInstalledTransportPlugins();

        if (transports != nullptr && transports.Size() > 0)
        {
            for (auto const& transport : transports)
            {
                OutputGuidField(L"id", transport.Id());
                OutputStringField(L"name", transport.Name());
                OutputStringField(L"version", transport.Version());
                OutputStringField(L"author", transport.Author());
                OutputStringField(L"description", transport.Description());
            }
        }
        else
        {
            OutputError(L"Enumerating transports returned no matches. This is not expected and indicates an installation problem or that the service is not running.");
            return false;
        }
    }
    catch (...)
    {
        OutputError(L"Exception enumerating transports.");
        return false;
    }

    return true;
}

bool DoSectionMidi2ApiEndpoints(_In_ bool verbose)
{
    OutputSectionHeader(L"enum_ump_api_endpoints");

    // list devices

    collections::IVectorView<midi2::MidiEndpointDeviceInformation> devices{ nullptr };

    try
    {
        // list all devices
        devices = midi2::MidiEndpointDeviceInformation::FindAll(
            midi2::MidiEndpointDeviceInformationSortOrder::Name,
            midi2::MidiEndpointDeviceInformationFilters::IncludeClientByteStreamNative |
            midi2::MidiEndpointDeviceInformationFilters::IncludeClientUmpNative |
            midi2::MidiEndpointDeviceInformationFilters::IncludeDiagnosticLoopback |
            midi2::MidiEndpointDeviceInformationFilters::IncludeDiagnosticPing |
            midi2::MidiEndpointDeviceInformationFilters::IncludeVirtualDeviceResponder
        );
    }
    catch (...)
    {
        OutputError(L"Unable to access Windows MIDI Services and enumerate devices.");
        return false;
    }

    if (devices != nullptr && devices.Size() > 0)
    {
        for (uint32_t i = 0; i < devices.Size(); i++)
        {
            auto device = devices.GetAt(i);

            // These names should not be localized because customers may parse these output fields

            OutputStringField(L"endpoint_device_id", device.Id());
            OutputStringField(L"name", device.Name());
            OutputStringField(L"transport_mnemonic", device.TransportMnemonic());

            if (verbose)
            {
                OutputStringField(L"name_user_supplied", device.UserSuppliedName());
                OutputStringField(L"name_endpoint_supplied", device.EndpointSuppliedName());
                OutputStringField(L"name_transport_supplied", device.TransportSuppliedName());
                OutputStringField(L"description_transport_supplied", device.TransportSuppliedDescription());
                OutputStringField(L"description_user_supplied", device.UserSuppliedDescription());
            }

            auto parent = device.GetParentDeviceInformation();

            if (parent != nullptr)
            {
                OutputStringField(L"parent_id", parent.Id());
                OutputStringField(L"parent_name", parent.Name());
            }
            else
            {
                OutputError(L"Unable to find endpoint parent");
            }

            if (i != devices.Size() - 1)
            {
                OutputItemSeparator();
            }
        }
    }
    else
    {
        OutputError(L"Enumerating devices returned no matches. This is not expected and indicates an installation problem or that the service is not running.");
        return false;
    }

    return true;
}

bool DoSectionMidi1ApiEndpoints(_In_ bool verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(L"enum_midi1_api_input_endpoints");

    try
    {
        // inputs
        auto midi1Inputs = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
            winrt::Windows::Devices::Midi::MidiInPort::GetDeviceSelector()).get();

        for (uint32_t i = 0; i < midi1Inputs.Size(); i++)
        {
            auto device = midi1Inputs.GetAt(i);

            OutputStringField(L"endpoint_device_id", device.Id());
            OutputStringField(L"name", device.Name());

            if (i != midi1Inputs.Size() - 1)
            {
                OutputItemSeparator();
            }
        }
    }
    catch (...)
    {
        OutputError(L"Enumerating MIDI 1.0 devices encountered an exception.");

        return false;
    }

    OutputSectionHeader(L"enum_midi1_api_output_endpoints");

    try
    {// outputs
        auto midi1Outputs = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
            winrt::Windows::Devices::Midi::MidiOutPort::GetDeviceSelector()).get();

        for (uint32_t i = 0; i < midi1Outputs.Size(); i++)
        {
            auto device = midi1Outputs.GetAt(i);

            OutputStringField(L"endpoint_device_id", device.Id());
            OutputStringField(L"name", device.Name());

            if (i != midi1Outputs.Size() - 1)
            {
                OutputItemSeparator();
            }
        }
    }
    catch (...)
    {
        OutputError(L"Enumerating MIDI 1.0 devices encountered an exception.");

        return false;
    }
}

bool DoSectionPingTest(_In_ bool verbose, _In_ uint8_t pingCount)
{
    try
    {
        UNREFERENCED_PARAMETER(verbose);

        OutputSectionHeader(L"ping_test");

        OutputTimestampField(L"ping_attempt_count", pingCount);

        auto pingResult = midi2::MidiService::PingService(pingCount);

        OutputNumericField(L"ping_return_count", pingResult.Responses().Size());

        if (pingResult.Success())
        {
            OutputTimestampField(L"ping_time_round_trip_total_ticks", pingResult.TotalPingRoundTripMidiClock());
            OutputTimestampField(L"ping_time_round_trip_average_ticks", pingResult.AveragePingRoundTripMidiClock());

        }
        else
        {
            OutputError(L"Ping test failed");
            OutputStringField(L"ping_failure_reason", pingResult.FailureReason());

            return false;
        }
    }
    catch (...)
    {
        OutputError(L"Ping test failed with exception");

        return false;
    }

    return true;
}

bool DoSectionClock(_In_ bool verbose)
{
    OutputSectionHeader(L"midi_clock");

    OutputTimestampField(L"clock_frequency", midi2::MidiClock::TimestampFrequency());
    OutputTimestampField(L"clock_now", midi2::MidiClock::Now());

    return true;
}

int __cdecl main()
{
    winrt::init_apartment();

    bool verbose = true;
    bool pingTest = true;
    bool midiClock = true;

    OutputHeader(L"Microsoft Windows MIDI Services");

    OutputSectionHeader(L"header");

    OutputCurrentTime();

    try
    {
        auto transportsWorked = DoSectionTransports(verbose);

        if (transportsWorked)
        {
            if (!DoSectionMidi2ApiEndpoints(verbose)) RETURN_FAIL;
        }

        DoSectionMidi1ApiEndpoints(verbose);  // we don't bail if this fails

        if (transportsWorked)
        {
            // ping the service
            if (pingTest)
            {
                const uint8_t pingCount = 10;

                if (!DoSectionPingTest(verbose, pingCount)) RETURN_FAIL;
            }
        }

        if (midiClock)
        {
            if (!DoSectionClock(verbose)) RETURN_FAIL;
        }

    }
    catch (...)
    {
        OutputError(L"Exception attempting to gather MIDI information.");

        RETURN_FAIL;
    }

    OutputSectionHeader(L"end_of_file");

    RETURN_SUCCESS;
}
