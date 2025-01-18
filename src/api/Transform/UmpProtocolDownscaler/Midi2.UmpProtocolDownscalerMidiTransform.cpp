// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



#include "pch.h"

#include "Midi2.UmpProtocolDownscalerMidiTransform.h"


_Use_decl_annotations_
HRESULT
CMidi2UmpProtocolDownscalerMidiTransform::Initialize(
    LPCWSTR endpointDeviceInterfaceId,
    PTRANSFORMCREATIONPARAMS creationParams,
    DWORD *,
    IMidiCallback * callback,
    LONGLONG context,
    IMidiDeviceManagerInterface* /*midiDeviceManager*/
)
{
    TraceLoggingWrite(
        MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    // this only converts UMP to UMP
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), creationParams->DataFormatIn != MidiDataFormats_UMP);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), creationParams->DataFormatOut != MidiDataFormats_UMP);

    m_Device = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointDeviceInterfaceId);
    m_Callback = callback;
    m_Context = context;


    // get the deviceinstanceid and also the support for m1 and m2

    auto dev = DeviceInformation::CreateFromIdAsync(endpointDeviceInterfaceId,
        {
            STRING_PKEY_MIDI_EndpointSupportsMidi1Protocol,
            STRING_PKEY_MIDI_EndpointSupportsMidi2Protocol,
        }
    ).get();

    RETURN_HR_IF_NULL(E_INVALIDARG, dev);

    auto m1Prop = dev.Properties().Lookup(STRING_PKEY_MIDI_EndpointSupportsMidi1Protocol);
    RETURN_HR_IF_NULL(E_INVALIDARG, m1Prop);
    m_endpointSupportsMidi1Protocol = winrt::unbox_value<bool>(m1Prop);

    auto m2Prop = dev.Properties().Lookup(STRING_PKEY_MIDI_EndpointSupportsMidi2Protocol);
    RETURN_HR_IF_NULL(E_INVALIDARG, m1Prop);
    m_endpointSupportsMidi2Protocol = winrt::unbox_value<bool>(m2Prop);

    winrt::hstring deviceInstanceId;

    // retrieve the device instance id from the DeviceInformation property store
    auto devInstanceIdProp = dev.Properties().Lookup(L"System.Devices.DeviceInstanceId");
    RETURN_HR_IF_NULL(E_INVALIDARG, devInstanceIdProp);
    deviceInstanceId = winrt::unbox_value<winrt::hstring>(devInstanceIdProp).c_str();

    winrt::hstring deviceSelector(
        L"System.Devices.DeviceInstanceId:=\"" + deviceInstanceId + L"\" AND " +
        L"System.Devices.InterfaceClassGuid:=\"{6994AD04-93EF-11D0-A3CC-00A0C9223196}\" AND " +
        L"System.Devices.InterfaceEnabled: = System.StructuredQueryType.Boolean#True");

    // Set up device watcher to check properties to catch when a new protocol is negotiated
    // See issue #380 in github repo

    m_Watcher = DeviceInformation::CreateWatcher(deviceSelector, 
        { 
            STRING_PKEY_MIDI_EndpointConfiguredProtocol,
            STRING_PKEY_MIDI_EndpointSupportsMidi1Protocol,
            STRING_PKEY_MIDI_EndpointSupportsMidi2Protocol,
        },
        DeviceInformationKind::DeviceInterface
    );

    auto deviceAddedHandler = winrt::Windows::Foundation::TypedEventHandler<DeviceWatcher, DeviceInformation>(
        this, &CMidi2UmpProtocolDownscalerMidiTransform::OnDeviceAdded);
    auto deviceRemovedHandler = winrt::Windows::Foundation::TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(
        this, &CMidi2UmpProtocolDownscalerMidiTransform::OnDeviceRemoved);
    auto deviceUpdatedHandler = winrt::Windows::Foundation::TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(
        this, &CMidi2UmpProtocolDownscalerMidiTransform::OnDeviceUpdated);

    m_DeviceAdded = m_Watcher.Added(winrt::auto_revoke, deviceAddedHandler);
    m_DeviceRemoved = m_Watcher.Removed(winrt::auto_revoke, deviceRemovedHandler);
    m_DeviceUpdated = m_Watcher.Updated(winrt::auto_revoke, deviceUpdatedHandler);

    m_Watcher.Start();

    return S_OK;
}


