// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

TransportState&
TransportState::Current()
{
    static TransportState current;

    return current;
}

HRESULT
TransportState::ConstructEndpointManager()
{
    auto lock = m_stateLock.lock_exclusive();

    if (m_endpointManager == nullptr)
    {
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2MidiSynthEndpointManager>(&m_endpointManager));
    }

    return S_OK;
}

HRESULT
TransportState::ConstructConfigurationManager()
{
    auto lock = m_stateLock.lock_exclusive();

    if (m_configurationManager == nullptr)
    {
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2MidiSynthConfigurationManager>(&m_configurationManager));
    }

    return S_OK;
}

HRESULT
TransportState::Shutdown()
{
    // Snapshot and clear under the lock, then do the work outside it: tearing the device down
    // joins a worker thread and stops an audio stream, neither of which should run with a lock
    // held that an accessor might be waiting on.
    wil::com_ptr<CMidi2MidiSynthEndpointManager> endpointManager;
    wil::com_ptr<CMidi2MidiSynthConfigurationManager> configurationManager;
    std::shared_ptr<MidiSynthDevice> device;

    {
        auto lock = m_stateLock.lock_exclusive();

        endpointManager = std::move(m_endpointManager);
        m_endpointManager.reset();

        configurationManager = std::move(m_configurationManager);
        m_configurationManager.reset();

        device = m_device;
    }

    if (device)
    {
        LOG_IF_FAILED(device->Shutdown());
    }

    return S_OK;
}
