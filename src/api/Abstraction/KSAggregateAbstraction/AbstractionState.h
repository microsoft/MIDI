// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

// singleton
class AbstractionState
{

public:
    static AbstractionState& Current();

    // no copying
    AbstractionState(_In_ const AbstractionState&) = delete;
    AbstractionState& operator=(_In_ const AbstractionState&) = delete;


    wil::com_ptr<CMidi2KSAggregateMidiEndpointManager> GetEndpointManager()
    {
        return m_endpointManager;
    }

    wil::com_ptr<CMidi2KSAggregateMidiConfigurationManager> GetConfigurationManager()
    {
        return m_configurationManager;
    }


    HRESULT Cleanup()
    {
        m_endpointManager.reset();
        m_configurationManager.reset();

        return S_OK;
    }


    HRESULT ConstructEndpointManager();
    HRESULT ConstructConfigurationManager();


private:
    AbstractionState();
    ~AbstractionState();


    wil::com_ptr<CMidi2KSAggregateMidiEndpointManager> m_endpointManager;
    wil::com_ptr<CMidi2KSAggregateMidiConfigurationManager> m_configurationManager;

};