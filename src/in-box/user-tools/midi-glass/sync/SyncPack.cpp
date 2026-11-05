// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SyncPack.h"

#include "DeviceLinkFrames.h"
#include "LayoutPack.h"
#include "LayoutStore.h"
#include "ZipArchive.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>

namespace glass::sync
{
    namespace
    {
        using midiapp::ContentPackProblem;

        bool EqualsIgnoreCase(_In_ std::wstring_view first, _In_ std::wstring_view second) noexcept
        {
            return ::CompareStringOrdinal(first.data(), static_cast<int>(first.size()),
                second.data(), static_cast<int>(second.size()), TRUE) == CSTR_EQUAL;
        }

        bool IsManifestName(_In_ std::wstring_view name) noexcept
        {
            return EqualsIgnoreCase(name, midiapp::ContentPackManifestName);
        }

        bool IsSignatureName(_In_ std::wstring_view name) noexcept
        {
            std::wstring_view const folder{ midiapp::ContentPackSignaturesFolder };

            return name.size() > folder.size() + 1 &&
                EqualsIgnoreCase(name.substr(0, folder.size()), folder) &&
                name[folder.size()] == L'/';
        }

        // Read whole, but only when it's exactly the expected size, so a file that grew can't
        // make this read more than the manifest allows.
        bool ReadExpected(_In_ std::wstring const& path, _In_ uint64_t size, _Out_ std::vector<uint8_t>& bytes) noexcept
        {
            bytes.clear();

            try
            {
                std::error_code ec{};

                if (std::filesystem::file_size(path, ec) != size || ec || size > midiapp::MaximumContentPackBytes)
                {
                    return false;
                }

                std::ifstream file{ std::filesystem::path{ path }, std::ios::binary };

                if (!file)
                {
                    return false;
                }

                bytes.resize(static_cast<size_t>(size));

                if (size > 0)
                {
                    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));

                    if (file.gcount() != static_cast<std::streamsize>(size))
                    {
                        bytes.clear();
                        return false;
                    }
                }

