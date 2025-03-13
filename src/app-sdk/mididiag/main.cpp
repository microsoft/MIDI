// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

#include "color.hpp"

void OutputFieldSeparator()
{
    std::cout
        << dye::grey(MIDIDIAG_FIELD_SEPARATOR);
}

void OutputBlankLine()
{
    std::wcout
        << std::endl;
}

void OutputSectionHeader(_In_ std::string const& headerText)
{
    const auto sectionHeaderSeparator = std::string(MIDIDIAG_SEPARATOR_REPEATING_CHAR_COUNT_PER_LINE, MIDIDIAG_SECTION_HEADER_SEPARATOR_CHAR);

    std::cout
        << std::endl
        << dye::grey(sectionHeaderSeparator)
        << std::endl
        << dye::aqua(headerText)
        << std::endl
        << dye::grey(sectionHeaderSeparator)
        << std::endl
        << std::endl;
}

void OutputItemSeparator()
{
    const auto itemSeparator = std::string(MIDIDIAG_SEPARATOR_REPEATING_CHAR_COUNT_PER_LINE, MIDIDIAG_ITEM_SEPARATOR_CHAR);

    std::cout
        << dye::grey(itemSeparator)
        << std::endl;
}

void OutputHeader(_In_ std::string const& headerText)
{
    std::cout
        << dye::aqua(headerText)
        << std::endl;
}

void OutputFieldLabel(_In_ std::string const& fieldName)
{
    std::cout
        << std::setw(MIDIDIAG_MAX_FIELD_LABEL_WIDTH)
        << std::left
        << dye::yellow(fieldName);
}

void OutputStringField(_In_ std::string const& fieldName, _In_ winrt::hstring const& value)
{
    OutputFieldLabel(fieldName);
    OutputFieldSeparator();

    std::wcout
        << value.c_str()
        << std::endl;
}

void OutputStringField(_In_ std::string const& fieldName, _In_ std::wstring const& value)
{
    OutputFieldLabel(fieldName);
    OutputFieldSeparator();

    std::wcout
        << value
        << std::endl;
}

void OutputBooleanField(_In_ std::string const& fieldName, _In_ bool const& value)
{
    OutputFieldLabel(fieldName);
    OutputFieldSeparator();

    std::wcout
        << std::boolalpha
        << value
        << std::endl;
}

void OutputGuidField(_In_ std::string const& fieldName, _In_ winrt::guid const& value)
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



void OutputTimestampField(_In_ std::string const& fieldName, _In_ uint64_t const value)
{
    OutputFieldLabel(fieldName);
    OutputFieldSeparator();

    std::wcout
        << value
        << std::endl;
}

void OutputNumericField(_In_ std::string const& fieldName, _In_ uint32_t const value)
{
    OutputFieldLabel(fieldName);
    OutputFieldSeparator();

    std::wcout
        << std::dec
        << value
        << std::endl;
}

void OutputDoubleField(_In_ std::string const& fieldName, _In_ double const value, _In_ uint32_t precision)
{
    OutputFieldLabel(fieldName);
    OutputFieldSeparator();

    std::wcout
        << std::dec
        << std::setprecision(precision)
        << value
        << std::endl;
}

void OutputDecimalMillisecondsField(_In_ std::string const& fieldName, _In_ double const value, _In_ uint32_t precision)
{
    OutputFieldLabel(fieldName);
    OutputFieldSeparator();

    std::wcout
        << std::dec
        << std::setprecision(precision)
        << std::fixed
        << value
        << " ms"
        << std::endl;
}



void OutputHexNumericField(_In_ std::string const& fieldName, _In_ uint32_t const value)
{
    OutputFieldLabel(fieldName);
    OutputFieldSeparator();

    std::wcout
        << L"0x"
        << std::hex
        << value
        << std::endl;
}


