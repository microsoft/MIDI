// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "GlassSyncTests.h"

// Before windows.h, so the JSON projection is declared without the GetObject macro.
#include "ContentProvenance.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>

#include "ContentPack.h"
#include "ZipArchive.h"
#include "PairedDevices.h"
#include "SyncFileIndex.h"
#include "SyncMemory.h"
#include "SyncPack.h"
#include "SyncProtocol.h"
#include "DeviceLinkTestKeys.h"

#include <ncrypt.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    using midiapp::ContentPackKind;
    using midiapp::ContentPackProblem;

    std::filesystem::path g_folder{};
    std::unique_ptr<DeviceLinkTestKey> g_sender{};
    std::unique_ptr<DeviceLinkTestKey> g_receiver{};

    std::vector<uint8_t> Bytes(_In_ std::string_view text)
    {
        return std::vector<uint8_t>{ text.begin(), text.end() };
    }

    void WriteAll(_In_ std::filesystem::path const& path, _In_ std::vector<uint8_t> const& bytes)
    {
        std::error_code ignored{};
        std::filesystem::create_directories(path.parent_path(), ignored);

        std::ofstream file{ path, std::ios::binary | std::ios::trunc };
        file.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    std::wstring HashOf(_In_ std::vector<uint8_t> const& bytes)
    {
        return midiapp::Sha256Hex(bytes.data(), bytes.size());
    }

    std::vector<uint8_t> LayoutBytes() { return Bytes(R"({ "name": "Sample" })"); }
    std::vector<uint8_t> PictureBytes() { return Bytes("not really a picture"); }
    std::vector<uint8_t> ThemeBytes() { return Bytes(R"({ "name": "Mine" })"); }

    std::vector<uint8_t> SamplePack()
    {
        midiapp::ContentPackInput layout{ L"Layout.midilayout", LayoutBytes(), std::nullopt };
        midiapp::ContentPackInput picture{ L"Picture.png", PictureBytes(), std::nullopt };
        midiapp::ContentPackInput theme{ L"theme/Mine.miditheme", ThemeBytes(), std::nullopt };

        return midiapp::BuildContentPack(ContentPackKind::GlassLayout, L"Layout.midilayout", { layout, picture, theme });
    }

    glass::sync::SyncSource SampleSource(_In_ std::vector<uint8_t> const& pack)
    {
        glass::sync::SyncSource source{};
        VERIFY_IS_TRUE(glass::sync::SyncSourceFromPack(pack, source));

        source.Offer.LayoutId = L"0d9c8b7a-6f5e-4d3c-2b1a-098f7e6d5c4b";
        source.Offer.LayoutName = L"Sample";

        return source;
    }

    // Files this PC "has", by hash.
    glass::sync::HaveFile HaveFrom(_In_ std::map<std::wstring, std::wstring> const& files)
    {
        return [files](std::wstring const& sha256) -> std::wstring
            {
                auto const found = files.find(sha256);
                return found == files.end() ? std::wstring{} : found->second;
            };
    }

    midiapp::ContentPackVerifyOptions Offline()
    {
        midiapp::ContentPackVerifyOptions options{};
        options.CheckRevocation = false;

        return options;
    }

    // What the receiver would be sent, taken from the sender's files.
    std::map<std::wstring, std::vector<uint8_t>> Send(
        _In_ glass::sync::SyncSource const& source,
        _In_ std::vector<std::wstring> const& needed)
    {
        std::map<std::wstring, std::vector<uint8_t>> received{};

        for (auto const& hash : needed)
        {
            received.emplace(hash, source.FilesBySha256.at(hash));
        }

        return received;
    }

    // A code-signing certificate signed by itself, as ContentPackTests makes one.
    PCCERT_CONTEXT MakeSigningCertificate()
    {
        NCRYPT_PROV_HANDLE provider{ 0 };

        if (FAILED(::NCryptOpenStorageProvider(&provider, MS_KEY_STORAGE_PROVIDER, 0)))
        {
            return nullptr;
        }

        NCRYPT_KEY_HANDLE key{ 0 };
        auto const created = ::NCryptCreatePersistedKey(provider, &key, BCRYPT_RSA_ALGORITHM, nullptr, 0, 0);
        ::NCryptFreeObject(provider);

        if (FAILED(created))
        {
            return nullptr;
        }

        DWORD length{ 2048 };
        ::NCryptSetProperty(key, NCRYPT_LENGTH_PROPERTY, reinterpret_cast<PBYTE>(&length), sizeof(length), 0);

        if (FAILED(::NCryptFinalizeKey(key, 0)))
        {
            ::NCryptFreeObject(key);
            return nullptr;
        }

        std::wstring const x500 = L"CN=Glass Sync Test Publisher";

        DWORD nameSize{ 0 };
        ::CertStrToNameW(X509_ASN_ENCODING, x500.c_str(), CERT_X500_NAME_STR, nullptr, nullptr, &nameSize, nullptr);

        std::vector<BYTE> name(nameSize);
        ::CertStrToNameW(X509_ASN_ENCODING, x500.c_str(), CERT_X500_NAME_STR, nullptr, name.data(), &nameSize, nullptr);

        CERT_NAME_BLOB subject{ nameSize, name.data() };

        LPSTR usages[]{ const_cast<LPSTR>(szOID_PKIX_KP_CODE_SIGNING) };
        CERT_ENHKEY_USAGE usage{ 1, usages };

        DWORD usageSize{ 0 };
        ::CryptEncodeObject(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &usage, nullptr, &usageSize);

        std::vector<BYTE> usageBytes(usageSize);
        ::CryptEncodeObject(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &usage, usageBytes.data(), &usageSize);

        CERT_EXTENSION extension{ const_cast<LPSTR>(szOID_ENHANCED_KEY_USAGE), FALSE, { usageSize, usageBytes.data() } };
        CERT_EXTENSIONS extensions{ 1, &extension };

        CRYPT_ALGORITHM_IDENTIFIER algorithm{ const_cast<LPSTR>(szOID_RSA_SHA256RSA), {} };

        SYSTEMTIME start{};
        ::GetSystemTime(&start);
        start.wDay = 1;
        start.wYear = static_cast<WORD>(start.wYear - 1);

        SYSTEMTIME end = start;
        end.wYear = static_cast<WORD>(end.wYear + 3);

        auto const certificate = ::CertCreateSelfSignCertificate(
            key, &subject, CERT_CREATE_SELFSIGN_NO_KEY_INFO, nullptr, &algorithm, &start, &end, &extensions);

        if (certificate == nullptr)
        {
            ::NCryptFreeObject(key);
            return nullptr;
        }

        CERT_KEY_CONTEXT keyContext{};
        keyContext.cbSize = sizeof(keyContext);
        keyContext.hNCryptKey = key;
        keyContext.dwKeySpec = CERT_NCRYPT_KEY_SPEC;

        if (!::CertSetCertificateContextProperty(certificate, CERT_KEY_CONTEXT_PROP_ID, 0, &keyContext))
        {
            ::NCryptFreeObject(key);
        }

        return certificate;
    }

    std::shared_ptr<midiapp::DeviceIdentity> Borrowed(_In_ DeviceLinkTestKey const& key)
    {
        // The test key outlives every receiver here.
        return std::shared_ptr<midiapp::DeviceIdentity>(key.Identity.get(), [](midiapp::DeviceIdentity*) {});
    }

    // Something one thread sets and another waits for.
    template <typename T>
    struct Waitable
    {
        std::mutex Lock{};
        std::condition_variable Changed{};
        std::optional<T> Value{};

        void Set(T value)
        {
            {
                std::scoped_lock guard{ Lock };
                Value = std::move(value);
            }

            Changed.notify_all();
        }

        bool Wait(_In_ std::chrono::milliseconds timeout)
        {
            std::unique_lock lock{ Lock };
            return Changed.wait_for(lock, timeout, [this]() { return Value.has_value(); });
        }
    };

    glass::sync::SyncReceiverOptions ReceiverOptions(_In_ std::wstring const& name)
    {
        auto const folder = g_folder / name;
        std::error_code ignored{};
        std::filesystem::create_directories(folder / L"Layouts", ignored);

        glass::sync::SyncReceiverOptions options{};
        options.Identity = Borrowed(*g_receiver);
        options.OwnName = L"Receiver";
        options.PairedListPath = (folder / L"Paired.json").wstring();
        options.Folders = { (folder / L"Layouts").wstring() };
        options.FileIndexPath = (folder / L"Index.json").wstring();
        options.LoopbackOnly = true;
        options.Verify = Offline();

        return options;
    }
}

