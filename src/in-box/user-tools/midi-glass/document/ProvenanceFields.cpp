// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "ProvenanceFields.h"

namespace glass
{
    namespace
    {
        // The reader cuts the AI details to this length too.
        constexpr size_t MaximumTermLength = 64;

        std::wstring Clean(_In_ std::wstring const& text, _In_ size_t maximumLength)
        {
            return midiapp::SanitizeProvenanceText(text, maximumLength);
        }

        bool IsEmpty(_In_ midiapp::ContentProvenance const& block) noexcept
        {
            return block.Author.empty() && block.Organization.empty() && block.Url.empty() &&
                block.License.empty() && block.Version.empty() && block.DigitalSourceType.empty() &&
                block.HumanOversightLevel.empty() && block.AiModelName.empty();
        }
    }

    _Use_decl_annotations_
    ProvenanceFields FieldsOf(std::optional<midiapp::ContentProvenance> const& provenance)
    {
        ProvenanceFields fields{};

        if (!provenance.has_value())
        {
            return fields;
        }

        fields.Author = provenance->Author;
        fields.Organization = provenance->Organization;
        fields.Url = provenance->Url;
        fields.License = provenance->License;
        fields.Version = provenance->Version;
        fields.DigitalSourceType = provenance->DigitalSourceType;
        fields.HumanOversightLevel = provenance->HumanOversightLevel;
        fields.AiModelName = provenance->AiModelName;

        return fields;
    }

    _Use_decl_annotations_
    AppliedProvenanceFields ApplyProvenanceFields(
        std::optional<midiapp::ContentProvenance> const& current,
        ProvenanceFields const& fields)
    {
        AppliedProvenanceFields result{};

        auto block = current.value_or(midiapp::ContentProvenance{});

        block.Author = Clean(fields.Author, midiapp::MaximumProvenanceNameLength);
        block.Organization = Clean(fields.Organization, midiapp::MaximumProvenanceNameLength);
        block.License = Clean(fields.License, midiapp::MaximumProvenanceNameLength);
        block.Version = Clean(fields.Version, midiapp::MaximumProvenanceVersionLength);

        // What the file already said is never refused, so opening its details and typing
        // somewhere else can't complain about a website nobody touched.
        auto const url = Clean(fields.Url, midiapp::MaximumProvenanceUrlLength);

        if (url.empty() || url == block.Url || midiapp::IsSafeWebLink(url))
        {
            block.Url = url;
        }
        else
        {
            result.WebsiteRejected = true;
        }

        auto const source = midiapp::NormalizeDigitalSourceType(
            Clean(fields.DigitalSourceType, midiapp::MaximumProvenanceNameLength));

        auto const pickedAnother = !source.empty() && source != block.DigitalSourceType;

        if (!source.empty())
        {
            block.DigitalSourceType = source;
        }

        // Made without AI, it takes no AI details from the fields, and picking that drops the ones
        // it had. Only the pick drops them, so a file that says both keeps both until somebody
        // chooses. A term this build has no words for might still involve AI, so the AI fields
        // still count for it.
        auto const withoutAi = midiapp::IsKnownDigitalSourceType(block.DigitalSourceType) &&
            !midiapp::InvolvesGenerativeAi(block.DigitalSourceType);

        if (withoutAi)
        {
            if (pickedAnother)
            {
                block.HumanOversightLevel.clear();
                block.AiModelName.clear();
            }
        }
        else
        {
            auto const oversight = Clean(fields.HumanOversightLevel, MaximumTermLength);

            if (!oversight.empty())
            {
                block.HumanOversightLevel = oversight;
            }

            block.AiModelName = Clean(fields.AiModelName, midiapp::MaximumProvenanceNameLength);
        }

        if (!current.has_value())
        {
            if (IsEmpty(block))
            {
                return result;
            }

            // A new block, for a file that didn't have one. When and with what it was made
            // aren't known, so they aren't guessed.
            block.Id = midiapp::NewProvenanceId();
        }

        result.Provenance = std::move(block);

        return result;
    }
}
