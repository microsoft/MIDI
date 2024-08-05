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

int __cdecl main()
{
    winrt::init_apartment();

    std::cout << dye::grey("===================================================================") << std::endl;
    std::cout << dye::aqua(" Enumerating kernel streaming devices to discover MIDI pins") << std::endl;
    std::cout << dye::aqua(" Typically, these are USB, but other KS drivers will be included") << std::endl;
    std::cout << dye::grey("-------------------------------------------------------------------") << std::endl;
    std::cout << dye::aqua(" If the MIDI service is running when you run this utility, some") << std::endl;
    std::cout << dye::aqua(" devices may not report all pin properties. It is recommended") << std::endl;
    std::cout << dye::aqua(" that you stop the midisrv service before running this tool.") << std::endl;
    std::cout << dye::grey("===================================================================") << std::endl;

    // enumerate all in-scope devices
    winrt::hstring deviceSelector(
        L"System.Devices.InterfaceClassGuid:=\"{6994AD04-93EF-11D0-A3CC-00A0C9223196}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True");

    auto allDevices = DeviceInformation::FindAllAsync(deviceSelector).get();

    if (allDevices != nullptr && allDevices.Size() > 0)
    {
        for (auto const& device : allDevices)
        {
            std::cout << std::endl;

            wil::unique_handle hFilter;
            std::string deviceName;
            std::wstring deviceId;
            std::wstring deviceInstanceId;
            ULONG cPins{ 0 };

            auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

            auto properties = device.Properties();

            // retrieve the device instance id from the DeviceInformation property store
            auto prop = properties.Lookup(winrt::to_hstring(L"System.Devices.DeviceInstanceId"));
            RETURN_HR_IF_NULL(E_INVALIDARG, prop);
            deviceInstanceId = winrt::unbox_value<winrt::hstring>(prop).c_str();
            deviceId = device.Id().c_str();

            // Get the parent device name so it doesn't show as just the PC name
            auto parentDeviceInfo = DeviceInformation::CreateFromIdAsync(deviceInstanceId,
                additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::Device).get();
            deviceName = winrt::to_string(parentDeviceInfo.Name());

            WriteBrightLabel("Filter Name");
            std::cout << dye::light_yellow(winrt::to_string(device.Name())) << std::endl;
            WriteBrightLabel("Filter Id");
            std::cout << dye::aqua(winrt::to_string(device.Id())) << std::endl;

            WriteBrightLabel("Parent Name");
            std::cout << dye::aqua(deviceName) << std::endl;
            WriteBrightLabel("Parent Id");
            std::cout << dye::aqua(winrt::to_string(parentDeviceInfo.Id())) << std::endl;

            // instantiate the interface
            if (FAILED(FilterInstantiate(deviceId.c_str(), &hFilter)))
            {
                std::cout << " - Failed to instantiate filter." << std::endl;

                continue;
            }

            if (FAILED(PinPropertySimple(hFilter.get(), 0, KSPROPSETID_Pin, KSPROPERTY_PIN_CTYPES, &cPins, sizeof(cPins))))
            {
                std::cout << " - Failed to get pin count." << std::endl;

                continue;
            }

            // Enumerate all the pins

            WriteLabel("Pin Count");
            std::cout << cPins << std::endl;

            for (UINT i = 0; i < cPins; i++)
            {
                std::cout << std::endl;

                bool isMidiPin{ false };

                wil::unique_handle hPin;
                KSPIN_DATAFLOW dataFlow = (KSPIN_DATAFLOW)0;
                KSPIN_COMMUNICATION communication = (KSPIN_COMMUNICATION)0;
                GUID nativeDataFormat{ 0 };

                std::cout << dye::yellow("Pin ") << dye::aqua(i) << std::endl;

                WriteLabel(" - Communication");

                if (FAILED(PinPropertySimple(hFilter.get(), i, KSPROPSETID_Pin, KSPROPERTY_PIN_COMMUNICATION, &communication, sizeof(KSPIN_COMMUNICATION))))
                {
                    std::cout << "Failed to get pin communication property." << std::endl;
                    continue;
                }

                if (communication == KSPIN_COMMUNICATION_NONE)
                {
                    std::cout << "None" << std::endl;
                    continue;
                }
                else if (communication == KSPIN_COMMUNICATION_SINK)
                {
                    std::cout << "Sink" << std::endl;
                }
                else if (communication == KSPIN_COMMUNICATION_SOURCE)
                {
                    std::cout << "Source" << std::endl;
                }
                else if (communication == KSPIN_COMMUNICATION_BOTH)
                {
                    std::cout << "Source and Sink" << std::endl;
                }
                else if (communication == KSPIN_COMMUNICATION_BRIDGE)
                {
                    std::cout << "Bridge" << std::endl;
                    continue;
                }
                else
                {
                    std::cout << "Unknown (" << communication << ")" << std::endl;
                    continue;
                }


                // try to instantiate as UMP
                if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), i, MidiTransport_CyclicUMP, &hPin)))
                {
                    isMidiPin = true;

                    WriteLabel(" - Communication Format");
                    std::cout << "UMP cyclic format" << std::endl;

                    auto hr = PinPropertySimple(hPin.get(),
                        i,
                        KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
                        KSPROPERTY_MIDI2_NATIVEDATAFORMAT,
                        &nativeDataFormat,
                        sizeof(nativeDataFormat));

                    WriteLabel(" - Native Data Format");

                    if (SUCCEEDED(hr) || hr == HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND))
                    {
                        if (nativeDataFormat == KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET)
                        {
                            std::cout << "Universal MIDI Packet (UMP)" << std::endl;
                        }
                        else if (nativeDataFormat == KSDATAFORMAT_SUBTYPE_MIDI)
                        {
                            std::cout << "MIDI 1.0 Data Format (bytes)" << std::endl;
                        }
                        else
                        {
                            std::cout << "Unknown" << std::endl;
                        }
                    }
                    else
                    {
                        std::cout << dye::red("Unable to get native data format") << std::endl;
                    }
                }

                hPin.reset();

                if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), i, MidiTransport_StandardByteStream, &hPin)))
                {
                    isMidiPin = true;

                    std::unique_ptr<WCHAR> pinNameData;
                    ULONG pinNameDataSize{ 0 };

                    WriteLabel(" - Communication Format");
                    std::cout << "MIDI 1.0 byte format" << std::endl;

                    auto pinNameHR = PinPropertyAllocate(
                            hFilter.get(),
                            i,
                            KSPROPSETID_Pin,
                            KSPROPERTY_PIN_NAME,
                            (PVOID*)&pinNameData,
                            &pinNameDataSize);

                    if (SUCCEEDED(pinNameHR) || pinNameHR == HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND))
                    {
                        WriteLabel(" - Pin Name");

                        // Check to see if the pin has an iJack name
                        if (pinNameDataSize > 0)
                        {
                            std::wstring pinName{ pinNameData.get() };
                            std::wcout << pinName << std::endl;
                        }
                        else
                        {
                            // no pin name provided
                            std::cout << "Not provided" << std::endl;
                        }
                    }
                }

                hPin.reset();


                if (isMidiPin)
                {

                    auto dataFlowHR = PinPropertySimple(
                        hFilter.get(),
                        i,
                        KSPROPSETID_Pin,
                        KSPROPERTY_PIN_DATAFLOW,
                        &dataFlow,
                        sizeof(KSPIN_DATAFLOW)
                    );

                    WriteLabel(" - Data Flow");

                    if (SUCCEEDED(dataFlowHR))
                    {
                        if (dataFlow == KSPIN_DATAFLOW_IN)
                        {
                            std::cout << "KSPIN_DATAFLOW_IN " << dye::light_purple("(MIDI Out from PC)") << std::endl;

                        }
                        else if (dataFlow == KSPIN_DATAFLOW_OUT)
                        {
                            std::cout << "KSPIN_DATAFLOW_OUT " << dye::light_green("(MIDI In to PC)") << std::endl;

                        }
                        else
                        {
                            std::cout << "Unknown" << std::endl;
                        }
                    }
                    else
                    {
                        std::cout << dye::red("Failed to get data flow") << std::endl;
                    }
                }

            }

        }
    }
    else
    {
        std::cout << std::endl << dye::red("No compatible kernel streaming devices found") << std::endl;
    }

    std::cout << std::endl;

    return 0;
}

