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


    wil::com_ptr<CMidi2BasicLoopbackMidiEndpointManager> GetEndpointManager()
    {
        auto lock = m_stateLock.lock_shared();
        return m_endpointManager;
    }

    wil::com_ptr<CMidi2BasicLoopbackMidiConfigurationManager> GetConfigurationManager()
    {
        auto lock = m_stateLock.lock_shared();
        return m_configurationManager;
    }

    std::shared_ptr<MidiBasicLoopbackDeviceTable> GetEndpointTable()
    {
        auto lock = m_stateLock.lock_shared();
        return m_endpointTable;
    }

    std::shared_ptr<TransportWorkQueue> GetEndpointWorkQueue()
    {
        auto lock = m_stateLock.lock_shared();
        return m_workQueue;
    }

    HRESULT Shutdown()
    {
        // Snapshot-and-clear the managed members under the lock, but run the
        // (potentially long) table shutdown outside the lock so we don't hold
        // m_stateLock across a call that itself takes other locks.
        wil::com_ptr<CMidi2BasicLoopbackMidiEndpointManager> endpointManager;
        wil::com_ptr<CMidi2BasicLoopbackMidiConfigurationManager> configurationManager;
        std::shared_ptr<MidiBasicLoopbackDeviceTable> endpointTable;
        {
            auto lock = m_stateLock.lock_exclusive();

            endpointManager = std::move(m_endpointManager);
            m_endpointManager.reset();

            configurationManager = std::move(m_configurationManager);
            m_configurationManager.reset();

            // keep m_endpointTable / m_workQueue alive (never reset) so late
            // accessor calls still return a valid, empty object rather than null.
            endpointTable = m_endpointTable;
        }

        if (endpointTable)
        {
            LOG_IF_FAILED(endpointTable->Shutdown());
        }

        return S_OK;
    }


    HRESULT ConstructEndpointManager();
    HRESULT ConstructConfigurationManager();


private:
    TransportState();
    ~TransportState();


    // guards the managed pointers below against concurrent accessor reads
    // and Shutdown()/Construct*() writes.
    wil::srwlock m_stateLock;

    wil::com_ptr<CMidi2BasicLoopbackMidiEndpointManager> m_endpointManager{ nullptr };
    wil::com_ptr<CMidi2BasicLoopbackMidiConfigurationManager> m_configurationManager{ nullptr };

    std::shared_ptr<MidiBasicLoopbackDeviceTable> m_endpointTable = std::make_shared<MidiBasicLoopbackDeviceTable>();

    std::shared_ptr<TransportWorkQueue> m_workQueue = std::make_shared<TransportWorkQueue>();

};