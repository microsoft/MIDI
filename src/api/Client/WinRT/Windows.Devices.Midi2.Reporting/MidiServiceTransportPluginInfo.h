// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiServiceTransportPluginInfo.g.h"


namespace winrt::Windows::Devices::Midi2::Reporting::implementation
{
    struct MidiServiceTransportPluginInfo : MidiServiceTransportPluginInfoT<MidiServiceTransportPluginInfo>
    {
        MidiServiceTransportPluginInfo() = default;

        winrt::guid TransportId() const noexcept { return m_transportId; }
        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring TransportCode() const noexcept { return m_transportCode; }
        winrt::hstring Description() const noexcept { return m_description; }
        winrt::hstring ImageFileName() const noexcept { return m_imageFileName; }
        winrt::hstring Author() const noexcept { return m_author; }
        winrt::hstring Version() const noexcept { return m_version; }
        bool IsRuntimeCreatableByApps() const noexcept { return m_isRuntimeCreatableByApps; }
        bool IsRuntimeCreatableBySettings() const noexcept { return m_isRuntimeCreatableBySettings; }
        bool IsSystemManaged() const noexcept { return m_isSystemManaged; }
        bool CanConfigure() const noexcept { return m_canConfigure; }

        bool InternalInitialize(
            _In_ winrt::guid const& transportId,
            _In_ winrt::hstring const& name,
            _In_ winrt::hstring const& transportCode,
            _In_ winrt::hstring const& description,
            _In_ winrt::hstring const& imageFileName,
            _In_ winrt::hstring const& author,
            _In_ winrt::hstring const& version,
            _In_ bool isRuntimeCreatableByApps,
            _In_ bool isRuntimeCreatableBySettings,
            _In_ bool isSystemManaged,
            _In_ bool canConfigure)
        {
            m_transportId = transportId;
            m_name = name;
            m_transportCode = transportCode;
            m_description = description;
            m_imageFileName = imageFileName;
            m_author = author;
            m_version = version;
            m_isRuntimeCreatableByApps = isRuntimeCreatableByApps;
            m_isRuntimeCreatableBySettings = isRuntimeCreatableBySettings;
            m_isSystemManaged = isSystemManaged;
            m_canConfigure = canConfigure;

            return true;
        }

    private:
        winrt::guid m_transportId;
        winrt::hstring m_name;
        winrt::hstring m_transportCode;
        winrt::hstring m_description;
        winrt::hstring m_imageFileName;
        winrt::hstring m_author;
        winrt::hstring m_version;
        bool m_isRuntimeCreatableByApps{ false };
        bool m_isRuntimeCreatableBySettings{ false };
        bool m_isSystemManaged{ false };
        bool m_canConfigure{ false };
    };
}
