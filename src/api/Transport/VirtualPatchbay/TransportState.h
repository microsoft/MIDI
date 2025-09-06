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


    wil::com_ptr<CMidi2VirtualPatchBayEndpointManager> GetEndpointManager()
    {
        return m_endpointManager;
    }

    wil::com_ptr<CMidi2VirtualPatchBayConfigurationManager> GetConfigurationManager()
    {
        return m_configurationManager;
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

    wil::com_ptr<CMidi2VirtualPatchBayEndpointManager> m_endpointManager;
    wil::com_ptr<CMidi2VirtualPatchBayConfigurationManager> m_configurationManager;


};