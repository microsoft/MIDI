// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointConnection.h"


// auto deviceAddedHandler = TypedEventHandler<DeviceWatcher, DeviceInformation>(this, &CMidi2KSMidiEndpointManager::OnDeviceAdded);
// auto deviceUpdatedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidi2KSMidiEndpointManager::OnDeviceUpdated);
// auto deviceRemovedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidi2KSMidiEndpointManager::OnDeviceRemoved);
// m_DeviceAdded = m_Watcher.Added(winrt::auto_revoke, deviceAddedHandler);
// m_DeviceUpdated = m_Watcher.Updated(winrt::auto_revoke, deviceUpdatedHandler);
// m_DeviceRemoved = m_Watcher.Removed(winrt::auto_revoke, deviceRemovedHandler);


namespace MIDI_CPP_NAMESPACE::implementation
{
    _Use_decl_annotations_
    bool MidiEndpointConnection::StartDeviceWatcher()
    {
        // start the device watcher for the specific Id
        winrt::hstring deviceSelector(
            L"Id:=\"" + m_endpointDeviceId + L"\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True");

        m_autoReconnectDeviceWatcher = enumeration::DeviceInformation::CreateWatcher(deviceSelector);

        if (m_autoReconnectDeviceWatcher != nullptr)
        {
            if (m_autoReconnectDeviceWatcher.Status() == enumeration::DeviceWatcherStatus::Stopped)
            {
                auto deviceAddedHandler   = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformation>(this, &MidiEndpointConnection::DeviceAddedHandler);
                auto deviceUpdatedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate>(this, &MidiEndpointConnection::DeviceUpdatedHandler);
                auto deviceRemovedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate>(this, &MidiEndpointConnection::DeviceRemovedHandler);

                m_autoReconnectDeviceWatcherAddedRevoker   = m_autoReconnectDeviceWatcher.Added(winrt::auto_revoke, deviceAddedHandler);
                m_autoReconnectDeviceWatcherUpdatedRevoker = m_autoReconnectDeviceWatcher.Updated(winrt::auto_revoke, deviceUpdatedHandler);
                m_autoReconnectDeviceWatcherRemovedRevoker = m_autoReconnectDeviceWatcher.Removed(winrt::auto_revoke, deviceRemovedHandler);

                m_autoReconnectDeviceWatcher.Start();

                return true;
            }
            else
            {
                // can only start the watcher when it's in a stopped state
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    _Use_decl_annotations_
    bool MidiEndpointConnection::StopDeviceWatcher()
    {
        if (m_autoReconnectDeviceWatcher != nullptr)
        {
            m_autoReconnectDeviceWatcher.Stop();

            m_autoReconnectDeviceWatcherAddedRevoker.revoke();
            m_autoReconnectDeviceWatcherUpdatedRevoker.revoke();
            m_autoReconnectDeviceWatcherRemovedRevoker.revoke();

            return true;
        }
        else
        {
            return false;
        }
    }



    _Use_decl_annotations_
    void MidiEndpointConnection::DeviceAddedHandler(
            enumeration::DeviceWatcher source,
            enumeration::DeviceInformation args
    )
    {
        UNREFERENCED_PARAMETER(source);
        UNREFERENCED_PARAMETER(args);

        internal::LogInfo(__FUNCTION__, args.Id().c_str());

        // if not m_isOpen, then perform all the opening logic again
        if (!m_isOpen)
        {
            InternalReopenAfterDisconnect();
        }

    }

    _Use_decl_annotations_
    void MidiEndpointConnection::DeviceUpdatedHandler(
            enumeration::DeviceWatcher source,
            enumeration::DeviceInformationUpdate args
        )
    {
        UNREFERENCED_PARAMETER(source);
        UNREFERENCED_PARAMETER(args);

        internal::LogInfo(__FUNCTION__, args.Id().c_str());


        // may have nothing to do here. TBD

    }


    _Use_decl_annotations_
    void MidiEndpointConnection::DeviceRemovedHandler(
            enumeration::DeviceWatcher source,
            enumeration::DeviceInformationUpdate args
    )
    {
        UNREFERENCED_PARAMETER(source);
        UNREFERENCED_PARAMETER(args);

        internal::LogInfo(__FUNCTION__, args.Id().c_str());

        InternalClose();

    }
}