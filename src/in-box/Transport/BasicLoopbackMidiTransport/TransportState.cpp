// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"


TransportState::TransportState() = default;
TransportState::~TransportState() = default;

TransportState& TransportState::Current()
{
    // explanation: http://www.modernescpp.com/index.php/thread-safe-initialization-of-data/

    static TransportState current;

    return current;
}



HRESULT
TransportState::ConstructEndpointManager()
{
    auto lock = m_stateLock.lock_exclusive();

    // Two activations can race here. Whichever is first publishes, and the other uses that one.
    if (m_endpointManager == nullptr)
    {
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2BasicLoopbackMidiEndpointManager>(&m_endpointManager));
    }

    return S_OK;
}


HRESULT
TransportState::ConstructConfigurationManager()
{
    auto lock = m_stateLock.lock_exclusive();

    if (m_configurationManager == nullptr)
    {
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2BasicLoopbackMidiConfigurationManager>(&m_configurationManager));
    }

    return S_OK;
}
