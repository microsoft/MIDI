// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ProvenanceTests.h"

#include "ContentProvenance.h"
#include "LayoutModel.h"
#include "LayoutSerializer.h"
#include "ThemeModel.h"
#include "ThemeStore.h"

#include <string>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    namespace mjson = winrt::Windows::Data::Json;

    midiapp::ContentProvenance FullProvenance()
    {
        midiapp::ContentProvenance provenance{};

        provenance.Id = L"3f2b8c1e-7d4a-4b9e-9c1f-2a6d8e0b5c47";
        provenance.Version = L"1.2";
        provenance.Author = L"Pat Example";
        provenance.Organization = L"Example Instruments";
        provenance.Url = L"https://example.com/layouts";
        provenance.License = L"CC-BY-4.0";
        provenance.Created = L"2026-10-08T21:14:00Z";
        provenance.Tool = L"MIDI Glass 0.99.91-preview.11";
        provenance.DigitalSourceType = midiapp::DigitalSourceTypes::TrainedAlgorithmicMedia;
        provenance.HumanOversightLevel = midiapp::HumanOversightLevels::PromptGuided;
        provenance.AiModelName = L"Example Model";

        midiapp::ProvenanceSource source{};
        source.Name = L"Studio Dark";
        source.BuiltIn = true;

        provenance.BasedOn = source;

        return provenance;
    }

    midiapp::ContentProvenance ParseBlock(_In_ std::wstring const& text)
    {
        mjson::JsonObject object{ nullptr };

        VERIFY_IS_TRUE(mjson::JsonObject::TryParse(winrt::hstring{ text }, object));

        return midiapp::ProvenanceFromJson(object);
    }

    midiapp::AuthorProfile Sam()
    {
        midiapp::AuthorProfile profile{};
        profile.Name = L"Sam";
        profile.License = L"CC0-1.0";

        return profile;
    }
}

void ProvenanceTests::EveryFieldRoundTrips()
{
    auto const original = FullProvenance();
    auto const text = midiapp::ProvenanceToJsonText(original, 0);
    auto const read = ParseBlock(text);

    VERIFY_ARE_EQUAL(original.Id, read.Id);
    VERIFY_ARE_EQUAL(original.Version, read.Version);
    VERIFY_ARE_EQUAL(original.Author, read.Author);
    VERIFY_ARE_EQUAL(original.Organization, read.Organization);
    VERIFY_ARE_EQUAL(original.Url, read.Url);
    VERIFY_ARE_EQUAL(original.License, read.License);
    VERIFY_ARE_EQUAL(original.Created, read.Created);
    VERIFY_ARE_EQUAL(original.Tool, read.Tool);
    VERIFY_ARE_EQUAL(original.DigitalSourceType, read.DigitalSourceType);
    VERIFY_ARE_EQUAL(original.HumanOversightLevel, read.HumanOversightLevel);
    VERIFY_ARE_EQUAL(original.AiModelName, read.AiModelName);

    VERIFY_IS_TRUE(read.BasedOn.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"Studio Dark" }, read.BasedOn->Name);
    VERIFY_IS_TRUE(read.BasedOn->BuiltIn);

    // Written again, it is the same text.
    VERIFY_ARE_EQUAL(text, midiapp::ProvenanceToJsonText(read, 0));
}

void ProvenanceTests::UnknownFieldsSurviveAtEveryLevel()
{
    std::wstring const text = LR"({
  "id": "abc",
  "author": "Pat",
  "future": { "b": 2, "a": 1 },
  "aiDisclosure": { "humanOversightLevel": "human_validated", "reviewNotes": "checked twice" },
  "basedOn": { "name": "Old one", "manifestHash": "1234" }
})";

    auto const read = ParseBlock(text);
    auto const written = midiapp::ProvenanceToJsonText(read, 0);

    VERIFY_IS_TRUE(written.find(L"\"future\"") != std::wstring::npos);
    VERIFY_IS_TRUE(written.find(L"\"reviewNotes\": \"checked twice\"") != std::wstring::npos);
    VERIFY_IS_TRUE(written.find(L"\"manifestHash\": \"1234\"") != std::wstring::npos);

    VERIFY_ARE_EQUAL(written, midiapp::ProvenanceToJsonText(ParseBlock(written), 0));
}