bool GlassSyncTests::ClassSetup()
{
    g_folder = std::filesystem::temp_directory_path() / (L"GlassSyncTests-" + std::to_wstring(::GetCurrentProcessId()));

    std::error_code ignored{};
    std::filesystem::remove_all(g_folder, ignored);
    std::filesystem::create_directories(g_folder, ignored);

    g_sender = std::make_unique<DeviceLinkTestKey>();
    g_receiver = std::make_unique<DeviceLinkTestKey>();

    return g_sender->Identity != nullptr && g_receiver->Identity != nullptr;
}

bool GlassSyncTests::ClassCleanup()
{
    g_sender.reset();
    g_receiver.reset();

    std::error_code ignored{};
    std::filesystem::remove_all(g_folder, ignored);

    return true;
}

// ---------------------------------------------------------------------------- what to ask for

void GlassSyncTests::TheFirstSyncNeedsEverything()
{
    auto const source = SampleSource(SamplePack());

    auto const need = glass::sync::ComputeSyncNeed(source.Offer.ManifestBytes, HaveFrom({}));

    VERIFY_IS_TRUE(need.Problem == ContentPackProblem::None);
    VERIFY_ARE_EQUAL(size_t{ 3 }, need.Needed.size());
    VERIFY_ARE_EQUAL(HashOf(LayoutBytes()), need.Needed.front());
    VERIFY_ARE_EQUAL(source.TotalBytes, need.NeededBytes);
}

