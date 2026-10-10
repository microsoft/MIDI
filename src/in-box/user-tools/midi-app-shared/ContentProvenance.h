// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Who made a file, with what, and from what. Self-stated: only a pack signature proves a publisher.
// Field names follow C2PA Content Credentials so the block can be carried into a C2PA manifest.

#include <sal.h>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

namespace midiapp
{
    constexpr wchar_t ProvenanceKey[] = L"provenance";

    // IPTC digital source types (short form of http://cv.iptc.org/newscodes/digitalsourcetype/...).
    namespace DigitalSourceTypes
    {
        constexpr wchar_t DigitalCreation[] = L"digitalCreation";
        constexpr wchar_t DigitalCapture[] = L"digitalCapture";
        constexpr wchar_t TrainedAlgorithmicMedia[] = L"trainedAlgorithmicMedia";
        constexpr wchar_t CompositeWithTrainedAlgorithmicMedia[] = L"compositeWithTrainedAlgorithmicMedia";
        constexpr wchar_t CompositeSynthetic[] = L"compositeSynthetic";
        constexpr wchar_t Composite[] = L"composite";
        constexpr wchar_t AlgorithmicMedia[] = L"algorithmicMedia";
    }

    // C2PA 2.4 humanOversightLevel values, spelled as C2PA spells them.
    namespace HumanOversightLevels
    {
        constexpr wchar_t FullyAutonomous[] = L"fully_autonomous";
        constexpr wchar_t PromptGuided[] = L"prompt_guided";
        constexpr wchar_t HumanValidated[] = L"human_validated";
    }

    constexpr size_t MaximumProvenanceNameLength = 256;
    constexpr size_t MaximumProvenanceIdLength = 128;
    constexpr size_t MaximumProvenanceVersionLength = 64;
    constexpr size_t MaximumProvenanceUrlLength = 2048;

    // What an item was made from (a C2PA parentOf ingredient).
    struct ProvenanceSource
    {
        std::wstring Name{};
        std::wstring Author{};
        std::wstring Id{};
        std::wstring Version{};
        bool BuiltIn{ false };

        winrt::Windows::Data::Json::JsonObject Unknown{ nullptr };

        bool IsEmpty() const noexcept;
    };

    struct ContentProvenance
    {
        // Stays the same across versions of one item, so an update can be told from a namesake.
        std::wstring Id{};
        std::wstring Version{};

        std::wstring Author{};
        std::wstring Organization{};
        std::wstring Url{};

        // SPDX license expression.
        std::wstring License{};

        // RFC 3339.
        std::wstring Created{};

        std::wstring Tool{};

        // Kept as written, so a term this build doesn't know still round-trips.
        std::wstring DigitalSourceType{};

        // The C2PA AI disclosure assertion.
        std::wstring HumanOversightLevel{};
        std::wstring AiModelName{};
        winrt::Windows::Data::Json::JsonObject AiDisclosureUnknown{ nullptr };

        std::optional<ProvenanceSource> BasedOn{};

        winrt::Windows::Data::Json::JsonObject Unknown{ nullptr };

        bool IsEmpty() const noexcept;
    };

    // Set once in settings and used for everything the person makes on this PC.
    struct AuthorProfile
    {
        std::wstring Name{};
        std::wstring Organization{};
        std::wstring Url{};
        std::wstring License{};
    };

    std::optional<ContentProvenance> ReadProvenance(
        _In_ winrt::Windows::Data::Json::JsonObject const& root) noexcept;

    ContentProvenance ProvenanceFromJson(
        _In_ winrt::Windows::Data::Json::JsonObject const& block) noexcept;

    winrt::Windows::Data::Json::JsonObject ProvenanceToJson(
        _In_ ContentProvenance const& provenance) noexcept;

    // Fixed key order, two-space indents starting at indentDepth, for writers that keep key order.
    std::wstring ProvenanceToJsonText(
        _In_ ContentProvenance const& provenance,
        _In_ int32_t indentDepth) noexcept;

    // Also strips direction overrides, zero-width spaces and tag characters, which can disguise a name.
    std::wstring SanitizeProvenanceText(
        _In_ std::wstring_view value,
        _In_ size_t maximumLength) noexcept;

    // https with a host and no user name before the host.
    bool IsSafeWebLink(_In_ std::wstring_view url) noexcept;

    std::wstring NormalizeDigitalSourceType(_In_ std::wstring_view value) noexcept;

    bool IsKnownDigitalSourceType(_In_ std::wstring_view value) noexcept;
    bool IsKnownHumanOversightLevel(_In_ std::wstring_view value) noexcept;
    bool InvolvesGenerativeAi(_In_ std::wstring_view digitalSourceType) noexcept;

    std::wstring NewProvenanceId() noexcept;
    std::wstring CurrentProvenanceTime() noexcept;

    // A FILETIME, or 0 when the text is neither an RFC 3339 date-time nor a plain date.
    int64_t ParseProvenanceTime(_In_ std::wstring_view value) noexcept;

    // Part by part, so "1.10" comes after "1.9".
    int32_t CompareContentVersions(_In_ std::wstring_view left, _In_ std::wstring_view right) noexcept;

    ContentProvenance StartProvenance(
        _In_ AuthorProfile const& author,
        _In_ std::wstring_view toolName) noexcept;

    ProvenanceSource SourceOf(
        _In_ std::wstring_view name,
        _In_ std::optional<ContentProvenance> const& provenance) noexcept;

    // A new item made from another: its own id, credited to this author, based on the original.
    // Made-with-AI carries over as a composite, because the content it describes did.
    ContentProvenance DeriveProvenance(
        _In_ AuthorProfile const& author,
        _In_ std::wstring_view toolName,
        _In_ std::wstring_view sourceName,
        _In_ std::optional<ContentProvenance> const& source,
        _In_ bool sourceIsBuiltIn) noexcept;

    // Shared by every Windows MIDI Services tool on this PC.
    AuthorProfile LoadAuthorProfile() noexcept;
    void SaveAuthorProfile(_In_ AuthorProfile const& profile) noexcept;
}
