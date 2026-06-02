// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"


bool MidiLegacyPortDeviceInformationTests::ClassSetup()
{

    StartWinRTMTA();
    return true;
}

bool MidiLegacyPortDeviceInformationTests::ClassCleanup() 
{
    ShutdownWinRT();
    return true;
}


winrt::hstring GetStringVersionOfPropertyValue(foundation::IInspectable const& value)
{
    if (value)
    {
        auto prop = value.as<foundation::IPropertyValue>();

        if (prop.Type() == foundation::PropertyType::String)
        {
            return prop.GetString();
        }
        else if (prop.Type() == foundation::PropertyType::Guid)
        {
            return winrt::to_hstring(prop.GetGuid());
        }
        else if (prop.Type() == foundation::PropertyType::UInt64)
        {
            return winrt::to_hstring(prop.GetUInt64());
        }
        else if (prop.Type() == foundation::PropertyType::UInt32)
        {
            return winrt::to_hstring(prop.GetUInt32());
        }
        else if (prop.Type() == foundation::PropertyType::UInt16)
        {
            return winrt::to_hstring(prop.GetUInt16());
        }
        else if (prop.Type() == foundation::PropertyType::UInt8)
        {
            return winrt::to_hstring(prop.GetUInt8());
        }
        else if (prop.Type() == foundation::PropertyType::Boolean)
        {
            return winrt::to_hstring(prop.GetBoolean());
        }
        else
        {
            return L"Unhandled type: " + winrt::to_hstring(static_cast<uint32_t>(prop.Type()));
        }
    }

    return L"(null)";
}

#pragma warning(disable : 4702)
void MidiLegacyPortDeviceInformationTests::TestCreateFromId()
{
    // use basic WinRT device support to find a MIDI Device id

    auto selectors = { winrt::Windows::Devices::Midi::MidiInPort::GetDeviceSelector(), winrt::Windows::Devices::Midi::MidiOutPort::GetDeviceSelector() };

    for (auto const& selector : selectors)
    {
        auto devices = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(selector).get();

        for (const auto& device : devices)
        {
            std::wcout << L"----- Checking device: " << device.Name().c_str() << std::endl;

            auto portInfo = winrt::Windows::Devices::Midi2::Enumeration::Legacy::MidiLegacyPortDeviceInformation::CreateFromPortDeviceId(device.Id());

            VERIFY_IS_NOT_NULL(portInfo);

            std::wcout << L"Name:        " << portInfo.Name().c_str() << std::endl;
            std::wcout << L"Id:          " << portInfo.PortDeviceId().c_str() << std::endl;
            std::wcout << L"Instance Id: " << portInfo.PortDeviceInstanceId().c_str() << std::endl;
            std::wcout << L"WinMM Port:  " << portInfo.WinMMPortNumber() << std::endl;
            std::wcout << L"Parent:      " << portInfo.ParentDeviceInstanceId().c_str() << std::endl;
            std::wcout << L"Assoc Ep:    " << portInfo.EndpointDeviceId().c_str() << std::endl;
            std::wcout << L"Group:       " << portInfo.Group().ToString().c_str() << std::endl;
            std::wcout << L"DevDrivIfId: " << portInfo.DriverDeviceInterfaceId().c_str() << std::endl;
            std::wcout << L"Flow:        " << (uint32_t)portInfo.PortFlow() << std::endl;
            std::wcout << L"Container:   " << winrt::to_hstring(portInfo.ContainerId()).c_str() << std::endl;

            //for (auto const& [key, value] : portInfo.Properties())
            //{
            //    auto val = GetStringVersionOfPropertyValue(value);

            //    std::wcout << L"  Property: " << key.c_str() << L" = " << val.c_str() << std::endl;
            //}

            // ensure we can get the associated endpoint device
       }
    }
}

