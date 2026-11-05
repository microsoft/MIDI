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
#include <fstream>
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

void SysExReviewTests::UnknownSysExAsksWhereverTheFileIs()
{
    // In the layouts folder or not: a file dropped there by Explorer or OneDrive is no safer.
    auto inside = WithSysEx();
    inside.IsImported = false;

    auto outside = WithSysEx();
    outside.IsImported = true;

    VERIFY_IS_TRUE(glass::CheckSysExReview(inside) == glass::SysExReview::Ask);
    VERIFY_IS_TRUE(glass::CheckSysExReview(outside) == glass::SysExReview::Ask);

    // Running without SysEx isn't an answer: it asks again next time.
    VERIFY_IS_TRUE(glass::CheckSysExReview(inside) == glass::SysExReview::Ask);
}

void SysExReviewTests::TrustedSysExIsNotAskedAgain()
{
    glass::TrustSysEx(WithSysEx());

    VERIFY_IS_TRUE(glass::CheckSysExReview(WithSysEx()) == glass::SysExReview::Allowed);

    // A newer copy with its controls moved and renamed, but the same SysEx.
    auto copy = WithSysEx();
    copy.Name = L"Borrowed, again";
    copy.IsImported = true;
    VERIFY_IS_TRUE(glass::CheckSysExReview(copy) == glass::SysExReview::Allowed);

    // Trusting it twice keeps one entry.
    glass::TrustSysEx(WithSysEx());
    auto const text = [&]()
        {
            std::ifstream file{ g_folder / L"SysExReview.json" };
            return std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
        }();

    auto const fingerprint = glass::SystemExclusiveFingerprint(WithSysEx());
    // Lowercase hex, so each character fits a byte.
    std::string narrow{};

    for (auto const c : fingerprint)
    {
        narrow += static_cast<char>(c);
    }
    auto const first = text.find(narrow);

    VERIFY_IS_TRUE(first != std::string::npos);
    VERIFY_IS_TRUE(text.find(narrow, first + 1) == std::string::npos);
}

void SysExReviewTests::ChangedSysExIsAskedAgain()
{
    glass::TrustSysEx(WithSysEx());

    VERIFY_IS_TRUE(glass::CheckSysExReview(WithSysEx({ 0xF0, 0x00, 0x20, 0x33, 0xF7 })) == glass::SysExReview::Ask);

    // The same bytes sent to a different device are different SysEx.
    auto moved = WithSysEx();
    moved.Pages[0].Controls[0].Messages[0].DeviceName = L"Other synth";
    VERIFY_IS_TRUE(glass::CheckSysExReview(moved) == glass::SysExReview::Ask);
}

void SysExReviewTests::ALayoutWithNoSysExIsNeverAsked()
{
    glass::LayoutDocument plain{};
    plain.IsImported = true;

    VERIFY_IS_TRUE(glass::CheckSysExReview(plain) == glass::SysExReview::Allowed);

    // And trusting it writes nothing.
    glass::TrustSysEx(plain);
    VERIFY_IS_FALSE(std::filesystem::exists(g_folder / L"SysExReview.json"));
}

void SysExReviewTests::ALostOrDamagedListAsksAgain()
{
    glass::TrustSysEx(WithSysEx());
    VERIFY_IS_TRUE(glass::CheckSysExReview(WithSysEx()) == glass::SysExReview::Allowed);

    auto const list = g_folder / L"SysExReview.json";

    {
        std::ofstream file{ list, std::ios::binary | std::ios::trunc };
        file << "{ \"trusted\": [ 12, \"not a fingerprint\" ";
    }

    VERIFY_IS_TRUE(glass::CheckSysExReview(WithSysEx()) == glass::SysExReview::Ask);

    std::filesystem::remove(list);
    VERIFY_IS_TRUE(glass::CheckSysExReview(WithSysEx()) == glass::SysExReview::Ask);

    // Allowing it again starts a fresh list.
    glass::TrustSysEx(WithSysEx());
    VERIFY_IS_TRUE(glass::CheckSysExReview(WithSysEx()) == glass::SysExReview::Allowed);
}