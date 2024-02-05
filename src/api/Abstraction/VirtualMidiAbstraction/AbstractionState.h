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

    std::shared_ptr<MidiEndpointTable> m_endpointTable = std::make_shared<MidiEndpointTable>();

    wil::com_ptr<CMidi2VirtualMidiEndpointManager> m_endpointManager;
    wil::com_ptr<CMidi2VirtualMidiConfigurationManager> m_configurationManager;


};