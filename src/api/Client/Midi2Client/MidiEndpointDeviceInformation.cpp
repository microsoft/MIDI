// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointDeviceInformation.h"
#include "MidiEndpointDeviceInformation.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    collections::IVectorView<winrt::hstring> MidiEndpointDeviceInformation::GetAdditionalPropertiesList() noexcept
    {
        // TODO

        return nullptr;
    }


    _Use_decl_annotations_
    midi2::MidiEndpointDeviceInformation MidiEndpointDeviceInformation::CreateFromId(
        winrt::hstring const& id) noexcept
    {
        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

        // group terminal blocks - USB only
        additionalProperties.Append(STRING_PKEY_MIDI_IN_GroupTerminalBlocks);
        additionalProperties.Append(STRING_PKEY_MIDI_OUT_GroupTerminalBlocks);

        // bytestream or UMP (GUID right now. Gary may change)
        additionalProperties.Append(STRING_PKEY_MIDI_NativeDataFormat);


        auto di = Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
            id, additionalProperties).get();

        if (di != nullptr)
        {
            auto endpointInfo = winrt::make_self<MidiEndpointDeviceInformation>();

            endpointInfo->InternalUpdateFromDeviceInformation(di);

            return *endpointInfo;

        }
        else
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    void MidiEndpointDeviceInformation::InternalUpdateFromDeviceInformation(
        Windows::Devices::Enumeration::DeviceInformation const& info) noexcept
    {
        if (info != nullptr)
        {
            auto nativeDataFormat = info.Properties().Lookup(STRING_PKEY_MIDI_NativeDataFormat);

            auto terminalIn = info.Properties().Lookup(STRING_PKEY_MIDI_IN_GroupTerminalBlocks);
            auto terminalOut = info.Properties().Lookup(STRING_PKEY_MIDI_OUT_GroupTerminalBlocks);

            // group terminal blocks are a set of structures

            // populate group terminal blocks
            //m_groupTerminalBlocks.Append();


            // todo: other properties

            // standard

            m_deviceInformation = info;
            m_id = info.Id();
            m_name = info.Name();   // todo: fold in from settings
            m_transportSuppliedName = info.Name();
            //m_parentDeviceId = info.

        }
    }

    _Use_decl_annotations_
    void MidiEndpointDeviceInformation::InternalUpdate(
        winrt::hstring const& /*deviceId*/) noexcept
    {

    }


}
