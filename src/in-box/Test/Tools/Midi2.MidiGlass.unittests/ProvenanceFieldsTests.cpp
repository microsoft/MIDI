// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ProvenanceFieldsTests.h"

#include "ContentProvenance.h"
#include "ProvenanceFields.h"

#include <string>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    namespace mjson = winrt::Windows::Data::Json;
    namespace sources = midiapp::DigitalSourceTypes;
    namespace levels = midiapp::HumanOversightLevels;

    // Everything a block can hold, AI details included, plus a key from a newer build.
    midiapp::ContentProvenance Full()
    {
        midiapp::ContentProvenance provenance{};

        provenance.Id = L"3f2b8c1e-7d4a-4b9e-9c1f-2a6d8e0b5c47";
        provenance.Version = L"1.2";
        provenance.Author = L"Pat Example";
        provenance.Organization = L"Example Instruments";
        provenance.Url = L"https://example.com/layouts";
        provenance.License = L"CC-BY-4.0";
        provenance.Created = L"2026-10-08T21:14:00Z";
        provenance.Tool = L"Windows MIDI Glass 0.99.92-preview.11";
        provenance.DigitalSourceType = sources::TrainedAlgorithmicMedia;
        provenance.HumanOversightLevel = levels::PromptGuided;
        provenance.AiModelName = L"Example Model";

        midiapp::ProvenanceSource source{};
        source.Name = L"Studio Dark";
        source.BuiltIn = true;

        provenance.BasedOn = source;

        provenance.Unknown = mjson::JsonObject{};
        provenance.Unknown.Insert(L"futureKey", mjson::JsonValue::CreateStringValue(L"kept"));

        return provenance;
    }

    std::wstring Text(_In_ midiapp::ContentProvenance const& provenance)
    {
        return midiapp::ProvenanceToJsonText(provenance, 0);
    }

    glass::ProvenanceFields Typed(_In_ std::wstring const& author)
    {
        glass::ProvenanceFields fields{};
        fields.Author = author;

        return fields;
    }
}

void ProvenanceFieldsTests::ReadingTheFieldsBackChangesNothing()
{
    // The editor's boxes report a change after it fills them in itself, so reading them back
    // has to give exactly the block it started with, or opening the page would rewrite the file.
    auto const full = Full();
    auto const same = glass::ApplyProvenanceFields(full, glass::FieldsOf(full));

    VERIFY_IS_TRUE(same.Provenance.has_value());
    VERIFY_IS_FALSE(same.WebsiteRejected);
    VERIFY_ARE_EQUAL(Text(full), Text(*same.Provenance));

    // A hand-written file with a website that isn't a link and no source term.
    auto odd = Full();
    odd.Url = L"http://example.com/plain";
    odd.DigitalSourceType.clear();

    auto const oddSame = glass::ApplyProvenanceFields(odd, glass::FieldsOf(odd));

    VERIFY_IS_TRUE(oddSame.Provenance.has_value());
    VERIFY_IS_FALSE(oddSame.WebsiteRejected);
    VERIFY_ARE_EQUAL(Text(odd), Text(*oddSame.Provenance));

    // No block stays no block.
    auto const none = glass::ApplyProvenanceFields(std::nullopt, glass::FieldsOf(std::nullopt));

    VERIFY_IS_FALSE(none.Provenance.has_value());
    VERIFY_IS_FALSE(none.WebsiteRejected);
}

void ProvenanceFieldsTests::NothingTypedAddsNoBlock()
{
    glass::ProvenanceFields fields{};
    fields.Author = L"   ";
    fields.License = L"\u200B";

    auto const blank = glass::ApplyProvenanceFields(std::nullopt, fields);

    VERIFY_IS_FALSE(blank.Provenance.has_value());

    // A website that can't be taken adds nothing either, and says why.
    fields.Url = L"http://example.com";

    auto const refused = glass::ApplyProvenanceFields(std::nullopt, fields);

    VERIFY_IS_FALSE(refused.Provenance.has_value());
    VERIFY_IS_TRUE(refused.WebsiteRejected);
}

