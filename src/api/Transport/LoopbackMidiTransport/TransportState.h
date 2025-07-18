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


private:
    TransportState();
    ~TransportState();


    wil::com_ptr<CMidi2LoopbackMidiEndpointManager> m_endpointManager{ nullptr };
    wil::com_ptr<CMidi2LoopbackMidiConfigurationManager> m_configurationManager{ nullptr };

    std::shared_ptr<MidiLoopbackDeviceTable> m_endpointTable = std::make_shared<MidiLoopbackDeviceTable>();

    std::shared_ptr<TransportWorkQueue> m_workQueue = std::make_shared<TransportWorkQueue>();

};