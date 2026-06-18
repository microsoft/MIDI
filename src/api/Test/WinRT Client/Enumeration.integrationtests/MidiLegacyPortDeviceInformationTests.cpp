// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"


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

//#pragma warning(disable : 4702)
void MidiLegacyPortDeviceInformationTests::TestCreateFromId()
{
    // use basic WinRT device support to find a MIDI Device id

    auto selectors = { winrt::Windows::Devices::Midi::MidiInPort::GetDeviceSelector(), winrt::Windows::Devices::Midi::MidiOutPort::GetDeviceSelector() };

    for (auto const& selector : selectors)
    {
        auto devices = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(selector).get();

        for (const auto& device : devices)
        {
            std::wcout << L"\n----- Checking device: " << device.Name().c_str() << std::endl;

            auto portInfo = winrt::Windows::Devices::Midi2::Enumeration::Legacy::MidiLegacyPortDeviceInformation::CreateFromPortDeviceId(device.Id());

            VERIFY_IS_NOT_NULL(portInfo);

            std::wcout << L"Name:        " << portInfo.Name().c_str() << std::endl;
            std::wcout << L"Id:          " << portInfo.PortDeviceId().c_str() << std::endl;
            std::wcout << L"Instance Id: " << portInfo.PortDeviceInstanceId().c_str() << std::endl;
            std::wcout << L"WinMM Port:  " << portInfo.Number() << std::endl;
            std::wcout << L"Parent:      " << portInfo.ParentDeviceInstanceId().c_str() << std::endl;
            std::wcout << L"Assoc Ep:    " << portInfo.EndpointDeviceId().c_str() << std::endl;
            std::wcout << L"Group:       " << portInfo.Group().ToString().c_str() << std::endl;
            std::wcout << L"DevDrivIfId: " << portInfo.DriverDeviceInterfaceId().c_str() << std::endl;
            std::wcout << L"Flow:        " << (uint32_t)portInfo.Flow() << std::endl;
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
            std::wcout << L"\n---------------------------------------- Checking MIDI 1 port: " << device.Name().c_str() << std::endl;

            auto portInfo = winrt::Windows::Devices::Midi2::Enumeration::Legacy::MidiLegacyPortDeviceInformation::CreateFromPortDeviceId(device.Id());
            VERIFY_IS_NOT_NULL(portInfo);

            std::wcout << L" - Port Name:            " << portInfo.Name().c_str() << std::endl;
            std::wcout << L" - Id:                   " << portInfo.PortDeviceId().c_str() << std::endl;
            std::wcout << L" - Flow:                 " << (portInfo.Flow() == Midi1PortFlow::MidiMessageSource ? L"Source/Input" : L"Destination/Output") << std::endl;
            std::wcout << L" - Number:               " << std::setw(0) << std::dec << portInfo.Number() << std::endl;
            std::wcout << L" - Parent Id:            " << portInfo.ParentDeviceInstanceId().c_str() << std::endl;
            std::wcout << L" - DrivDevIface:         " << portInfo.DriverDeviceInterfaceId().c_str() << std::endl << std::endl;

            // gs synth has empty endpoint device id, as do any .drv-based ports
            std::wcout << L" Related UMP Endpoint -----------------------------------" << std::endl;
            if (!portInfo.EndpointDeviceId().empty())
            {
                auto endpointDevice = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(portInfo.EndpointDeviceId());
                VERIFY_IS_NOT_NULL(endpointDevice);

                std::wcout << L" - Name:                 " << endpointDevice.Name().c_str() << std::endl;
                std::wcout << L" - Id:                   " << endpointDevice.EndpointDeviceId().c_str() << std::endl;
                std::wcout << L" - Container:            " << winrt::to_hstring(endpointDevice.ContainerId()).c_str() << std::endl;
                std::wcout << L" - Parent Id:            " << endpointDevice.ParentDeviceInstanceId().c_str() << std::endl;
            }
            else
            {
                std::wcout << L"** NO ASSOCIATED MIDISRV ENDPOINT." << std::endl;
            }

            std::wcout << L" Port's Parent Device -----------------------------------" << std::endl;

            if (!portInfo.ParentDeviceInstanceId().empty())
            {
                auto parentDevice = portInfo.GetParentDeviceInformation();
                VERIFY_IS_NOT_NULL(parentDevice);

                std::wcout << L" - This Parent Id:       " << parentDevice.Id().c_str() << std::endl;
                std::wcout << L" - Media Driver Parent:  " << parentDevice.RelatedParentMediaDriverDeviceInstanceId().c_str() << std::endl;
                std::wcout << L" - Name:                 " << parentDevice.Name().c_str() << std::endl;
                std::wcout << L" - Container:            " << winrt::to_hstring(parentDevice.ContainerId()).c_str() << std::endl;
                std::wcout << L" - Driver Inf Path:      " << parentDevice.DriverInfPath().c_str() << std::endl;
                std::wcout << L" - Driver Provider:      " << parentDevice.DriverProvider().c_str() << std::endl;
                std::wcout << L" - Driver Version:       " << parentDevice.DriverVersion().c_str() << std::endl;
                std::wcout << L" - Enumerator Name:      " << parentDevice.EnumeratorName().c_str() << std::endl;
                std::wcout << L" - Service Name:         " << parentDevice.ServiceName().c_str() << std::endl;
                std::wcout << L" - USB VID:              " << std::setw(4) << std::setfill(L'0') << std::hex << parentDevice.UsbVendorId() << std::endl;
                std::wcout << L" - USB PID:              " << std::setw(4) << std::setfill(L'0') << std::hex << parentDevice.UsbProductId() << std::endl;
                std::wcout << L" - USB Serial Number:    " << parentDevice.UsbSerialNumber().c_str() << std::endl;
                std::wcout << L" - Parent Device Id:     " << parentDevice.ParentDeviceInstanceId().c_str() << std::endl;
                std::wcout << L" - Reported Ids Hash:    " << std::setw(8) << std::setfill(L'0') << std::hex << parentDevice.ReportedDeviceIdsHash() << std::endl;
            }
            else
            {
                std::wcout << L"** NO PARENT." << std::endl;
            }



        }

    }

}


