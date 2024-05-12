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

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    
    // This monitors all endpoints that are in-scope for Windows MIDI Services
    // Filtering to ones for this session happens in the event handler
    _Use_decl_annotations_
    bool MidiSession::StartEndpointWatcher() noexcept
    {
        try
        {
            winrt::hstring deviceSelector = midi2::MidiEndpointConnection::GetDeviceSelector();
            
            // start the device watcher for the specific Id
            //winrt::hstring deviceSelector(
            //    L"System.Devices.DeviceInstanceId:=\"" + m_endpointDeviceId + L"\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True");

            m_autoReconnectDeviceWatcher = enumeration::DeviceInformation::CreateWatcher(deviceSelector);

            if (m_autoReconnectDeviceWatcher != nullptr)
            {
                auto deviceAddedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformation>(this, &MidiSession::DeviceAddedHandler);
                auto deviceUpdatedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate>(this, &MidiSession::DeviceUpdatedHandler);
                auto deviceRemovedHandler = foundation::TypedEventHandler<enumeration::DeviceWatcher, enumeration::DeviceInformationUpdate>(this, &MidiSession::DeviceRemovedHandler);

                m_autoReconnectDeviceWatcherAddedRevoker = m_autoReconnectDeviceWatcher.Added(winrt::auto_revoke, deviceAddedHandler);
                m_autoReconnectDeviceWatcherUpdatedRevoker = m_autoReconnectDeviceWatcher.Updated(winrt::auto_revoke, deviceUpdatedHandler);
                m_autoReconnectDeviceWatcherRemovedRevoker = m_autoReconnectDeviceWatcher.Removed(winrt::auto_revoke, deviceRemovedHandler);

                m_autoReconnectDeviceWatcher.Start();

                return true;
            }
            else
            {
                return false;
            }
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult exception starting device watcher", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            return false;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception starting device watcher", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }
    }

    _Use_decl_annotations_
    bool MidiSession::StopEndpointWatcher() noexcept
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
    void MidiSession::DeviceAddedHandler(
        enumeration::DeviceWatcher source,
        enumeration::DeviceInformation args
    )
    {
        UNREFERENCED_PARAMETER(source);

        auto id = internal::NormalizeEndpointInterfaceIdHStringCopy(args.Id());

        for (auto const& conn : m_connectionsForAutoReconnect)
        {
            if (id == conn->ConnectedEndpointDeviceId() && conn->InternalWasAlreadyOpened())
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
        UNREFERENCED_PARAMETER(args);

        // may have nothing to do here. TBD. But we have to sink this event or else the others don't fire
    }


    _Use_decl_annotations_
    void MidiSession::DeviceRemovedHandler(
        enumeration::DeviceWatcher source,
        enumeration::DeviceInformationUpdate args
    )
    {
        UNREFERENCED_PARAMETER(source);

        // Search all connections to see if we're tracking this one.
        // If we are, and it is not in the auto-reconnect map, then
        // close/disconnect the connection completely
        
        auto id = internal::NormalizeEndpointInterfaceIdHStringCopy(args.Id());

        for (auto const& conn : m_connectionsForAutoReconnect)
        {
            // there can be more than one match, so we don't
            // break after the first one is found
            if (id == conn->ConnectedEndpointDeviceId())
            {
                conn->InternalOnDeviceDisconnect();
            }
        }
    }



}