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
std::shared_ptr<MidiBluetoothEndpointEntry>
MidiEndpointTable::GetEndpointEntryForBluetoothAddress(uint64_t const bluetoothAddress) const noexcept
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingUInt64(bluetoothAddress, "address")
    );

    try
    {
        if (auto it = m_endpoints.find(bluetoothAddress); it != m_endpoints.end())
        {
            return it->second;
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


_Use_decl_annotations_
std::shared_ptr<MidiBluetoothEndpointEntry>
MidiEndpointTable::CreateAndAddNewEndpointEntry(
    MidiBluetoothDeviceDefinition definition,
    bt::BluetoothLEDevice device,
    gatt::GattDeviceService service
) noexcept
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingUInt64(definition.BluetoothAddress, "address")
    );

    try
    {
        MidiBluetoothEndpointEntry entry;

        entry.Definition = definition;
        entry.MidiDeviceBiDi = nullptr;
        entry.Device = device;
        entry.Service = service;

        m_endpoints[definition.BluetoothAddress] = std::make_shared<MidiBluetoothEndpointEntry>(entry);

        return m_endpoints[definition.BluetoothAddress];
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
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingUInt64(bluetoothAddress, "address")
    );

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
