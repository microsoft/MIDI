// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Update.MidiRuntimeRelease.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Update::implementation
{
    struct MidiRuntimeRelease : MidiRuntimeReleaseT<MidiRuntimeRelease>
    {
        MidiRuntimeRelease() = default;

        midi2::Utilities::Update::MidiRuntimeUpdateReleaseType Type() const noexcept { return m_type; }
        winrt::hstring Source() const noexcept { return m_source; }
        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring Description() const noexcept { return m_description; }
        winrt::hstring VersionFull() const noexcept { return m_versionFull; }

        uint32_t VersionMajor() const noexcept { return m_versionMajor; }
        uint32_t VersionMinor() const noexcept { return m_versionMinor; }
        uint32_t VersionRevision() const noexcept { return m_versionRevision; }
        uint32_t VersionDateNumber() const noexcept { return m_versionDateNumber; }
        uint32_t VersionTimeNumber() const noexcept { return m_versionTimeNumber; }
        foundation::Uri ReleasePageUri() const noexcept { return m_releasePageUri; }
        foundation::Uri DirectDownloadUriX64() const noexcept { return m_directDownloadUriX64; }
        foundation::Uri DirectDownloadUriArm64() const noexcept { return m_directDownloadUriArm64; }

        void InternalInitialize(
            _In_ midi2::Utilities::Update::MidiRuntimeUpdateReleaseType const type,
            _In_ winrt::hstring const& source,
            _In_ winrt::hstring const& name,
            _In_ winrt::hstring const& description,
            _In_ winrt::hstring const& versionFull,
            _In_ uint32_t const versionMajor,
            _In_ uint32_t const versionMinor,
            _In_ uint32_t const versionRevision,
            _In_ uint32_t const versionDateNumber,
            _In_ uint32_t const versionTimeNumber,
            _In_ foundation::Uri const& releasePageUri,
            _In_ foundation::Uri const& directDownloadUriX64,
            _In_ foundation::Uri const& directDownloadUriArm64
            ) noexcept;

    private:

        midi2::Utilities::Update::MidiRuntimeUpdateReleaseType m_type{};
        winrt::hstring m_source{};
        winrt::hstring m_name {};
        winrt::hstring m_description{};

        winrt::hstring m_versionFull{};
        uint32_t m_versionMajor{};
        uint32_t m_versionMinor{};
        uint32_t m_versionRevision{};
        uint32_t m_versionDateNumber{};
        uint32_t m_versionTimeNumber{};

        foundation::Uri m_releasePageUri{ nullptr };
        foundation::Uri m_directDownloadUriX64{ nullptr };
        foundation::Uri m_directDownloadUriArm64{ nullptr };

    };
}