bool OutputFileVersion(_In_ std::string const& fieldName, _In_ std::wstring const& fileName)
{
    DWORD handle{ 0 };

    if (DWORD bufferSize = GetFileVersionInfoSize(fileName.c_str(), &handle); bufferSize > 0)
    {
        std::vector<byte> buffer;
        buffer.resize(bufferSize);

        if (GetFileVersionInfo(fileName.c_str(), handle, bufferSize, buffer.data()))
        {
            LPBYTE verInfoBuffer{ nullptr };
            UINT verInfoBufferSize{ 0 };

            if (VerQueryValue(buffer.data(), L"\\", (LPVOID*)&verInfoBuffer, &verInfoBufferSize))
            {
                if (verInfoBufferSize > 0 && verInfoBufferSize >= sizeof(VS_FIXEDFILEINFO))
                {
                    VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)(verInfoBuffer);

                    OutputFieldLabel(fieldName);
                    OutputFieldSeparator();

                    // major.minor.build.revision format
                    std::wcout
                        << std::dec
                        << static_cast<uint16_t>((verInfo->dwFileVersionMS >> 16) & 0xffff)
                        << L"."
                        << static_cast<uint16_t>((verInfo->dwFileVersionMS) & 0xffff)
                        << L"."
                        << static_cast<uint16_t>((verInfo->dwFileVersionLS >> 16) & 0xffff)
                        << L"."
                        << static_cast<uint16_t>((verInfo->dwFileVersionLS) & 0xffff)
                        << std::endl;
                }
            }
        }
    }

    return false;
}


void OutputError(_In_ winrt::hresult_error const& error)
{
    OutputFieldLabel(MIDIDIAG_FIELD_LABEL_ERROR);
    OutputFieldSeparator();

    std::cout
        << std::hex
        << error.code()
        << " : ";

    std::cout
        << dye::light_red(winrt::to_string(error.message()))
        << std::endl;

}

void OutputError(_In_ std::string const& errorMessage)
{
    OutputFieldLabel(MIDIDIAG_FIELD_LABEL_ERROR);
    OutputFieldSeparator();

    std::cout 
        << dye::light_red(errorMessage)
        << std::endl;
}


#define RETURN_SUCCESS return 0
#define RETURN_FAIL return 1







void OutputRegStringValue(std::string label, HKEY const key, std::wstring value)
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
void OutputRegDWordBooleanValue(std::string label, HKEY const key, std::wstring value)
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



void OutputCOMComponentInfo(std::string const dllNameFieldName, std::wstring const classid)
{
    // InprocServer32 Value

    std::wstring inprocServerKeyLocation = std::wstring{ L"CLSID\\" } + classid + std::wstring{ L"\\InprocServer32" };
    wil::unique_hkey pathKey{ };    // the path is the "(default)" entry

    if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_CLASSES_ROOT, inprocServerKeyLocation.c_str(), pathKey, wil::reg::key_access::read)))
    {
        auto path = wil::reg::try_get_value_string(pathKey.get(), nullptr);

        if (path.has_value())
        {
            OutputStringField(dllNameFieldName, path.value());
            OutputFileVersion(MIDIDIAG_FIELD_LABEL_FILE_VERSION, path.value());
        }
    }
    else
    {
        OutputStringField(dllNameFieldName, std::wstring{ L"ERROR" });
        OutputError("Could not find COM component in-proc server value under HKEY_CLASSES_ROOT");
    }
}


bool DoSectionDrivers32RegistryEntries(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_ENUM_REGISTRY_DRIVERS32);

    // list all MIDI values under Drivers32

    try
    {
        std::wstring drivers32KeyLocation = std::wstring{ L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32" };
        wil::unique_hkey drivers32Key{ };

        if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, drivers32KeyLocation.c_str(), drivers32Key, wil::reg::key_access::read)))
        {
            for (const auto& valueData : wil::make_range(wil::reg::value_iterator{ drivers32Key.get() }, wil::reg::value_iterator{}))
            {
                //valueData.name;
                //valueData.type;

                if (valueData.name.starts_with(L"midi") && valueData.name != L"midimapper")
                {
                    auto val = wil::reg::try_get_value_string(drivers32Key.get(), valueData.name.c_str());

                    if (val.has_value())
                    {
                        OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_DRIVERS32_ENTRY, valueData.name + L" = " + val.value());
                    }
                }
                else if (valueData.name == L"MidisrvTransferComplete")
                {
                    auto val = wil::reg::try_get_value_dword(drivers32Key.get(), valueData.name.c_str());

                    if (val.has_value())
                    {
                        OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_DRIVERS32_ENTRY, valueData.name + L" = " + std::to_wstring(val.value()));
                    }
                }

            }
        }
        else
        {
            OutputStringField("ERROR", drivers32KeyLocation);
            OutputError("Could not open Drivers32 Key");
        }
    }
    catch (...)
    {
        OutputError("Exception enumerating registry keys and values.");

        return false;
}


    return true;
}


