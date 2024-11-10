// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

const std::wstring fieldSeparator = MIDIDIAG_FIELD_SEPARATOR;

void OutputBlankLine()
{
    std::wcout
        << std::endl;
}

void OutputSectionHeader(_In_ std::wstring const& headerText)
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

void OutputHeader(_In_ std::wstring const& headerText)
{
    std::wcout
        << headerText
        << std::endl;
}

void OutputFieldLabel(_In_ std::wstring const& fieldName)
{
    std::wcout
        << std::setw(MIDIDIAG_MAX_FIELD_LABEL_WIDTH)
        << std::left
        << fieldName;
}

void OutputStringField(_In_ std::wstring const& fieldName, _In_ winrt::hstring const& value)
{
    OutputFieldLabel(fieldName);

    std::wcout
        << fieldSeparator
        << value.c_str()
        << std::endl;
}

void OutputStringField(_In_ std::wstring const& fieldName, _In_ std::wstring const& value)
{
    OutputFieldLabel(fieldName);

    std::wcout
        << fieldSeparator
        << value
        << std::endl;
}

void OutputBooleanField(_In_ std::wstring const& fieldName, _In_ bool const& value)
{
    OutputFieldLabel(fieldName);

    std::wcout
        << fieldSeparator
        << std::boolalpha
        << value
        << std::endl;
}

void OutputGuidField(_In_ std::wstring const& fieldName, _In_ winrt::guid const& value)
{
    OutputStringField(fieldName, internal::GuidToString(value));
}


void OutputCurrentTime()
{
    // At the time of this writing, C++ 20 consteval and format are broken in the MSVC 
    // std library so need to use vformat instead of format

    const auto time = std::time(nullptr);
    auto formattedDateTime = std::vformat(L"{0:%F} {0:%X}", std::make_wformat_args(time));

    OutputStringField(MIDIDIAG_FIELD_LABEL_CURRENT_TIME, formattedDateTime);
}



void OutputTimestampField(_In_ std::wstring const& fieldName, _In_ uint64_t const value)
{
    OutputFieldLabel(fieldName);

    std::wcout
        << fieldSeparator
        << value
        << std::endl;
}

void OutputNumericField(_In_ std::wstring const& fieldName, _In_ uint32_t const value)
{
    OutputFieldLabel(fieldName);

    std::wcout
        << fieldSeparator
        << std::dec
        << value
        << std::endl;
}

void OutputDoubleField(_In_ std::wstring const& fieldName, _In_ double const value, _In_ uint32_t precision)
{
    OutputFieldLabel(fieldName);

    std::wcout
        << fieldSeparator
        << std::dec
        << std::setprecision(precision)
        << value
        << std::endl;
}

void OutputDecimalMillisecondsField(_In_ std::wstring const& fieldName, _In_ double const value, _In_ uint32_t precision)
{
    OutputFieldLabel(fieldName);

    std::wcout
        << fieldSeparator
        << std::dec
        << std::setprecision(precision)
        << std::fixed
        << value
        << " ms"
        << std::endl;
}



void OutputHexNumericField(_In_ std::wstring const& fieldName, _In_ uint32_t const value)
{
    OutputFieldLabel(fieldName);

    std::wcout
        << fieldSeparator
        << L"0x"
        << std::hex
        << value
        << std::endl;
}


void OutputError(_In_ std::wstring const& errorMessage)
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


void OutputRegStringValue(std::wstring label, HKEY const key, std::wstring value)
{
    auto keyValue = wil::reg::try_get_value_string(key, value.c_str());
    if (keyValue.has_value())
    {
        OutputStringField(label, keyValue.value());
    }
    else
    {
        OutputStringField(label, std::wstring{ L"Not present" });
    }
}

// dword value > 0 == true
void OutputRegDWordBooleanValue(std::wstring label, HKEY const key, std::wstring value)
{
    auto keyValue = wil::reg::try_get_value_dword(key, value.c_str());
    if (keyValue.has_value())
    {
        OutputBooleanField(label, keyValue.value() > 0);
    }
    else
    {
        OutputStringField(label, std::wstring{ L"Not present" });
    }
}

