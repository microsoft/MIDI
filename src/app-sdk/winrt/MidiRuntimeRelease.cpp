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
    _Use_decl_annotations_
    void MidiRuntimeRelease::InternalInitialize(
        midi2::Utilities::Update::MidiRuntimeUpdateReleaseType const type,
        winrt::hstring const& source,
        winrt::hstring const& name,
        winrt::hstring const& description,
        winrt::hstring const& versionFull,
        uint32_t const versionMajor,
        uint32_t const versionMinor,
        uint32_t const versionRevision,
        uint32_t const versionDateNumber,
        uint32_t const versionTimeNumber,
        foundation::Uri const& releasePageUri,
        foundation::Uri const& directDownloadUriX64,
        foundation::Uri const& directDownloadUriArm64
    ) noexcept
    {
        m_type = type;
        m_source = source;
        m_name = name;
        m_description = description;

        m_versionFull = versionFull;
        m_versionMajor = versionMajor;
        m_versionMinor = versionMinor;
        m_versionRevision = versionRevision;
        m_versionDateNumber = versionDateNumber;
        m_versionTimeNumber = versionTimeNumber;

        m_releasePageUri = releasePageUri;
        m_directDownloadUriX64 = directDownloadUriX64;
        m_directDownloadUriArm64 = directDownloadUriArm64;
    }
}
