// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

namespace midisettings
{
    // appearance and window placement live in the shared base, but stay reachable through the
    // app's own namespace so call sites read the same
    using midiapp::AppTheme;
    using midiapp::WindowBackdrop;
    using midiapp::WindowPlacementInfo;

    enum class EndpointViewMode : uint32_t
    {
        Cards = 0,
        List = 1
    };

    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        EndpointViewMode ViewMode() const noexcept { return m_viewMode; }
        void ViewMode(EndpointViewMode value) noexcept;

        // Transport code the endpoint list is filtered to, or empty for all of them.
        std::wstring TransportFilter() const noexcept { return m_transportFilter; }
        void TransportFilter(std::wstring const& value) noexcept;

        // The folder the configuration file was last copied to.
        std::wstring LastConfigCopyFolder() const noexcept { return m_lastConfigCopyFolder; }
        void LastConfigCopyFolder(std::wstring const& value) noexcept;

    private:
        AppSettings() noexcept;

        EndpointViewMode m_viewMode{ EndpointViewMode::Cards };
        std::wstring m_transportFilter{};
        std::wstring m_lastConfigCopyFolder{};
    };
}
