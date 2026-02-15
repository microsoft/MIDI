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


    wil::com_ptr<CMidi2KSAggregateMidiEndpointManager> GetEndpointManager()
    {
        if (!Feature_Servicing_MIDI2VirtualPortDriversFix::IsEnabled())
        {
            return m_endpointManager;
        }
        else
        {
            return nullptr;
        }
    }

    // for Feature_Servicing_MIDI2VirtualPortDriversFix
    wil::com_ptr<CMidi2KSAggregateMidiEndpointManager2> GetEndpointManager2()
    {
        if (Feature_Servicing_MIDI2VirtualPortDriversFix::IsEnabled())
        {
            return m_endpointManager2;
        }
        else
        {
            return nullptr;
        }
    }

    wil::com_ptr<CMidi2KSAggregateMidiConfigurationManager> GetConfigurationManager()
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


    wil::com_ptr<CMidi2KSAggregateMidiEndpointManager> m_endpointManager;

    // for Feature_Servicing_MIDI2VirtualPortDriversFix
    wil::com_ptr<CMidi2KSAggregateMidiEndpointManager2> m_endpointManager2 { nullptr };

    wil::com_ptr<CMidi2KSAggregateMidiConfigurationManager> m_configurationManager;

};