void ProvenanceTests::TheBlockIsWrittenInAFixedOrder()
{
    auto const text = midiapp::ProvenanceToJsonText(FullProvenance(), 0);

    std::vector<std::wstring> const order{
        L"\"id\"", L"\"version\"", L"\"author\"", L"\"organization\"", L"\"url\"", L"\"license\"",
        L"\"created\"", L"\"tool\"", L"\"digitalSourceType\"", L"\"aiDisclosure\"", L"\"basedOn\"" };

    size_t last{ 0 };

    for (auto const& key : order)
    {
        auto const at = text.find(key);

        VERIFY_IS_TRUE(at != std::wstring::npos);
        VERIFY_IS_TRUE(at >= last);

        last = at;
    }
}

void ProvenanceTests::AStringBasedOnIsTheName()
{
    auto const read = ParseBlock(LR"({ "basedOn": "Cathode" })");

    VERIFY_IS_TRUE(read.BasedOn.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"Cathode" }, read.BasedOn->Name);
}

void ProvenanceTests::AFullIptcAddressIsShortened()
{
    auto const read = ParseBlock(
        LR"({ "digitalSourceType": "http://cv.iptc.org/newscodes/digitalsourcetype/compositeWithTrainedAlgorithmicMedia" })");

    VERIFY_ARE_EQUAL(std::wstring{ L"compositeWithTrainedAlgorithmicMedia" }, read.DigitalSourceType);
    VERIFY_IS_TRUE(midiapp::InvolvesGenerativeAi(read.DigitalSourceType));
    VERIFY_IS_FALSE(midiapp::InvolvesGenerativeAi(midiapp::DigitalSourceTypes::DigitalCreation));
}

void ProvenanceTests::DirectionOverridesAndZeroWidthCharactersAreRemoved()
{
    VERIFY_ARE_EQUAL(std::wstring{ L"PatExample" }, midiapp::SanitizeProvenanceText(L"Pat\u202EExample", 256));
    VERIFY_ARE_EQUAL(std::wstring{ L"Pat Example" }, midiapp::SanitizeProvenanceText(L"  Pat\u200B Example\u2066  ", 256));
    VERIFY_ARE_EQUAL(std::wstring{ L"AB" }, midiapp::SanitizeProvenanceText(L"A\nB", 256));

    // A tag character, as its surrogate pair.
    VERIFY_ARE_EQUAL(std::wstring{ L"Pat" }, midiapp::SanitizeProvenanceText(L"Pat\xDB40\xDC41", 256));

    // A surrogate with no partner.
    VERIFY_ARE_EQUAL(std::wstring{ L"AB" }, midiapp::SanitizeProvenanceText(L"A\xD800" L"B", 256));

    // An ordinary character outside the basic plane stays.
    VERIFY_ARE_EQUAL(std::wstring{ L"Keys \xD83C\xDFB9" }, midiapp::SanitizeProvenanceText(L"Keys \xD83C\xDFB9", 256));
}

void ProvenanceTests::ALongNameIsCutWithoutSplittingACharacter()
{
    VERIFY_ARE_EQUAL(std::wstring{ L"ab" }, midiapp::SanitizeProvenanceText(L"ab\xD83C\xDFB9", 3));
    VERIFY_ARE_EQUAL(std::wstring{ L"abc" }, midiapp::SanitizeProvenanceText(L"abcdef", 3));
}

void ProvenanceTests::OnlyAPlainHttpsAddressIsALink()
{
    VERIFY_IS_TRUE(midiapp::IsSafeWebLink(L"https://example.com/layouts"));

    VERIFY_IS_FALSE(midiapp::IsSafeWebLink(L"http://example.com"));
    VERIFY_IS_FALSE(midiapp::IsSafeWebLink(L"https://user@example.com"));
    VERIFY_IS_FALSE(midiapp::IsSafeWebLink(L"https://user:secret@example.com"));
    VERIFY_IS_FALSE(midiapp::IsSafeWebLink(L"javascript:alert(1)"));
    VERIFY_IS_FALSE(midiapp::IsSafeWebLink(L"https://exa mple.com"));
    VERIFY_IS_FALSE(midiapp::IsSafeWebLink(L"https://example.com/\u202Egnp.exe"));
    VERIFY_IS_FALSE(midiapp::IsSafeWebLink(L"file:///C:/Windows"));
}

void ProvenanceTests::VersionsCompareNumberByNumber()
{
    VERIFY_ARE_EQUAL(1, midiapp::CompareContentVersions(L"1.10", L"1.9"));
    VERIFY_ARE_EQUAL(-1, midiapp::CompareContentVersions(L"1.9", L"1.10"));
    VERIFY_ARE_EQUAL(0, midiapp::CompareContentVersions(L"1.0", L"1"));
    VERIFY_ARE_EQUAL(0, midiapp::CompareContentVersions(L"2.01", L"2.1"));
    VERIFY_ARE_EQUAL(1, midiapp::CompareContentVersions(L"2.0", L"1.99"));
}

void ProvenanceTests::DatesAndTimesAreRead()
{
    auto const utc = midiapp::ParseProvenanceTime(L"2026-10-08T21:14:00Z");

    VERIFY_IS_TRUE(utc > 0);
    VERIFY_ARE_EQUAL(utc, midiapp::ParseProvenanceTime(L"2026-10-08T17:14:00-04:00"));
    VERIFY_IS_TRUE(midiapp::ParseProvenanceTime(L"2026-10-08") > 0);
    VERIFY_IS_TRUE(midiapp::ParseProvenanceTime(midiapp::CurrentProvenanceTime()) > 0);

    VERIFY_ARE_EQUAL(0ll, midiapp::ParseProvenanceTime(L"yesterday"));
    VERIFY_ARE_EQUAL(0ll, midiapp::ParseProvenanceTime(L"2026-13-01"));
    VERIFY_ARE_EQUAL(0ll, midiapp::ParseProvenanceTime(L"2026-10-08T21:14:00Zjunk"));
}

void ProvenanceTests::ACopyCreditsTheOriginal()
{
    auto original = FullProvenance();
    original.DigitalSourceType = midiapp::DigitalSourceTypes::DigitalCreation;
    original.HumanOversightLevel.clear();
    original.AiModelName.clear();

    auto const copy = midiapp::DeriveProvenance(Sam(), L"MIDI Glass test", L"Original Layout", original, false);

    VERIFY_IS_FALSE(copy.Id.empty());
    VERIFY_IS_TRUE(copy.Id != original.Id);
    VERIFY_ARE_EQUAL(std::wstring{ L"Sam" }, copy.Author);
    VERIFY_ARE_EQUAL(std::wstring{ L"CC0-1.0" }, copy.License);
    VERIFY_ARE_EQUAL(std::wstring{ L"1.0" }, copy.Version);
    VERIFY_ARE_EQUAL(std::wstring{ midiapp::DigitalSourceTypes::DigitalCreation }, copy.DigitalSourceType);

    VERIFY_IS_TRUE(copy.BasedOn.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"Original Layout" }, copy.BasedOn->Name);
    VERIFY_ARE_EQUAL(original.Author, copy.BasedOn->Author);
    VERIFY_ARE_EQUAL(original.Id, copy.BasedOn->Id);
    VERIFY_ARE_EQUAL(original.Version, copy.BasedOn->Version);
    VERIFY_IS_FALSE(copy.BasedOn->BuiltIn);
}