void GlassSyncTests::NeedAsksOnlyForMissingFiles()
{
    auto const source = SampleSource(SamplePack());

    auto const need = glass::sync::ComputeSyncNeed(source.Offer.ManifestBytes,
        HaveFrom({ { HashOf(PictureBytes()), L"C:\\somewhere\\Picture.png" } }));

    VERIFY_IS_TRUE(need.Problem == ContentPackProblem::None);
    VERIFY_ARE_EQUAL(size_t{ 2 }, need.Needed.size());
    VERIFY_ARE_EQUAL(HashOf(LayoutBytes()), need.Needed[0]);
    VERIFY_ARE_EQUAL(HashOf(ThemeBytes()), need.Needed[1]);
}

void GlassSyncTests::TheLayoutFileIsAlwaysAskedFor()
{
    auto const source = SampleSource(SamplePack());

    auto const need = glass::sync::ComputeSyncNeed(source.Offer.ManifestBytes, HaveFrom({
        { HashOf(LayoutBytes()), L"C:\\a" },
        { HashOf(PictureBytes()), L"C:\\b" },
        { HashOf(ThemeBytes()), L"C:\\c" } }));

    VERIFY_IS_TRUE(need.Problem == ContentPackProblem::None);
    VERIFY_ARE_EQUAL(size_t{ 1 }, need.Needed.size());
    VERIFY_ARE_EQUAL(HashOf(LayoutBytes()), need.Needed[0]);
}

void GlassSyncTests::NeedRefusesTheWrongKindAndTooMuch()
{
    midiapp::ContentPackInput theme{ L"Mine.miditheme", ThemeBytes(), std::nullopt };
    auto const themePack = midiapp::BuildContentPack(ContentPackKind::GlassTheme, L"Mine.miditheme", { theme });

    auto const themeManifest = midiapp::ReadContentPackManifestBytes(themePack);
    VERIFY_IS_TRUE(glass::sync::ComputeSyncNeed(themeManifest, nullptr).Problem == ContentPackProblem::WrongKind);

    // A manifest that promises more than any pack may hold.
    midiapp::ContentPackManifest manifest{};
    manifest.Kind = ContentPackKind::GlassLayout;
    manifest.Primary = L"Layout.midilayout";
    manifest.Files.push_back(midiapp::ContentPackFile{ L"Layout.midilayout", 10, std::wstring(64, L'a'), std::nullopt });
    manifest.Files.push_back(midiapp::ContentPackFile{ L"Huge.mp4", midiapp::MaximumContentPackBytes, std::wstring(64, L'b'), std::nullopt });

    auto const text = midiapp::WriteContentPackManifest(manifest);
    auto const utf8 = midiapp::DeviceLinkUtf8(text);

    auto const need = glass::sync::ComputeSyncNeed(std::vector<uint8_t>{ utf8.begin(), utf8.end() }, nullptr);
    VERIFY_IS_TRUE(need.Problem == ContentPackProblem::TooBig);

    // And something that isn't a manifest at all.
    VERIFY_IS_TRUE(glass::sync::ComputeSyncNeed(Bytes("not json"), nullptr).Problem != ContentPackProblem::None);
    VERIFY_IS_TRUE(glass::sync::ComputeSyncNeed({}, nullptr).Problem != ContentPackProblem::None);
}

