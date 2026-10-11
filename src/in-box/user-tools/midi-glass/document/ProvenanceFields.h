// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// What the "who made it" fields say, and how typing in them changes a provenance block.
// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "ContentProvenance.h"

#include <sal.h>
#include <optional>
#include <string>

namespace glass
{
    // The parts of a provenance block a person types or picks. The id, the date, the tool and
    // what it was based on are written by the app, never typed.
    struct ProvenanceFields
    {
        std::wstring Author{};
        std::wstring Organization{};
        std::wstring Url{};
        std::wstring License{};
        std::wstring Version{};

        // Empty when nothing is picked, which leaves the block's own term as it is.
        std::wstring DigitalSourceType{};
        std::wstring HumanOversightLevel{};
        std::wstring AiModelName{};
    };

    ProvenanceFields FieldsOf(_In_ std::optional<midiapp::ContentProvenance> const& provenance);

    struct AppliedProvenanceFields
    {
        // Empty when there was no block and nothing was typed, so looking at an older file's
        // details never writes a block into it.
        std::optional<midiapp::ContentProvenance> Provenance{};

        // The website wasn't a plain https address, so the block kept the one it had.
        bool WebsiteRejected{ false };
    };

    // Everything typed is cleaned the way a file is read. A source the person picked that
    // involves no AI drops the AI details, and a term this build doesn't know keeps them.
    AppliedProvenanceFields ApplyProvenanceFields(
        _In_ std::optional<midiapp::ContentProvenance> const& current,
        _In_ ProvenanceFields const& fields);
}
