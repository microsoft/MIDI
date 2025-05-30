// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

#pragma warning(push)
#pragma warning(disable: 4244)
#include "color.hpp"
#pragma warning(pop)

void WriteBrightLabel(std::string label)
{
    auto fullLabel = label + ":";
    std::cout << std::left << std::setw(25) << std::setfill(' ') << dye::white(fullLabel);
}

void WriteLabel(std::string label)
{
    auto fullLabel = label + ":";
    std::cout << std::left << std::setw(25) << std::setfill(' ') << dye::grey(fullLabel);
}





winrt::hstring 
GetStringProperty(_In_ DeviceInformation di, _In_ winrt::hstring propertyName, _In_ winrt::hstring defaultValue)
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

#define LINE_LENGTH 131


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
                name = std::wstring(nameFromRegistry) + L" (From: " + regKey + L")";
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
    std::cout << std::endl;

    if (m_devices.size() == 0)
    {
        std::cout
            << dye::light_red("No devices with MIDI pins found. This can happen if all devices are MIDI 2.0 and the service is running.")
            << std::endl;

        return;
    }


    for (auto const& device : m_devices)
    {
        uint16_t indent{ 0 };

        std::cout
            << std::string(indent, ' ')
            << dye::grey("Device Name")
            << " "
            << dye::light_aqua(winrt::to_string(device.Name)) 
            << std::endl;

        std::cout
            << std::string(indent, ' ')
            << dye::grey("Instance Id")
            << " "
            << dye::yellow(winrt::to_string(device.DeviceInstanceId)) 
            << std::endl;

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
                    std::cout << std::endl;

                    std::cout
                        << std::string(indent, ' ')
                        << dye::aqua("Device")
                        << " "
                        << dye::light_aqua(winrt::to_string(device.Name))
                        << " "
                        << dye::aqua("includes")
                        << " "
                        << dye::light_aqua(device.Filters.size())
                        << " "
                        << dye::aqua(device.Filters.size() == 1 ? "filter" : "filters")
                        << " "
                        << dye::aqua("with MIDI Format Pins")
                        << std::endl;

                    std::cout << std::endl;

                    // header row

                    std::cout
                        << std::string(indent, ' ')
                        << std::setw(FilterNameColumnWidth) << std::left << dye::grey("Name")
                        << std::setw(FilterIdColumnWidth) << std::left << dye::grey("Instance Id")
                        << std::endl;

                    std::cout
                        << std::string(indent, ' ')
                        << std::setw(FilterNameColumnWidth) << std::left << dye::grey(std::string(FilterNameColumnWidth - 1, '-'))
                        << std::setw(FilterIdColumnWidth) << std::left << dye::grey(std::string(FilterIdColumnWidth - 1, '-'))
                        << std::endl;

                    firstFilter = false;
                }


                std::cout
                    << std::string(indent, ' ')
                    << std::setw(FilterNameColumnWidth) << std::left << dye::light_aqua(winrt::to_string(filter.Name))
                    << std::setw(FilterIdColumnWidth) << std::left << dye::yellow(winrt::to_string(filter.Id))
                    << std::endl;

            }
        }

        // now we list all filters and their pins in more detail

        bool firstFilter{ true };
        for (auto const& filter : device.Filters)
        {
            indent = 5;

            if (firstFilter)
            {
                std::cout << std::endl;

                std::cout
                    << std::string(indent, ' ')
                    << dye::aqua("Details and MIDI Pins for each Filter")
                    << std::endl;

                std::cout << std::endl;

                firstFilter = false;
            }


            std::cout
                << std::string(indent, ' ') 
                << std::setw(19) << std::left
                << dye::grey("Filter Id")
                << " "
                << dye::yellow(winrt::to_string(filter.Id)) 
                << std::endl;

            std::cout
                << std::string(indent, ' ')
                << std::setw(19) << std::left
                << dye::grey("Filter Name")
                << " "
                << dye::light_aqua(winrt::to_string(filter.Name)) 
                << std::endl;


            std::string nameFromRegistry{};

            if (filter.NameFromRegistry.empty())
            {
                nameFromRegistry = "(Not provided. This is normal for MIDI 2.0 drivers and common with some devices using MIDI 1.0 drivers.)";
            }
            else
            {
                nameFromRegistry = winrt::to_string(filter.NameFromRegistry);
            }

            std::cout
                << std::string(indent, ' ')
                << std::setw(19) << std::left
                << dye::grey("Name from Registry")
                << " "
                << (filter.NameFromRegistry.empty() ? dye::grey(nameFromRegistry) : dye::light_aqua(nameFromRegistry))
                << std::endl;


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
                    std::cout << std::endl;

                    std::cout
                        << std::string(indent, ' ')
                        << dye::aqua("Filter")
                        << " "
                        << dye::light_aqua(winrt::to_string(filter.Name))
                        << " "
                        << dye::aqua("includes")
                        << " "
                        << dye::light_aqua(filter.Pins.size())
                        << " "
                        << dye::aqua("MIDI Format")
                        << " "
                        << dye::aqua(filter.Pins.size() == 1 ? "pin" : "pins")
                        << std::endl;

                    std::cout << std::endl;

                    // header row

                    std::cout
                        << std::string(indent, ' ')
                        << std::setw(PinIndexColumnWidth) << std::left << dye::grey("Index")
                        << std::setw(PinDataFormatColumnWidth) << std::left << dye::grey("Data Format")
                        << std::setw(PinDataFlowColumnWidth) << std::left << dye::grey("Data Flow")
                        << std::setw(PinDataFlowExplanationColumnWidth) << std::left << dye::grey("Port Type")
                        << std::setw(PinNameColumnWidth) << std::left << dye::grey("Pin Name (MIDI 1 drivers only)")
                        << std::endl;

                    std::cout
                        << std::string(indent, ' ')
                        << std::setw(PinIndexColumnWidth) << std::left << dye::grey(std::string(PinIndexColumnWidth-1, '-'))
                        << std::setw(PinDataFormatColumnWidth) << std::left << dye::grey(std::string(PinDataFormatColumnWidth - 1, '-'))
                        << std::setw(PinDataFlowColumnWidth) << std::left << dye::grey(std::string(PinDataFlowColumnWidth-1, '-'))
                        << std::setw(PinDataFlowExplanationColumnWidth) << std::left << dye::grey(std::string(PinDataFlowExplanationColumnWidth - 1, '-'))
                        << std::setw(PinNameColumnWidth) << std::left << dye::grey(std::string(PinNameColumnWidth-1, '-'))
                        << std::endl;

                    firstPin = false;
                }

                std::cout
                    << std::string(indent, ' ')
                    << std::setw(PinIndexColumnWidth) << std::left << dye::yellow(pin.Number);
                    
                if (WI_AreAllFlagsSet(pin.DataFormat,MidiDataFormats::MidiDataFormats_ByteStream))
                {
                    std::cout << std::setw(PinDataFormatColumnWidth) << std::left << dye::white("MIDI 1 byte format");
                }
                else if (WI_AreAllFlagsSet(pin.DataFormat, MidiDataFormats::MidiDataFormats_UMP))
                {
                    std::cout << std::setw(PinDataFormatColumnWidth) << std::left << dye::white("MIDI 2 UMP format");
                }


                std::wstring stringDataFlowExplanation{};

                if (pin.PinFlow == KSPIN_DATAFLOW_IN)
                {
                    std::cout << std::setw(PinDataFlowColumnWidth) << std::left << dye::aqua("Message Destination");
                    std::cout << std::setw(PinDataFlowExplanationColumnWidth) << std::left << dye::aqua("MIDI Output from PC");
                }
                else if (pin.PinFlow == KSPIN_DATAFLOW_OUT)
                {
                    std::cout << std::setw(PinDataFlowColumnWidth) << std::left << dye::light_purple("Message Source");
                    std::cout << std::setw(PinDataFlowExplanationColumnWidth) << std::left << dye::light_purple ("MIDI Input to PC");
                }


                if (pin.Name.empty())
                {
                    std::cout
                        << std::setw(PinNameColumnWidth) << std::left << dye::grey("(not provided)")
                        << std::endl;
                }
                else
                {
                    std::cout
                        << std::setw(PinNameColumnWidth) << std::left << dye::light_aqua(winrt::to_string(pin.Name))
                        << std::endl;
                }
            }

            if (filter.Pins.size() > 0)
            {
                std::cout << std::endl;
            }

        }

        std::cout << std::endl;
        std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;

    }

    std::cout << std::endl;
    std::cout << dye::yellow("-- End of Information --") << std::endl << std::endl;

}