// ---------------------------------------------------------------------------- putting it back together

void GlassSyncTests::AssemblyGivesAPackThatChecksOut()
{
    auto const source = SampleSource(SamplePack());

    auto const picture = g_folder / L"Assembly" / L"Picture.png";
    WriteAll(picture, PictureBytes());

    auto const have = HaveFrom({ { HashOf(PictureBytes()), picture.wstring() } });
    auto const need = glass::sync::ComputeSyncNeed(source.Offer.ManifestBytes, have);

    auto const assembled = glass::sync::AssembleSyncPack(source.Offer, Send(source, need.Needed), have);
    VERIFY_IS_TRUE(assembled.Problem == ContentPackProblem::None);

    auto const opened = midiapp::OpenContentPack(assembled.Pack, ContentPackKind::GlassLayout, Offline());
    VERIFY_IS_TRUE(opened.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 3 }, opened.Files.size());
    VERIFY_IS_TRUE(opened.ManifestBytes == source.Offer.ManifestBytes);
    VERIFY_IS_TRUE(opened.FindFile(L"Picture.png")->Bytes == PictureBytes());
}

void GlassSyncTests::AChangedLocalFileCountsAsMissing()
{
    auto const source = SampleSource(SamplePack());

    // Indexed with the right hash, then changed on disk before the sync came in.
    auto const picture = g_folder / L"Changed" / L"Picture.png";
    WriteAll(picture, Bytes("not really a pictur3"));

    auto const have = HaveFrom({ { HashOf(PictureBytes()), picture.wstring() } });
    auto const need = glass::sync::ComputeSyncNeed(source.Offer.ManifestBytes, have);

    auto const assembled = glass::sync::AssembleSyncPack(source.Offer, Send(source, need.Needed), have);
    VERIFY_IS_TRUE(assembled.Problem == ContentPackProblem::MissingFile);
    VERIFY_ARE_EQUAL(std::wstring{ L"Picture.png" }, assembled.ProblemFile);
}

void GlassSyncTests::AMissingFileIsNamed()
{
    auto const source = SampleSource(SamplePack());

    auto received = Send(source, { HashOf(LayoutBytes()), HashOf(PictureBytes()) });

    auto const assembled = glass::sync::AssembleSyncPack(source.Offer, received, HaveFrom({}));
    VERIFY_IS_TRUE(assembled.Problem == ContentPackProblem::MissingFile);
    VERIFY_ARE_EQUAL(std::wstring{ L"theme/Mine.miditheme" }, assembled.ProblemFile);
}

void GlassSyncTests::ATamperedFileFailsTheCheck()
{
    auto const source = SampleSource(SamplePack());

    auto received = Send(source, { HashOf(LayoutBytes()), HashOf(PictureBytes()), HashOf(ThemeBytes()) });
    received[HashOf(PictureBytes())] = Bytes("something else entirely");

    auto const assembled = glass::sync::AssembleSyncPack(source.Offer, received, HaveFrom({}));
    VERIFY_IS_TRUE(assembled.Problem == ContentPackProblem::None);

    // Assembly only moves bytes; the pack reader is what catches it, as it would for a file.
    auto const opened = midiapp::OpenContentPack(assembled.Pack, ContentPackKind::GlassLayout, Offline());
    VERIFY_IS_FALSE(opened.Succeeded());
    VERIFY_IS_TRUE(opened.Problem == ContentPackProblem::SizeMismatch || opened.Problem == ContentPackProblem::HashMismatch);
}

