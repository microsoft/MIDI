// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRuntimeRelease.h"
#include "Utilities.Update.MidiRuntimeRelease.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Update::implementation
{
    winrt::hstring MidiRuntimeRelease::ToString() const noexcept
    { 
        // the enum is a Flags enum, but in this use, it should never be more than one of these values

        if (!m_versionFull.empty())
        {
            if (m_type == midi2::Utilities::Update::MidiRuntimeUpdateReleaseTypes::Preview)
            {
                // TODO: This string should come from resources
                return m_versionFull + L" (Preview)";
            }
            else if (m_type == midi2::Utilities::Update::MidiRuntimeUpdateReleaseTypes::Stable)
            {
                // no suffix if stable release
                return m_versionFull;
            }
        }

        // TODO: This shoudl come from resources
        return L"(Invalid Version Information)";
    };


    _Use_decl_annotations_
    void MidiRuntimeRelease::InternalInitialize(
        midi2::Utilities::Update::MidiRuntimeUpdateReleaseTypes const type,
        winrt::hstring const& source,
        winrt::hstring const& name,
        winrt::hstring const& description,
        foundation::DateTime const& buildDate,
        winrt::hstring const& versionFull,
        uint16_t const versionMajor,
        uint16_t const versionMinor,
        uint16_t const versionPatch,
        winrt::hstring const& preview,
        foundation::Uri const& releaseNotesUri,
        foundation::Uri const& directDownloadUriX64,
        foundation::Uri const& directDownloadUriArm64
    ) noexcept
    {
        m_type = type;
        m_source = source;
        m_name = name;
        m_description = description;

        m_buildDate = buildDate;

        m_versionFull = versionFull;
        m_versionMajor = versionMajor;
        m_versionMinor = versionMinor;
        m_versionPatch = versionPatch;

        m_preview = preview;

        m_releaseNotesUri = releaseNotesUri;
        m_directDownloadUriX64 = directDownloadUriX64;
        m_directDownloadUriArm64 = directDownloadUriArm64;
    }
}
