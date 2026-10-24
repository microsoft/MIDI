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


    wil::com_ptr<CMidi2LoopbackMidiEndpointManager> GetEndpointManager()
    {
        return m_endpointManager;
    }

    wil::com_ptr<CMidi2LoopbackMidiConfigurationManager> GetConfigurationManager()
    {
        return m_configurationManager;
    }

    std::shared_ptr<MidiLoopbackDeviceTable> GetEndpointTable()
    {
        return m_endpointTable;
    }

    std::shared_ptr<TransportWorkQueue> GetEndpointWorkQueue()
    {
        return m_workQueue;
    }

    HRESULT Shutdown()
    {
        m_endpointManager.reset();
        m_configurationManager.reset();

        return S_OK;
    }


    HRESULT ConstructEndpointManager();
    HRESULT ConstructConfigurationManager(); 

    // Net-new for Feature_Servicing_MIDI2LoopbackAndClientOpenHardening. Held for a whole configuration update,
    // and while the endpoint manager finishes initializing and creates the loopbacks queued from
    // the configuration file, so those never run at the same time as each other.
    std::mutex& ConfigurationLock() { return m_configurationLock; }

private:
    TransportState();
    ~TransportState();

    std::mutex m_configurationLock;


    wil::com_ptr<CMidi2LoopbackMidiEndpointManager> m_endpointManager{ nullptr };
    wil::com_ptr<CMidi2LoopbackMidiConfigurationManager> m_configurationManager{ nullptr };

    std::shared_ptr<MidiLoopbackDeviceTable> m_endpointTable = std::make_shared<MidiLoopbackDeviceTable>();
 
    std::shared_ptr<TransportWorkQueue> m_workQueue = std::make_shared<TransportWorkQueue>();

};