bool DoSectionMidi2RegistryEntries(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_ENUM_REGISTRY);

    try
    {
        // check to see if the root is there

        wil::unique_hkey rootKey{};
        if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, MIDI_ROOT_REG_KEY, rootKey, wil::reg::key_access::read)))
        {
            // list all values in the root

            OutputRegStringValue(MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_CURRENT_CONFIG, rootKey.get(), MIDI_CONFIG_FILE_REG_VALUE);
            OutputRegDWordBooleanValue(MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_DISCOVERY_ENABLED, rootKey.get(), MIDI_DISCOVERY_ENABLED_REG_VALUE);
            OutputRegDWordBooleanValue(MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_USE_MMCSS, rootKey.get(), MIDI_USE_MMCSS_REG_VALUE);
            OutputItemSeparator();
        }
        else
        {
            OutputError("Unable to find or open root Windows MIDI Services registry key. Is Windows MIDI Services installed?");
        }

        // List midisrv info

        wil::unique_hkey midisrvkey{ };
        if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Midisrv", midisrvkey, wil::reg::key_access::read)))
        {
            if (midisrvkey.is_valid())
            {
                auto midisrvImagePath = wil::reg::try_get_value_string(midisrvkey.get(), L"ImagePath");

                if (midisrvImagePath.has_value())
                {
                    OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_MIDISRV_EXENAME, midisrvImagePath.value());
               //     OutputFileVersion(MIDIDIAG_FIELD_LABEL_FILE_VERSION, midisrvImagePath.value());
                }
                else
                {
                    OutputError("Found midisrv service entry in registry, but it does not have an ImagePath");
                }

            }
            else
            {
                OutputError("midisrv registry key not found or is not valid, or user does not have permissions to read it.");
            }
        }
        else
        {
            OutputError("Unable to find or open Midisrv entry in registry services key. Is Windows MIDI Services installed?");
        }

        OutputItemSeparator();

        // list values under the desktop app sdk runtime

        wil::unique_hkey sdkRuntimeRootKey{ };
        if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, MIDI_ROOT_APP_SDK_REG_KEY, sdkRuntimeRootKey)))
        {
            auto sdkRuntimeInstalledValue = wil::reg::try_get_value_string(sdkRuntimeRootKey.get(), MIDI_APP_SDK_INSTALLED_REG_VALUE);

            if (sdkRuntimeInstalledValue.has_value())
            {
                OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_SDK_INSTALLED, sdkRuntimeInstalledValue.value());
            }
            else
            {
                OutputError("No MIDI Desktop app SDK runtime registry 'Installed' value. The Windows MIDI Services SDK does not appear to be installed.");
            }
        }
        else
        {
            OutputError("No MIDI Desktop app SDK runtime registry root key. The Windows MIDI Services SDK does not appear to be installed.");
        }

        OutputItemSeparator();


        //  List midisrvtransport info, even though it is not in the Windows MIDI Services registry key

        std::wstring midisrvTransportClsidString{ L"{2BA15E4E-5417-4A66-85B8-2B2260EFBC84}" };
        OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_NAME, std::wstring{ L"(Midisrv Transport)" });
        OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_CLSID, midisrvTransportClsidString);
        OutputCOMComponentInfo(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_DLLNAME, midisrvTransportClsidString);
        OutputItemSeparator();


        std::wstring diagnosticsTransportClsidString{ L"{ac9b5417-3fe0-4e62-960f-034ee4235a1a}" };
        OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_NAME, std::wstring{ L"(Diagnostics Transport)" });
        OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_CLSID, diagnosticsTransportClsidString);
        OutputCOMComponentInfo(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_DLLNAME, diagnosticsTransportClsidString);
        OutputItemSeparator();


        //  TODO: List diagnostics transport info, even though it is not in the Windows MIDI Services registry key

        // TODO: list all values under message processing plugins



        // list all values under transport plugins

        wil::unique_hkey transportPluginsKey{ };
        if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, MIDI_ROOT_TRANSPORT_PLUGINS_REG_KEY, transportPluginsKey)))
        {
            for (const auto& keyData : wil::make_range(wil::reg::key_iterator{ transportPluginsKey.get() }, wil::reg::key_iterator{}))
            {
                // name of the transport in the registry (this doesn't really mean anything)
                OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_NAME, keyData.name);

                wil::unique_hkey key{ };
                if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, std::wstring(std::wstring(MIDI_ROOT_TRANSPORT_PLUGINS_REG_KEY) + L"\\" + keyData.name).c_str(), key)))
                {
                    OutputRegStringValue(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_CLSID, key.get(), MIDI_PLUGIN_CLSID_REG_VALUE);
                    OutputRegDWordBooleanValue(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_ENABLED, key.get(), MIDI_PLUGIN_ENABLED_REG_VALUE);

                    // resolve the DLL path for the transport

                    auto midiClsid = wil::reg::try_get_value_string(key.get(), MIDI_PLUGIN_CLSID_REG_VALUE);

                    if (midiClsid.has_value())
                    {
                        OutputCOMComponentInfo(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_DLLNAME, midiClsid.value());
                    }
                    else
                    {
                        OutputError("No clsid found in MIDI transport entry");
                    }

                }

                OutputItemSeparator();
            }
        }
        else
        {
            OutputError("Unable to enumerate transport plugins from registry.");
        }

    }
    catch (...)
    {
        OutputError("Exception enumerating registry keys and values.");

        return false;
    }

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
            OutputError("Enumerating transports returned no matches. This is not expected and indicates an installation problem or that the service is not running.");
            return false;
        }
    }
    catch (...)
    {
        OutputError("Exception enumerating transports.");
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
        OutputError("Unable to access Windows MIDI Services and enumerate devices.");
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


            // show any GTBs. This is needed to help debug some winmm enumeration issues

            for (auto const& gtb : device.GetGroupTerminalBlocks())
            {
                OutputBlankLine();

                OutputNumericField(MIDIDIAG_FIELD_LABEL_GTB_NUMBER, gtb.Number());
                OutputStringField(MIDIDIAG_FIELD_LABEL_GTB_NAME, gtb.Name());
                OutputNumericField(MIDIDIAG_FIELD_LABEL_GTB_FIRST_GROUP, gtb.FirstGroup().DisplayValue());
                OutputNumericField(MIDIDIAG_FIELD_LABEL_GTB_GROUP_COUNT, gtb.GroupCount());

                std::wstring gtbDirection{};

                if (gtb.Direction() == midi2::MidiGroupTerminalBlockDirection::Bidirectional)
                {
                    gtbDirection = L"Bidirectional";
                }
                else if (gtb.Direction() == midi2::MidiGroupTerminalBlockDirection::BlockInput)
                {
                    gtbDirection = L"Message Destination";
                }
                else if (gtb.Direction() == midi2::MidiGroupTerminalBlockDirection::BlockOutput)
                {
                    gtbDirection = L"Message Source";
                }

                OutputStringField(MIDIDIAG_FIELD_LABEL_GTB_DIRECTION, gtbDirection);
            }

            if (device.GetGroupTerminalBlocks().Size() > 0)
            {
                OutputBlankLine();
            }

            auto parent = device.GetParentDeviceInformation();

            if (parent != nullptr)
            {
                OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_ID, parent.Id());
                OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_NAME, parent.Name());
            }
            else
            {
                OutputError("Unable to find endpoint parent");
            }

            if (i != devices.Size() - 1)
            {
                OutputItemSeparator();
            }
        }
    }
    else
    {
        OutputError("Enumerating devices returned no matches. This is not expected and indicates an installation");
        OutputError("problem, the service couldn't start, or you are running developer service components and do");
        OutputError("not have developer mode set in Windows Settings.");
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
        OutputError("Enumerating WinRT MIDI 1.0 devices encountered an exception.");

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
        OutputError("Enumerating WinRT MIDI 1.0 devices encountered an exception.");

        return false;
    }

    return true;
}