void GlassSyncTests::ASignedPackSurvivesADeltaSync()
{
    auto const certificate = MakeSigningCertificate();
    VERIFY_IS_NOT_NULL(certificate);

    HRESULT error{ E_FAIL };
    auto const pack = SamplePack();
    auto const signature = midiapp::SignContentPackManifest(midiapp::ReadContentPackManifestBytes(pack), certificate, std::wstring{}, error);
    VERIFY_SUCCEEDED(error);

    auto const signedPack = midiapp::AddContentPackSignature(pack, midiapp::ContentPackSignatureFileName(certificate), signature);

    auto const source = SampleSource(signedPack);
    VERIFY_ARE_EQUAL(size_t{ 1 }, source.Offer.Signatures.size());

    auto const picture = g_folder / L"Signed" / L"Picture.png";
    WriteAll(picture, PictureBytes());

    auto const have = HaveFrom({ { HashOf(PictureBytes()), picture.wstring() } });
    auto const need = glass::sync::ComputeSyncNeed(source.Offer.ManifestBytes, have);
    VERIFY_ARE_EQUAL(size_t{ 2 }, need.Needed.size());

    auto const assembled = glass::sync::AssembleSyncPack(source.Offer, Send(source, need.Needed), have);
    VERIFY_IS_TRUE(assembled.Problem == ContentPackProblem::None);

    auto const store = ::CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, nullptr);
    VERIFY_IS_NOT_NULL(store);
    VERIFY_IS_TRUE(::CertAddCertificateContextToStore(store, certificate, CERT_STORE_ADD_ALWAYS, nullptr) != FALSE);

    auto options = Offline();
    options.ExclusiveRootStore = store;

    auto const opened = midiapp::OpenContentPack(assembled.Pack, ContentPackKind::GlassLayout, options);

    ::CertCloseStore(store, 0);
    ::CertFreeCertificateContext(certificate);

    VERIFY_IS_TRUE(opened.Succeeded());
    VERIFY_IS_NOT_NULL(opened.TrustedSignature());
    VERIFY_ARE_EQUAL(std::wstring{ L"Glass Sync Test Publisher" }, opened.TrustedSignature()->SignerName);
}

void GlassSyncTests::ABadSignatureNameIsRefused()
{
    auto source = SampleSource(SamplePack());

    glass::sync::SyncSignature outside{ L"Picture.png", Bytes("sig") };
    source.Offer.Signatures.push_back(outside);

    auto const received = Send(source, { HashOf(LayoutBytes()), HashOf(PictureBytes()), HashOf(ThemeBytes()) });

    auto const assembled = glass::sync::AssembleSyncPack(source.Offer, received, HaveFrom({}));
    VERIFY_IS_TRUE(assembled.Problem == ContentPackProblem::BadName);

    VERIFY_IS_FALSE(glass::sync::IsAllowedSyncSignature({ L"signatures/../manifest.json", Bytes("x") }));
    VERIFY_IS_FALSE(glass::sync::IsAllowedSyncSignature({ L"signatures/a.p7s", {} }));
    VERIFY_IS_TRUE(glass::sync::IsAllowedSyncSignature({ L"signatures/a.p7s", Bytes("x") }));
}

// ---------------------------------------------------------------------------- what the receiver remembers

void GlassSyncTests::TheIndexFindsFilesByHash()
{
    auto const folder = g_folder / L"Index";
    WriteAll(folder / L"a.bin", Bytes("alpha"));
    WriteAll(folder / L"b.bin", Bytes("bravo!"));

    auto const cache = (g_folder / L"IndexCache.json").wstring();

    {
        glass::sync::FileHashIndex index{ cache };

        // Only files of a size that could be wanted are hashed.
        std::vector<uint64_t> const sizes{ 5 };
        index.Refresh({ folder.wstring() }, &sizes);

        VERIFY_ARE_EQUAL((folder / L"a.bin").wstring(), index.Find(HashOf(Bytes("alpha"))));
        VERIFY_IS_TRUE(index.Find(HashOf(Bytes("bravo!"))).empty());

        index.Refresh({ folder.wstring() }, nullptr);
        VERIFY_ARE_EQUAL((folder / L"b.bin").wstring(), index.Find(HashOf(Bytes("bravo!"))));

        VERIFY_IS_TRUE(index.Save());
    }

    {
        // From the cache: nothing needs hashing again, so even a size filter that excludes them
        // keeps files that haven't changed.
        glass::sync::FileHashIndex index{ cache };

        std::vector<uint64_t> const none{};
        index.Refresh({ folder.wstring() }, &none);

        VERIFY_ARE_EQUAL(size_t{ 2 }, index.Count());
        VERIFY_ARE_EQUAL((folder / L"b.bin").wstring(), index.Find(HashOf(Bytes("bravo!"))));
    }

    VERIFY_ARE_EQUAL(HashOf(Bytes("alpha")), glass::sync::HashFileSha256((folder / L"a.bin").wstring(), 1024));
    VERIFY_IS_TRUE(glass::sync::HashFileSha256((folder / L"a.bin").wstring(), 4).empty());
}

