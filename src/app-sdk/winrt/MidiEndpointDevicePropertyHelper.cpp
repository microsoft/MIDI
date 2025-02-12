// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "MidiDefs.h"

#include "MidiEndpointDevicePropertyHelper.h"
#include "MidiEndpointDevicePropertyHelper.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    bool MidiEndpointDevicePropertyHelper::m_initialized{ false };
    collections::IMap<winrt::hstring, winrt::hstring>  MidiEndpointDevicePropertyHelper::m_propertyFriendlyNames{ winrt::single_threaded_map<winrt::hstring, winrt::hstring>() };
    winrt::guid MidiEndpointDevicePropertyHelper::m_midiPropertyGuid;


    collections::IMapView<winrt::hstring, winrt::hstring> MidiEndpointDevicePropertyHelper::GetAllMidiProperties() noexcept
    {
        if (!m_initialized)
        {
            BuildPropertyMap();
        }

        return m_propertyFriendlyNames.GetView();
    }

    _Use_decl_annotations_
    winrt::hstring MidiEndpointDevicePropertyHelper::BuildPropertyStringKey(winrt::guid const& fmtid, uint32_t const pid)
    {
        return winrt::hstring{ internal::ToUpperTrimmedWStringCopy(internal::GuidToString(fmtid)) } + winrt::hstring{ MIDI_STRING_PKEY_PID_SEPARATOR } + winrt::to_hstring(pid);
    }

    _Use_decl_annotations_
    bool MidiEndpointDevicePropertyHelper::IsMidiPropertyKey(winrt::hstring const& key) noexcept
    {
        if (!m_initialized)
        {
            BuildPropertyMap();
        }

        return m_propertyFriendlyNames.HasKey(internal::TrimmedHStringCopy(key));
    }


    _Use_decl_annotations_
    bool MidiEndpointDevicePropertyHelper::IsMidiPropertyKey(winrt::guid fmtid, uint32_t const pid) noexcept
    {
        winrt::hstring propKeyString = BuildPropertyStringKey(fmtid, pid);

        return IsMidiPropertyKey(propKeyString);
    }


    _Use_decl_annotations_
    winrt::hstring MidiEndpointDevicePropertyHelper::GetMidiPropertyNameFromPropertyKey(winrt::guid const& fmtid, uint32_t const pid) noexcept
    {
        winrt::hstring propKeyString = BuildPropertyStringKey(fmtid, pid);

        return GetMidiPropertyNameFromPropertyKey(propKeyString);
    }

    _Use_decl_annotations_
    winrt::hstring MidiEndpointDevicePropertyHelper::GetMidiPropertyNameFromPropertyKey(winrt::hstring const& key) noexcept
    {
        if (!m_initialized)
        {
            BuildPropertyMap();
        }

        if (m_propertyFriendlyNames.HasKey(key))
        {
            return m_propertyFriendlyNames.Lookup(key);
        }

        return L"";
    }

    _Use_decl_annotations_
    void MidiEndpointDevicePropertyHelper::AddSingleMapEntry(winrt::hstring const& stringPropKey, winrt::hstring const& friendlyName)
    {
        m_propertyFriendlyNames.Insert(stringPropKey, friendlyName);
    }


#define PROPERTY_PAIR(p) p, winrt::hstring{ L#p }

    void MidiEndpointDevicePropertyHelper::BuildPropertyMap()
    {
        m_midiPropertyGuid = winrt::guid{ internal::StringToGuid(MIDI_STRING_PKEY_GUID) };

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_TransportLayer));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_TransportCode));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_NativeDataFormat));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_SupportsMulticlient));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_SupportedDataFormats));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_ManufacturerName));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_GenerateIncomingTimestamp));

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointName));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_Description));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_CustomPortNumber));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_ServiceAssignedPortNumber));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_PortAssignedGroupIndex));

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_GroupTerminalBlocks));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_AssociatedUMP));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_SerialNumber));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_UsbVID));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_UsbPID));

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointDevicePurpose));

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointSupportsMidi2Protocol));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointSupportsMidi1Protocol));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointSupportsReceivingJRTimestamps));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointSupportsSendingJRTimestamps));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointUmpVersionMajor));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointUmpVersionMinor));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointInformationLastUpdateTime));

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_DeviceIdentity));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_DeviceIdentityLastUpdateTime));

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointProvidedName));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointProvidedProductInstanceId));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointProvidedNameLastUpdateTime));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointProvidedProductInstanceIdLastUpdateTime));

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockDeclaredCount));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlocksAreStatic));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlocksLastUpdateTime));

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock00));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock01));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock02));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock03));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock04));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock05));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock06));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock07));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock08));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock09));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock10));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock11));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock12));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock13));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock14));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock15));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock16));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock17));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock18));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock19));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock20));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock21));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock22));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock23));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock24));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock25));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock26));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock27));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock28));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock29));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock30));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlock31));

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName00));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName01));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName02));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName03));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName04));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName05));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName06));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName07));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName08));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName09));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName10));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName11));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName12));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName13));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName14));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName15));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName16));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName17));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName18));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName19));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName20));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName21));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName22));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName23));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName24));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName25));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName26));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName27));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName28));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName29));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName30));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_FunctionBlockName31));

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointConfiguredProtocol));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointConfiguredToSendJRTimestamps));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointConfiguredToReceiveJRTimestamps));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_EndpointConfigurationLastUpdateTime));

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_CustomEndpointName));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_CustomLargeImagePath));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_CustomSmallImagePath));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_CustomDescription));

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_RequiresNoteOffTranslation));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_RecommendedCCAutomationIntervalMS));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_SupportsMidiPolyphonicExpression));

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_MidiOutCalculatedLatencyTicks));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_MidiOutCustomLatencyTicks));
        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_MidiOutLatencyTicksUserOverride));

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator));

        AddSingleMapEntry(PROPERTY_PAIR(STRING_PKEY_MIDI_UseOldMidi1PortNamingScheme));

        m_initialized = true;
    }
}