bool DoSectionRegistryEntries(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_ENUM_REGISTRY);

    try
    {
        // check to see if the root is there

        const auto rootKey = wil::reg::open_unique_key(HKEY_LOCAL_MACHINE, MIDI_ROOT_REG_KEY, wil::reg::key_access::read);

        // list all values in the root

        OutputRegStringValue(MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_CURRENT_CONFIG, rootKey.get(), MIDI_CONFIG_FILE_REG_VALUE);
        OutputRegDWordBooleanValue(MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_DISCOVERY_ENABLED, rootKey.get(), MIDI_DISCOVERY_ENABLED_REG_VALUE);
        OutputRegDWordBooleanValue(MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_USE_MMCSS, rootKey.get(), MIDI_USE_MMCSS_REG_VALUE);
        OutputItemSeparator();

        // TODO: list all values under the desktop app sdk runtime


        // TODO: list all values under message processing plugins



        // list all values under transport plugins

        const auto transportPluginsKey = wil::reg::open_unique_key(HKEY_LOCAL_MACHINE, MIDI_ROOT_TRANSPORT_PLUGINS_REG_KEY);

        for (const auto& keyData : wil::make_range(wil::reg::key_iterator{ transportPluginsKey.get() }, wil::reg::key_iterator{}))
        {
            // name of the transport in the registry (this doesn't really mean anything)
            OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_NAME, keyData.name);

            const auto key = wil::reg::open_unique_key(HKEY_LOCAL_MACHINE, std::wstring(std::wstring(MIDI_ROOT_TRANSPORT_PLUGINS_REG_KEY) + L"\\" + keyData.name).c_str());

            OutputRegStringValue(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_CLSID, key.get(), MIDI_PLUGIN_CLSID_REG_VALUE);
            OutputRegDWordBooleanValue(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_ENABLED, key.get(), MIDI_PLUGIN_ENABLED_REG_VALUE);

            OutputItemSeparator();
        }
    }
    catch (...)
    {
        OutputError(L"Could not open required registry key(s).");

        return false;
    }

    return true;
}

bool DoSectionCOMComponents(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_ENUM_COM);

    return true;

}



bool DoSectionTransports(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    try
    {
        OutputSectionHeader(MIDIDIAG_SECTION_LABEL_ENUM_TRANSPORTS);

        auto transports = rept::MidiReporting::GetInstalledTransportPlugins();

        if (transports != nullptr && transports.Size() > 0)
        {
            for (auto const& transport : transports)
            {
                OutputGuidField(MIDIDIAG_FIELD_LABEL_TRANSPORT_ID, transport.Id);
                OutputStringField(MIDIDIAG_FIELD_LABEL_TRANSPORT_NAME, transport.Name);
                OutputStringField(MIDIDIAG_FIELD_LABEL_TRANSPORT_CODE, transport.TransportCode);
                OutputStringField(MIDIDIAG_FIELD_LABEL_TRANSPORT_VERSION, transport.Version);
                OutputStringField(MIDIDIAG_FIELD_LABEL_TRANSPORT_AUTHOR, transport.Author);
                OutputStringField(MIDIDIAG_FIELD_LABEL_TRANSPORT_DESCRIPTION, transport.Description);
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

bool DoSectionMidi2ApiEndpoints(_In_ bool const verbose)
{
    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_MIDI2_API_ENDPOINTS);

    // list devices

    collections::IVectorView<midi2::MidiEndpointDeviceInformation> devices{ nullptr };

    try
    {
        // list all devices
        devices = midi2::MidiEndpointDeviceInformation::FindAll(
            midi2::MidiEndpointDeviceInformationSortOrder::Name,
            midi2::MidiEndpointDeviceInformationFilters::StandardNativeMidi1ByteFormat |
            midi2::MidiEndpointDeviceInformationFilters::StandardNativeUniversalMidiPacketFormat |
            midi2::MidiEndpointDeviceInformationFilters::DiagnosticLoopback |
            midi2::MidiEndpointDeviceInformationFilters::DiagnosticPing |
            midi2::MidiEndpointDeviceInformationFilters::VirtualDeviceResponder
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

            auto transportInfo = device.GetTransportSuppliedInfo();
            auto userInfo = device.GetUserSuppliedInfo();
            auto endpointInfo = device.GetDeclaredEndpointInfo();

            // These names should not be localized because customers may parse these output fields

            OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_ID, device.EndpointDeviceId());
            OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_NAME, device.Name());
            OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_CODE, transportInfo.TransportCode);

            if (verbose)
            {
                OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_USER_SUPPLIED_NAME, userInfo.Name);
                OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_ENDPOINT_SUPPLIED_NAME, endpointInfo.Name);
                OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_SUPPLIED_NAME, transportInfo.Name);
                OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_SUPPLIED_DESC, transportInfo.Description);
                OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_USER_SUPPLIED_DESC, userInfo.Description);
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

