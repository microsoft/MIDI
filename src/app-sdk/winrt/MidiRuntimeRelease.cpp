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

    foundation::Uri MidiRuntimeRelease::DirectDownloadUriForCurrentRuntimeArchitecture() const noexcept
    {
#if defined(_M_ARM64EC) || defined(_M_ARM64)
        // Arm64 and Arm64EC use same Arm64X binaries
        return DirectDownloadUriArm64();
#elif defined(_M_AMD64)
        // other 64 bit Intel/AMD
        return DirectDownloadUriX64();
#endif
    }


    _Use_decl_annotations_
    void MidiRuntimeRelease::InternalInitialize(
        midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes const type,
        winrt::hstring const& source,
        winrt::hstring const& name,
        winrt::hstring const& description,
        foundation::DateTime const& buildDate,
        midi2::Utilities::RuntimeInformation::MidiRuntimeVersion const& version,
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

        m_version = version;

        m_releaseNotesUri = releaseNotesUri;
        m_directDownloadUriX64 = directDownloadUriX64;
        m_directDownloadUriArm64 = directDownloadUriArm64;
    }
}
