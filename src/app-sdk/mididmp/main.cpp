// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

#include "pch.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;

const std::wstring fieldSeparator = MIDIDIAG_FIELD_SEPARATOR;

void OutputBlankLine()
{
    std::wcout
        << std::endl;
}

void OutputSectionHeader(_In_ std::wstring headerText)
{
    const std::wstring sectionHeaderSeparator = std::wstring(MIDIDIAG_SEPARATOR_REPEATING_CHAR_COUNT_PER_LINE, MIDIDIAG_SECTION_HEADER_SEPARATOR_CHAR);

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
    const std::wstring itemSeparator = std::wstring(MIDIDIAG_SEPARATOR_REPEATING_CHAR_COUNT_PER_LINE, MIDIDIAG_ITEM_SEPARATOR_CHAR);

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

void OutputFieldLabel(_In_ std::wstring fieldName)
{
    std::wcout
        << std::setw(MIDIDIAG_MAX_FIELD_LABEL_WIDTH)
        << std::left
        << fieldName;
}

void OutputStringField(_In_ std::wstring fieldName, _In_ winrt::hstring value)
{
    OutputFieldLabel(fieldName);

    std::wcout
        << fieldSeparator
        << value.c_str()
        << std::endl;
}

void OutputStringField(_In_ std::wstring fieldName, _In_ std::wstring value)
{
    OutputFieldLabel(fieldName);

    std::wcout
        << fieldSeparator
        << value
        << std::endl;
}

void OutputBooleanField(_In_ std::wstring fieldName, _In_ bool value)
{
    OutputFieldLabel(fieldName);

    std::wcout
        << fieldSeparator
        << std::boolalpha
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

    OutputStringField(MIDIDIAG_FIELD_LABEL_CURRENT_TIME, std::format(L"{:%Y-%m-%d %X}", time));

}



void OutputTimestampField(_In_ std::wstring fieldName, _In_ uint64_t value)
{
    OutputFieldLabel(fieldName);

    std::wcout
        << fieldSeparator
        << value
        << std::endl;
}

void OutputNumericField(_In_ std::wstring fieldName, _In_ uint32_t value)
{
    OutputFieldLabel(fieldName);

    std::wcout
        << fieldSeparator
        << std::dec
        << value
        << std::endl;
}

void OutputHexNumericField(_In_ std::wstring fieldName, _In_ uint32_t value)
{
    OutputFieldLabel(fieldName);

    std::wcout
        << fieldSeparator
        << L"0x"
        << std::hex
        << value
        << std::endl;
}


void OutputError(_In_ std::wstring errorMessage)
{
    const std::wstring errorLabel = L"ERROR";

    OutputFieldLabel(errorLabel);

    std::wcout 
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
        OutputSectionHeader(MIDIDIAG_SECTION_LABEL_ENUM_TRANSPORTS);

        auto transports = midi2::MidiService::GetInstalledTransportPlugins();

        if (transports != nullptr && transports.Size() > 0)
        {
            for (auto const& transport : transports)
            {
                OutputGuidField(MIDIDIAG_FIELD_LABEL_TRANSPORT_ID, transport.Id());
                OutputStringField(MIDIDIAG_FIELD_LABEL_TRANSPORT_NAME, transport.Name());
                OutputStringField(MIDIDIAG_FIELD_LABEL_TRANSPORT_MNEMONIC, transport.Mnemonic());
                OutputStringField(MIDIDIAG_FIELD_LABEL_TRANSPORT_VERSION, transport.Version());
                OutputStringField(MIDIDIAG_FIELD_LABEL_TRANSPORT_AUTHOR, transport.Author());
                OutputStringField(MIDIDIAG_FIELD_LABEL_TRANSPORT_DESCRIPTION, transport.Description());
                OutputItemSeparator();
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
    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_MIDI2_API_ENDPOINTS);

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

            OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_ID, device.Id());
            OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_NAME, device.Name());
            OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_MNEMONIC, device.TransportMnemonic());

            if (verbose)
            {
                OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_USER_SUPPLIED_NAME, device.UserSuppliedName());
                OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_ENDPOINT_SUPPLIED_NAME, device.EndpointSuppliedName());
                OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_SUPPLIED_NAME, device.TransportSuppliedName());
                OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_SUPPLIED_DESC, device.TransportSuppliedDescription());
                OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_USER_SUPPLIED_DESC, device.UserSuppliedDescription());
            }

            auto parent = device.GetParentDeviceInformation();

            if (parent != nullptr)
            {
                OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_ID, parent.Id());
                OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_NAME, parent.Name());
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

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_MIDI1_API_INPUT_ENDPOINTS);

    try
    {
        // inputs
        auto midi1Inputs = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
            winrt::Windows::Devices::Midi::MidiInPort::GetDeviceSelector()).get();

        for (uint32_t i = 0; i < midi1Inputs.Size(); i++)
        {
            auto device = midi1Inputs.GetAt(i);

            OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI1_ENDPOINT_ID, device.Id());
            OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI1_ENDPOINT_NAME, device.Name());

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

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_MIDI1_API_OUTPUT_ENDPOINTS);

    try
    {// outputs
        auto midi1Outputs = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
            winrt::Windows::Devices::Midi::MidiOutPort::GetDeviceSelector()).get();

        for (uint32_t i = 0; i < midi1Outputs.Size(); i++)
        {
            auto device = midi1Outputs.GetAt(i);

            OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI1_ENDPOINT_ID, device.Id());
            OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI1_ENDPOINT_NAME, device.Name());

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
    UNREFERENCED_PARAMETER(verbose);

    try
    {
        OutputSectionHeader(MIDIDIAG_SECTION_LABEL_PING_TEST);

        OutputNumericField(MIDIDIAG_FIELD_LABEL_PING_ATTEMPT_COUNT, (uint32_t)pingCount);

        auto pingResult = midi2::MidiService::PingService(pingCount);

        //std::cout << "DEBUG: PingService returned" << std::endl;

        if (pingResult != nullptr)
        {
            //std::cout << "DEBUG: pingresult != nullptr" << std::endl;

            OutputNumericField(MIDIDIAG_FIELD_LABEL_PING_RETURN_COUNT, pingResult.Responses().Size());

            if (pingResult.Success())
            {
                //std::cout << "DEBUG: pingresult.Success()" << std::endl;

                OutputTimestampField(MIDIDIAG_FIELD_LABEL_PING_ROUND_TRIP_TOTAL_TICKS, pingResult.TotalPingRoundTripMidiClock());
                OutputTimestampField(MIDIDIAG_FIELD_LABEL_PING_ROUND_TRIP_AVERAGE_TICKS, pingResult.AveragePingRoundTripMidiClock());

                return true;
            }
            else
            {
                OutputError(L"Ping test failed");
                OutputStringField(MIDIDIAG_FIELD_LABEL_PING_FAILURE_REASON, pingResult.FailureReason());

                return false;
            }
        }
        else
        {
            OutputError(L"Ping test failed. Return was null.");
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
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_MIDI_CLOCK);

    OutputTimestampField(MIDIDIAG_FIELD_LABEL_CLOCK_FREQUENCY, midi2::MidiClock::TimestampFrequency());
    OutputTimestampField(MIDIDIAG_FIELD_LABEL_CLOCK_NOW, midi2::MidiClock::Now());

    return true;
}