void MidiLegacyPortDeviceInformationTests::TestWalkUpToParent()
{
    // use basic WinRT device support to find a MIDI Device id
    auto selectors = { winrt::Windows::Devices::Midi::MidiInPort::GetDeviceSelector(), winrt::Windows::Devices::Midi::MidiOutPort::GetDeviceSelector() };

    for (auto const& selector : selectors)
    {
        UNREFERENCED_PARAMETER(selector);
        auto devices = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(selector).get();

        for (const auto& device : devices)
        {
            std::wcout << L"----- Checking device: " << device.Name().c_str() << std::endl;

            auto portInfo = winrt::Windows::Devices::Midi2::Enumeration::Legacy::MidiLegacyPortDeviceInformation::CreateFromPortDeviceId(device.Id());
            VERIFY_IS_NOT_NULL(portInfo);

            std::wcout << L"Name:        " << portInfo.Name().c_str() << std::endl;
            std::wcout << L"Id:          " << portInfo.PortDeviceId().c_str() << std::endl;
            std::wcout << L"Flow:        " << (uint32_t)portInfo.PortFlow() << std::endl;
            std::wcout << L"Parent:      " << portInfo.ParentDeviceInstanceId().c_str() << std::endl;

            // gs synth has empty endpoint device id, as do any .drv-based ports
            if (!portInfo.EndpointDeviceId().empty())
            {
                auto endpointDevice = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(portInfo.EndpointDeviceId());
                VERIFY_IS_NOT_NULL(endpointDevice);

                std::wcout << L" - Endpoint Device:      " << endpointDevice.Name().c_str() << std::endl;
                std::wcout << L" - Endpoint Container:   " << winrt::to_hstring(endpointDevice.ContainerId()).c_str() << std::endl;
                //std::wcout << L" - Endpoint Parent:      " << winrt::to_hstring(endpointDevice.ContainerId()).c_str() << std::endl;
            }
            else
            {
                std::wcout << L"** NO ASSOCIATED MIDISRV ENDPOINT." << std::endl;
            }

            if (!portInfo.ParentDeviceInstanceId().empty())
            {
                auto parentDevice = MidiParentDeviceInformation::CreateFromId(portInfo.ParentDeviceInstanceId());
                VERIFY_IS_NOT_NULL(parentDevice);

                std::wcout << L" - Parent Device:      " << parentDevice.Name().c_str() << std::endl;
                std::wcout << L" - Parent Container:   " << winrt::to_hstring(parentDevice.ContainerId()).c_str() << std::endl;
                std::wcout << L" - Driver Inf Path:    " << parentDevice.DriverInfPath().c_str() << std::endl;
                std::wcout << L" - Driver Provider:    " << parentDevice.DriverProvider().c_str() << std::endl;
                std::wcout << L" - Driver Version:     " << parentDevice.DriverVersion().c_str() << std::endl;
                std::wcout << L" - Enumerator Name:    " << parentDevice.EnumeratorName().c_str() << std::endl;
                std::wcout << L" - Service Name:       " << parentDevice.ServiceName().c_str() << std::endl;
            }
            else
            {
                std::wcout << L"** NO PARENT." << std::endl;
            }



        }

    }

}



void MidiLegacyPortDeviceInformationTests::TestFindAll()
{
    auto ports = winrt::Windows::Devices::Midi2::Enumeration::Legacy::MidiLegacyPortDeviceInformation::FindAll();

    uint32_t sourcePortCount{ 0 };
    uint32_t destinationPortCount{ 0 };

    for (const auto& port : ports)
    {
        if (port.PortFlow() == Midi1PortFlow::MidiMessageSource)
        {
            std::wcout << L"Found Source: " << port.Name().c_str() << std::endl;
            sourcePortCount++;
        }
        else if (port.PortFlow() == Midi1PortFlow::MidiMessageDestination)
        {
            std::wcout << L"Found Destination: " << port.Name().c_str() << std::endl;
            destinationPortCount++;
        }
    }

    std::cout << "Count Source Ports:      " << sourcePortCount << std::endl;
    std::cout << "Count Destination Ports: " << destinationPortCount << std::endl;

    // this will fail for .drv-based ports, so we can't do this check. Additionally, BLE MIDI 1 ports 
    // used in WinRT MIDI 1.0 will show in the above, but not in the winmm counts.

    //auto winmmInputCount = midiInGetNumDevs();
    //auto winmmOutputCount = midiOutGetNumDevs();

    //VERIFY_ARE_EQUAL(winmmInputCount, sourcePortCount);
    //VERIFY_ARE_EQUAL(winmmOutputCount, destinationPortCount);


    // at a bare minimum, we'll have the GS synth
    VERIFY_IS_TRUE(destinationPortCount > 0);

}

void MidiLegacyPortDeviceInformationTests::TestFindAllInputs()
{
}

void MidiLegacyPortDeviceInformationTests::TestFindAllOutputs()
{
}

void MidiLegacyPortDeviceInformationTests::TestFindAllForEndpoint()
{

}