void MidiLegacyPortDeviceInformationTests::TestFindAllForName()
{
    auto devices = winrt::Windows::Devices::Midi2::Enumeration::Legacy::MidiLegacyPortDeviceInformation::FindAll();

    for (auto const& device : devices)
    {
        std::wcout << L"Port: " << device.Name().c_str() << L" Id: " << device.PortDeviceId().c_str() << std::endl;

        auto start = std::chrono::high_resolution_clock::now();
        auto ports = winrt::Windows::Devices::Midi2::Enumeration::Legacy::MidiLegacyPortDeviceInformation::FindAllForName(device.Name());
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::wcout << L"Found " << ports.Size() << L" legacy ports. (Time taken: " << duration << L" ms)" << std::endl;
        VERIFY_IS_TRUE(ports.Size() > 0);

        for (auto const& port: ports)
        {
            if (port.Flow() == winrt::Windows::Devices::Midi2::Enumeration::Midi1PortFlow::MidiMessageDestination)
            {
                std::wcout << L" -- Destination: " << port.PortDeviceId() << std::endl;
            }
            else
            {
                std::wcout << L" -- Source:      " << port.PortDeviceId() << std::endl;
            }
        }

        std::wcout << std::endl;

    }
}


void MidiLegacyPortDeviceInformationTests::TestFindAll()
{
    auto start = std::chrono::high_resolution_clock::now();

    auto ports = winrt::Windows::Devices::Midi2::Enumeration::Legacy::MidiLegacyPortDeviceInformation::FindAll();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::wcout << L"Found " << ports.Size() << L" legacy ports. (Time taken: " << duration << L" ms)" << std::endl;


    uint32_t sourcePortCount{ 0 };
    uint32_t destinationPortCount{ 0 };

    for (const auto& port : ports)
    {
        if (port.Flow() == Midi1PortFlow::MidiMessageSource)
        {
            std::wcout << L"Found Source: " << port.Name().c_str() << std::endl;
            sourcePortCount++;
        }
        else if (port.Flow() == Midi1PortFlow::MidiMessageDestination)
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
    auto start = std::chrono::high_resolution_clock::now();

    auto ports = winrt::Windows::Devices::Midi2::Enumeration::Legacy::MidiLegacyPortDeviceInformation::FindAll(Midi1PortFlow::MidiMessageSource);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::wcout << L"Found " << ports.Size() << L" legacy ports. (Time taken: " << duration << L" ms)" << std::endl;


    for (const auto& port : ports)
    {
        VERIFY_IS_TRUE(port.Flow() == Midi1PortFlow::MidiMessageSource);
    }
}

void MidiLegacyPortDeviceInformationTests::TestFindAllOutputs()
{
    auto start = std::chrono::high_resolution_clock::now();

    auto ports = winrt::Windows::Devices::Midi2::Enumeration::Legacy::MidiLegacyPortDeviceInformation::FindAll(Midi1PortFlow::MidiMessageDestination);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::wcout << L"Found " << ports.Size() << L" legacy ports. (Time taken: " << duration << L" ms)" << std::endl;


    for (const auto& port : ports)
    {
        VERIFY_IS_TRUE(port.Flow() == Midi1PortFlow::MidiMessageDestination);
    }
}

void MidiLegacyPortDeviceInformationTests::TestFindAllForEndpoint()
{
    // find some endpoints

    auto eps = winrt::Windows::Devices::Midi2::Enumeration::MidiEndpointDeviceInformation::FindAll();

    for (auto const& ep: eps)
    {
        // find the child ports for each endpoint. 

        auto start = std::chrono::high_resolution_clock::now();

        auto ports = winrt::Windows::Devices::Midi2::Enumeration::Legacy::MidiLegacyPortDeviceInformation::FindAllForEndpoint(ep.EndpointDeviceId());

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::wcout << L"Endpoint: " << ep.Name().c_str() << L" has " << ports.Size() << L" legacy ports. (Time taken: " << duration << L" ms)" << std::endl;

        for (auto const& port : ports)
        {
            std::wcout << L" - Port: " << port.Name().c_str() << L" Flow: " << (uint32_t)port.Flow() << std::endl;
        }
    }

}


//void MidiLegacyPortDeviceInformationTests::TestFindAllForParentId()
//{
//    // build up a list of parent ids
//    std::set<std::wstring> parentIds;
//    auto devices = winrt::Windows::Devices::Midi2::Enumeration::Legacy::MidiLegacyPortDeviceInformation::FindAll();
//
//    for (auto const& device: devices)
//    {
//        parentIds.insert(std::wstring{ device.ParentDeviceInstanceId().c_str() });
//    }
//
//
//    for (auto const& parentId : parentIds)
//    {
//        // find the child ports for each parent ID. 
//
//        auto start = std::chrono::high_resolution_clock::now();
//        auto ports = winrt::Windows::Devices::Midi2::Enumeration::Legacy::MidiLegacyPortDeviceInformation::FindAllForParentDevice(parentId);
//        auto end = std::chrono::high_resolution_clock::now();
//        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
//
//        std::wcout << L"Parent ID: " << parentId.c_str() << L" has " << ports.Size() << L" legacy ports. (Time taken: " << duration << L" ms)" << std::endl;
//
//        // we already know there are ports for this parent, so it needs to return more than 0
//        VERIFY_IS_TRUE(ports.Size() > 0);
//
//        for (auto const& port : ports)
//        {
//            std::wcout << L" - Port: " << port.Name().c_str() << L" Flow: " << (uint32_t)port.PortFlow() << std::endl;
//        }
//    }
//
//}