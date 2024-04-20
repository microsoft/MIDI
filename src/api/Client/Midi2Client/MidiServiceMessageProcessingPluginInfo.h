// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiServiceMessageProcessingPluginInfo.g.h"

namespace MIDI_CPP_NAMESPACE::implementation
{
    struct MidiServiceMessageProcessingPluginInfo : MidiServiceMessageProcessingPluginInfoT<MidiServiceMessageProcessingPluginInfo>
    {
        MidiServiceMessageProcessingPluginInfo() = default;

        winrt::guid Id() const { return m_classId; }
        //winrt::hstring RegistryKey() const { return m_registryKey; }
        //bool IsEnabled() const { return m_isEnabled; }
        winrt::hstring Name() const { return m_name; }
        winrt::hstring Description() const { return m_description; }
        winrt::hstring SmallImagePath() const { return m_iconPath; }
        winrt::hstring Author() const { return m_author; }
        winrt::hstring Version() const { return m_version; }
        //winrt::hstring ServicePluginFileName() const { return m_servicePluginFileName; }

        bool SupportsMultipleInstancesPerDevice() const { return m_supportsMultipleInstancesPerDevice; }

        bool IsSystemManaged() const { return m_isSystemManaged; }
        bool IsClientConfigurable() const { return m_isClientConfigurable; }

        winrt::hstring ClientConfigurationAssemblyName() const { return m_clientConfigurationAssemblyName; }

    private:
        winrt::guid m_classId{};
        //winrt::hstring m_registryKey{};
        //bool m_isEnabled{ false };
        winrt::hstring m_name{};
        winrt::hstring m_shortName{};
        winrt::hstring m_description{};
        winrt::hstring m_iconPath{};
        winrt::hstring m_author{};
        //winrt::hstring m_servicePluginFileName{};
        bool m_isSystemManaged{ false };
        bool m_isClientConfigurable{ false };
        winrt::hstring m_clientConfigurationAssemblyName{};
        bool m_supportsMultipleInstancesPerDevice{ false };
        winrt::hstring m_version{};

    };
}
