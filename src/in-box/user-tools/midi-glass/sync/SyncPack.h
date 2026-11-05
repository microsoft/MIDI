// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The pack side of "Sync to device", with no network in it.
//
// The sender builds the same layout pack "Pack for sharing" builds and offers its manifest. The
// receiver answers with the SHA-256 of each file it doesn't already have; the layout file itself
// is always asked for. The sender sends those, and the receiver puts the whole pack back together
// from what arrived plus its own copies: the same manifest, files and signatures. That pack then
// goes through exactly the checks a pack file from a USB stick gets (midiapp::OpenContentPack):
// every size and hash against the manifest, every signature against the manifest bytes. A hash
// the receiver thought it had but doesn't still fails there, never later. Pch-free.

#include <sal.h>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ContentPack.h"

namespace glass::sync
{
    constexpr size_t MaximumOfferSignatures = 8;
    constexpr size_t MaximumLayoutIdLength = 128;
    constexpr size_t MaximumOfferNameLength = 256;

    // Generous: a manifest lists at most 256 files.
    constexpr uint32_t MaximumManifestBytes = 512 * 1024;

    struct SyncSignature
    {
        // Like "signatures/0123456789abcdef.p7s", as it sits in the pack.
        std::wstring EntryName{};
        std::vector<uint8_t> Bytes{};
    };

    // What the sender says it's about to send.
    struct SyncOffer
    {
        // The layout's provenance id, or empty for a layout made before layouts had one.
        std::wstring LayoutId{};
        std::wstring LayoutName{};

        // Exactly the bytes of the pack's manifest.json.
        std::vector<uint8_t> ManifestBytes{};

        std::vector<SyncSignature> Signatures{};
    };

    // The sender's side: the offer, and every file in the pack by its hash.
    struct SyncSource
    {
        SyncOffer Offer{};
        std::map<std::wstring, std::vector<uint8_t>> FilesBySha256{};

        // The layout file's hash, which is always sent.
        std::wstring PrimarySha256{};
        uint64_t TotalBytes{ 0 };
    };

    // From a whole pack, as BuildLayoutPack makes it. False for anything that isn't one.
    bool SyncSourceFromPack(
        _In_ std::vector<uint8_t> const& pack,
        _Out_ SyncSource& source) noexcept;

    // Builds the pack for a layout file, the way "Pack for sharing" does, and splits it up.
    // `failureKey` names what went wrong, as a resource key.
    bool PrepareSyncSource(
        _In_ std::wstring const& layoutFilePath,
        _Out_ SyncSource& source,
        _Out_ std::wstring& failureKey) noexcept;

    // A file on this PC with this hash, or empty.
    using HaveFile = std::function<std::wstring(std::wstring const& sha256)>;

    struct SyncNeed
    {
        midiapp::ContentPackProblem Problem{ midiapp::ContentPackProblem::BadManifest };

        // Lowercase hex, each once, the layout file's first.
        std::vector<std::wstring> Needed{};

        // What the needed files add up to, from the manifest.
        uint64_t NeededBytes{ 0 };

        // Every distinct hash in the manifest with its size.
        std::map<std::wstring, uint64_t> Sizes{};

        std::wstring PrimarySha256{};
        uint64_t TotalBytes{ 0 };
    };

    // Reads the offered manifest with the pack reader's own rules and works out what to ask for.
    // A manifest of the wrong kind, too many files, or more bytes than a pack may hold, is refused.
    SyncNeed ComputeSyncNeed(
        _In_ std::vector<uint8_t> const& manifestBytes,
        _In_ HaveFile const& have) noexcept;

    struct SyncAssembly
    {
        midiapp::ContentPackProblem Problem{ midiapp::ContentPackProblem::MissingFile };
        std::wstring ProblemFile{};

        // A stored zip, ready for midiapp::OpenContentPack. Unchecked until then.
        std::vector<uint8_t> Pack{};
    };

    // The pack again, from the offer, the files that arrived (by hash) and the files this PC
    // already has. A file from this PC is read fresh and must still have the hash it's listed
    // under; otherwise it's missing.
    SyncAssembly AssembleSyncPack(
        _In_ SyncOffer const& offer,
        _In_ std::map<std::wstring, std::vector<uint8_t>> const& received,
        _In_ HaveFile const& have) noexcept;

    // A signature entry an offer may carry: under signatures/, a safe name, and not too big.
    bool IsAllowedSyncSignature(_In_ SyncSignature const& signature) noexcept;

    bool IsSha256Hex(_In_ std::wstring const& value) noexcept;
}