bool DoSectionServiceStatus(_In_ bool verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_SERVICE_STATUS);

    OutputBooleanField(MIDIDIAG_FIELD_LABEL_SERVICE_AVAILABLE, midi2::MidiService::EnsureAvailable());

    return true;
}


std::wstring GetOSVersion()
{
    try
    {
        OSVERSIONINFOW versionInfo{};

        NTSTATUS (WINAPI *rtlGetVersion)(PRTL_OSVERSIONINFOW) = nullptr;

        HINSTANCE ntdll = LoadLibrary(L"ntdll.dll");

        if (ntdll != nullptr)
        {
            rtlGetVersion = (NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW)) GetProcAddress(ntdll, "RtlGetVersion");

            if (rtlGetVersion != nullptr)
            {
                rtlGetVersion((PRTL_OSVERSIONINFOW)&versionInfo);
            }

            // do this before anything else so we ensure if frees
            FreeLibrary(ntdll);

            if (rtlGetVersion != nullptr)
            {
                return std::wstring(
                    std::to_wstring(versionInfo.dwMajorVersion) +
                    L"." +
                    std::to_wstring(versionInfo.dwMinorVersion) +
                    L"." +
                    std::to_wstring(versionInfo.dwBuildNumber) +
                    L" " +
                    versionInfo.szCSDVersion
                );
            }

        }

        return L"unknown";

    }
    catch (...)
    {
        return L"exception";
    }
}