bool DoSectionWinRTMidi1ApiEndpoints(_In_ bool const verbose)
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
        OutputError(L"Enumerating WinRT MIDI 1.0 devices encountered an exception.");

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
        OutputError(L"Enumerating WinRT MIDI 1.0 devices encountered an exception.");

        return false;
    }

    return true;
}

bool DoSectionWinMMMidi1ApiEndpoints(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_WINMM_API_INPUT_ENDPOINTS);

    try
    {
        // inputs
        uint32_t errorCount{ 0 };

        auto inputDeviceCount = midiInGetNumDevs();

        OutputNumericField(MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_COUNT, inputDeviceCount);
        OutputItemSeparator();

        for (uint32_t i = 0; i < inputDeviceCount; i++)
        {
            MIDIINCAPS inputCaps{};

            auto result = midiInGetDevCaps(i, &inputCaps, sizeof(inputCaps));

            if (result == MMSYSERR_NOERROR)
            {
                OutputNumericField(MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_ID, i);
                OutputStringField(MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_NAME, std::wstring{ inputCaps.szPname });

                if (i < inputDeviceCount - 1)
                {
                    OutputItemSeparator();
                }
            }
            else
            {
                errorCount++;
            }
        }

        OutputItemSeparator();
        OutputNumericField(MIDIDIAG_FIELD_LABEL_WINMM_ERROR_COUNT, errorCount);

    }
    catch (...)
    {
        OutputError(L"Enumerating WinMM MIDI 1.0 input devices encountered an exception.");

        return false;
    }

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_WINMM_API_OUTPUT_ENDPOINTS);

    try
    {
        // outputs
        uint32_t errorCount{ 0 };

        auto outputDeviceCount = midiOutGetNumDevs();

        OutputNumericField(MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_COUNT, outputDeviceCount);
        OutputItemSeparator();

        for (uint32_t i = 0; i < outputDeviceCount; i++)
        {
            MIDIOUTCAPS outputCaps{};

            auto result = midiOutGetDevCaps(i, &outputCaps, sizeof(outputCaps));

            if (result == MMSYSERR_NOERROR)
            {
                OutputNumericField(MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_ID, i);
                OutputStringField(MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_NAME, std::wstring{ outputCaps.szPname });

                if (i < outputDeviceCount - 1)
                {
                    OutputItemSeparator();
                }
            }
            else
            {
                errorCount++;
            }
        }

        OutputItemSeparator();
        OutputNumericField(MIDIDIAG_FIELD_LABEL_WINMM_ERROR_COUNT, errorCount);

    }
    catch (...)
    {
        OutputError(L"Enumerating WinMM MIDI 1.0 output devices encountered an exception.");

        return false;
    }

    return true;
}


bool DoSectionPingTest(_In_ bool const verbose, _In_ uint8_t const pingCount)
{
    UNREFERENCED_PARAMETER(verbose);

    try
    {
        OutputSectionHeader(MIDIDIAG_SECTION_LABEL_PING_TEST);

        OutputNumericField(MIDIDIAG_FIELD_LABEL_PING_ATTEMPT_COUNT, (uint32_t)pingCount);

        auto pingResult = diag::MidiDiagnostics::PingService(pingCount);

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

bool DoSectionClock(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_MIDI_CLOCK);

    OutputTimestampField(MIDIDIAG_FIELD_LABEL_CLOCK_FREQUENCY, midi2::MidiClock::TimestampFrequency());
    OutputTimestampField(MIDIDIAG_FIELD_LABEL_CLOCK_NOW, midi2::MidiClock::Now());

    return true;
}

init::MidiDesktopAppSdkInitializer initializer;

bool DoSectionSdkStatus(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_SDK_STATUS);


    // next, try to init the SDK
    bool initialized = initializer.InitializeSdkRuntime();

    OutputBooleanField(MIDIDIAG_FIELD_LABEL_SDK_INITIALIZED, initialized);

    return initialized;
}

