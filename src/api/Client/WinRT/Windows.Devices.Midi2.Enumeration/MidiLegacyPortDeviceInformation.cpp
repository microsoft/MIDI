// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiLegacyPortDeviceInformation.h"
#include "Legacy.MidiLegacyPortDeviceInformation.g.cpp"

#include <mmdeviceapi.h>    // for the DEVINTERFACE GUIDs for MIDI 1 ports

namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::implementation
{
    _Use_decl_annotations_
    legacy::MidiLegacyPortDeviceInformation MidiLegacyPortDeviceInformation::CreateFromPortDeviceId(
        winrt::hstring const& portDeviceId,
        midi2enum::Midi1PortFlow const& flow) noexcept
    {
        try
        {
            auto device = enumeration::DeviceInformation::CreateFromIdAsync(portDeviceId, GetAdditionalPropertiesList()).get();
            if (device == nullptr) return nullptr;

            auto midiLegacyPortDeviceInformation = winrt::make_self<MidiLegacyPortDeviceInformation>();
            if (midiLegacyPortDeviceInformation == nullptr) return nullptr;

            // TODO: Ensure that the return device matches the flow
            midiLegacyPortDeviceInformation->InternalInitialize(device, flow);

            return *midiLegacyPortDeviceInformation;
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error creating legacy MIDI port device information", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            return nullptr;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error creating legacy MIDI port device information", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return nullptr;
        }
    }

    collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> MidiLegacyPortDeviceInformation::FindAll() noexcept
    {
        auto results = winrt::single_threaded_vector<legacy::MidiLegacyPortDeviceInformation>();

        try
        {
            // get all sources
            auto sourceDevices = enumeration::DeviceInformation::FindAllAsync(
                winrt::Windows::Devices::Midi::MidiInPort::GetDeviceSelector(), 
                GetAdditionalPropertiesList()).get();

            for (auto const& device: sourceDevices)
            {
                auto midiLegacyPortDeviceInformation = winrt::make_self<MidiLegacyPortDeviceInformation>();

                if (midiLegacyPortDeviceInformation != nullptr)
                {
                    midiLegacyPortDeviceInformation->InternalInitialize(device, midi2enum::Midi1PortFlow::MidiMessageSource);
                    results.Append(*midiLegacyPortDeviceInformation);
                }
            }

            // get all destinations
            auto destinationDevices = enumeration::DeviceInformation::FindAllAsync(
                winrt::Windows::Devices::Midi::MidiOutPort::GetDeviceSelector(),
                GetAdditionalPropertiesList()).get();

            for (auto const& device : destinationDevices)
            {
                auto midiLegacyPortDeviceInformation = winrt::make_self<MidiLegacyPortDeviceInformation>();

                if (midiLegacyPortDeviceInformation != nullptr)
                {
                    midiLegacyPortDeviceInformation->InternalInitialize(device, midi2enum::Midi1PortFlow::MidiMessageDestination);
                    results.Append(*midiLegacyPortDeviceInformation);
                }
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error enumerating legacy ports", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error enumerating legacy ports", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

        }

        return results.GetView();
    }

    _Use_decl_annotations_
    collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> MidiLegacyPortDeviceInformation::FindAll(
        midi2enum::Midi1PortFlow const& flow) noexcept
    {
        auto results = winrt::single_threaded_vector<legacy::MidiLegacyPortDeviceInformation>();

        try
        {
            if (flow == midi2enum::Midi1PortFlow::MidiMessageSource)
            {
                // get all sources
                auto sourceDevices = enumeration::DeviceInformation::FindAllAsync(
                    winrt::Windows::Devices::Midi::MidiInPort::GetDeviceSelector(),
                    GetAdditionalPropertiesList()).get();

                for (auto const& device : sourceDevices)
                {
                    auto midiLegacyPortDeviceInformation = winrt::make_self<MidiLegacyPortDeviceInformation>();

                    if (midiLegacyPortDeviceInformation != nullptr)
                    {
                        midiLegacyPortDeviceInformation->InternalInitialize(device, midi2enum::Midi1PortFlow::MidiMessageSource);
                        results.Append(*midiLegacyPortDeviceInformation);
                    }
                }
            }
            else if (flow == midi2enum::Midi1PortFlow::MidiMessageDestination)
            {
                // get all destinations
                auto destinationDevices = enumeration::DeviceInformation::FindAllAsync(
                    winrt::Windows::Devices::Midi::MidiOutPort::GetDeviceSelector(),
                    GetAdditionalPropertiesList()).get();

                for (auto const& device : destinationDevices)
                {
                    auto midiLegacyPortDeviceInformation = winrt::make_self<MidiLegacyPortDeviceInformation>();
                    if (midiLegacyPortDeviceInformation != nullptr)
                    {
                        midiLegacyPortDeviceInformation->InternalInitialize(device, midi2enum::Midi1PortFlow::MidiMessageDestination);
                        results.Append(*midiLegacyPortDeviceInformation);
                    }
                }
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error creating legacy MIDI port device information", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error creating legacy MIDI port device information", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

        }

        return results.GetView();
    }

    _Use_decl_annotations_
    collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> MidiLegacyPortDeviceInformation::FindAllForEndpoint(
        winrt::hstring const& endpointDeviceId) noexcept
    {
        auto results = winrt::single_threaded_vector<legacy::MidiLegacyPortDeviceInformation>();

        try
        {
            auto sourceClass= internal::GuidToString(DEVINTERFACE_MIDI_INPUT);
            auto destinationClass = internal::GuidToString(DEVINTERFACE_MIDI_OUTPUT);

            for (auto const& deviceClass : { sourceClass, destinationClass })
            {
                // we only want MIDI 1 ports which have the specified endpoing as the parent device
                winrt::hstring selector = L"System.Devices.InterfaceClassGuid:=\"" + deviceClass + L"\" AND " + 
                    L"System.Devices.Parent:=\"" + endpointDeviceId + L"\" AND " +
                    L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

                auto devices = enumeration::DeviceInformation::FindAllAsync(
                    selector,
                    GetAdditionalPropertiesList()).get();

                midi2enum::Midi1PortFlow flow{};

                // figure out if we're dealing with inputs or outputs
                if (deviceClass == sourceClass)
                {
                    flow = midi2enum::Midi1PortFlow::MidiMessageSource;
                }
                else
                {
                    flow = midi2enum::Midi1PortFlow::MidiMessageDestination;
                }

                // now add all the ports
                for (auto const& device : devices)
                {
                    auto midiLegacyPortDeviceInformation = winrt::make_self<MidiLegacyPortDeviceInformation>();
                    if (midiLegacyPortDeviceInformation != nullptr)
                    {
                        midiLegacyPortDeviceInformation->InternalInitialize(device, flow);
                        results.Append(*midiLegacyPortDeviceInformation);
                    }
                }

            }
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(ex.code());   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error finding all MIDI 1 ports for UMP endpoint", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Error finding all MIDI 1 ports for UMP endpoint", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

        }

        return results.GetView();
    }

    collections::IVectorView<hstring> MidiLegacyPortDeviceInformation::GetAdditionalPropertiesList() noexcept
    {
        auto props = winrt::single_threaded_vector<winrt::hstring>();

        props.Append(L"System.Devices.ContainerId");
        props.Append(L"System.Devices.Parent");

        props.Append(STRING_PKEY_MIDI_DriverDeviceInterface);
        props.Append(STRING_PKEY_MIDI_PortAssignedGroupIndex);
        props.Append(STRING_PKEY_MIDI_NativeDataFormat);
        props.Append(STRING_PKEY_MIDI_TransportLayer);
        props.Append(STRING_PKEY_MIDI_AssociatedUMP);
        props.Append(STRING_PKEY_MIDI_ServiceAssignedPortNumber);

        return props.GetView();
    }

    winrt::hstring MidiLegacyPortDeviceInformation::ToString() const noexcept
    {
        // TODO: Get from resources
        return L"MIDI 1 Port: " + Name() + L": " + Group().ToString();
    }
}
