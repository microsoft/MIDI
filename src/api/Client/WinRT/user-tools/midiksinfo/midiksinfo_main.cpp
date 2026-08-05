// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

#include "console_tools_shared.h"

void WriteBrightLabel(std::wstring const& label)
{
    auto fullLabel = label + L":";
    fmt::print(L"{:<25}", fmt::styled(label, fmt::fg(fmt::color::white)));
}

void WriteLabel(std::wstring const& label)
{
    auto fullLabel = label + L":";
    fmt::print(L"{:<25}", fmt::styled(label, fmt::fg(fmt::color::gray)));
}





winrt::hstring 
GetStringProperty(_In_ DeviceInformation const& di, _In_ winrt::hstring const& propertyName, _In_ winrt::hstring const& defaultValue)
{
    auto prop = di.Properties().Lookup(propertyName);

    if (prop == nullptr)
    {
        return defaultValue;
    }

    auto value = winrt::unbox_value<winrt::hstring>(prop);

    if (value.empty())
    {
        return defaultValue;
    }

    return value;
}


HRESULT
GetKSDriverSuppliedName(_In_ HANDLE hInstantiatedFilter, _Inout_ std::wstring& name)
{
    // get the name GUID

    KSCOMPONENTID componentId{};
    KSPROPERTY prop{};
    ULONG countBytesReturned{};

    prop.Id = KSPROPERTY_GENERAL_COMPONENTID;
    prop.Set = KSPROPSETID_General;
    prop.Flags = KSPROPERTY_TYPE_GET;

    auto hrComponent = SyncIoctl(
        hInstantiatedFilter,
        IOCTL_KS_PROPERTY,
        &prop,
        sizeof(KSPROPERTY),
        &componentId,
        sizeof(KSCOMPONENTID),
        &countBytesReturned
    );

    if (SUCCEEDED(hrComponent))
    {
        if (componentId.Name != GUID_NULL)
        {
            // we have the GUID where this name is stored, so get the driver-supplied name from the registry

            WCHAR nameFromRegistry[MAX_PATH]{ 0 };   // this should only be MAXPNAMELEN, but if someone tampered with it, could be larger, hence MAX_PATH

            std::wstring regKey = L"SYSTEM\\CurrentControlSet\\Control\\MediaCategories\\" + internal::GuidToString(componentId.Name);

            if (SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_LOCAL_MACHINE, regKey.c_str(), L"Name", nameFromRegistry)))
            {
                name = std::wstring(nameFromRegistry) + internal::ResourceGetWString(IDS_LABEL_REGISTRY_NAME_SOURCE_PREFIX) + regKey + L")";
                return S_OK;
            }
        }
    }
    else
    {
        RETURN_IF_FAILED(hrComponent);
    }

    return E_NOTFOUND;
}


struct MidiKsPinInformation
{
    uint32_t Number;
    std::wstring Name;
    MidiDataFormats DataFormat{ MidiDataFormats::MidiDataFormats_Invalid };
    KSPIN_DATAFLOW PinFlow{ };
};

struct MidiKsFilterInformation
{
    std::wstring Id;
    std::wstring Name;
    std::wstring NameFromRegistry;

    std::vector<MidiKsPinInformation> Pins;
};

struct MidiKsDeviceInformation
{
    std::wstring Name;
    std::wstring DeviceInstanceId;

    bool IsMidi1Device{ false };
    bool IsMidi2Device{ false };

    std::vector<MidiKsFilterInformation> Filters;
};


std::vector<MidiKsDeviceInformation> m_devices{ };