void DisplayWinMMGetDevCapsErrorResult(MMRESULT result)
{
    switch (result)
    {
    case MMSYSERR_NOERROR:
        // don't display anything
        break;

    case MMSYSERR_NODRIVER:
        OutputError("Port returned MMSYSERR_NODRIVER. This is normal for unused port numbers.");
        break;

    case MMSYSERR_INVALPARAM:
        OutputError("Port returned MMSYSERR_INVALPARAM.");
        break;

    case MMSYSERR_BADDEVICEID:
        OutputError("Port returned MMSYSERR_BADDEVICEID.");
        break;

    case MMSYSERR_NOMEM:
        OutputError("Port returned MMSYSERR_NOMEM.");
        break;

    }
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
            MIDIINCAPSW inputCaps{};

            auto result = midiInGetDevCaps(i, &inputCaps, sizeof(inputCaps));

            OutputNumericField(MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_ID, i);
            OutputStringField(MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_NAME, std::wstring{ inputCaps.szPname });

            DisplayWinMMGetDevCapsErrorResult(result);

            if (result != MMSYSERR_NOERROR)
            {
                errorCount++;
            }

            if (i < inputDeviceCount - 1)
            {
                OutputItemSeparator();
            }
        }

        OutputItemSeparator();
        OutputNumericField(MIDIDIAG_FIELD_LABEL_WINMM_ERROR_COUNT, errorCount);

    }
    catch (...)
    {
        OutputError("Enumerating WinMM MIDI 1.0 input devices encountered an exception.");

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
            MIDIOUTCAPSW outputCaps{};

            auto result = midiOutGetDevCaps(i, &outputCaps, sizeof(outputCaps));

            OutputNumericField(MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_ID, i);
            OutputStringField(MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_NAME, std::wstring{ outputCaps.szPname });
            DisplayWinMMGetDevCapsErrorResult(result);

            if (result != MMSYSERR_NOERROR)
            {
                errorCount++;
            }

            if (i < outputDeviceCount - 1)
            {
                OutputItemSeparator();
            }
        }

        OutputItemSeparator();
        OutputNumericField(MIDIDIAG_FIELD_LABEL_WINMM_ERROR_COUNT, errorCount);

    }
    catch (...)
    {
        OutputError("Enumerating WinMM MIDI 1.0 output devices encountered an exception.");

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
                OutputError("Ping test failed");
                OutputStringField(MIDIDIAG_FIELD_LABEL_PING_FAILURE_REASON, pingResult.FailureReason());

                return false;
            }
        }
        else
        {
            OutputError("Ping test failed. Return was null.");
            return false;
        }
    }
    catch (...)
    {
        OutputError("Ping test failed with exception");

        return false;
    }


    return true;
}