_Use_decl_annotations_
HRESULT 
CMidi2UmpProtocolDownscalerMidiTransform::OnDeviceAdded(DeviceWatcher, DeviceInformation)
{
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2UmpProtocolDownscalerMidiTransform::OnDeviceUpdated(DeviceWatcher, DeviceInformationUpdate update)
{
    if (update.Properties().HasKey(STRING_PKEY_MIDI_EndpointConfiguredProtocol))
    {
        TraceLoggingWrite(
            MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Configured Protocol has changed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_Device.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        // retrieve the device instance id from the DeviceInformation property store
        auto prop = update.Properties().Lookup(STRING_PKEY_MIDI_EndpointConfiguredProtocol);
        RETURN_HR_IF_NULL(E_INVALIDARG, prop);
        byte newVal = winrt::unbox_value<uint8_t>(prop);

        // TODO: We should also upscale if the endpoint supports MIDI 2 but not MIDI 1

        if (newVal == MIDI_PROP_CONFIGURED_PROTOCOL_MIDI1)
        {
            TraceLoggingWrite(
                MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"New protocol is MIDI 1.0. Downscaling enabled", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_Device.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            m_downscalingRequiredForEndpoint = true;
            m_upscalingRequiredForEndpoint = false;
            
        }
        else if(newVal == MIDI_PROP_CONFIGURED_PROTOCOL_MIDI2)
        {
            m_downscalingRequiredForEndpoint = false;

            if (m_endpointSupportsMidi1Protocol /* && m_endpointSupportsMidi2Protocol*/)
            {
                TraceLoggingWrite(
                    MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"New protocol is MIDI 2.0. Downscaling disabled. Upscaling is not required because endpoint supports MIDI 1 protocol as well", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(m_Device.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );

                // endpoint supports both MIDI 1 and MIDI 2 protocols, so we don't upscale MIDI 1 messages
                // and instead prefer to retain their original form.
                m_upscalingRequiredForEndpoint = false;
            }
            else
            {
                TraceLoggingWrite(
                    MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"New protocol is MIDI 2.0. Endpoint doesn't declare MIDI 1 support, so we will upscale MIDI 1 protocol messages.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(m_Device.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );

                // endpoint declares it supports only MIDI 2 protocol, so we also need to upscale MIDI 1 messages
                m_upscalingRequiredForEndpoint = true;
            }
        }
    }


    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2UmpProtocolDownscalerMidiTransform::OnDeviceRemoved(DeviceWatcher, DeviceInformationUpdate)
{
    return S_OK;
}


HRESULT
CMidi2UmpProtocolDownscalerMidiTransform::Shutdown()
{
    TraceLoggingWrite(
        MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_Device.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2UmpProtocolDownscalerMidiTransform::SendMidiMessage(
    PVOID inputData,
    UINT length,
    LONGLONG timestamp
)
{
    //TraceLoggingWrite(
    //    MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_INFO,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
    //    TraceLoggingPointer(this, "this")
    //);


    // if downscaling and upscaling aren't required, quickly move on
    if (!m_downscalingRequiredForEndpoint && !m_upscalingRequiredForEndpoint)
    {
        RETURN_IF_FAILED(m_Callback->Callback(inputData, length, timestamp, m_Context));
    }
    else if (m_downscalingRequiredForEndpoint)
    {
        if (length >= sizeof(uint32_t))
        {
            // downscale only MIDI 2.0 channel voice messages : message type 4. Everything else passes through
            if (internal::GetUmpMessageTypeFromFirstWord(internal::MidiWord0FromVoidMessageDataPointer(inputData)) != 4)
            {
                RETURN_IF_FAILED(m_Callback->Callback(inputData, length, timestamp, m_Context));

                return S_OK;
            }

            // Send the UMP(s) to the parser
            uint32_t* data = (uint32_t*)inputData;
            for (UINT i = 0; i < (length / sizeof(uint32_t)); i++)
            {
                m_umpToMidi1.UMPStreamParse(data[i]);
            }

            // retrieve the message from the parser
            // and send it on
            while (m_umpToMidi1.availableUMP())
            {
                uint32_t words[MAXIMUM_LOOPED_UMP_DATASIZE / sizeof(uint32_t)];
                UINT wordCount{ 0 };

                for (wordCount = 0; wordCount < _countof(words) && m_umpToMidi1.availableUMP(); wordCount++)
                {
                    words[wordCount] = m_umpToMidi1.readUMP();
                }

                if (wordCount > 0)
                {
                    // we use return here instead of log, because the number of UMPs created should be no more than 1
                    RETURN_IF_FAILED(m_Callback->Callback(&(words[0]), wordCount * sizeof(uint32_t), timestamp, m_Context));
                }
            }
        }
        else
        {
            // not a valid UMP

            TraceLoggingWrite(
                MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Invalid UMP", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_Device.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                TraceLoggingUInt32(length, "Message length in Bytes"),
                TraceLoggingUInt64(timestamp, "Message timestamp")
            );
        }

    }
    else if (m_upscalingRequiredForEndpoint)
    {
        // Endpoint requires upscaling, but libmidi2 doesn't currently do upscaling, so we will
        // just send the original message along. This branch and the detection is to enable
        // easy addition of this in the future when libmidi2 supports protocol upscaling
        // https://github.com/midi2-dev/AM_MIDI2.0Lib/issues/25

        RETURN_IF_FAILED(m_Callback->Callback(inputData, length, timestamp, m_Context));

        return S_OK;
    }

    return S_OK;
}









_Use_decl_annotations_
HRESULT
CMidi2UmpProtocolDownscalerTransform::Activate(
    REFIID iid,
    void** activatedInterface
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == activatedInterface);

    if (__uuidof(IMidiDataTransform) == iid)
    {
        TraceLoggingWrite(
            MidiUmpProtocolDownscalerTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiDataTransform", MIDI_TRACE_EVENT_INTERFACE_FIELD)
        );

        wil::com_ptr_nothrow<IMidiDataTransform> midiTransform;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2UmpProtocolDownscalerMidiTransform>(&midiTransform));
        *activatedInterface = midiTransform.detach();
    }

    else
    {
        return E_NOINTERFACE;
    }

    return S_OK;
}