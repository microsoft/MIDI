// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "ContentProvenance.h"
#include "WindowsMidiServicesVersion.h"

namespace midiglass
{
    // What goes in a provenance block's tool field. A product name and a version, so not translated.
    inline std::wstring ProvenanceToolName()
    {
        return std::wstring{ L"Windows MIDI Glass " } + WINDOWS_MIDI_SERVICES_NUGET_BUILD_VERSION_FULL;
    }

    // For something made from scratch on this PC.
    inline midiapp::ContentProvenance NewProvenance()
    {
        return midiapp::StartProvenance(midiapp::LoadAuthorProfile(), ProvenanceToolName());
    }

    // For a copy, or a theme saved from another one.
    inline midiapp::ContentProvenance CopyProvenance(
        _In_ std::wstring_view sourceName,
        _In_ std::optional<midiapp::ContentProvenance> const& source,
        _In_ bool sourceIsBuiltIn)
    {
        return midiapp::DeriveProvenance(
            midiapp::LoadAuthorProfile(), ProvenanceToolName(), sourceName, source, sourceIsBuiltIn);
    }
}
