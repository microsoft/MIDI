// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SysExReviewTests.h"

#include "LayoutModel.h"
#include "SysExReview.h"

#include <windows.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    std::filesystem::path g_folder{};

    glass::ControlMessage SysExRow(_In_ std::wstring const& device, _In_ std::vector<uint8_t> bytes)
    {
        glass::ControlMessage message{};
        message.Kind = glass::MessageKind::SystemExclusive;
        message.DeviceName = device;
        message.SystemExclusive = std::move(bytes);

        return message;
    }

    // A control with one SysEx row, and a sequence with a SysEx step to another device.
    glass::LayoutDocument WithSysEx(_In_ std::vector<uint8_t> bytes = { 0xF0, 0x43, 0x10, 0x4C, 0xF7 })
    {
        glass::LayoutDocument document{};
        document.Name = L"Borrowed";

        glass::Control control{};
        control.Messages.push_back(SysExRow(L"Synth", bytes));

        glass::Page page{};
        page.Controls.push_back(control);
        document.Pages.push_back(page);

        glass::SequenceStep step{};
        step.Kind = glass::SequenceStepKind::SendSystemExclusive;
        step.Message = SysExRow(L"Drums", { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 });

        glass::Sequence sequence{};
        sequence.Name = L"Reset";
        sequence.Steps.push_back(step);
        document.Sequences.push_back(sequence);

        return document;
    }

    std::wstring PathOf(_In_ wchar_t const* name)
    {
        return (g_folder / name).wstring();
    }
}

bool SysExReviewTests::Setup()
{
    g_folder = std::filesystem::temp_directory_path() / (L"GlassSysExReviewTests-" + std::to_wstring(::GetCurrentProcessId()));

    std::error_code ignored{};
    std::filesystem::remove_all(g_folder, ignored);
    std::filesystem::create_directories(g_folder, ignored);

    glass::UseSysExReviewFile((g_folder / L"SysExReview.json").wstring());

    return true;
}

bool SysExReviewTests::Cleanup()
{
    glass::UseSysExReviewFile({});

    std::error_code ignored{};
    std::filesystem::remove_all(g_folder, ignored);

    return true;
}

void SysExReviewTests::SysExPacketsAreFoundAndTakenOut()
{
    VERIFY_IS_TRUE(glass::IsSystemExclusivePacket(0x30160000));
    VERIFY_IS_TRUE(glass::IsSystemExclusivePacket(0x50000000));
    VERIFY_IS_FALSE(glass::IsSystemExclusivePacket(0x20903C7F));
    VERIFY_IS_FALSE(glass::IsSystemExclusivePacket(0x40903C00));

    VERIFY_ARE_EQUAL(1u, glass::UmpWordCount(0x20903C7F));
    VERIFY_ARE_EQUAL(2u, glass::UmpWordCount(0x30160000));
    VERIFY_ARE_EQUAL(2u, glass::UmpWordCount(0x40903C00));
    VERIFY_ARE_EQUAL(4u, glass::UmpWordCount(0x50000000));

    // A note, a SysEx7 packet, another note, a SysEx8 packet.
    std::vector<uint32_t> const words{
        0x20903C7F,
        0x30164310, 0x4C000000,
        0x20803C00,
        0x50000000, 0x00000000, 0x00000000, 0x00000000,
    };

    VERIFY_IS_TRUE(glass::ContainsSystemExclusive(words.data(), static_cast<uint32_t>(words.size())));

    auto const kept = glass::WithoutSystemExclusive(words.data(), static_cast<uint32_t>(words.size()));
    VERIFY_IS_TRUE((kept == std::vector<uint32_t>{ 0x20903C7F, 0x20803C00 }));

    VERIFY_IS_FALSE(glass::ContainsSystemExclusive(kept.data(), static_cast<uint32_t>(kept.size())));

    // Half a packet at the end isn't sent either.
    std::vector<uint32_t> const cut{ 0x20903C7F, 0x40903C00 };
    VERIFY_IS_TRUE((glass::WithoutSystemExclusive(cut.data(), 2) == std::vector<uint32_t>{ 0x20903C7F }));

    VERIFY_IS_FALSE(glass::ContainsSystemExclusive(nullptr, 4));
}

void SysExReviewTests::DevicesAreListedFromControlsAndSequences()
{
    auto document = WithSysEx();

    // A raw UMP row that's SysEx counts too; one that isn't doesn't.
    glass::ControlMessage raw{};
    raw.Kind = glass::MessageKind::RawUmp;
    raw.DeviceName = L"Synth";
    raw.RawWords = { 0x30020102, 0x00000000 };

    glass::ControlMessage note{};
    note.Kind = glass::MessageKind::RawUmp;
    note.DeviceName = L"Lights";
    note.RawWords = { 0x20903C7F };

    document.Pages[0].Controls[0].Messages.push_back(raw);
    document.Pages[0].Controls[0].Messages.push_back(note);

    // A step this build doesn't know is never sent, so it isn't asked about.
    glass::SequenceStep unknown{};
    unknown.Kind = glass::SequenceStepKind::SendSystemExclusive;
    unknown.UnrecognizedKind = L"somethingNew";
    unknown.Message = SysExRow(L"Mystery", { 0xF0, 0x01, 0xF7 });
    document.Sequences[0].Steps.push_back(unknown);

    auto const devices = glass::SystemExclusiveDevices(document);

    VERIFY_ARE_EQUAL(size_t{ 2 }, devices.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"Synth" }, devices[0]);
    VERIFY_ARE_EQUAL(std::wstring{ L"Drums" }, devices[1]);
}