bool DoSectionClock(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_MIDI_CLOCK);

    try
    {
        OutputTimestampField(MIDIDIAG_FIELD_LABEL_CLOCK_FREQUENCY, midi2::MidiClock::TimestampFrequency());
        OutputTimestampField(MIDIDIAG_FIELD_LABEL_CLOCK_NOW, midi2::MidiClock::Now());

        return true;
    }
    catch (winrt::hresult_error ex)
    {
        OutputError(winrt::to_string(ex.message()));
        OutputError("winrt::hresult_error getting clock information. Likely an issue with SDK activation.");

        return false;
    }
}




init::MidiDesktopAppSdkInitializer initializer{ };

bool DoSectionSdkStatus(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_SDK_STATUS);

    
    // next, try to init the SDK
    bool initialized = initializer.InitializeSdkRuntime();

    OutputBooleanField(MIDIDIAG_FIELD_LABEL_SDK_INITIALIZED, initialized);

    if (!initialized)
    {
        OutputError("Failed to initialize Windows MIDI Services SDK runtime. Is it installed?");
    }

    return initialized;
}

bool DoSectionServiceStatus(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_SERVICE_STATUS);

    // this needs to be done before any other service calls

    bool available = initializer.EnsureServiceAvailable();

    OutputBooleanField(MIDIDIAG_FIELD_LABEL_SERVICE_AVAILABLE, available);

    if (!available)
    {
        OutputError("Failed to start MIDI Service");
    }

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