void ProvenanceFieldsTests::TypingStartsABlockWithAnIdAndNoGuesses()
{
    auto const first = glass::ApplyProvenanceFields(std::nullopt, Typed(L"Sam"));

    VERIFY_IS_TRUE(first.Provenance.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"Sam" }, first.Provenance->Author);
    VERIFY_ARE_EQUAL(static_cast<size_t>(36), first.Provenance->Id.size());

    // When it was made, with what and from what aren't known for a file that never said.
    VERIFY_IS_TRUE(first.Provenance->Created.empty());
    VERIFY_IS_TRUE(first.Provenance->Tool.empty());
    VERIFY_IS_TRUE(first.Provenance->DigitalSourceType.empty());
    VERIFY_IS_FALSE(first.Provenance->BasedOn.has_value());

    auto const second = glass::ApplyProvenanceFields(std::nullopt, Typed(L"Sam"));

    VERIFY_ARE_NOT_EQUAL(first.Provenance->Id, second.Provenance->Id);

    // Picking how it was made is something said too.
    glass::ProvenanceFields picked{};
    picked.DigitalSourceType = sources::DigitalCreation;

    auto const chosen = glass::ApplyProvenanceFields(std::nullopt, picked);

    VERIFY_IS_TRUE(chosen.Provenance.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ sources::DigitalCreation }, chosen.Provenance->DigitalSourceType);
}

void ProvenanceFieldsTests::WhatIsTypedIsCleanedAndCut()
{
    glass::ProvenanceFields fields{};
    fields.Author = L"  Sam\u202E Example  ";
    fields.Organization = std::wstring(300, L'a');
    fields.Version = std::wstring(100, L'1');

    auto const applied = glass::ApplyProvenanceFields(Full(), fields);

    VERIFY_ARE_EQUAL(std::wstring{ L"Sam Example" }, applied.Provenance->Author);
    VERIFY_ARE_EQUAL(midiapp::MaximumProvenanceNameLength, applied.Provenance->Organization.size());
    VERIFY_ARE_EQUAL(midiapp::MaximumProvenanceVersionLength, applied.Provenance->Version.size());
}

void ProvenanceFieldsTests::OnlyAPlainHttpsWebsiteIsTaken()
{
    auto const current = Full();
    auto fields = glass::FieldsOf(current);

    fields.Url = L"http://example.com";
    auto const plain = glass::ApplyProvenanceFields(current, fields);

    VERIFY_IS_TRUE(plain.WebsiteRejected);
    VERIFY_ARE_EQUAL(current.Url, plain.Provenance->Url);

    fields.Url = L"https://someone@example.com";
    auto const withUser = glass::ApplyProvenanceFields(current, fields);

    VERIFY_IS_TRUE(withUser.WebsiteRejected);
    VERIFY_ARE_EQUAL(current.Url, withUser.Provenance->Url);

    fields.Url = L" https://contoso.example/glass ";
    auto const good = glass::ApplyProvenanceFields(current, fields);

    VERIFY_IS_FALSE(good.WebsiteRejected);
    VERIFY_ARE_EQUAL(std::wstring{ L"https://contoso.example/glass" }, good.Provenance->Url);

    fields.Url.clear();
    auto const cleared = glass::ApplyProvenanceFields(current, fields);

    VERIFY_IS_FALSE(cleared.WebsiteRejected);
    VERIFY_IS_TRUE(cleared.Provenance->Url.empty());
}

void ProvenanceFieldsTests::TheAppsOwnPartsAreKept()
{
    auto const current = Full();
    auto fields = glass::FieldsOf(current);
    fields.Author = L"Sam";

    auto const applied = glass::ApplyProvenanceFields(current, fields);

    VERIFY_ARE_EQUAL(std::wstring{ L"Sam" }, applied.Provenance->Author);
    VERIFY_ARE_EQUAL(current.Id, applied.Provenance->Id);
    VERIFY_ARE_EQUAL(current.Created, applied.Provenance->Created);
    VERIFY_ARE_EQUAL(current.Tool, applied.Provenance->Tool);
    VERIFY_IS_TRUE(applied.Provenance->BasedOn.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"Studio Dark" }, applied.Provenance->BasedOn->Name);
    VERIFY_IS_TRUE(applied.Provenance->BasedOn->BuiltIn);
    VERIFY_IS_TRUE(applied.Provenance->Unknown != nullptr && applied.Provenance->Unknown.HasKey(L"futureKey"));

    // Clearing every field keeps the block, because it still says what it is.
    auto const emptied = glass::ApplyProvenanceFields(current, glass::ProvenanceFields{});

    VERIFY_IS_TRUE(emptied.Provenance.has_value());
    VERIFY_ARE_EQUAL(current.Id, emptied.Provenance->Id);
    VERIFY_IS_TRUE(emptied.Provenance->Author.empty());
}

