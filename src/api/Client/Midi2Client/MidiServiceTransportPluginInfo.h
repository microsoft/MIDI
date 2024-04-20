// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiServiceTransportPluginInfo.g.h"

namespace MIDI_CPP_NAMESPACE::implementation
{
    struct MidiServiceTransportPluginInfo : MidiServiceTransportPluginInfoT<MidiServiceTransportPluginInfo>
    {
        MidiServiceTransportPluginInfo() = default;

        winrt::guid Id() const { return m_classId; }
        //winrt::hstring RegistryKey() const { return m_registryKey; }
        //bool IsEnabled() const { return m_isEnabled; }
        winrt::hstring Name() const { return m_name; }
        winrt::hstring Mnemonic() const { return m_mnemonic; }
        winrt::hstring Description() const { return m_description; }
        winrt::hstring SmallImagePath() const { return m_iconPath; }
        winrt::hstring Author() const { return m_author; }
        winrt::hstring Version() const { return m_version; }

        //winrt::hstring ServicePluginFileName() const { return m_servicePluginFileName; }
        bool IsSystemManaged() const { return m_isSystemManaged; }
        bool IsRuntimeCreatableByApps() const { return m_isRuntimeCreatableByApps; }
        bool IsRuntimeCreatableBySettings() const { return m_isRuntimeCreatableBySettings; }
        bool IsClientConfigurable() const { return m_isClientConfigurable; }

        winrt::hstring ClientConfigurationAssemblyName() const { return m_clientConfigurationAssemblyName; }


        void InternalInitialize(
            _In_ winrt::guid const id,
            _In_ winrt::hstring const name,
            _In_ winrt::hstring const mnemonic,
            _In_ winrt::hstring const description,
            _In_ winrt::hstring const smallImagePath,
            _In_ winrt::hstring const author,
            _In_ winrt::hstring const version,
            _In_ bool const isSystemManaged,
            _In_ bool const isRuntimeCreatableByApps,
            _In_ bool const isRuntimeCreatableBySettings,
            _In_ bool const isClientConfigurable,
            _In_ winrt::hstring clientConfigurationAssemblyName
        )
        {
            m_classId = id;
            m_name = name;
            m_mnemonic = mnemonic;
            m_description = description;
            m_iconPath = smallImagePath;
            m_author = author;
            m_version = version;
            m_isSystemManaged = isSystemManaged;
            m_isRuntimeCreatableByApps = isRuntimeCreatableByApps;
            m_isRuntimeCreatableBySettings = isRuntimeCreatableBySettings;
            m_isClientConfigurable = isClientConfigurable;
            m_clientConfigurationAssemblyName = clientConfigurationAssemblyName;
        }



    private:
        winrt::guid m_classId{};
        winrt::hstring m_registryKey{};
        bool m_isEnabled{ false };
        winrt::hstring m_name{};
        winrt::hstring m_shortName{};
        winrt::hstring m_mnemonic{};
        winrt::hstring m_description{};
        winrt::hstring m_iconPath{};
        winrt::hstring m_author{};
        winrt::hstring m_servicePluginFileName{};
        bool m_isSystemManaged{ false };
        bool m_isRuntimeCreatableByApps{ false };
        bool m_isRuntimeCreatableBySettings{ false };
        bool m_isClientConfigurable{ false };
        winrt::hstring m_clientConfigurationAssemblyName{};
        winrt::hstring m_version{};
    };
}
