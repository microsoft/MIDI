// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI_ENDPOINT_NAME_TABLE_H
#define MIDI_ENDPOINT_NAME_TABLE_H

#include <winrt/Windows.Foundation.h>

#include "WindowsMidiServices.h"    // for MidiFlow

#include "midi_naming.h"

class MidiEndpointNameTable
{
public:
    MidiEndpointNameTable() = default;

    static std::shared_ptr<MidiEndpointNameTable> FromEndpointDeviceId(_In_ winrt::hstring const& endpointDeviceId);
    static std::shared_ptr<MidiEndpointNameTable> FromPropertyData(_In_ winrt::com_array<uint8_t> const& propertyData);

    std::shared_ptr<WindowsMidiServicesInternal::Midi1PortNaming::Midi1PortNameEntry> GetEntry(
        _In_ uint8_t const groupIndex,
        _In_ MidiFlow const portFlow) const;

    bool SetEntry(_In_ std::shared_ptr<WindowsMidiServicesInternal::Midi1PortNaming::Midi1PortNameEntry> const newEntry);

    bool UpdateCustomName(_In_ uint8_t const groupIndex, _In_ MidiFlow const flow, _In_ winrt::hstring const& name);


    bool WriteProperties(_In_ std::vector<DEVPROPERTY>& destination);

private:
    std::vector<std::byte> m_nameTablePropertyData{}; // need to retain this here for property writing

    std::map<uint8_t, std::shared_ptr<WindowsMidiServicesInternal::Midi1PortNaming::Midi1PortNameEntry>> m_sourceEntries{};
    std::map<uint8_t, std::shared_ptr<WindowsMidiServicesInternal::Midi1PortNaming::Midi1PortNameEntry>> m_destinationEntries{};

};



#endif