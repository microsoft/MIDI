// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. Endpoint properties for an rtpMIDI connection. Adapted from the Bluetooth MIDI
// transport, which has the same shape: one MIDI 1.0 byte stream in each direction.
// ============================================================================

#pragma once

namespace RtpMidiEndpointProperties
{
    // rtpMIDI carries a single MIDI 1.0 byte stream with no notion of groups, so the transport
    // declares one group terminal block in each direction on group 1. Without them the service has
    // nothing to build MIDI 1.0 ports from.
    //
    // The two buffers are owned by the caller because the DEVPROPERTY entries point into them and
    // must stay valid until the device manager call returns.
    inline HRESULT BuildMidi1PortProperties(
        _In_ std::wstring const& portName,
        _In_opt_ std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties> const customProperties,
        _Inout_ std::vector<std::byte>& groupTerminalBlockData,
        _Inout_ WindowsMidiServicesNamingLib::MidiEndpointNameTable& nameTable,
        _Inout_ std::vector<DEVPROPERTY>& properties) noexcept
    {
        try
        {
            std::vector<internal::GroupTerminalBlockInternal> blocks{};

            // block numbers are 1-based, group indexes are 0-based
            internal::GroupTerminalBlockInternal destinationBlock{};
            destinationBlock.Number = 1;
            destinationBlock.Direction = MIDI_GROUP_TERMINAL_BLOCK_INPUT;    // MIDI Out from the user's perspective
            destinationBlock.FirstGroupIndex = 0;
            destinationBlock.GroupCount = 1;
            destinationBlock.Protocol = 0x01;                                // MIDI_1_0_UP_TO_64_BITS
            destinationBlock.Name = portName;
            blocks.push_back(destinationBlock);

            internal::GroupTerminalBlockInternal sourceBlock{};
            sourceBlock.Number = 2;
            sourceBlock.Direction = MIDI_GROUP_TERMINAL_BLOCK_OUTPUT;        // MIDI In from the user's perspective
            sourceBlock.FirstGroupIndex = 0;
            sourceBlock.GroupCount = 1;
            sourceBlock.Protocol = 0x01;
            sourceBlock.Name = portName;
            blocks.push_back(sourceBlock);

            groupTerminalBlockData.clear();
            RETURN_HR_IF(E_FAIL, !internal::WriteGroupTerminalBlocksToPropertyDataPointer(blocks, groupTerminalBlockData));

            properties.push_back({ { PKEY_MIDI_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_BINARY, static_cast<ULONG>(groupTerminalBlockData.size()), (PVOID)groupTerminalBlockData.data() });

            // The port name the service ends up using comes from this table, not from the blocks
            RETURN_IF_FAILED(nameTable.PopulateAllEntriesForMidi1DeviceUsingUmpDriver(portName, blocks));

            // A name given to an individual port outranks the endpoint name
            if (customProperties != nullptr)
            {
                for (auto const& source : customProperties->Midi1Sources)
                {
                    nameTable.UpdateSourceEntryCustomName(source.second.GroupIndex, source.second.Name);
                }

                for (auto const& destination : customProperties->Midi1Destinations)
                {
                    nameTable.UpdateDestinationEntryCustomName(destination.second.GroupIndex, destination.second.Name);
                }
            }

            if (Feature_Servicing_MIDI2PortNamingRework::IsEnabled())
            {
                nameTable.SetPortNamesHaveLegacyEquivalent(false);

                LOG_IF_FAILED(nameTable.RebuildNewStyleNames(std::wstring{ portName }, false));
            }

            RETURN_IF_FAILED(nameTable.WriteProperties(properties));

            return S_OK;
        }
        CATCH_RETURN();
    }
}