int __cdecl main()
{
    winrt::init_apartment();

    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
    std::cout << dye::aqua(" This tool is part of the Windows MIDI Services SDK and tools") << std::endl;
    std::cout << dye::aqua(" Copyright 2025- Microsoft Corporation.") << std::endl;
    std::cout << dye::aqua(" Information, license, and source available at https ://aka.ms/midi") << std::endl;
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
    std::cout << dye::aqua(" Enumerating MIDI kernel streaming devices, using the MIDI class drivers or a third-party MIDI 1.0/2.0 driver.") << std::endl;
    std::cout << dye::aqua(" Typically, these devices are USB, but other KS drivers will be included in the enumeration.") << std::endl;
    std::cout << dye::grey(std::string(LINE_LENGTH, '-')) << std::endl;
    std::cout << dye::aqua(" If the MIDI service is running when you run this utility, some devices may not report all pin properties because they are in-use.") << std::endl;
    std::cout << dye::aqua(" To see MIDI 2.0 devices, you must stop midisrv (the MIDI service) before running this.") << std::endl << std::endl;
    std::cout << dye::aqua(" Use ");
    std::cout << dye::light_green("midi service stop");
    std::cout << dye::aqua(" or ");
    std::cout << dye::light_green("net stop midisrv");
    std::cout << dye::aqua(" from an Administrator command prompt to stop the service.") << std::endl;

    std::cout << dye::grey(std::string(LINE_LENGTH, '-')) << std::endl;

    std::cout
        << " "
        << dye::aqua(winrt::to_string(WINDOWS_MIDI_SERVICES_NUGET_BUILD_VERSION_NAME))
        << dye::grey(" (")
        << dye::aqua(winrt::to_string(WINDOWS_MIDI_SERVICES_NUGET_BUILD_SOURCE))
        << dye::grey(")")
        << " -- "
        << dye::aqua(winrt::to_string(WINDOWS_MIDI_SERVICES_NUGET_BUILD_VERSION_FULL))
        << std::endl;

    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;


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

