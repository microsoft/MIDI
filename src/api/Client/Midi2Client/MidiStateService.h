// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
class MidiStateService
{
public:

    // returns the json for function blocks
    std::wstring GetFunctionBlockEntriesForEndpoint(_In_ std::wstring endpointDeviceId);
    bool UpdateFunctionBlockEntriesForEndpoint(_In_ std::wstring endpointDeviceId, _In_ std::wstring functionBlocksJson);

    // other cache types, MUID table, etc.



private:
};

