// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// singleton
class TransportState
{
public:
    static TransportState& Current();

    // no copying
    TransportState(_In_ const TransportState&) = delete;
    TransportState& operator=(_In_ const TransportState&) = delete;

    wil::com_ptr<CMidi2MidiSynthEndpointManager> GetEndpointManager()
    {
        auto lock = m_stateLock.lock_shared();
        return m_endpointManager;
    }

    wil::com_ptr<CMidi2MidiSynthConfigurationManager> GetConfigurationManager()
    {
        auto lock = m_stateLock.lock_shared();
        return m_configurationManager;
    }

    // There is exactly one synthesizer, and it outlives individual client connections so the
    // endpoint can be opened again without rereading the sound set.
    std::shared_ptr<MidiSynthDevice> GetDevice()
    {
        auto lock = m_stateLock.lock_shared();
        return m_device;
    }

    HRESULT Shutdown();

    HRESULT ConstructEndpointManager();
    HRESULT ConstructConfigurationManager();

private:
    TransportState() = default;
    ~TransportState() = default;

    // Guards the managed pointers below against concurrent accessor reads and
    // Shutdown() / Construct*() writes.
    wil::srwlock m_stateLock;

    wil::com_ptr<CMidi2MidiSynthEndpointManager> m_endpointManager{ nullptr };
    wil::com_ptr<CMidi2MidiSynthConfigurationManager> m_configurationManager{ nullptr };

    // Never reset, so a late accessor gets a valid idle object rather than null.
    std::shared_ptr<MidiSynthDevice> m_device = std::make_shared<MidiSynthDevice>();
};