void DisplayMidiDevices()
{
    WriteBlankLine();

    if (m_devices.size() == 0)
    {
        WriteErrorLine(internal::ResourceGetWString(IDS_ERROR_NO_DEVICES_FOUND));

        return;
    }


    for (auto const& device : m_devices)
    {
        uint16_t indent{ 0 };

        fmt::println(L"{}{} {}", 
            std::wstring(indent, L' '),
            fmt::styled(internal::ResourceGetWString(IDS_LABEL_DEVICE_NAME), darkLabelTextStyle),
            fmt::styled(device.Name, highlightTextStyle)
        );

        fmt::println(L"{}{} {}",
            std::wstring(indent, L' '),
            fmt::styled(internal::ResourceGetWString(IDS_LABEL_INSTANCE_ID), darkLabelTextStyle),
            fmt::styled(device.DeviceInstanceId, fmt::fg(fmt::color::golden_rod))
        );

        // we list all the filters once in a short format, to make it easier to read for some devices
        // and then we list each filter and its pins. Only do this if there's more than one filter.

        if (device.Filters.size() > 1)
        {
            uint16_t FilterNameColumnWidth{ 0 };
            uint16_t FilterIdColumnWidth{ 0 };

            for (auto const& filter : device.Filters)
            {
                FilterNameColumnWidth = static_cast<uint16_t>(max(FilterNameColumnWidth, filter.Name.length() + 1));
                FilterIdColumnWidth = static_cast<uint16_t>(max(FilterIdColumnWidth, filter.Id.length() + 1));
            }

            bool firstFilter{ true };
            for (auto const& filter : device.Filters)
            {
                indent = 5;

                if (firstFilter)
                {
                    WriteBlankLine();

                    fmt::println(L"{}{} {} {} {} {} {}",
                        std::wstring(indent, L' '),
                        fmt::styled(internal::ResourceGetWString(IDS_LABEL_DEVICE), infoTextStyle),
                        fmt::styled(device.Name, highlightTextStyle),
                        fmt::styled(internal::ResourceGetWString(IDS_LABEL_INCLUDES), infoTextStyle),
                        fmt::styled(device.Filters.size(), highlightTextStyle),
                        fmt::styled(internal::ResourceGetWString(device.Filters.size() == 1 ? IDS_LABEL_FILTER_SINGULAR : IDS_LABEL_FILTER_PLURAL), infoTextStyle),
                        fmt::styled(internal::ResourceGetWString(IDS_LABEL_WITH_MIDI_FORMAT_PINS), infoTextStyle)
                        );

                    WriteBlankLine();

                    // header row

                    fmt::println(L"{}{:<{}}{:<{}}",
                        std::wstring(indent, L' '),
                        fmt::styled(internal::ResourceGetWString(IDS_HEADER_NAME), darkLabelTextStyle), FilterNameColumnWidth,
                        fmt::styled(internal::ResourceGetWString(IDS_HEADER_INSTANCE_ID), darkLabelTextStyle), FilterIdColumnWidth
                    );

                    fmt::println(L"{}{:<{}}{:<{}}",
                        std::wstring(indent, L' '),
                        fmt::styled(std::wstring(FilterNameColumnWidth - 1, L'-'), darkLabelTextStyle), FilterNameColumnWidth,
                        fmt::styled(std::wstring(FilterIdColumnWidth - 1, L'-'), darkLabelTextStyle), FilterIdColumnWidth
                    );

                    firstFilter = false;
                }


                fmt::println(L"{}{:<{}}{:<{}}",
                    std::wstring(indent, L' '),
                    fmt::styled(filter.Name, highlightTextStyle), FilterNameColumnWidth,
                    fmt::styled(filter.Id, filterIdFieldValueTextStyle), FilterIdColumnWidth
                );

            }
        }

        // now we list all filters and their pins in more detail

        bool firstFilter{ true };
        for (auto const& filter : device.Filters)
        {
            indent = 5;

            if (firstFilter)
            {
                WriteBlankLine();

                fmt::println(L"{}{}",
                    std::wstring(indent, L' '),
                    fmt::styled(internal::ResourceGetWString(IDS_LABEL_FILTER_DETAILS_HEADER), infoTextStyle)
                );

                WriteBlankLine();

                firstFilter = false;
            }


            fmt::println(L"{}{:<19} {}",
                std::wstring(indent, L' '),
                fmt::styled(internal::ResourceGetWString(IDS_LABEL_FILTER_ID), darkLabelTextStyle),
                fmt::styled(filter.Id, filterIdFieldValueTextStyle)
            );

            fmt::println(L"{}{:<19} {}",
                std::wstring(indent, L' '),
                fmt::styled(internal::ResourceGetWString(IDS_LABEL_FILTER_NAME), darkLabelTextStyle),
                fmt::styled(filter.Name, highlightTextStyle)
            );


            std::wstring nameFromRegistry{};

            if (filter.NameFromRegistry.empty())
            {
                nameFromRegistry = internal::ResourceGetWString(IDS_VALUE_REGISTRY_NAME_NOT_PROVIDED);
            }
            else
            {
                nameFromRegistry = filter.NameFromRegistry;
            }

            fmt::println(L"{}{:<19} {}",
                std::wstring(indent, L' '),
                fmt::styled(internal::ResourceGetWString(IDS_LABEL_NAME_FROM_REGISTRY), darkLabelTextStyle),
                fmt::styled(nameFromRegistry, fmt::fg(filter.NameFromRegistry.empty() ? fmt::color::dark_slate_gray : fmt::color::aqua))
            );


            bool firstPin = true;
            for (auto const& pin : filter.Pins)
            {
                indent = 10;
                const uint16_t PinIndexColumnWidth = 6;
                const uint16_t PinDataFormatColumnWidth = 20;
                const uint16_t PinDataFlowColumnWidth = 22;
                const uint16_t PinDataFlowExplanationColumnWidth = 22;
                const uint16_t PinNameColumnWidth = 32;

                if (firstPin)
                {
                    WriteBlankLine();

                    fmt::println(L"{}{} {} {} {} {} {}",
                        std::wstring(indent, L' '),
                        fmt::styled(internal::ResourceGetWString(IDS_LABEL_FILTER), infoTextStyle),
                        fmt::styled(filter.Name, highlightTextStyle),
                        fmt::styled(internal::ResourceGetWString(IDS_LABEL_INCLUDES), infoTextStyle),
                        fmt::styled(filter.Pins.size(), highlightTextStyle),
                        fmt::styled(internal::ResourceGetWString(IDS_LABEL_MIDI_FORMAT), infoTextStyle),
                        fmt::styled(internal::ResourceGetWString(filter.Pins.size() == 1 ? IDS_LABEL_PIN_SINGULAR : IDS_LABEL_PIN_PLURAL), infoTextStyle)
                    );

                    WriteBlankLine();

                    // header row

                    fmt::println(L"{}{:<{}}{:<{}}{:<{}}{:<{}}{:<{}}",
                        std::wstring(indent, L' '),
                        fmt::styled(internal::ResourceGetWString(IDS_HEADER_PIN_INDEX), fmt::fg(fmt::color::gray)), PinIndexColumnWidth,
                        fmt::styled(internal::ResourceGetWString(IDS_HEADER_PIN_DATA_FORMAT), fmt::fg(fmt::color::gray)), PinDataFormatColumnWidth,
                        fmt::styled(internal::ResourceGetWString(IDS_HEADER_PIN_DATA_FLOW), fmt::fg(fmt::color::gray)), PinDataFlowColumnWidth,
                        fmt::styled(internal::ResourceGetWString(IDS_HEADER_PIN_PORT_TYPE), fmt::fg(fmt::color::gray)), PinDataFlowExplanationColumnWidth,
                        fmt::styled(internal::ResourceGetWString(IDS_HEADER_PIN_NAME), fmt::fg(fmt::color::gray)), PinNameColumnWidth
                    );

                    fmt::println(L"{}{:<{}}{:<{}}{:<{}}{:<{}}{:<{}}",
                        std::wstring(indent, L' '),
                        fmt::styled(std::wstring(PinIndexColumnWidth - 1, L'-'), fmt::fg(fmt::color::gray)), PinIndexColumnWidth,
                        fmt::styled(std::wstring(PinDataFormatColumnWidth - 1, L'-'), fmt::fg(fmt::color::gray)), PinDataFormatColumnWidth,
                        fmt::styled(std::wstring(PinDataFlowColumnWidth - 1, L'-'), fmt::fg(fmt::color::gray)), PinDataFlowColumnWidth,
                        fmt::styled(std::wstring(PinDataFlowExplanationColumnWidth - 1, L'-'), fmt::fg(fmt::color::gray)), PinDataFlowExplanationColumnWidth,
                        fmt::styled(std::wstring(PinNameColumnWidth - 1, L'-'), fmt::fg(fmt::color::gray)), PinNameColumnWidth
                    );

                    firstPin = false;
                }

                fmt::print(L"{}{:<{}}",
                    std::wstring(indent, L' '),
                    fmt::styled(pin.Number, fmt::fg(fmt::color::golden_rod)), PinIndexColumnWidth
                );

                if (WI_AreAllFlagsSet(pin.DataFormat,MidiDataFormats::MidiDataFormats_ByteStream))
                {
                    fmt::print(L"{:<{}}", fmt::styled(internal::ResourceGetWString(IDS_VALUE_MIDI1_BYTE_FORMAT), fmt::fg(fmt::color::light_gray)), PinDataFormatColumnWidth);
                }
                else if (WI_AreAllFlagsSet(pin.DataFormat, MidiDataFormats::MidiDataFormats_UMP))
                {
                    fmt::print(L"{:<{}}", fmt::styled(internal::ResourceGetWString(IDS_VALUE_MIDI2_UMP_FORMAT), fmt::fg(fmt::color::light_gray)), PinDataFormatColumnWidth);
                }


                std::wstring stringDataFlowExplanation{};

                if (pin.PinFlow == KSPIN_DATAFLOW_IN)
                {
                    fmt::print(L"{:<{}}", fmt::styled(internal::ResourceGetWString(IDS_VALUE_MESSAGE_DESTINATION), infoTextStyle), PinDataFlowColumnWidth);
                    fmt::print(L"{:<{}}", fmt::styled(internal::ResourceGetWString(IDS_VALUE_MIDI_OUTPUT_FROM_PC), infoTextStyle), PinDataFlowExplanationColumnWidth);
                }
                else if (pin.PinFlow == KSPIN_DATAFLOW_OUT)
                {
                    fmt::print(L"{:<{}}", fmt::styled(internal::ResourceGetWString(IDS_VALUE_MESSAGE_SOURCE), fmt::fg(fmt::color::medium_purple)), PinDataFlowColumnWidth);
                    fmt::print(L"{:<{}}", fmt::styled(internal::ResourceGetWString(IDS_VALUE_MIDI_INPUT_TO_PC), fmt::fg(fmt::color::medium_purple)), PinDataFlowExplanationColumnWidth);
                }


                if (pin.Name.empty())
                {
                    fmt::println(L"{:<{}}", fmt::styled(internal::ResourceGetWString(IDS_VALUE_PIN_NAME_NOT_PROVIDED), fmt::fg(fmt::color::gray)), PinNameColumnWidth);
                }
                else
                {
                    fmt::println(L"{:<{}}", fmt::styled(pin.Name, highlightTextStyle), PinNameColumnWidth);
                }
            }

            if (filter.Pins.size() > 0)
            {
                WriteBlankLine();
            }

        }

        WriteBlankLine();
        fmt::println(L"{}", fmt::styled(std::wstring(LINE_LENGTH, L'='), fmt::fg(fmt::color::gray)));

    }

    WriteBlankLine();
    fmt::println(L"{}", fmt::styled(internal::ResourceGetWString(IDS_STATUS_END_OF_INFORMATION), fmt::fg(fmt::color::golden_rod)));
    WriteBlankLine();

}


