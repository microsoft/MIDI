// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiInputEndpointConnection.h"
#include "MidiInputEndpointConnection.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    IFACEMETHODIMP MidiInputEndpointConnection::Callback(_In_ PVOID /*Data*/, _In_ UINT /*Size*/, _In_ LONGLONG /*Position*/)
    {
        return S_OK;
    }

    _Success_(return == true)
    bool MidiInputEndpointConnection::InternalStart()
    {
        throw hresult_not_implemented();
    }

}
