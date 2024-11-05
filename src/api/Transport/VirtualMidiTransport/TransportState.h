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


    wil::com_ptr<CMidi2VirtualMidiEndpointManager> GetEndpointManager()
    {
        return m_endpointManager;
    }

    wil::com_ptr<CMidi2VirtualMidiConfigurationManager> GetConfigurationManager()
    {
        return m_configurationManager;
    }

    std::shared_ptr<MidiEndpointTable> GetEndpointTable()
    {
        return m_endpointTable;
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

    std::shared_ptr<MidiEndpointTable> m_endpointTable = std::make_shared<MidiEndpointTable>();

    wil::com_ptr<CMidi2VirtualMidiEndpointManager> m_endpointManager;
    wil::com_ptr<CMidi2VirtualMidiConfigurationManager> m_configurationManager;


};