void GlassSyncTests::ApprovalsArePerLayoutAndSender()
{
    glass::sync::SessionApprovals approvals{};

    auto const layout = glass::sync::SyncLayoutKey(L"ABC", L"Layout.midilayout");
    auto const other = glass::sync::SyncLayoutKey(L"", L"Other.midilayout");

    VERIFY_ARE_EQUAL(std::wstring{ L"id:abc" }, layout);
    VERIFY_ARE_EQUAL(std::wstring{ L"file:other.midilayout" }, other);

    approvals.Approve(layout, L"AAAA");

    VERIFY_IS_TRUE(approvals.IsApproved(layout, L"aaaa"));
    VERIFY_IS_FALSE(approvals.IsApproved(layout, L"bbbb"));
    VERIFY_IS_FALSE(approvals.IsApproved(other, L"aaaa"));

    // Unpairing a PC ends what it was allowed.
    approvals.ForgetSender(L"aaaa");
    VERIFY_IS_FALSE(approvals.IsApproved(layout, L"aaaa"));
}

void GlassSyncTests::ALocalEditIsNoticed()
{
    auto const layout = g_folder / L"Edits" / L"Song.midilayout";
    WriteAll(layout, Bytes(R"({ "name": "Song" })"));

    glass::sync::SyncRecords records{ (g_folder / L"Edits" / L"Records.json").wstring() };

    auto const key = glass::sync::SyncLayoutKey(L"song-id", L"Song.midilayout");

    VERIFY_IS_TRUE(glass::sync::CheckLocalCopy(records, key, L"sender", layout.wstring()) == glass::sync::LocalCopyState::Unknown);

    glass::sync::SyncRecord record{};
    record.LayoutKey = key;
    record.SenderKey = L"sender";
    record.InstalledPath = layout.wstring();
    record.InstalledSha256 = glass::sync::HashFileSha256(layout.wstring(), 1024);

    VERIFY_IS_TRUE(records.Remember(record));

    VERIFY_IS_TRUE(glass::sync::CheckLocalCopy(records, key, L"SENDER", layout.wstring()) == glass::sync::LocalCopyState::Unchanged);
    VERIFY_IS_TRUE(glass::sync::CheckLocalCopy(records, key, L"another", layout.wstring()) == glass::sync::LocalCopyState::Unknown);

    // Changed on this PC.
    WriteAll(layout, Bytes(R"({ "name": "Song, my version" })"));
    VERIFY_IS_TRUE(glass::sync::CheckLocalCopy(records, key, L"sender", layout.wstring()) == glass::sync::LocalCopyState::ChangedHere);

    // A different file now holds that layout.
    VERIFY_IS_TRUE(glass::sync::CheckLocalCopy(records, key, L"sender", (g_folder / L"Elsewhere.midilayout").wstring()) == glass::sync::LocalCopyState::ChangedHere);

    VERIFY_IS_TRUE(records.ForgetSender(L"sender"));
    VERIFY_IS_FALSE(records.Find(key, L"sender").has_value());
}

// ---------------------------------------------------------------------------- over the network