                return true;
            }
            catch (...)
            {
                bytes.clear();
                return false;
            }
        }

        std::optional<midiapp::ContentPackManifest> ReadManifest(
            _In_ std::vector<uint8_t> const& manifestBytes,
            _Out_ ContentPackProblem& problem) noexcept
        {
            problem = ContentPackProblem::BadManifest;

            try
            {
                if (manifestBytes.empty() || manifestBytes.size() > MaximumManifestBytes)
                {
                    return std::nullopt;
                }

                bool valid{ false };
                auto const text = midiapp::DeviceLinkFromUtf8(manifestBytes, valid);

                if (!valid)
                {
                    return std::nullopt;
                }

                return midiapp::ReadContentPackManifest(text, problem);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }
    }

    _Use_decl_annotations_
    bool IsSha256Hex(std::wstring const& value) noexcept
    {
        return value.size() == 64 && std::all_of(value.begin(), value.end(),
            [](wchar_t c) { return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'); });
    }

    _Use_decl_annotations_
    bool IsAllowedSyncSignature(SyncSignature const& signature) noexcept
    {
        return IsSignatureName(signature.EntryName) &&
            midiapp::IsAllowedContentPackPath(signature.EntryName) &&
            !signature.Bytes.empty() &&
            signature.Bytes.size() <= midiapp::MaximumContentPackSignatureBytes;
    }

    _Use_decl_annotations_
    bool SyncSourceFromPack(std::vector<uint8_t> const& pack, SyncSource& source) noexcept
    {
        source = SyncSource{};

        try
        {
            std::vector<midiapp::StoredZipEntry> entries{};

            if (midiapp::ReadStoredZip(pack, midiapp::MaximumContentPackEntries, entries) != midiapp::StoredZipStatus::Read)
            {
                return false;
            }

            auto const manifestEntry = std::find_if(entries.begin(), entries.end(),
                [](midiapp::StoredZipEntry const& entry) { return IsManifestName(entry.Name); });

            if (manifestEntry == entries.end())
            {
                return false;
            }

            ContentPackProblem problem{};
            auto const manifest = ReadManifest(manifestEntry->Bytes, problem);

            if (!manifest || manifest->Kind != midiapp::ContentPackKind::GlassLayout)
            {
                return false;
            }

            source.Offer.ManifestBytes = manifestEntry->Bytes;

            for (auto const& entry : entries)
            {
                if (IsSignatureName(entry.Name))
                {
                    SyncSignature signature{ entry.Name, entry.Bytes };

                    if (IsAllowedSyncSignature(signature) && source.Offer.Signatures.size() < MaximumOfferSignatures)
                    {
                        source.Offer.Signatures.push_back(std::move(signature));
                    }
                }
            }

            for (auto const& file : manifest->Files)
            {
                auto const entry = std::find_if(entries.begin(), entries.end(),
                    [&file](midiapp::StoredZipEntry const& candidate) { return candidate.Name == file.Path; });

                if (entry == entries.end() || entry->Bytes.size() != file.Size ||
                    midiapp::Sha256Hex(entry->Bytes.data(), entry->Bytes.size()) != file.Sha256)
                {
                    return false;
                }

                if (file.Path == manifest->Primary)
                {
                    source.PrimarySha256 = file.Sha256;
                }

                if (source.FilesBySha256.emplace(file.Sha256, entry->Bytes).second)
                {
                    source.TotalBytes += file.Size;
                }
            }

            return !source.PrimarySha256.empty();
        }
        catch (...)
        {
            source = SyncSource{};
            return false;
        }
    }

    _Use_decl_annotations_
    bool PrepareSyncSource(std::wstring const& layoutFilePath, SyncSource& source, std::wstring& failureKey) noexcept
    {
        source = SyncSource{};
        failureKey.clear();

        try
        {
            auto const read = ReadLayoutFile(layoutFilePath);

            if (!read.Succeeded)
            {
                failureKey = L"PackageFailedUnreadable";
                return false;
            }

            auto const built = BuildLayoutPack(layoutFilePath);

            if (!built.Succeeded)
            {
                failureKey = built.FailureKey.empty() ? L"PackageFailedUnreadable" : built.FailureKey;
                return false;
            }

            if (!SyncSourceFromPack(built.Bytes, source))
            {
                failureKey = L"PackageFailedUnreadable";
                return false;
            }

            if (read.Document.Provenance.has_value())
            {
                source.Offer.LayoutId = read.Document.Provenance->Id.substr(0, MaximumLayoutIdLength);
            }

            source.Offer.LayoutName = read.Document.Name.substr(0, MaximumOfferNameLength);

            return true;
        }
        catch (...)
        {
            source = SyncSource{};
            failureKey = L"PackageFailedUnreadable";
            return false;
        }
    }

    _Use_decl_annotations_
    SyncNeed ComputeSyncNeed(std::vector<uint8_t> const& manifestBytes, HaveFile const& have) noexcept
    {
        SyncNeed need{};

        try
        {
            ContentPackProblem problem{};
            auto const manifest = ReadManifest(manifestBytes, problem);

            if (!manifest)
            {
                need.Problem = problem;
                return need;
            }

            if (manifest->Kind != midiapp::ContentPackKind::GlassLayout)
            {
                need.Problem = ContentPackProblem::WrongKind;
                return need;
            }

            if (manifest->Files.size() + 1 > midiapp::MaximumContentPackEntries)
            {
                need.Problem = ContentPackProblem::TooManyEntries;
                return need;
            }

            for (auto const& file : manifest->Files)
            {
                if (!IsSha256Hex(file.Sha256) || !midiapp::IsAllowedContentPackPath(file.Path))
                {
                    need.Problem = ContentPackProblem::BadManifest;
                    return need;
                }

                // Two listings of one hash with different sizes can't both be right.
                if (auto const known = need.Sizes.find(file.Sha256); known != need.Sizes.end())
                {
                    if (known->second != file.Size)
                    {
                        need.Problem = ContentPackProblem::BadManifest;
                        return need;
                    }

                    continue;
                }

                need.TotalBytes += file.Size;

                if (file.Size > midiapp::MaximumContentPackBytes || need.TotalBytes > midiapp::MaximumContentPackBytes)
                {
                    need.Problem = ContentPackProblem::TooBig;
                    return need;
                }

                need.Sizes.emplace(file.Sha256, file.Size);

                if (file.Path == manifest->Primary)
                {
                    need.PrimarySha256 = file.Sha256;
                }
            }

            if (need.PrimarySha256.empty() || manifest->Primary.find(L'/') != std::wstring::npos)
            {
                need.Problem = ContentPackProblem::NoPrimary;
                return need;
            }

            // The layout file is always sent: it's the thing being synced, and it's small.
            need.Needed.push_back(need.PrimarySha256);
            need.NeededBytes = need.Sizes[need.PrimarySha256];

            for (auto const& [sha256, size] : need.Sizes)
            {
                if (sha256 == need.PrimarySha256)
                {
                    continue;
                }

                if (!have || have(sha256).empty())
                {
                    need.Needed.push_back(sha256);
                    need.NeededBytes += size;
                }
            }

            need.Problem = ContentPackProblem::None;
            return need;
        }
        catch (...)
        {
            need = SyncNeed{};
            need.Problem = ContentPackProblem::Damaged;
            return need;
        }
    }

    _Use_decl_annotations_
    SyncAssembly AssembleSyncPack(
        SyncOffer const& offer,
        std::map<std::wstring, std::vector<uint8_t>> const& received,
        HaveFile const& have) noexcept
    {
        SyncAssembly assembly{};

        try
        {
            ContentPackProblem problem{};
            auto const manifest = ReadManifest(offer.ManifestBytes, problem);

            if (!manifest)
            {
                assembly.Problem = problem;
                return assembly;
            }

            if (offer.Signatures.size() > MaximumOfferSignatures)
            {
                assembly.Problem = ContentPackProblem::TooManyEntries;
                return assembly;
            }

            std::vector<midiapp::StoredZipEntry> entries{};
            entries.push_back(midiapp::StoredZipEntry{ midiapp::ContentPackManifestName, offer.ManifestBytes });

            // Each file this PC already had is read once, even when the pack lists it twice.
            std::map<std::wstring, std::vector<uint8_t>> local{};
            uint64_t total{ 0 };

            for (auto const& file : manifest->Files)
            {
                std::vector<uint8_t> const* bytes{ nullptr };

                if (auto const arrived = received.find(file.Sha256); arrived != received.end())
                {
                    bytes = &arrived->second;
                }
                else if (auto const loaded = local.find(file.Sha256); loaded != local.end())
                {
                    bytes = &loaded->second;
                }
                else
                {
                    auto const path = have ? have(file.Sha256) : std::wstring{};
                    std::vector<uint8_t> read{};

                    // Checked again now: the file may have changed since it was indexed.
                    if (path.empty() || !ReadExpected(path, file.Size, read) ||
                        midiapp::Sha256Hex(read.data(), read.size()) != file.Sha256)
                    {
                        assembly.Problem = ContentPackProblem::MissingFile;
                        assembly.ProblemFile = file.Path;
                        return assembly;
                    }

                    bytes = &local.emplace(file.Sha256, std::move(read)).first->second;
                }

                total += bytes->size();

                if (total > midiapp::MaximumContentPackBytes)
                {
                    assembly.Problem = ContentPackProblem::TooBig;
                    return assembly;
                }

                entries.push_back(midiapp::StoredZipEntry{ file.Path, *bytes });
            }

            std::set<std::wstring> signatureNames{};

            for (auto const& signature : offer.Signatures)
            {
                if (!IsAllowedSyncSignature(signature))
                {
                    assembly.Problem = ContentPackProblem::BadName;
                    assembly.ProblemFile = signature.EntryName;
                    return assembly;
                }

                std::wstring lower{ signature.EntryName };
                std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

                if (!signatureNames.insert(lower).second)
                {
                    assembly.Problem = ContentPackProblem::DuplicateName;
                    assembly.ProblemFile = signature.EntryName;
                    return assembly;
                }

                entries.push_back(midiapp::StoredZipEntry{ signature.EntryName, signature.Bytes });
            }

            assembly.Pack = midiapp::BuildStoredZip(entries);

            if (assembly.Pack.empty())
            {
                assembly.Problem = ContentPackProblem::TooBig;
                return assembly;
            }

            assembly.Problem = ContentPackProblem::None;
            return assembly;
        }
        catch (...)
        {
            assembly = SyncAssembly{};
            assembly.Problem = ContentPackProblem::Damaged;
            return assembly;
        }
    }
}