void ProvenanceTests::ACopyOfAiWorkSaysSo()
{
    auto const copy = midiapp::DeriveProvenance(Sam(), L"MIDI Glass test", L"AI layout", FullProvenance(), false);

    VERIFY_ARE_EQUAL(std::wstring{ midiapp::DigitalSourceTypes::CompositeWithTrainedAlgorithmicMedia }, copy.DigitalSourceType);
    VERIFY_ARE_EQUAL(std::wstring{ L"Example Model" }, copy.AiModelName);

    // A built-in theme saved under a new name says where it came from.
    auto const theme = midiapp::DeriveProvenance(Sam(), L"MIDI Glass test", L"Studio Dark", std::nullopt, true);

    VERIFY_IS_TRUE(theme.BasedOn.has_value());
    VERIFY_IS_TRUE(theme.BasedOn->BuiltIn);
    VERIFY_ARE_EQUAL(std::wstring{ midiapp::DigitalSourceTypes::DigitalCreation }, theme.DigitalSourceType);
}

void ProvenanceTests::ALayoutKeepsItsProvenance()
{
    glass::LayoutDocument document{};
    document.Name = L"Provenance";
    document.Provenance = FullProvenance();

    auto const json = glass::WriteLayoutToJson(document);
    auto const read = glass::ReadLayoutFromJson(json);

    VERIFY_IS_TRUE(read.Succeeded);
    VERIFY_IS_TRUE(read.Document.Provenance.has_value());
    VERIFY_ARE_EQUAL(
        midiapp::ProvenanceToJsonText(*document.Provenance, 0),
        midiapp::ProvenanceToJsonText(*read.Document.Provenance, 0));

    // Written once, as a block this build reads, rather than again as a key it doesn't know.
    VERIFY_ARE_EQUAL(json.find(L"\"provenance\""), json.rfind(L"\"provenance\""));
    VERIFY_ARE_EQUAL(json, glass::WriteLayoutToJson(read.Document));
}

