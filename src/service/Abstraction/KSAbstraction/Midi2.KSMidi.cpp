// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "midi2.ksabstraction.h"

_Use_decl_annotations_
HRESULT
CMidi2KSMidi::Initialize(
    LPCWSTR Device,
    MidiFlow Flow,
    DWORD * MmCssTaskId,
    IMidiCallback * Callback
)
{
    RETURN_HR_IF(E_INVALIDARG, Flow == MidiFlowIn && nullptr == Callback);
    RETURN_HR_IF(E_INVALIDARG, Flow == MidiFlowBidirectional && nullptr == Callback);
    RETURN_HR_IF(E_INVALIDARG, nullptr == Device);
    RETURN_HR_IF(E_INVALIDARG, nullptr == MmCssTaskId);

    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(Device, "Device"),
        TraceLoggingHexUInt32(Flow, "MidiFlow"),
        TraceLoggingHexUInt32(*MmCssTaskId, "MmCssTaskId"),
        TraceLoggingPointer(Callback, "callback")
        );

    wil::unique_handle filter;
    std::unique_ptr<KSMidiInDevice> midiInDevice;
    std::unique_ptr<KSMidiOutDevice> midiOutDevice;
    winrt::guid interfaceClass;

    ULONG inPinId{ 0 };
    ULONG outPinId{ 0 };
    bool ump{ false };
    bool looped{ false };
    bool midiOne{ false };

    std::wstring filterInterfaceId;
    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiPort_KsFilterInterfaceId));
    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiPort_KsPinId));
    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiPort_InPinId));
    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiPort_OutPinId));
    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiPort_SupportsUMPFormat));
    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiPort_SupportsMidiOneFormat));
    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiPort_SupportsLooped));
    additionalProperties.Append(winrt::to_hstring(L"System.Devices.InterfaceClassGuid"));

    auto deviceInfo = DeviceInformation::CreateFromIdAsync(Device, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiPort_KsFilterInterfaceId);
    filterInterfaceId = winrt::unbox_value<winrt::hstring>(prop).c_str();

    prop = deviceInfo.Properties().Lookup(L"System.Devices.InterfaceClassGuid");
    interfaceClass = winrt::unbox_value<winrt::guid>(prop);

    if (winrt::guid(DEVINTERFACE_MIDI_INPUT) == interfaceClass ||
        winrt::guid(DEVINTERFACE_MIDI_OUTPUT) == interfaceClass)
    {
        // if we're activated with the legacy interface class,
        // then we activate as a midi one peripheral
        ump = false;
        looped = false;
        midiOne = true;
    }
    else if (winrt::guid(DEVINTERFACE_UNIVERSALMIDIPACKET_INPUT) == interfaceClass ||
        winrt::guid(DEVINTERFACE_UNIVERSALMIDIPACKET_OUTPUT) == interfaceClass ||
        winrt::guid(DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI) == interfaceClass)
    {
        // UMP interface class, determine the endpoint capabilities.
        prop = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiPort_SupportsUMPFormat);
        ump = winrt::unbox_value<bool>(prop);

        prop = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiPort_SupportsMidiOneFormat);
        midiOne = winrt::unbox_value<bool>(prop);

        prop = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiPort_SupportsLooped);
        looped = winrt::unbox_value<bool>(prop);
    }
    else
    {
        RETURN_IF_FAILED(E_UNEXPECTED);
    }

    // must support either ump or midiOne formats, fail if neither supported
    RETURN_HR_IF(E_INVALIDARG, false == ump && false == midiOne);

    if (Flow == MidiFlowBidirectional)
    {
        prop = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiPort_InPinId);
        inPinId = outPinId = winrt::unbox_value<uint32_t>(prop);

        prop = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiPort_OutPinId);
        outPinId = outPinId = winrt::unbox_value<uint32_t>(prop);
    }
    else
    {
        prop = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiPort_KsPinId);
        inPinId = outPinId = winrt::unbox_value<uint32_t>(prop);
    }

    ULONG requestedBufferSize = PAGE_SIZE;
    RETURN_IF_FAILED(GetRequiredBufferSize(requestedBufferSize));

    RETURN_IF_FAILED(FilterInstantiate(filterInterfaceId.c_str(), &filter));

    if (Flow == MidiFlowIn || Flow == MidiFlowBidirectional)
    {
        midiInDevice.reset(new (std::nothrow) KSMidiInDevice());
        RETURN_IF_NULL_ALLOC(midiInDevice);
        RETURN_IF_FAILED(midiInDevice->Initialize(Device, filter.get(), inPinId, looped, ump, requestedBufferSize, MmCssTaskId, Callback));
        m_MidiInDevice = std::move(midiInDevice);
    }

    if (Flow == MidiFlowOut || Flow == MidiFlowBidirectional)
    {
        midiOutDevice.reset(new (std::nothrow) KSMidiOutDevice());
        RETURN_IF_NULL_ALLOC(midiOutDevice);
        RETURN_IF_FAILED(midiOutDevice->Initialize(Device, filter.get(), outPinId, looped, ump, requestedBufferSize, MmCssTaskId));
        m_MidiOutDevice = std::move(midiOutDevice);
    }

    return S_OK;
}

HRESULT
CMidi2KSMidi::Cleanup()
{
    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    if (m_MidiOutDevice)
    {
        m_MidiOutDevice->Cleanup();
        m_MidiOutDevice.reset();
    }
    if (m_MidiInDevice)
    {
        m_MidiInDevice->Cleanup();
        m_MidiInDevice.reset();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSMidi::SendMidiMessage(
    PVOID Data,
    UINT Length,
    LONGLONG Position
)
{
    if (m_MidiOutDevice)
    {
        return m_MidiOutDevice->SendMidiMessage(Data, Length, Position);
    }

    return E_ABORT;
}