int __cdecl main()
{
    if (!TrySetConsoleTextMode())
    {
        return RETURN_ERROR_SETTING_CONSOLE_MODE;
    }

    winrt::init_apartment();

    WriteDoubleSeparatorLine();
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_TOOL_INFO));
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_COPYRIGHT));
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_INFO_URL));
    WriteDoubleSeparatorLine();
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_DESCRIPTION_1));
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_DESCRIPTION_2));
    WriteSingleSeparatorLine();
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_SERVICE_RUNNING_WARNING));
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_SERVICE_STOP_REQUIRED));
    WriteBlankLine();

    fmt::print(L"{}", fmt::styled(internal::ResourceGetWString(IDS_BANNER_SERVICE_STOP_USE), infoTextStyle));
    fmt::print(L"{}", fmt::styled(L"midi service stop", fmt::fg(fmt::color::light_green)));
    fmt::print(L"{}", fmt::styled(internal::ResourceGetWString(IDS_BANNER_SERVICE_STOP_OR), infoTextStyle));
    fmt::print(L"{}", fmt::styled(L"net stop midisrv", fmt::fg(fmt::color::light_green)));
    fmt::println(L"{}", fmt::styled(internal::ResourceGetWString(IDS_BANNER_SERVICE_STOP_ADMIN_PROMPT), infoTextStyle));
    
    WriteDoubleSeparatorLine();


    // {4d36e96c-e325-11ce-bfc1-08002be10318} is the MEDIA class guid
    winrt::hstring mediaDeviceSelector(
        L"System.Devices.ClassGuid:=\"{4d36e96c-e325-11ce-bfc1-08002be10318}\" AND " \
        L"System.Devices.Present:=System.StructuredQueryType.Boolean#True");

    auto mediaDevices = DeviceInformation::FindAllAsync(mediaDeviceSelector, nullptr, DeviceInformationKind::Device).get();

    if (mediaDevices.Size() > 0)
    {
        for (auto const& parentDevice : mediaDevices)
        {
            bool isMidi1Device{ false };
            bool isMidi2Device{ false };

            auto deviceInstanceId = GetStringProperty(parentDevice, L"System.Devices.DeviceInstanceId", L"");

            if (deviceInstanceId.empty())
            {
                // this shouldn't happen
                continue;
            }

            MidiKsDeviceInformation deviceInfo{};

            deviceInfo.DeviceInstanceId = deviceInstanceId;
            deviceInfo.Name = parentDevice.Name();

            // enumerate all KS_CATEGORY_AUDIO filters for this parent media device
            winrt::hstring filterDeviceSelector(
                L"System.Devices.InterfaceClassGuid:=\"{6994AD04-93EF-11D0-A3CC-00A0C9223196}\""\
                L" AND System.Devices.InterfaceEnabled:= System.StructuredQueryType.Boolean#True"\
                L" AND System.Devices.DeviceInstanceId:= \"" + deviceInstanceId + L"\"");

            auto filterDevices = DeviceInformation::FindAllAsync(filterDeviceSelector).get();

            if (filterDevices.Size() > 0)
            {
                for (auto const& filterDevice : filterDevices)
                {
                    MidiKsFilterInformation filterInfo{};

                    filterInfo.Id = filterDevice.Id();
                    filterInfo.Name = filterDevice.Name();

                    bool isMidi1Filter{ false };
                    bool isMidi2Filter{ false };

                    // instantiate the filter and then enumerate the pins

                    wil::unique_handle hFilter;
                    if (FAILED(FilterInstantiate(filterDevice.Id().c_str(), &hFilter)))
                    {
                        // can't instantiate the filter
                        continue;
                    }

                    ULONG cPins{ 0 };
                    if (FAILED(PinPropertySimple(hFilter.get(), 0, KSPROPSETID_Pin, KSPROPERTY_PIN_CTYPES, &cPins, sizeof(cPins))))
                    {
                        // couldn't get pin info
                        continue;
                    }

                    for (UINT pinIndex = 0; pinIndex < cPins; pinIndex++)
                    {
                        bool isMidi1Pin{ false };
                        bool isMidi2Pin{ false };
                        wil::unique_handle hPin;


                        if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), pinIndex, MidiTransport_CyclicUMP, &hPin)))
                        {
                            // MIDI 2 pin
                            isMidi2Pin = true;
                            isMidi2Filter = true;
                            isMidi2Device = true;

                            MidiKsPinInformation pinInfo{};
                            pinInfo.Number = pinIndex;
                            pinInfo.DataFormat = MidiDataFormats::MidiDataFormats_UMP;

                            std::unique_ptr<WCHAR> pinNameData;
                            ULONG pinNameDataSize{ 0 };

                            auto pinNameHR = PinPropertyAllocate(
                                hFilter.get(),
                                pinIndex,
                                KSPROPSETID_Pin,
                                KSPROPERTY_PIN_NAME,
                                (PVOID*)&pinNameData,
                                &pinNameDataSize
                            );

                            if (SUCCEEDED(pinNameHR) || pinNameHR == HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND))
                            {
                                // Check to see if the pin has an iJack name
                                if (pinNameDataSize > 0)
                                {
                                    std::wstring pinName{ pinNameData.get() };
                                    pinInfo.Name = pinName;
                                }
                            }

                            pinNameData.reset();

                            KSPIN_DATAFLOW dataFlow = (KSPIN_DATAFLOW)0;

                            auto dataFlowHR = PinPropertySimple(
                                hFilter.get(),
                                pinIndex,
                                KSPROPSETID_Pin,
                                KSPROPERTY_PIN_DATAFLOW,
                                &dataFlow,
                                sizeof(KSPIN_DATAFLOW)
                            );

                            if (SUCCEEDED(dataFlowHR))
                            {
                                pinInfo.PinFlow = dataFlow;
                            }

                            if (isMidi2Pin)
                            {
                                filterInfo.Pins.push_back(pinInfo);
                            }

                        }
                        else if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), pinIndex, MidiTransport_StandardByteStream, &hPin)))
                        {
                            // MIDI 1 pin

                            MidiKsPinInformation pinInfo{};
                            pinInfo.Number = pinIndex;
                            pinInfo.DataFormat = MidiDataFormats::MidiDataFormats_ByteStream;

                            isMidi1Pin = true;
                            isMidi1Filter = true;
                            isMidi1Device = true;

                            std::unique_ptr<WCHAR> pinNameData;
                            ULONG pinNameDataSize{ 0 };

                            auto pinNameHR = PinPropertyAllocate(
                                hFilter.get(),
                                pinIndex,
                                KSPROPSETID_Pin,
                                KSPROPERTY_PIN_NAME,
                                (PVOID*)&pinNameData,
                                &pinNameDataSize
                            );

                            if (SUCCEEDED(pinNameHR) || pinNameHR == HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND))
                            {
                                // Check to see if the pin has an iJack name
                                if (pinNameDataSize > 0)
                                {
                                    std::wstring pinName{ pinNameData.get() };
                                    pinInfo.Name = pinName;
                                }
                            }

                            pinNameData.reset();

                            KSPIN_DATAFLOW dataFlow = (KSPIN_DATAFLOW)0;

                            auto dataFlowHR = PinPropertySimple(
                                hFilter.get(),
                                pinIndex,
                                KSPROPSETID_Pin,
                                KSPROPERTY_PIN_DATAFLOW,
                                &dataFlow,
                                sizeof(KSPIN_DATAFLOW)
                            );

                            if (SUCCEEDED(dataFlowHR))
                            {
                                pinInfo.PinFlow = dataFlow;
                            }

                            if (isMidi1Pin)
                            {
                                filterInfo.Pins.push_back(pinInfo);
                            }
                        }

                        hPin.reset();
                    }


                    // get the name that the device reported during installation. This is often empty

                    std::wstring nameFromDriver{};
                    auto driverNameHR = GetKSDriverSuppliedName(hFilter.get(), nameFromDriver);

                    if (SUCCEEDED(driverNameHR))
                    {
                        filterInfo.NameFromRegistry = nameFromDriver;
                    }

                    if (isMidi1Filter || isMidi2Filter)
                    {
                        deviceInfo.Filters.push_back(filterInfo);
                    }

                    hFilter.reset();
                }
            }

            if (isMidi1Device || isMidi2Device)
            {
                deviceInfo.IsMidi1Device = isMidi1Device;
                deviceInfo.IsMidi2Device = isMidi2Device;

                m_devices.push_back(deviceInfo);
            }
        }
    }


    DisplayMidiDevices();

    return 0;
}