void ProvenanceTests::AFavoriteIsReadButNeverWritten()
{
    glass::LayoutDocument document{};
    document.Name = L"Starred";
    document.IsFavorite = true;

    VERIFY_ARE_EQUAL(std::wstring::npos, glass::WriteLayoutToJson(document).find(L"isFavorite"));

    // A file an older build wrote still says so, for the one-time move into settings.
    auto const read = glass::ReadLayoutFromJson(LR"({ "fileVersion": 1, "name": "Old", "isFavorite": true })");

    VERIFY_IS_TRUE(read.Succeeded);
    VERIFY_IS_TRUE(read.Document.IsFavorite);
    VERIFY_ARE_EQUAL(std::wstring::npos, glass::WriteLayoutToJson(read.Document).find(L"isFavorite"));
}

void ProvenanceTests::AThemeKeepsItsProvenance()
{
    auto theme = glass::BuiltInThemes()[0];
    theme.Name = L"Mine";
    theme.IsBuiltIn = false;
    theme.Provenance = FullProvenance();

    auto const json = glass::WriteThemeToJson(theme);
    auto const read = glass::ReadThemeFromJson(json);

    VERIFY_IS_TRUE(read.Succeeded);
    VERIFY_IS_TRUE(read.Value.Provenance.has_value());
    VERIFY_ARE_EQUAL(theme.Provenance->Author, read.Value.Provenance->Author);
    VERIFY_IS_TRUE(read.Value.Unknown == nullptr);
    VERIFY_ARE_EQUAL(json, glass::WriteThemeToJson(read.Value));
}

void ProvenanceTests::AThemeKeepsKeysFromANewerBuild()
{
    auto theme = glass::BuiltInThemes()[0];
    theme.Name = L"Mine";

    auto json = glass::WriteThemeToJson(theme);
    json.insert(json.find(L'{') + 1, L"\n  \"sparkle\": { \"amount\": 3 },");

    auto const read = glass::ReadThemeFromJson(json);

    VERIFY_IS_TRUE(read.Succeeded);
    VERIFY_IS_TRUE(read.Value.Unknown != nullptr);

    // Only the new key. Nothing this build writes was taken for one it doesn't know.
    VERIFY_ARE_EQUAL(1u, read.Value.Unknown.Size());

    auto const written = glass::WriteThemeToJson(read.Value);

    VERIFY_IS_TRUE(written.find(L"\"sparkle\"") != std::wstring::npos);
    VERIFY_IS_TRUE(written.find(L"\"amount\": 3") != std::wstring::npos);
}
