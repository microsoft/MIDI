// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "midi2.kstransport.h"


_Use_decl_annotations_
HRESULT
CMidi2KSMidi::Initialize(
    LPCWSTR device,
    MidiFlow flow,
    PTRANSPORTCREATIONPARAMS creationParams,
    DWORD * mmcssTaskId,
    IMidiCallback * callback,
    LONGLONG context
)
{
    RETURN_HR_IF(E_INVALIDARG, flow == MidiFlowIn && nullptr == callback);
    RETURN_HR_IF(E_INVALIDARG, flow == MidiFlowBidirectional && nullptr == callback);
    RETURN_HR_IF(E_INVALIDARG, nullptr == device);
    RETURN_HR_IF(E_INVALIDARG, nullptr == mmcssTaskId);
    RETURN_HR_IF(E_INVALIDARG, nullptr == creationParams);

    TraceLoggingWrite(
        MidiKSTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingHexUInt32(creationParams->DataFormat, "MidiDataFormats"),
        TraceLoggingHexUInt32(flow, "MidiFlow"),
        TraceLoggingPointer(callback, "callback")
        );

    std::unique_ptr<KSMidiInDevice> midiInDevice;
    std::unique_ptr<KSMidiOutDevice> midiOutDevice;
    winrt::guid interfaceClass;

    ULONG inPinId{ 0 };
    ULONG outPinId{ 0 };
    MidiTransport transportCapabilities;
    MidiTransport transport;

    std::wstring filterInterfaceId;
    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

    additionalProperties.Append(STRING_DEVPKEY_KsMidiPort_KsFilterInterfaceId);
    additionalProperties.Append(STRING_DEVPKEY_KsMidiPort_KsPinId);
    additionalProperties.Append(STRING_DEVPKEY_KsMidiPort_InPinId);
    additionalProperties.Append(STRING_DEVPKEY_KsMidiPort_OutPinId);
    additionalProperties.Append(STRING_DEVPKEY_KsTransport);
    additionalProperties.Append(L"System.Devices.InterfaceClassGuid");

    auto deviceInfo = DeviceInformation::CreateFromIdAsync(device, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiPort_KsFilterInterfaceId);
    RETURN_HR_IF_NULL(E_INVALIDARG, prop);
    filterInterfaceId = winrt::unbox_value<winrt::hstring>(prop).c_str();

    prop = deviceInfo.Properties().Lookup(L"System.Devices.InterfaceClassGuid");
    RETURN_HR_IF_NULL(E_INVALIDARG, prop);
    interfaceClass = winrt::unbox_value<winrt::guid>(prop);

    prop = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsTransport);
    RETURN_HR_IF_NULL(E_INVALIDARG, prop);
    transportCapabilities = (MidiTransport) winrt::unbox_value<uint32_t>(prop);

    // if creation params specifies MIDI_DATAFORMAT_ANY, the best available transport
    // will be chosen.

    // if creation params specifies that bytestream is requested, then
    // limit the transport to bytestream
    if (creationParams->DataFormat == MidiDataFormats_ByteStream)
    {
        transportCapabilities = (MidiTransport) ((DWORD) transportCapabilities & (DWORD) MidiTransport_StandardAndCyclicByteStream);
    }
    // if creation params specifies that UMP is requested, then
    // limit the transport to UMP
    else if (creationParams->DataFormat == MidiDataFormats_UMP)
    {
        transportCapabilities = (MidiTransport) ((DWORD) transportCapabilities & (DWORD) MidiTransport_CyclicUMP);
    }

    // choose the best available transport among the available transports.
    // fill in the MidiDataFormats that is going to be used for the caller.
    if (0 != ((DWORD) transportCapabilities & (DWORD) MidiTransport_CyclicUMP))
    {
        // Cyclic UMP is available, that's the most preferred transport.
        transport = MidiTransport_CyclicUMP;
        creationParams->DataFormat = MidiDataFormats_UMP;
    }
    else if (0 != ((DWORD) transportCapabilities & (DWORD) MidiTransport_CyclicByteStream))
    {
        // Cyclic bytestream is available, that's the next preferred transport.
        transport = MidiTransport_CyclicByteStream;
        creationParams->DataFormat = MidiDataFormats_ByteStream;
    }
    else if (0 != ((DWORD) transportCapabilities & (DWORD) MidiTransport_StandardByteStream))
    {
        // Standard bytestream is available, that's the least transport.
        transport = MidiTransport_StandardByteStream;
        creationParams->DataFormat = MidiDataFormats_ByteStream;
    }
    else
    {
        // invalid/no transport available, error.
        RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }


    ULONG requestedBufferSize = PAGE_SIZE * 2;
    RETURN_IF_FAILED(GetRequiredBufferSize(requestedBufferSize));

    // Wrapper opens the handle internally.
    KsHandleWrapper deviceHandleWrapper(filterInterfaceId.c_str());
    RETURN_IF_FAILED(deviceHandleWrapper.Open());

    if (flow == MidiFlowBidirectional)
    {
        prop = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiPort_InPinId);
        RETURN_HR_IF_NULL(E_INVALIDARG, prop);
        inPinId = winrt::unbox_value<uint32_t>(prop);

        prop = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiPort_OutPinId);
        RETURN_HR_IF_NULL(E_INVALIDARG, prop);
        outPinId = winrt::unbox_value<uint32_t>(prop);
    }
    else
    {
        // first check for the legacy pin id, if that's not present then look for the newer in/out
        // pin id's that is on bidi endpoints, which can be activated separately in and out.
        prop = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiPort_KsPinId);
        if (prop)
        {
            inPinId = outPinId = winrt::unbox_value<uint32_t>(prop);
        }
        else if (flow == MidiFlowIn)
        {
            prop = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiPort_InPinId);
            RETURN_HR_IF_NULL(E_INVALIDARG, prop);
            inPinId = winrt::unbox_value<uint32_t>(prop);
        }
        else
        {
            prop = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiPort_OutPinId);
            RETURN_HR_IF_NULL(E_INVALIDARG, prop);
            outPinId = winrt::unbox_value<uint32_t>(prop);
        }
    }

    // Duplicate the handle to safely pass it to another component or store it.
    wil::unique_handle handleDupe(deviceHandleWrapper.GetHandle());
    RETURN_IF_NULL_ALLOC(handleDupe);

    if (flow == MidiFlowIn || flow == MidiFlowBidirectional)
    {
        midiInDevice.reset(new (std::nothrow) KSMidiInDevice());
        RETURN_IF_NULL_ALLOC(midiInDevice);
        RETURN_IF_FAILED(midiInDevice->Initialize(device, handleDupe.get(), inPinId, transport, requestedBufferSize, mmcssTaskId, callback, context));
        m_MidiInDevice = std::move(midiInDevice);
    }

    if (flow == MidiFlowOut || flow == MidiFlowBidirectional)
    {

        midiOutDevice.reset(new (std::nothrow) KSMidiOutDevice());
        RETURN_IF_NULL_ALLOC(midiOutDevice);
        RETURN_IF_FAILED(midiOutDevice->Initialize(device, handleDupe.get(), outPinId, transport, requestedBufferSize, mmcssTaskId));
        m_MidiOutDevice = std::move(midiOutDevice);
    }

    return S_OK;
}

HRESULT
CMidi2KSMidi::Shutdown()
{
    TraceLoggingWrite(
        MidiKSTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

    if (m_MidiOutDevice)
    {
        m_MidiOutDevice->Shutdown();
        m_MidiOutDevice.reset();
    }
    if (m_MidiInDevice)
    {
        m_MidiInDevice->Shutdown();
        m_MidiInDevice.reset();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSMidi::SendMidiMessage(
    PVOID data,
    UINT length,
    LONGLONG position
)
{
    if (m_MidiOutDevice)
    {
        return m_MidiOutDevice->SendMidiMessage(data, length, position);
    }

    return E_ABORT;
}
