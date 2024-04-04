// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"


MidiEndpointTable::MidiEndpointTable() = default;
MidiEndpointTable::~MidiEndpointTable() = default;

MidiEndpointTable& MidiEndpointTable::Current()
{
    // explanation: http://www.modernescpp.com/index.php/thread-safe-initialization-of-data/

    static MidiEndpointTable current;

    return current;
}




_Use_decl_annotations_
MidiBluetoothEndpointEntry*
MidiEndpointTable::GetEndpointEntryForBluetoothAddress(_In_ uint64_t const bluetoothAddress) const noexcept
{
    try
    {
        if (auto it = m_endpoints.find(bluetoothAddress); it != m_endpoints.end())
        {
            return it->second.get();
        }
        else
        {
            return nullptr;
        }
    }
    catch (...)
    {
        return nullptr;
    }
}



//_Use_decl_annotations_
//wil::com_ptr_nothrow<IMidiBiDi>
//MidiEndpointTable::GetEndpointInterfaceForId(
//    std::wstring const EndpointDeviceId
//) const noexcept
//{
//    try
//    {
//        auto result = m_endpoints.find(EndpointDeviceId);
//
//        if (result != m_endpoints.end())
//            return result->second.MidiDeviceBiDi;
//        else
//            return nullptr;
//    }
//    catch (...)
//    {
//        return nullptr;
//    }
//}


_Use_decl_annotations_
MidiBluetoothEndpointEntry*
MidiEndpointTable::CreateAndAddNewEndpointEntry(
    MidiBluetoothDeviceDefinition definition,
    bt::BluetoothLEDevice device,
    gatt::GattDeviceService service
) noexcept
{
    try
    {
        MidiBluetoothEndpointEntry entry;

        entry.Definition = definition;
        entry.MidiDeviceBiDi = nullptr;
        entry.Device = device;
        entry.Service = service;

        m_endpoints[definition.BluetoothAddress] = std::make_shared<MidiBluetoothEndpointEntry>(entry);

        return m_endpoints[definition.BluetoothAddress].get();
    }
    catch (...)
    {
        return nullptr;
    }
}




_Use_decl_annotations_
void
MidiEndpointTable::RemoveEndpointEntry(
    uint64_t bluetoothAddress
) noexcept
{
    try
    {
        auto result = m_endpoints.find(bluetoothAddress);

        if (result != m_endpoints.end())
        {
            m_endpoints.erase(result);
        }
    }
    catch (...)
    {

    }
}