bool DoSectionServiceStatus(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_SERVICE_STATUS);

    // this needs to be done before any other service calls

    bool available = initializer.EnsureServiceAvailable();

    OutputBooleanField(MIDIDIAG_FIELD_LABEL_SERVICE_AVAILABLE, available);

    return available;
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


std::wstring GetProcessorArchitectureString(WORD const arch)
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

void OutputSystemInfo(_In_ SYSTEM_INFO const& sysinfo)
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
    UNREFERENCED_PARAMETER(verbose);

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

    TIMECAPS timecaps;
    auto tcresult = ::timeGetDevCaps(&timecaps, sizeof(timecaps));

    OutputBlankLine();

    if (tcresult == MMSYSERR_NOERROR)
    {
        OutputNumericField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMECAPS_MIN_PERIOD, timecaps.wPeriodMin);
        OutputNumericField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMECAPS_MAX_PERIOD, timecaps.wPeriodMax);
    }
    else
    {
        OutputStringField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMECAPS_ERROR, std::wstring{ L"Could not get timecaps" });
    }

    ULONG minResolution;
    ULONG maxResolution;
    ULONG actualResolution;

    auto resresult = NtQueryTimerResolution(&maxResolution, &minResolution, &actualResolution);

    if (resresult == STATUS_SUCCESS)
    {
        double minResolutionMilliseconds = (double)minResolution / 10000.0;   // minResolution is in 100 nanosecond units
        double maxResolutionMilliseconds = (double)maxResolution / 10000.0;   // maxResolution is in 100 nanosecond units
        double actualResolutionMilliseconds = (double)actualResolution / 10000.0;   // actualResolution is in 100 nanosecond units

        // results here are in 100ns units
        OutputBlankLine();
        OutputDecimalMillisecondsField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMER_RESOLUTION_MIN_MS, minResolutionMilliseconds, 3);
        OutputDecimalMillisecondsField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMER_RESOLUTION_MAX_MS, maxResolutionMilliseconds, 3);
        OutputDecimalMillisecondsField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMER_RESOLUTION_CURRENT_MS, actualResolutionMilliseconds, 3);
    }

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
    OutputStringField(MIDIDIAG_HEADER_FIELD_LABEL_VERSION_BUILD_SOURCE, std::wstring{ WINDOWS_MIDI_SERVICES_BUILD_SOURCE });
    OutputStringField(MIDIDIAG_HEADER_FIELD_LABEL_VERSION_NAME, std::wstring{ WINDOWS_MIDI_SERVICES_BUILD_VERSION_NAME });
    OutputStringField(MIDIDIAG_HEADER_FIELD_LABEL_VERSION_FULL, std::wstring{ WINDOWS_MIDI_SERVICES_BUILD_VERSION_FULL });

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_HEADER);

    // this fails on Windows 10
//    OutputCurrentTime();

    try
    {
        DoSectionSystemInfo(verbose);

        DoSectionRegistryEntries(verbose);

//        DoSectionCOMComponents(verbose);

        // check the service first
        if (!DoSectionServiceStatus(verbose)) RETURN_FAIL;

        if (!DoSectionSdkStatus(verbose)) RETURN_FAIL;

        // only show midi clock info if the sdk init has worked
        if (midiClock)
        {
            if (!DoSectionClock(verbose)) RETURN_FAIL;
        }


        auto transportsWorked = DoSectionTransports(verbose);

        if (transportsWorked)
        {
            if (!DoSectionMidi2ApiEndpoints(verbose)) RETURN_FAIL;
        }

        DoSectionWinRTMidi1ApiEndpoints(verbose);  // we don't bail if this fails

        DoSectionWinMMMidi1ApiEndpoints(verbose);  // we don't bail if this fails

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
