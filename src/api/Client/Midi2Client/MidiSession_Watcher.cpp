// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiSession.h"
#include "MidiEndpointConnection.h"

namespace MIDI_CPP_NAMESPACE::implementation
{
    
    // This monitors all endpoints that are in-scope for Windows MIDI Services
    // Filtering to ones for this session happens in the event handler
    _Use_decl_annotations_
    bool MidiSession::StartEndpointWatcher() noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        try
        {
            winrt::hstring deviceSelector = midi2::MidiEndpointConnection::GetDeviceSelector();
            
            // start the device watcher for the specific Id
            //winrt::hstring deviceSelector(
            //    L"System.Devices.DeviceInstanceId:=\"" + m_endpointDeviceId + L"\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True");

            internal::LogInfo(__FUNCTION__, deviceSelector.c_str());

            m_autoReconnectDeviceWatcher = enumeration::DeviceInformation::CreateWatcher(deviceSelector);

            if (m_autoReconnectDeviceWatcher != nullptr)
            {
                internal::LogInfo(__FUNCTION__, L"Device watcher created");

                auto deviceAddedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformation>(this, &MidiSession::DeviceAddedHandler);
                auto deviceUpdatedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate>(this, &MidiSession::DeviceUpdatedHandler);
                auto deviceRemovedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate>(this, &MidiSession::DeviceRemovedHandler);

                m_autoReconnectDeviceWatcherAddedRevoker = m_autoReconnectDeviceWatcher.Added(winrt::auto_revoke, deviceAddedHandler);
                m_autoReconnectDeviceWatcherUpdatedRevoker = m_autoReconnectDeviceWatcher.Updated(winrt::auto_revoke, deviceUpdatedHandler);
                m_autoReconnectDeviceWatcherRemovedRevoker = m_autoReconnectDeviceWatcher.Removed(winrt::auto_revoke, deviceRemovedHandler);

                internal::LogInfo(__FUNCTION__, L"About to start device watcher");

                m_autoReconnectDeviceWatcher.Start();

                internal::LogInfo(__FUNCTION__, L"Device watcher started");

                return true;
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
    bool MidiSession::StopEndpointWatcher() noexcept
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
    void MidiSession::DeviceAddedHandler(
        enumeration::DeviceWatcher source,
        enumeration::DeviceInformation args
    )
    {
        UNREFERENCED_PARAMETER(source);

        internal::LogInfo(__FUNCTION__, args.Id().c_str());

        auto id = internal::NormalizeEndpointInterfaceIdHStringCopy(args.Id());

        for (auto const& conn : m_connectionsForAutoReconnect)
        {
            if (id == conn->EndpointDeviceId() && conn->InternalWasAlreadyOpened())
            {
                conn->InternalOnDeviceReconnect();
            }
        }

    }

    _Use_decl_annotations_
    void MidiSession::DeviceUpdatedHandler(
        enumeration::DeviceWatcher source,
        enumeration::DeviceInformationUpdate args
    )
    {
        UNREFERENCED_PARAMETER(source);

        internal::LogInfo(__FUNCTION__, args.Id().c_str(), args.Id());

        // may have nothing to do here. TBD
    }


    _Use_decl_annotations_
    void MidiSession::DeviceRemovedHandler(
        enumeration::DeviceWatcher source,
        enumeration::DeviceInformationUpdate args
    )
    {
        UNREFERENCED_PARAMETER(source);

        internal::LogInfo(__FUNCTION__, args.Id().c_str(), args.Id());

        // Search all connections to see if we're tracking this one.
        // If we are, and it is not in the auto-reconnect map, then
        // close/disconnect the connection completely
        
        auto id = internal::NormalizeEndpointInterfaceIdHStringCopy(args.Id());

        for (auto const& conn : m_connectionsForAutoReconnect)
        {
            // there can be more than one match, so we don't
            // break after the first one is found
            if (id == conn->EndpointDeviceId())
            {
                conn->InternalOnDeviceDisconnect();
            }
        }
    }



}