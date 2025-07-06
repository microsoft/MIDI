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

        midi2::Utilities::Update::MidiRuntimeUpdateReleaseTypes Type() const noexcept { return m_type; }
        winrt::hstring Source() const noexcept { return m_source; }
        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring Description() const noexcept { return m_description; }
        winrt::hstring VersionFull() const noexcept { return m_versionFull; }

        foundation::DateTime BuildDate() const noexcept { return m_buildDate; }

        uint16_t VersionMajor() const noexcept { return m_versionMajor; }
        uint16_t VersionMinor() const noexcept { return m_versionMinor; }
        uint16_t VersionPatch() const noexcept { return m_versionPatch; }
        winrt::hstring Preview() const noexcept{ return m_preview; }

        foundation::Uri ReleaseNotesUri() const noexcept { return m_releaseNotesUri; }
        foundation::Uri DirectDownloadUriX64() const noexcept { return m_directDownloadUriX64; }
        foundation::Uri DirectDownloadUriArm64() const noexcept { return m_directDownloadUriArm64; }


        winrt::hstring ToString() const noexcept;


        void InternalInitialize(
            _In_ midi2::Utilities::Update::MidiRuntimeUpdateReleaseTypes const type,
            _In_ winrt::hstring const& source,
            _In_ winrt::hstring const& name,
            _In_ winrt::hstring const& description,
            _In_ foundation::DateTime const& buildDate,
            _In_ winrt::hstring const& versionFull,
            _In_ uint16_t const versionMajor,
            _In_ uint16_t const versionMinor,
            _In_ uint16_t const versionPatch,
            _In_ winrt::hstring const& preview,
            _In_ foundation::Uri const& releaseNotesUri,
            _In_ foundation::Uri const& directDownloadUriX64,
            _In_ foundation::Uri const& directDownloadUriArm64
            ) noexcept;

    private:

        midi2::Utilities::Update::MidiRuntimeUpdateReleaseTypes m_type{};
        winrt::hstring m_source{};
        winrt::hstring m_name {};
        winrt::hstring m_description{};

        foundation::DateTime m_buildDate{};

        winrt::hstring m_versionFull{};
        uint16_t m_versionMajor{};
        uint16_t m_versionMinor{};
        uint16_t m_versionPatch{};
        winrt::hstring m_preview{};

        foundation::Uri m_releaseNotesUri{ nullptr };
        foundation::Uri m_directDownloadUriX64{ nullptr };
        foundation::Uri m_directDownloadUriArm64{ nullptr };

    };
}
