// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


// thread-safe meyers singleton for storing the devices we'll use


class MidiPatchBayTable
{
public:
    static MidiPatchBayTable& Current();

    // no copying
    MidiPatchBayTable(_In_ const MidiPatchBayTable&) = delete;
    MidiPatchBayTable& operator=(_In_ const MidiPatchBayTable&) = delete;


    CMidi2VirtualPatchBayRoutingEntry* GetEntryForId(_In_ std::wstring const associationId) const noexcept;
    void RemoveEntry(_In_ std::wstring const associationId) noexcept;

private:
    MidiPatchBayTable();
    ~MidiPatchBayTable();

    // key is the association Id
    std::map<std::wstring, CMidi2VirtualPatchBayRoutingEntry> m_patchBayEntries;
};