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
using namespace Windows::Foundation;

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
        OutputSectionHeader(L"enum_endpoints");

        // list devices

        collections::IVectorView<midi2::MidiEndpointDeviceInformation> devices{ nullptr };

        try
        {
            // list all devices
            devices = midi2::MidiEndpointDeviceInformation::FindAll(
                midi2::MidiEndpointDeviceInformationSortOrder::Name,
                midi2::MidiEndpointDeviceInformationFilter::IncludeClientByteStreamNative |
                midi2::MidiEndpointDeviceInformationFilter::IncludeClientUmpNative |
                midi2::MidiEndpointDeviceInformationFilter::IncludeDiagnosticLoopback |
                midi2::MidiEndpointDeviceInformationFilter::IncludeDiagnosticPing |
                midi2::MidiEndpointDeviceInformationFilter::IncludeVirtualDeviceResponder
            );
        }
        catch (...)
        {
            OutputError(L"Unable to access Windows MIDI Services and enumerate devices.");

            RETURN_FAIL;
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
            RETURN_FAIL;
        }


        const uint8_t pingCount = 10;

        // ping the service
        if (pingTest)
        {
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
            }
        }

        if (midiClock)
        {
            OutputSectionHeader(L"midi_clock");

            OutputTimestampField(L"clock_frequency", midi2::MidiClock::TimestampFrequency());
            OutputTimestampField(L"clock_now", midi2::MidiClock::Now());
        }

    }
    catch (...)
    {
        OutputError(L"Exception attempting to gather MIDI information.");

        RETURN_FAIL;
    }


    RETURN_SUCCESS;
}