#define ENV_PROCESSOR_ARCHITECTURE  "PROCESSOR_ARCHITECTURE"
#define ENV_PROCESSOR_IDENTIFIER    "PROCESSOR_IDENTIFIER"
#define ENV_PROCESSOR_LEVEL         "PROCESSOR_LEVEL"
#define ENV_PROCESSOR_REVISION      "PROCESSOR_REVISION"

#define ENV_PROCESSOR_ARCHITECTURE_WIDE  L"PROCESSOR_ARCHITECTURE"
#define ENV_PROCESSOR_IDENTIFIER_WIDE    L"PROCESSOR_IDENTIFIER"
#define ENV_PROCESSOR_LEVEL_WIDE         L"PROCESSOR_LEVEL"
#define ENV_PROCESSOR_REVISION_WIDE      L"PROCESSOR_REVISION"



void OutputProcessorEnvVariables()
{
    OutputStringField(ENV_PROCESSOR_ARCHITECTURE, std::wstring{ _wgetenv(ENV_PROCESSOR_ARCHITECTURE_WIDE)});
    OutputStringField(ENV_PROCESSOR_IDENTIFIER, std::wstring{ _wgetenv(ENV_PROCESSOR_IDENTIFIER_WIDE) });
    OutputStringField(ENV_PROCESSOR_LEVEL, std::wstring{ _wgetenv(ENV_PROCESSOR_LEVEL_WIDE) });
    OutputStringField(ENV_PROCESSOR_REVISION, std::wstring{ _wgetenv(ENV_PROCESSOR_REVISION_WIDE) });
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

void OutputProcessAndNativeMachine()
{
    USHORT processMachine{ 0 };
    USHORT nativeMachine{ 0 };

    HANDLE hProcess = ::GetCurrentProcess();

    if (hProcess)
    {
        auto worked = ::IsWow64Process2(hProcess, &processMachine, &nativeMachine);

        if (worked)
        {
            // if not running emulated, IsWow64Process2 returns machine unknown for the process.
            if ((processMachine == IMAGE_FILE_MACHINE_UNKNOWN || processMachine == IMAGE_FILE_MACHINE_ARM64) && nativeMachine == IMAGE_FILE_MACHINE_ARM64)
            {
                OutputStringField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_EMULATION, std::wstring{ L"Native Arm64 process on Arm64 PC (Not Emulated)" });
            }
            else if ((processMachine == IMAGE_FILE_MACHINE_UNKNOWN || processMachine == IMAGE_FILE_MACHINE_AMD64) && nativeMachine == IMAGE_FILE_MACHINE_AMD64)
            {
                OutputStringField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_EMULATION, std::wstring{ L"Native Intel/AMD x64 on x64 PC (Not Emulated)" });
            }
            else if (processMachine == IMAGE_FILE_MACHINE_AMD64 && nativeMachine == IMAGE_FILE_MACHINE_ARM64)
            {
                OutputStringField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_EMULATION, std::wstring{ L"Intel/AMD x64 process on Arm64 PC (Emulated)" });
            }
            else if (processMachine == IMAGE_FILE_MACHINE_ARM64 && nativeMachine == IMAGE_FILE_MACHINE_AMD64)
            {
                // not supported today, but here in case it is some day
                OutputStringField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_EMULATION, std::wstring{ L"Arm64 process on Intel/AMD x64 PC (Emulated)" });
            }
            else
            {
                OutputError("Unidentified process and/or machine architecture");
            }
        }
        else
        {
            OutputError("Unable to query process and machine architecture.");
        }

    }
}

bool DoSectionDevMode(_In_ bool verbose)
{
    // dev mode check

    OutputSectionHeader(MIDIDIAG_SECTION_DEV_MODE);

    wil::unique_hkey devModeKey{ };
    if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock", devModeKey)))
    {
        auto devModeEnabledValue = wil::reg::try_get_value_dword(devModeKey.get(), L"AllowDevelopmentWithoutDevLicense");

        if (devModeEnabledValue.has_value())
        {
            OutputBooleanField(MIDIDIAG_FIELD_LABEL_DEV_MODE_ENABLED, (bool)(devModeEnabledValue.value() > 0));
        }
        else
        {
            OutputBooleanField(MIDIDIAG_FIELD_LABEL_DEV_MODE_ENABLED, L"Value Not Present");
        }
    }
    else
    {
        OutputBooleanField(MIDIDIAG_FIELD_LABEL_DEV_MODE_ENABLED, L"Key Not Present");
    }


}