void SysExReviewTests::TheFingerprintFollowsTheSysEx()
{
    auto const first = glass::SystemExclusiveFingerprint(WithSysEx());

    VERIFY_ARE_EQUAL(size_t{ 64 }, first.size());
    VERIFY_ARE_EQUAL(first, glass::SystemExclusiveFingerprint(WithSysEx()));

    // One byte different.
    VERIFY_ARE_NOT_EQUAL(first, glass::SystemExclusiveFingerprint(WithSysEx({ 0xF0, 0x43, 0x10, 0x4D, 0xF7 })));

    // The same bytes to a different device.
    auto moved = WithSysEx();
    moved.Pages[0].Controls[0].Messages[0].DeviceName = L"Other synth";
    VERIFY_ARE_NOT_EQUAL(first, glass::SystemExclusiveFingerprint(moved));

    // Anything else about the layout doesn't matter.
    auto renamed = WithSysEx();
    renamed.Name = L"Something else";
    VERIFY_ARE_EQUAL(first, glass::SystemExclusiveFingerprint(renamed));

    VERIFY_IS_TRUE(glass::SystemExclusiveFingerprint(glass::LayoutDocument{}).empty());
}

void SysExReviewTests::ALayoutMadeHereIsNotAsked()
{
    VERIFY_IS_TRUE(glass::CheckSysExReview(WithSysEx(), PathOf(L"Mine.midilayout")) == glass::SysExReview::Allowed);
}

void SysExReviewTests::AMarkedLayoutAsksUntilApproved()
{
    auto const path = PathOf(L"FromAPack.midilayout");
    auto const document = WithSysEx();

    glass::MarkForSysExReview(path);

    VERIFY_IS_TRUE(glass::CheckSysExReview(document, path) == glass::SysExReview::Ask);

    // Running without SysEx isn't an answer: it asks again next time.
    VERIFY_IS_TRUE(glass::CheckSysExReview(document, path) == glass::SysExReview::Ask);

    glass::ApproveSysEx(document, path);

    VERIFY_IS_TRUE(glass::CheckSysExReview(document, path) == glass::SysExReview::Allowed);

    // Paths compare without regard to case, the way Windows does.
    VERIFY_IS_TRUE(glass::CheckSysExReview(document, PathOf(L"FROMAPACK.midilayout")) == glass::SysExReview::Allowed);
}

void SysExReviewTests::TheSameSysExAgainIsNotAskedAgain()
{
    auto const path = PathOf(L"Synced.midilayout");
    auto document = WithSysEx();

    glass::MarkForSysExReview(path);
    glass::ApproveSysEx(document, path);

    // A newer copy from the same PC, with its controls moved but the same SysEx.
    document.Name = L"Synced, again";
    glass::MarkForSysExReview(path);

    VERIFY_IS_TRUE(glass::CheckSysExReview(document, path) == glass::SysExReview::Allowed);
}

void SysExReviewTests::ChangedSysExIsAskedAgain()
{
    auto const path = PathOf(L"Changed.midilayout");

    glass::MarkForSysExReview(path);
    glass::ApproveSysEx(WithSysEx(), path);

    // A newer copy arrives with different SysEx.
    glass::MarkForSysExReview(path);

    VERIFY_IS_TRUE(glass::CheckSysExReview(WithSysEx({ 0xF0, 0x00, 0x20, 0x33, 0xF7 }), path) == glass::SysExReview::Ask);
}

void SysExReviewTests::AFileFromOutsideTheFolderAsks()
{
    auto const path = PathOf(L"Downloads\\Borrowed.midilayout");

    auto document = WithSysEx();
    document.IsImported = true;

    VERIFY_IS_TRUE(glass::CheckSysExReview(document, path) == glass::SysExReview::Ask);

    glass::ApproveSysEx(document, path);
    VERIFY_IS_TRUE(glass::CheckSysExReview(document, path) == glass::SysExReview::Allowed);

    // The file was swapped for one with other SysEx.
    auto swapped = WithSysEx({ 0xF0, 0x41, 0x10, 0xF7 });
    swapped.IsImported = true;
    VERIFY_IS_TRUE(glass::CheckSysExReview(swapped, path) == glass::SysExReview::Ask);
}

void SysExReviewTests::ALayoutWithNoSysExIsNotAsked()
{
    auto const path = PathOf(L"Faders.midilayout");

    glass::MarkForSysExReview(path);

    glass::LayoutDocument plain{};
    VERIFY_IS_TRUE(glass::CheckSysExReview(plain, path) == glass::SysExReview::Allowed);

    // The mark is gone, so SysEx the owner adds later is theirs.
    VERIFY_IS_TRUE(glass::CheckSysExReview(WithSysEx(), path) == glass::SysExReview::Allowed);
}

void SysExReviewTests::RenamingAndCopyingKeepTheMark()
{
    auto const first = PathOf(L"First.midilayout");
    auto const renamed = PathOf(L"Renamed.midilayout");
    auto const copy = PathOf(L"Renamed copy.midilayout");
    auto const document = WithSysEx();

    glass::MarkForSysExReview(first);
    glass::MoveSysExReview(first, renamed);

    VERIFY_IS_TRUE(glass::CheckSysExReview(document, first) == glass::SysExReview::Allowed);
    VERIFY_IS_TRUE(glass::CheckSysExReview(document, renamed) == glass::SysExReview::Ask);

    glass::CopySysExReview(renamed, copy);

    VERIFY_IS_TRUE(glass::CheckSysExReview(document, copy) == glass::SysExReview::Ask);
    VERIFY_IS_TRUE(glass::CheckSysExReview(document, renamed) == glass::SysExReview::Ask);

    glass::ForgetSysExReview(copy);
    VERIFY_IS_TRUE(glass::CheckSysExReview(document, copy) == glass::SysExReview::Allowed);
}
