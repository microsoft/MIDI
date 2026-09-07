// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointConnection.h"

#include "ump_iterator.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    // This is also shared with the WinRT interface
    UINT32 MidiEndpointConnection::GetSupportedMaxMidiWordsPerTransmission()
    {
        // the define is for bytes. Convert to MIDI words and return
        return MAXIMUM_LOOPED_UMP_DATASIZE / sizeof(UINT32);
    }

    _Use_decl_annotations_
    BOOL MidiEndpointConnection::ValidateBufferHasOnlyCompleteUmps(
        UINT32 wordCount,
        UINT32 const* messages
        )
    {
        if (messages == nullptr)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Buffer was null", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return FALSE;
        }

        if (wordCount == 0)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Buffer was empty", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return FALSE;
        }

        // The iterator wants a mutable pointer but only reads, so the caller's buffer is safe.
        auto result = internal::ValidateBufferHasCompleteUmps(const_cast<UINT32*>(messages), wordCount);

        if (!result)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Buffer did not contain complete UMPs", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }

        return result;
    }

    // this just assumes that messages have been validated in some way
    // before they are sent
    _Use_decl_annotations_
    HRESULT
    MidiEndpointConnection::SendMidiMessagesRaw(
        UINT64 timestamp,
        UINT32 wordCount,
        UINT32 const* completeMessages
    )
    {
        RETURN_HR_IF_NULL(E_FAIL, m_endpointTransport);
        RETURN_HR_IF(E_INVALIDARG, wordCount > GetSupportedMaxMidiWordsPerTransmission());

        MessageOptionFlags flags;

        if (m_connectionSettings.WaitForEndpointReceiptOnSend())
        {
            flags = MessageOptionFlags::MessageOptionFlags_WaitForSendComplete;
        }
        else
        {
            flags = MessageOptionFlags::MessageOptionFlags_None;
        }

        // send it

        // The transport takes PVOID but copies the buffer out, so it does not write to it.
        RETURN_IF_FAILED(m_endpointTransport->SendMidiMessage(
            flags,
            static_cast<PVOID>(const_cast<UINT32*>(completeMessages)),
            wordCount * sizeof(UINT32),
            timestamp));

        return S_OK;
    }

    _Use_decl_annotations_
    HRESULT
    MidiEndpointConnection::SetMessagesReceivedCallback(
        IMidiEndpointConnectionMessagesReceivedCallback* messagesReceivedCallback
    )
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, messagesReceivedCallback);

        // AddMessageProcessingPlugin needs both of these in the opposite order, so acquire
        // them together rather than nesting one inside the other.
        std::scoped_lock guard(m_messageProcessingPluginsLock, m_comCallbackLock);

        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_SDK_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
            TraceLoggingPointer(messagesReceivedCallback, "Callback")
        );

        // The callback bypasses everything else on the receive path, so wiring it up after
        // Open() would drop messages which have already been delivered to the other path.
        if (m_isOpen)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"The messages received callback must be registered before the connection is opened.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD)
            );

            OutputDebugString(L"MIDI App SDK: SetMessagesReceivedCallback must be called before the connection is opened.\n");

            RETURN_HR(E_ILLEGAL_METHOD_CALL);
        }

        // Registering the callback would silently disable every plugin already attached,
        // including a virtual device, so refuse instead of doing that quietly.
        if (m_messageProcessingPlugins != nullptr && m_messageProcessingPlugins.Size() > 0)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Cannot register a messages received callback when message processing plugins are attached. The callback bypasses all of them.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD),
                TraceLoggingUInt32(m_messageProcessingPlugins.Size(), "AttachedPluginCount")
            );

            OutputDebugString(L"MIDI App SDK: SetMessagesReceivedCallback failed. This connection has message processing plugins attached, and the callback bypasses all of them.\n");

            RETURN_HR(E_ILLEGAL_STATE_CHANGE);
        }

        // Attach does not increment the ref count. 
        //m_comCallback.copy_from(messagesReceivedCallback);
        m_comCallback.attach(messagesReceivedCallback);

        return S_OK;
    }


    HRESULT
    MidiEndpointConnection::RemoveMessagesReceivedCallback()
    {
        std::lock_guard<std::mutex> guard(m_comCallbackLock);

        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_SDK_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD)
        );

        // Clear out the callback
        
        if (m_comCallback)
        {
            m_comCallback.detach();
            m_comCallback = nullptr;
        }

        return S_OK;
    }

}
