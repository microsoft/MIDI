// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointConnection.h"



namespace MIDI_CPP_NAMESPACE::implementation
{
    // Called by the Session class when a device disconnect notification comes through
    void MidiEndpointConnection::InternalOnDeviceDisconnect()
    {
        internal::LogInfo(__FUNCTION__, L"Enter", m_endpointDeviceId);

        // Deactivate MIDI stream
        DeactivateMidiStream(true);

        // Let plugins know we're not connected
        // TODO

        // set IsOpen to false
        m_isOpen = false;

        if (m_endpointDeviceDisconnectedEvent)
        {
            m_endpointDeviceDisconnectedEvent(*this, nullptr);
        }
    }

    // Called by the Session class when a device connection notification comes through
    void MidiEndpointConnection::InternalOnDeviceReconnect()
    {
        internal::LogInfo(__FUNCTION__, L"Enter", m_endpointDeviceId);

        if (!ActivateMidiStream())
        {
            internal::LogGeneralError(__FUNCTION__, L"Failed to reactivate connection stream after a disconnect.", m_endpointDeviceId);

            return;
        }

        if (!InternalOpen())
        {
            internal::LogGeneralError(__FUNCTION__, L"Failed to reopen connection after a disconnect.", m_endpointDeviceId);

            return;
        }

        if (m_endpointDeviceReconnectedEvent)
        {
            m_endpointDeviceReconnectedEvent(*this, nullptr);
        }
    }

}