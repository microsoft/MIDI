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
    // TODO: An optimization here would be to host the watcher at the session level
    // and simply have it filter based on the Ids it sees in the update messages. Then
    // include internal functions in this class that it would call
    _Use_decl_annotations_
    bool MidiEndpointConnection::StartDeviceWatcher()
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        try
        {
            // start the device watcher for the specific Id
            winrt::hstring deviceSelector(
                L"System.Devices.DeviceInstanceId:=\"" + m_endpointDeviceId + L"\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True");

            internal::LogInfo(__FUNCTION__, deviceSelector.c_str());


            m_autoReconnectDeviceWatcher = enumeration::DeviceInformation::CreateWatcher(deviceSelector);

            if (m_autoReconnectDeviceWatcher != nullptr)
            {
                if (m_autoReconnectDeviceWatcher.Status() == enumeration::DeviceWatcherStatus::Stopped)
                {
                    auto deviceAddedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformation>(this, &MidiEndpointConnection::DeviceAddedHandler);
                    auto deviceUpdatedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate>(this, &MidiEndpointConnection::DeviceUpdatedHandler);
                    auto deviceRemovedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate>(this, &MidiEndpointConnection::DeviceRemovedHandler);

                    m_autoReconnectDeviceWatcherAddedRevoker = m_autoReconnectDeviceWatcher.Added(winrt::auto_revoke, deviceAddedHandler);
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
        catch (winrt::hresult_error hr)
        {
            internal::LogHresultError(__FUNCTION__, L"HRESULT Exception starting device watcher", hr);

            return false;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception starting device watcher");

            return false;
        }

    }

    _Use_decl_annotations_
    bool MidiEndpointConnection::StopDeviceWatcher()
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

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
            internal::LogInfo(__FUNCTION__, L"Reopening after disconnect (m_isOpen == false)");

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