void ProvenanceFieldsTests::PickingAPersonDropsTheAiDetails()
{
    auto const current = Full();
    auto fields = glass::FieldsOf(current);
    fields.DigitalSourceType = sources::DigitalCreation;

    auto const applied = glass::ApplyProvenanceFields(current, fields);

    VERIFY_ARE_EQUAL(std::wstring{ sources::DigitalCreation }, applied.Provenance->DigitalSourceType);
    VERIFY_IS_TRUE(applied.Provenance->HumanOversightLevel.empty());
    VERIFY_IS_TRUE(applied.Provenance->AiModelName.empty());

    // A program isn't AI either.
    fields.DigitalSourceType = sources::AlgorithmicMedia;

    auto const program = glass::ApplyProvenanceFields(current, fields);

    VERIFY_IS_TRUE(program.Provenance->HumanOversightLevel.empty());
    VERIFY_IS_TRUE(program.Provenance->AiModelName.empty());
}

void ProvenanceFieldsTests::PickingAiBackBringsTheFieldsBack()
{
    auto const current = Full();
    auto fields = glass::FieldsOf(current);
    fields.DigitalSourceType = sources::DigitalCreation;

    auto const person = glass::ApplyProvenanceFields(current, fields);

    // The AI fields are hidden now but still hold what they had. Typing somewhere else must not
    // bring the details back.
    fields.Author = L"Sam";

    auto const typed = glass::ApplyProvenanceFields(person.Provenance, fields);

    VERIFY_IS_TRUE(typed.Provenance->HumanOversightLevel.empty());
    VERIFY_IS_TRUE(typed.Provenance->AiModelName.empty());

    // Picking AI again shows the fields, and what they hold counts again.
    fields.DigitalSourceType = sources::CompositeWithTrainedAlgorithmicMedia;
    fields.HumanOversightLevel = levels::HumanValidated;

    auto const again = glass::ApplyProvenanceFields(typed.Provenance, fields);

    VERIFY_ARE_EQUAL(std::wstring{ sources::CompositeWithTrainedAlgorithmicMedia }, again.Provenance->DigitalSourceType);
    VERIFY_ARE_EQUAL(std::wstring{ levels::HumanValidated }, again.Provenance->HumanOversightLevel);
    VERIFY_ARE_EQUAL(std::wstring{ L"Example Model" }, again.Provenance->AiModelName);
}

void ProvenanceFieldsTests::AFileThatSaysBothKeepsBothUntilAPick()
{
    auto current = Full();
    current.DigitalSourceType = sources::DigitalCreation;

    auto fields = glass::FieldsOf(current);
    fields.Author = L"Sam";

    auto const applied = glass::ApplyProvenanceFields(current, fields);

    VERIFY_ARE_EQUAL(std::wstring{ levels::PromptGuided }, applied.Provenance->HumanOversightLevel);
    VERIFY_ARE_EQUAL(std::wstring{ L"Example Model" }, applied.Provenance->AiModelName);
}

void ProvenanceFieldsTests::NoPickLeavesTheTermAlone()
{
    auto current = Full();
    current.DigitalSourceType = sources::AlgorithmicMedia;
    current.HumanOversightLevel.clear();
    current.AiModelName.clear();

    auto fields = glass::FieldsOf(current);
    fields.DigitalSourceType.clear();
    fields.Author = L"Sam";

    auto const applied = glass::ApplyProvenanceFields(current, fields);

    VERIFY_ARE_EQUAL(std::wstring{ sources::AlgorithmicMedia }, applied.Provenance->DigitalSourceType);
}

void ProvenanceFieldsTests::ATermThisBuildDoesNotKnowKeepsItsAiDetails()
{
    auto current = Full();
    current.DigitalSourceType = L"someTermFromANewerBuild";

    auto fields = glass::FieldsOf(current);
    fields.Author = L"Sam";

    auto const applied = glass::ApplyProvenanceFields(current, fields);

    VERIFY_ARE_EQUAL(std::wstring{ L"someTermFromANewerBuild" }, applied.Provenance->DigitalSourceType);
    VERIFY_ARE_EQUAL(std::wstring{ levels::PromptGuided }, applied.Provenance->HumanOversightLevel);
    VERIFY_ARE_EQUAL(std::wstring{ L"Example Model" }, applied.Provenance->AiModelName);
}
