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
    wil::com_ptr<CMidi2BasicLoopbackMidiEndpointManager> endpointManager;
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2BasicLoopbackMidiEndpointManager>(&endpointManager));

    auto lock = m_stateLock.lock_exclusive();
    m_endpointManager = std::move(endpointManager);

    return S_OK;
}


HRESULT
TransportState::ConstructConfigurationManager()
{
    wil::com_ptr<CMidi2BasicLoopbackMidiConfigurationManager> configurationManager;
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2BasicLoopbackMidiConfigurationManager>(&configurationManager));

    auto lock = m_stateLock.lock_exclusive();
    m_configurationManager = std::move(configurationManager);

    return S_OK;
}
