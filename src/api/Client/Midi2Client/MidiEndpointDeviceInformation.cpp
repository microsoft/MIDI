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
        auto midiDevices = winrt::single_threaded_vector<midi2::MidiEndpointDeviceInformation>();

        try
        {
            auto devices = Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
                MidiEndpointConnection::GetDeviceSelector(),
                GetAdditionalPropertiesList()
            ).get();


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
        }
        catch (...)
        {
            // TODO: Log this
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
    winrt::hstring MidiEndpointDeviceInformation::GetStringProperty(
        winrt::hstring key,
        winrt::hstring defaultValue) const noexcept
    {
        if (m_deviceInformation == nullptr) return defaultValue;

        try
        {
            return winrt::unbox_value<winrt::hstring>(m_deviceInformation.Properties().Lookup(key));
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    _Use_decl_annotations_
    uint8_t MidiEndpointDeviceInformation::GetByteProperty(
        winrt::hstring key,
        uint8_t defaultValue) const noexcept
    {
        if (m_deviceInformation == nullptr) return defaultValue;

        try
        {
            return winrt::unbox_value<uint8_t>(m_deviceInformation.Properties().Lookup(key));
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    _Use_decl_annotations_
    bool MidiEndpointDeviceInformation::GetBoolProperty(
        winrt::hstring key,
        bool defaultValue) const noexcept
    {
        if (m_deviceInformation == nullptr) return defaultValue;

        try
        {
            return (bool)(m_deviceInformation.Properties().Lookup(key));
        }
        catch (...)
        {
            return defaultValue;
        }
    }


    winrt::hstring MidiEndpointDeviceInformation::Id() const noexcept
    { 
        if (m_deviceInformation != nullptr)
        {
            return m_deviceInformation.Id();
        }
        else
        {
            return L"";
        }
    }

    winrt::hstring MidiEndpointDeviceInformation::ParentDeviceId() const noexcept
    {
        // TODO

        return L"";
    }


    winrt::hstring MidiEndpointDeviceInformation::Name() const noexcept
    {
        // user-supplied name trumps all others
        if (UserSuppliedName() != L"") return UserSuppliedName();

        // endpoint name discovered in-protocol is next highest
        if (EndpointSuppliedName() != L"") return EndpointSuppliedName();

        // transport-supplied name is last
        if (TransportSuppliedName() != L"") return TransportSuppliedName();

        // this is typically the same as the transport-supplied name
        if (m_deviceInformation != nullptr)
            return m_deviceInformation.Name();

        return L"Unknown";
    }

    midi2::MidiEndpointNativeDataFormat MidiEndpointDeviceInformation::NativeDataFormat() const noexcept
    {
        // TODO
        return midi2::MidiEndpointNativeDataFormat::Unknown;
    }

    midi2::MidiEndpointDevicePurpose MidiEndpointDeviceInformation::EndpointPurpose() const noexcept
    {
        // TODO
        return midi2::MidiEndpointDevicePurpose::NormalMessageEndpoint;

    }

    midi2::MidiProtocol MidiEndpointDeviceInformation::ConfiguredProtocol() const noexcept
    {
        // TODO
        return midi2::MidiProtocol::Default;
    }


    collections::IVectorView<midi2::MidiFunctionBlock> MidiEndpointDeviceInformation::FunctionBlocks() const noexcept
    {
        collections::IVector<midi2::MidiFunctionBlock> blocks{ winrt::single_threaded_vector<midi2::MidiFunctionBlock>() };

        // TODO: Populate from property

        return blocks.GetView();
    }


    collections::IVectorView<midi2::MidiGroupTerminalBlock> MidiEndpointDeviceInformation::GroupTerminalBlocks() const noexcept
    {
        collections::IVector<midi2::MidiGroupTerminalBlock> blocks{ winrt::single_threaded_vector<midi2::MidiGroupTerminalBlock>() };

        // TODO: Populate from property

        return blocks.GetView();
    }






    _Use_decl_annotations_
    void MidiEndpointDeviceInformation::InternalUpdateFromDeviceInformation(
        winrt::Windows::Devices::Enumeration::DeviceInformation const& info) noexcept
    {
        if (info != nullptr)
        {
            m_deviceInformation = info;
        }
    }


}
