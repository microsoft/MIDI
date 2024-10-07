// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
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


    wil::com_ptr<CMidi2NetworkMidiEndpointManager> GetEndpointManager()
    {
        return m_endpointManager;
    }

    wil::com_ptr<CMidi2NetworkMidiConfigurationManager> GetConfigurationManager()
    {
        return m_configurationManager;
    }

    //std::shared_ptr<MidiNetworkDeviceTable> GetEndpointTable()
    //{
    //    return m_endpointTable;
    //}


    HRESULT Cleanup()
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


    wil::com_ptr<CMidi2NetworkMidiEndpointManager> m_endpointManager;
    wil::com_ptr<CMidi2NetworkMidiConfigurationManager> m_configurationManager;

//    std::shared_ptr<MidiNetworkDeviceTable> m_endpointTable = std::make_shared<MidiNetworkDeviceTable>();
};