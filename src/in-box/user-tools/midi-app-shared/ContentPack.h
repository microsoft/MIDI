// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// A pack is a stored zip with manifest.json listing every file's size and SHA-256. Signatures are
// detached CMS over the exact manifest bytes, one per file under signatures/. META-INF/ is kept
// free for a future C2PA manifest store (META-INF/content_credential.c2pa).

#include <sal.h>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Before windows.h, so the JSON projection is declared without the GetObject macro.
#include "ContentProvenance.h"
#include "StoredZip.h"

#include <windows.h>
#include <wincrypt.h>

namespace midiapp
{
    constexpr wchar_t ContentPackManifestName[] = L"manifest.json";
    constexpr wchar_t ContentPackSignaturesFolder[] = L"signatures";
    constexpr wchar_t ContentPackReservedFolder[] = L"META-INF";
    constexpr wchar_t ContentPackFormatName[] = L"microsoft.windows.midi.pack";
    constexpr uint32_t ContentPackFormatVersion = 1;

    constexpr uint64_t MaximumContentPackBytes = 512ull * 1024 * 1024;
    constexpr uint32_t MaximumContentPackEntries = 256;
    constexpr size_t MaximumContentPackSignatureBytes = 256 * 1024;

    enum class ContentPackKind : int32_t
    {
        Unknown = 0,
        GlassLayout = 1,
        GlassTheme = 2,
        PatchbayPatch = 3,
    };

    std::wstring ContentPackKindName(_In_ ContentPackKind kind) noexcept;
    ContentPackKind ContentPackKindFromName(_In_ std::wstring_view name) noexcept;

    struct ContentPackFile
    {
        std::wstring Path{};
        uint64_t Size{ 0 };

        // Lowercase hex.
        std::wstring Sha256{};

        // Credit for a file someone else made, such as a photo.
        std::optional<ContentProvenance> Provenance{};
    };

    struct ContentPackManifest
    {
        uint32_t FormatVersion{ ContentPackFormatVersion };
        ContentPackKind Kind{ ContentPackKind::Unknown };

        // The item the pack is for, at the top level of the pack.
        std::wstring Primary{};

        std::vector<ContentPackFile> Files{};
    };

    std::wstring WriteContentPackManifest(_In_ ContentPackManifest const& manifest) noexcept;

    enum class ContentPackProblem : int32_t
    {
        None = 0,
        NotAPack,
        Compressed,
        TooBig,
        TooManyEntries,
        Damaged,
        NoManifest,
        BadManifest,
        NewerFormat,
        WrongKind,
        BadName,
        DuplicateName,
        UnlistedFile,
        MissingFile,
        SizeMismatch,
        HashMismatch,
        NoPrimary,
        BrokenSignature,
        RevokedSignature,
    };

    std::optional<ContentPackManifest> ReadContentPackManifest(
        _In_ std::wstring_view json,
        _Out_ ContentPackProblem& problem) noexcept;

    std::wstring Sha256Hex(_In_reads_bytes_(size) uint8_t const* data, _In_ size_t size) noexcept;

    // Forward slashes, at most one folder, and nothing Windows would read as something else.
    bool IsAllowedContentPackPath(_In_ std::wstring_view path) noexcept;

    enum class PackSignatureStatus : int32_t
    {
        Trusted = 0,

        // Windows couldn't find out whether the certificate was revoked, usually because it's offline.
        TrustedRevocationUnknown,

        // Matches, but Windows doesn't trust the certificate for code signing.
        Untrusted,

        Revoked,

        // Doesn't match the manifest, or can't be read.
        Broken,
    };

    struct PackSignature
    {
        std::wstring EntryName{};
        PackSignatureStatus Status{ PackSignatureStatus::Broken };

        std::wstring SignerName{};
        std::wstring SignerSubject{};
        std::wstring IssuerName{};

        // SHA-256 of the certificate, lowercase hex.
        std::wstring Thumbprint{};

        // From a timestamp Windows trusts, as a FILETIME. 0 when there isn't one.
        int64_t SignedAt{ 0 };

        int64_t ValidFrom{ 0 };
        int64_t ValidTo{ 0 };

        HRESULT ChainError{ S_OK };

        bool IsTrusted() const noexcept
        {
            return Status == PackSignatureStatus::Trusted || Status == PackSignatureStatus::TrustedRevocationUnknown;
        }
    };

    struct OpenedContentPack
    {
        ContentPackProblem Problem{ ContentPackProblem::NotAPack };
        std::wstring ProblemFile{};

        ContentPackManifest Manifest{};
        std::vector<uint8_t> ManifestBytes{};

        // Only the files the manifest lists, already checked against it.
        std::vector<StoredZipEntry> Files{};

        std::vector<PackSignature> Signatures{};

        std::wstring PackSha256{};

        bool Succeeded() const noexcept { return Problem == ContentPackProblem::None; }

        PackSignature const* TrustedSignature() const noexcept;
        StoredZipEntry const* FindFile(_In_ std::wstring_view path) const noexcept;
        ContentPackFile const* FindListing(_In_ std::wstring_view path) const noexcept;
    };

    struct ContentPackVerifyOptions
    {
        // Tests only: trust exactly the roots in this store instead of the PC's.
        HCERTSTORE ExclusiveRootStore{ nullptr };

        bool CheckRevocation{ true };
    };

    // Checks revocation online, so it can take seconds. Never call it on a UI thread.
    OpenedContentPack OpenContentPack(
        _In_ std::vector<uint8_t> const& pack,
        _In_ ContentPackKind expectedKind,
        _In_ ContentPackVerifyOptions const& options) noexcept;

    struct ContentPackInput
    {
        std::wstring Path{};
        std::vector<uint8_t> Bytes{};
        std::optional<ContentProvenance> Provenance{};
    };

    // Unsigned. Empty when a path isn't allowed or two paths collide.
    std::vector<uint8_t> BuildContentPack(
        _In_ ContentPackKind kind,
        _In_ std::wstring const& primary,
        _In_ std::vector<ContentPackInput> const& files) noexcept;

    std::vector<uint8_t> ReadContentPackManifestBytes(_In_ std::vector<uint8_t> const& pack) noexcept;

    // A detached signature over the manifest bytes, with an RFC 3161 timestamp when a URL is given.
    std::vector<uint8_t> SignContentPackManifest(
        _In_ std::vector<uint8_t> const& manifestBytes,
        _In_ PCCERT_CONTEXT certificate,
        _In_ std::wstring const& timestampUrl,
        _Out_ HRESULT& error) noexcept;

    // Keeps every other entry's bytes; replaces a signature file with the same name.
    std::vector<uint8_t> AddContentPackSignature(
        _In_ std::vector<uint8_t> const& pack,
        _In_ std::wstring const& signatureFileName,
        _In_ std::vector<uint8_t> const& signature) noexcept;

    // Named after the certificate, so two publishers' signatures never collide.
    std::wstring ContentPackSignatureFileName(_In_ PCCERT_CONTEXT certificate) noexcept;

    struct SigningCertificate
    {
        std::wstring Name{};
        std::wstring Issuer{};
        int64_t ValidTo{ 0 };
        std::vector<uint8_t> Sha1Thumbprint{};
    };

    // Code-signing certificates in the user's personal store that have a private key and are in date.
    std::vector<SigningCertificate> ListSigningCertificates() noexcept;

    std::vector<uint8_t> SignContentPackWithStoreCertificate(
        _In_ std::vector<uint8_t> const& pack,
        _In_ std::vector<uint8_t> const& sha1Thumbprint,
        _In_ std::wstring const& timestampUrl,
        _Out_ HRESULT& error) noexcept;
}