bool DoSectionSystemInfo(_In_ bool verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_OS);
    OutputStringField(MIDIDIAG_FIELD_LABEL_OS_VERSION, GetOSVersion());


    // if running under emulation on Arm64, this is going to return the emulated sys info
    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_PROCESSOR_ENV);
    OutputProcessorEnvVariables();

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_NATIVE_SYSTEM_INFO);

    SYSTEM_INFO sysinfoNative;
    ::GetNativeSystemInfo(&sysinfoNative);
    OutputSystemInfo(sysinfoNative);

    OutputItemSeparator();
    OutputProcessAndNativeMachine();
    OutputItemSeparator();

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

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_HEADER);

    OutputHeader("This application Copyright (c) 2024- Microsoft Corporation");
    OutputHeader("Information, license, and source available at https://aka.ms/midi");
    OutputBlankLine();
    OutputHeader(MIDIDIAG_PRODUCT_NAME);
    OutputBlankLine();
    OutputStringField(MIDIDIAG_HEADER_FIELD_LABEL_VERSION_BUILD_SOURCE, std::wstring{ WINDOWS_MIDI_SERVICES_BUILD_SOURCE });
    OutputStringField(MIDIDIAG_HEADER_FIELD_LABEL_VERSION_NAME, std::wstring{ WINDOWS_MIDI_SERVICES_BUILD_VERSION_NAME });
    OutputStringField(MIDIDIAG_HEADER_FIELD_LABEL_VERSION_FULL, std::wstring{ WINDOWS_MIDI_SERVICES_BUILD_VERSION_FULL });

    // this fails on Windows 10
//    OutputCurrentTime();

    try
    {
        // do anything which doesn't rely on the service or SDK
        DoSectionSystemInfo(verbose);

        DoSectionDevMode(verbose);

        // try to get all the classic MIDI 1.0 info up-front, so there's
        // some level of info available even if Windows MIDI Services is not installed

        DoSectionDrivers32RegistryEntries(verbose); // don't bail if fails

        DoSectionWinRTMidi1ApiEndpoints(verbose);  // we don't bail if this fails

        DoSectionWinMMMidi1ApiEndpoints(verbose);  // we don't bail if this fails

        DoSectionMidi2RegistryEntries(verbose);     // don't bail if fails

        // check the service and SDK installs. Do SDK first because
        // the service status uses the SDK initializer to try to start
        // the windows service and verify the presence of the SDK runtime
        if (!DoSectionSdkStatus(verbose)) goto abort_run;

        // we've made it past initializing the SDK, so make sure we shut it down
        // when we exit. The initializer also has a destructor to do this, but 
        // I like being explicit
        auto cleanup = wil::scope_exit([&]
            {
                initializer.ShutdownSdkRuntime();
            });


        if (!DoSectionServiceStatus(verbose)) goto abort_run;

        // only show midi clock info if the sdk init has worked
        if (midiClock)
        {
            if (!DoSectionClock(verbose)) goto abort_run;
        }

        auto transportsWorked = DoSectionTransports(verbose);

        if (transportsWorked)
        {
            if (!DoSectionMidi2ApiEndpoints(verbose)) goto abort_run;

            // ping the service
            if (pingTest)
            {
                const uint8_t pingCount = 10;

                if (!DoSectionPingTest(verbose, pingCount)) goto abort_run;
            }
        }
    }
    catch (...)
    {
        OutputError("Exception attempting to gather MIDI information.");
        OutputSectionHeader(MIDIDIAG_SECTION_LABEL_END_OF_FILE);

        RETURN_FAIL;
    }

    OutputSectionHeader("*** Successful Run ***");
    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_END_OF_FILE);

    RETURN_SUCCESS;

abort_run:
    OutputSectionHeader("Aborted Run");
    OutputError("Aborting MIDI Diag run due to failure(s).");
    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_END_OF_FILE);

    RETURN_FAIL;
}