void GlassSyncTests::ASyncSendsOnlyWhatIsMissing()
{
    auto options = ReceiverOptions(L"Delta");

    midiapp::PairedDeviceList{ options.PairedListPath }.Remember(g_sender->Identity->KeyHash(), L"Sender");

    // The receiver already has the picture, under a different name even.
    WriteAll(std::filesystem::path{ options.Folders.front() } / L"Same picture.png", PictureBytes());

    Waitable<glass::sync::ReceivedSync> received{};
    options.SyncReceived = [&received](glass::sync::ReceivedSync&& sync) { received.Set(std::move(sync)); };

    glass::sync::SyncReceiver receiver{};
    VERIFY_SUCCEEDED(receiver.Start(options));

    auto const source = SampleSource(SamplePack());

    uint64_t lastSent{ 0 };
    uint64_t lastTotal{ 0 };

    glass::sync::SyncSender sender{};
    auto const result = sender.Send(L"127.0.0.1", receiver.Port(), *g_sender->Identity, L"Sender",
        g_receiver->Identity->KeyHash(), source,
        [&lastSent, &lastTotal](uint64_t sent, uint64_t total) { lastSent = sent; lastTotal = total; });

    VERIFY_IS_TRUE(received.Wait(std::chrono::seconds{ 20 }));
    receiver.Stop();

    VERIFY_IS_TRUE(result.Outcome == glass::sync::SendOutcome::Sent);
    VERIFY_ARE_EQUAL(2u, result.FilesSent);
    VERIFY_ARE_EQUAL(1u, result.FilesSkipped);
    VERIFY_ARE_EQUAL(static_cast<uint64_t>(LayoutBytes().size() + ThemeBytes().size()), result.BytesSent);
    VERIFY_ARE_EQUAL(lastTotal, lastSent);

    auto const& sync = *received.Value;
    VERIFY_IS_TRUE(sync.Pack.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 3 }, sync.Pack.Files.size());
    VERIFY_ARE_EQUAL(2u, sync.FilesReceived);
    VERIFY_ARE_EQUAL(1u, sync.FilesReused);
    VERIFY_ARE_EQUAL(std::wstring{ L"Sender" }, sync.SenderName);
    VERIFY_ARE_EQUAL(std::wstring{ L"Sample" }, sync.Offer.LayoutName);
    VERIFY_IS_TRUE(midiapp::SameDeviceKey(sync.Sender, g_sender->Identity->KeyHash()));
}

void GlassSyncTests::AnUnpairedSenderIsRefused()
{
    auto options = ReceiverOptions(L"Unpaired");

    auto started = false;
    options.SyncStarted = [&started](std::wstring const&, std::wstring const&) { started = true; };

    glass::sync::SyncReceiver receiver{};
    VERIFY_SUCCEEDED(receiver.Start(options));

    glass::sync::SyncSender sender{};
    auto const result = sender.Send(L"127.0.0.1", receiver.Port(), *g_sender->Identity, L"Sender",
        g_receiver->Identity->KeyHash(), SampleSource(SamplePack()), nullptr);

    receiver.Stop();

    VERIFY_IS_TRUE(result.Outcome == glass::sync::SendOutcome::NotPaired);
    VERIFY_ARE_EQUAL(0u, result.FilesSent);
    VERIFY_IS_FALSE(started);
}

void GlassSyncTests::NothingIsSentToTheWrongKey()
{
    auto options = ReceiverOptions(L"WrongKey");

    midiapp::PairedDeviceList{ options.PairedListPath }.Remember(g_sender->Identity->KeyHash(), L"Sender");

    auto started = false;
    options.SyncStarted = [&started](std::wstring const&, std::wstring const&) { started = true; };

    glass::sync::SyncReceiver receiver{};
    VERIFY_SUCCEEDED(receiver.Start(options));

    // The sender expects some other PC's key at this address.
    glass::sync::SyncSender sender{};
    auto const result = sender.Send(L"127.0.0.1", receiver.Port(), *g_sender->Identity, L"Sender",
        g_sender->Identity->KeyHash(), SampleSource(SamplePack()), nullptr);

    receiver.Stop();

    VERIFY_IS_TRUE(result.Outcome == glass::sync::SendOutcome::NotThePairedDevice);
    VERIFY_ARE_EQUAL(0u, result.FilesSent);
    VERIFY_IS_FALSE(started);
}

