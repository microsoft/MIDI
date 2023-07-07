// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiEndpointConnectOptions.h"
#include "MidiEndpointConnectOptions.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    winrt::Windows::Devices::Midi2::MidiEndpointConnectOptions MidiEndpointConnectOptions::Default()
    {
        // TODO: set the actual default parameters

        return *(winrt::make_self<implementation::MidiEndpointConnectOptions>());
    }

    bool MidiEndpointConnectOptions::DisableProcessingFunctionBlockInformationMessages()
    {
        return _disableProcessingFunctionBlockInformationMessages;
    }
    void MidiEndpointConnectOptions::DisableProcessingFunctionBlockInformationMessages(bool value)
    {
        _disableProcessingFunctionBlockInformationMessages = value;
    }
    bool MidiEndpointConnectOptions::DisableProcessingEndpointInformationMessages()
    {
        return _disableProcessingEndpointInformationMessages;
    }
    void MidiEndpointConnectOptions::DisableProcessingEndpointInformationMessages(bool value)
    {
        _disableProcessingEndpointInformationMessages = value;
    }

    bool MidiEndpointConnectOptions::UseSessionLevelMessageReceiveHandler()
    {
        return _useSessionLevelMessageReceiveHandler;
    }
    void MidiEndpointConnectOptions::UseSessionLevelMessageReceiveHandler(bool value)
    {
        _useSessionLevelMessageReceiveHandler = value;
    }

    


}
