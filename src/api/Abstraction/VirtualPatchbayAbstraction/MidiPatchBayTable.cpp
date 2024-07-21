// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"


MidiPatchBayTable::MidiPatchBayTable() = default;
MidiPatchBayTable::~MidiPatchBayTable() = default;

MidiPatchBayTable& MidiPatchBayTable::Current()
{
    // explanation: http://www.modernescpp.com/index.php/thread-safe-initialization-of-data/

    static MidiPatchBayTable current;

    return current;
}


_Use_decl_annotations_
MidiVirtualPatchBayEntry* MidiPatchBayTable::GetEntryForId(std::wstring const associationId) const noexcept
{
    try
    {
        auto result = m_patchBayEntries.find(associationId);

        if (result != m_patchBayEntries.end())
            return (MidiVirtualPatchBayEntry*)(&(result->second));
        else
            return nullptr;
    }
    catch (...)
    {
        return nullptr;
    }
}


_Use_decl_annotations_
void MidiPatchBayTable::RemoveEntry(std::wstring const associationId) noexcept
{
    try
    {
        auto result = m_patchBayEntries.find(associationId);

        if (result != m_patchBayEntries.end())
        {
            m_patchBayEntries.erase(result);
        }
    }
    catch (...)
    {

    }
}