void GlassSyncTests::PairingNeedsPairingOpenAndTwoYeses()
{
    // Pairing closed on the receiver.
    {
        auto options = ReceiverOptions(L"PairClosed");
        options.IsPairingOpen = []() { return false; };

        glass::sync::SyncReceiver receiver{};
        VERIFY_SUCCEEDED(receiver.Start(options));

        glass::sync::SyncPairingInitiator initiator{};
        auto const outcome = initiator.Start(L"127.0.0.1", receiver.Port(), *g_sender->Identity, L"Sender");

        receiver.Stop();

        VERIFY_IS_TRUE(outcome == glass::sync::PairOutcome::NotPairing);
        VERIFY_IS_FALSE(midiapp::PairedDeviceList{ options.PairedListPath }.Find(g_sender->Identity->KeyHash()).has_value());
    }

    // Open, and both people say the codes match.
    {
        auto options = ReceiverOptions(L"PairYes");
        options.IsPairingOpen = []() { return true; };

        std::wstring codeShownHere{};
        Waitable<bool> finished{};

        options.PairingAsked = [&codeShownHere](std::shared_ptr<glass::sync::PendingPairing> const& pending)
            {
                codeShownHere = pending->Code;
                pending->Answer(true);
            };

        options.PairingFinished = [&finished](std::shared_ptr<glass::sync::PendingPairing> const&, bool paired) { finished.Set(paired); };

        glass::sync::SyncReceiver receiver{};
        VERIFY_SUCCEEDED(receiver.Start(options));

        glass::sync::SyncPairingInitiator initiator{};
        VERIFY_IS_TRUE(initiator.Start(L"127.0.0.1", receiver.Port(), *g_sender->Identity, L"Sender") == glass::sync::PairOutcome::CodeReady);
        VERIFY_ARE_EQUAL(std::wstring{ L"Receiver" }, initiator.PeerName());

        midiapp::PairedDeviceList senderList{ (g_folder / L"PairYes" / L"SenderPaired.json").wstring() };
        VERIFY_IS_TRUE(initiator.Finish(true, senderList) == glass::sync::PairOutcome::Paired);

        VERIFY_IS_TRUE(finished.Wait(std::chrono::seconds{ 10 }));
        receiver.Stop();

        VERIFY_IS_TRUE(*finished.Value);
        VERIFY_ARE_EQUAL(codeShownHere, initiator.Code());
        VERIFY_ARE_EQUAL(size_t{ 7 }, initiator.Code().size());

        VERIFY_IS_TRUE(senderList.Find(g_receiver->Identity->KeyHash()).has_value());
        VERIFY_IS_TRUE(midiapp::PairedDeviceList{ options.PairedListPath }.Find(g_sender->Identity->KeyHash()).has_value());
    }

    // The person on the receiver says they don't match: neither side remembers anything.
    {
        auto options = ReceiverOptions(L"PairNo");
        options.IsPairingOpen = []() { return true; };
        options.PairingAsked = [](std::shared_ptr<glass::sync::PendingPairing> const& pending) { pending->Answer(false); };

        Waitable<bool> finished{};
        options.PairingFinished = [&finished](std::shared_ptr<glass::sync::PendingPairing> const&, bool paired) { finished.Set(paired); };

        glass::sync::SyncReceiver receiver{};
        VERIFY_SUCCEEDED(receiver.Start(options));

        glass::sync::SyncPairingInitiator initiator{};
        VERIFY_IS_TRUE(initiator.Start(L"127.0.0.1", receiver.Port(), *g_sender->Identity, L"Sender") == glass::sync::PairOutcome::CodeReady);

        midiapp::PairedDeviceList senderList{ (g_folder / L"PairNo" / L"SenderPaired.json").wstring() };
        VERIFY_IS_TRUE(initiator.Finish(true, senderList) == glass::sync::PairOutcome::Declined);

        VERIFY_IS_TRUE(finished.Wait(std::chrono::seconds{ 10 }));
        receiver.Stop();

        VERIFY_IS_FALSE(*finished.Value);
        VERIFY_IS_FALSE(senderList.Find(g_receiver->Identity->KeyHash()).has_value());
        VERIFY_IS_FALSE(midiapp::PairedDeviceList{ options.PairedListPath }.Find(g_sender->Identity->KeyHash()).has_value());
    }
}
