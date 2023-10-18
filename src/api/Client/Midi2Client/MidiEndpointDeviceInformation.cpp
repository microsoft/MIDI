// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointDeviceInformation.h"
#include "MidiEndpointDeviceInformation.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    winrt::Windows::Devices::Enumeration::DeviceInformation MidiEndpointDeviceInformation::GetParentDeviceInformation()
    {
        // TODO
        return nullptr;

    }


    collections::IVectorView<winrt::hstring> MidiEndpointDeviceInformation::GetAdditionalPropertiesList() noexcept
    {
        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

        // TODO: Group Terminal Blocks will likely end up as a single property
        additionalProperties.Append(STRING_PKEY_MIDI_IN_GroupTerminalBlocks);
        additionalProperties.Append(STRING_PKEY_MIDI_OUT_GroupTerminalBlocks);

        additionalProperties.Append(STRING_PKEY_MIDI_NativeDataFormat);

        additionalProperties.Append(STRING_PKEY_MIDI_TransportMnemonic);

        additionalProperties.Append(STRING_PKEY_MIDI_UmpLoopback);
        additionalProperties.Append(STRING_PKEY_MIDI_UmpPing);

        additionalProperties.Append(STRING_PKEY_MIDI_UniqueIdentifier);

        additionalProperties.Append(STRING_PKEY_MIDI_SupportsMultiClient);

        additionalProperties.Append(STRING_PKEY_MIDI_EndpointSupportsMidi2Protocol);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointSupportsMidi1Protocol);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointSupportsReceivingJRTimestamps);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointSupportsSendingJRTimestamps);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointUmpVersionMajor);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointUmpVersionMinor);

        additionalProperties.Append(STRING_PKEY_MIDI_EndpointProvidedName);
        additionalProperties.Append(STRING_PKEY_MIDI_EndpointProvidedProductInstanceId);
        additionalProperties.Append(STRING_PKEY_MIDI_FunctionBlocks);
        additionalProperties.Append(STRING_PKEY_MIDI_FunctionBlocksAreStatic);
        additionalProperties.Append(STRING_PKEY_MIDI_DeviceIdentification);

        return additionalProperties.GetView();
    }


    _Use_decl_annotations_
    midi2::MidiEndpointDeviceWatcher MidiEndpointDeviceInformation::CreateWatcher(bool /*includeDiagnosticsEndpoints*/)
    {
        // TODO
        return nullptr;
    }

    _Use_decl_annotations_
    collections::IVectorView<midi2::MidiEndpointDeviceInformation> MidiEndpointDeviceInformation::FindAll(bool includeDiagnosticsEndpoints)
    {
        auto devices = Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
            MidiEndpointConnection::GetDeviceSelector(),
            GetAdditionalPropertiesList()
            ).get();

        auto midiDevices = winrt::single_threaded_vector<midi2::MidiEndpointDeviceInformation>();

        if (devices != nullptr)
        {
            for (auto const& di : devices)
            {
                auto endpointInfo = winrt::make_self<MidiEndpointDeviceInformation>();

                endpointInfo->InternalUpdateFromDeviceInformation(di);

                if (endpointInfo->EndpointPurpose() == MidiEndpointDevicePurpose::NormalMessageEndpoint || includeDiagnosticsEndpoints)
                {
                    midiDevices.Append(*endpointInfo);
                }
            }
        }

        return midiDevices.GetView();
    }



    _Use_decl_annotations_
    midi2::MidiEndpointDeviceInformation MidiEndpointDeviceInformation::CreateFromId(
        winrt::hstring const& id) noexcept
    {
        auto di = Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
            id, GetAdditionalPropertiesList()).get();

        if (di != nullptr)
        {
            auto endpointInfo = winrt::make_self<MidiEndpointDeviceInformation>();

            endpointInfo->InternalUpdateFromDeviceInformation(di);

            return *endpointInfo;

        }
        else
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    void MidiEndpointDeviceInformation::InternalUpdateFromDeviceInformation(
        Windows::Devices::Enumeration::DeviceInformation const& info) noexcept
    {
        if (info != nullptr)
        {
            auto nativeDataFormat = info.Properties().Lookup(STRING_PKEY_MIDI_NativeDataFormat);

            auto transportMnemonic = info.Properties().Lookup(STRING_PKEY_MIDI_TransportMnemonic);
            auto isLoopback = info.Properties().Lookup(STRING_PKEY_MIDI_UmpLoopback);
            auto isPing = info.Properties().Lookup(STRING_PKEY_MIDI_UmpPing);

            auto uniqueIdentifier = info.Properties().Lookup(STRING_PKEY_MIDI_UniqueIdentifier);

            auto supportsMidi2Protocol = info.Properties().Lookup(STRING_PKEY_MIDI_EndpointSupportsMidi2Protocol);
            auto supportsMidi1Protocol = info.Properties().Lookup(STRING_PKEY_MIDI_EndpointSupportsMidi1Protocol);
            auto supportsReceivingJRTimestamps = info.Properties().Lookup(STRING_PKEY_MIDI_EndpointSupportsReceivingJRTimestamps);
            auto supportsSendingJRTimestamps = info.Properties().Lookup(STRING_PKEY_MIDI_EndpointSupportsSendingJRTimestamps);
            auto umpVersionMajor = info.Properties().Lookup(STRING_PKEY_MIDI_EndpointUmpVersionMajor);
            auto umpVersionMinor = info.Properties().Lookup(STRING_PKEY_MIDI_EndpointUmpVersionMinor);
            auto endpointProvidedName = info.Properties().Lookup(STRING_PKEY_MIDI_EndpointProvidedName);
            auto endpointProvidedProductInstanceId = info.Properties().Lookup(STRING_PKEY_MIDI_EndpointProvidedProductInstanceId);
            auto deviceIdentificationBin = info.Properties().Lookup(STRING_PKEY_MIDI_DeviceIdentification);


            // user-supplied info
            auto userSuppliedName = info.Properties().Lookup(STRING_PKEY_MIDI_UserSuppliedEndpointName);
            auto userSuppliedDescription = info.Properties().Lookup(STRING_PKEY_MIDI_UserSuppliedDescription);
            auto userSuppliedImagePath = info.Properties().Lookup(STRING_PKEY_MIDI_UserSuppliedImagePath);


            // Group Terminal Blocks
            auto groupTerminalsInBin = info.Properties().Lookup(STRING_PKEY_MIDI_IN_GroupTerminalBlocks);
            auto groupTerminalsOutBin = info.Properties().Lookup(STRING_PKEY_MIDI_OUT_GroupTerminalBlocks);



            // Function Blocks
            auto functionBlocksAreStatic = info.Properties().Lookup(STRING_PKEY_MIDI_FunctionBlocksAreStatic);
            auto functionBlocksBin = info.Properties().Lookup(STRING_PKEY_MIDI_FunctionBlocks);



            m_deviceInformation = info;
            m_id = info.Id();
            m_name = info.Name();   // todo: fold in from settings
            m_transportSuppliedName = info.Name();
            //m_parentDeviceId = info.

        }
    }

    _Use_decl_annotations_
    void MidiEndpointDeviceInformation::InternalUpdate(
        winrt::hstring const& /*deviceId*/) noexcept
    {

    }


}