std::wstring GetProcessorArchitectureString(WORD arch)
{
    switch (arch)
    {
    case PROCESSOR_ARCHITECTURE_AMD64:
        return L"64-bit Intel/AMD";
    case PROCESSOR_ARCHITECTURE_ARM:
        return L"32-bit Arm";
    case PROCESSOR_ARCHITECTURE_ARM64:
        return L"Arm64";
    case PROCESSOR_ARCHITECTURE_IA64:
        return L"Itanium";
    case PROCESSOR_ARCHITECTURE_INTEL:
        return L"32-bit Intel x86";
    default:
        return L"Unknown";
    }

}

void OutputSystemInfo(_In_ SYSTEM_INFO& sysinfo)
{
    // that sysinfo.dwNumberOfProcessors can return some strange results.
    
//    OutputNumericField(L"num_processors", sysinfo.dwNumberOfProcessors);
    std::wstring processorArchitecture = GetProcessorArchitectureString(sysinfo.wProcessorArchitecture);

    OutputStringField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_ARCH, processorArchitecture);
    OutputNumericField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_LEVEL, sysinfo.wProcessorLevel);
    OutputHexNumericField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_REVISION, sysinfo.wProcessorRevision);
}

bool DoSectionSystemInfo(_In_ bool verbose)
{
    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_OS);
    OutputStringField(MIDIDIAG_FIELD_LABEL_OS_VERSION, GetOSVersion());


    // if running under emulation on Arm64, this is going to return the emulated sys info
    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_APPARENT_SYSTEM_INFO);

    SYSTEM_INFO sysinfo;
    ::GetNativeSystemInfo(&sysinfo);
    OutputSystemInfo(sysinfo);

    // if running under emulation on Arm64, this is going to return the Arm info
    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_NATIVE_SYSTEM_INFO);

    SYSTEM_INFO sysinfoNative;
    ::GetNativeSystemInfo(&sysinfoNative);
    OutputSystemInfo(sysinfoNative);

    return true;
}


int __cdecl main()
{
    winrt::init_apartment();

    bool verbose = true;
    bool pingTest = true;
    bool midiClock = true;

    OutputHeader(MIDIDIAG_PRODUCT_NAME);
    OutputHeader(L"This application Copyright (c) 2024- Microsoft Corporation");
    OutputHeader(L"Information, license, and source available at https://aka.ms/midi");

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_HEADER);

    OutputCurrentTime();

    try
    {
        DoSectionSystemInfo(verbose);

        DoSectionServiceStatus(verbose);

        auto transportsWorked = DoSectionTransports(verbose);

        if (transportsWorked)
        {
            if (!DoSectionMidi2ApiEndpoints(verbose)) RETURN_FAIL;
        }

        DoSectionMidi1ApiEndpoints(verbose);  // we don't bail if this fails

        if (midiClock)
        {
            if (!DoSectionClock(verbose)) RETURN_FAIL;
        }

        if (transportsWorked)
        {
            // ping the service
            if (pingTest)
            {
                const uint8_t pingCount = 10;

                if (!DoSectionPingTest(verbose, pingCount)) RETURN_FAIL;
            }
        }
    }
    catch (...)
    {
        OutputError(L"Exception attempting to gather MIDI information.");

        RETURN_FAIL;
    }

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_END_OF_FILE);

    RETURN_SUCCESS;
}
