// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ContentPackTests.h"

#include "ContentPack.h"
#include "ZipArchive.h"
#include "LayoutModel.h"
#include "LayoutPack.h"
#include "LayoutStore.h"
#include "SignedItems.h"

#include <ncrypt.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    using midiapp::ContentPackKind;
    using midiapp::ContentPackProblem;
    using midiapp::StoredZipEntry;
    using midiapp::StoredZipStatus;

    std::filesystem::path g_folder{};

    std::vector<uint8_t> Bytes(_In_ std::string_view text)
    {
        return std::vector<uint8_t>{ text.begin(), text.end() };
    }

    std::vector<uint8_t> ReadAll(_In_ std::filesystem::path const& path)
    {
        std::ifstream file{ path, std::ios::binary };

        return std::vector<uint8_t>{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
    }

    void WriteAll(_In_ std::filesystem::path const& path, _In_ std::vector<uint8_t> const& bytes)
    {
        std::ofstream file{ path, std::ios::binary | std::ios::trunc };

        file.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    std::vector<midiapp::ContentPackInput> SampleInputs()
    {
        midiapp::ContentPackInput layout{};
        layout.Path = L"Layout.midilayout";
        layout.Bytes = Bytes(R"({ "name": "Sample" })");

        midiapp::ContentPackInput picture{};
        picture.Path = L"Picture.png";
        picture.Bytes = Bytes("not really a picture");

        midiapp::ContentPackInput theme{};
        theme.Path = L"theme/Mine.miditheme";
        theme.Bytes = Bytes(R"({ "name": "Mine" })");

        return { layout, picture, theme };
    }

    std::vector<uint8_t> SamplePack()
    {
        return midiapp::BuildContentPack(ContentPackKind::GlassLayout, L"Layout.midilayout", SampleInputs());
    }

    // No network: the certificates here have nowhere to ask about revocation.
    midiapp::ContentPackVerifyOptions Offline()
    {
        midiapp::ContentPackVerifyOptions options{};
        options.CheckRevocation = false;

        return options;
    }

    // The entries changed and zipped again with fresh checksums, so only the pack's own rules can
    // catch what was done.
    std::vector<uint8_t> Rebuild(
        _In_ std::vector<uint8_t> const& pack,
        _In_ std::function<void(std::vector<StoredZipEntry>&)> const& change)
    {
        std::vector<StoredZipEntry> entries{};

        VERIFY_IS_TRUE(midiapp::ReadStoredZip(pack, 256, entries) == StoredZipStatus::Read);

        change(entries);

        return midiapp::BuildStoredZip(entries);
    }

    // A code-signing certificate signed by itself, with a key that lives only as long as it does.
    class TestCertificate
    {
    public:
        explicit TestCertificate(_In_ wchar_t const* commonName) :
            m_context(Make(commonName))
        {
        }

        ~TestCertificate()
        {
            if (m_context != nullptr)
            {
                ::CertFreeCertificateContext(m_context);
            }
        }

        TestCertificate(TestCertificate const&) = delete;
        TestCertificate& operator=(TestCertificate const&) = delete;

        PCCERT_CONTEXT Get() const noexcept { return m_context; }

    private:
        static PCCERT_CONTEXT Make(_In_ wchar_t const* commonName)
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

            std::wstring const x500 = std::wstring{ L"CN=" } + commonName;

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

            // The certificate owns the key from here, which is how signing finds it.
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

        PCCERT_CONTEXT m_context{ nullptr };
    };

    std::vector<uint8_t> Signed(_In_ std::vector<uint8_t> const& pack, _In_ PCCERT_CONTEXT certificate)
    {
        HRESULT error{ E_FAIL };

        auto const signature = midiapp::SignContentPackManifest(
            midiapp::ReadContentPackManifestBytes(pack), certificate, std::wstring{}, error);

        VERIFY_SUCCEEDED(error);
        VERIFY_IS_FALSE(signature.empty());

        auto signedPack = midiapp::AddContentPackSignature(
            pack, midiapp::ContentPackSignatureFileName(certificate), signature);

        VERIFY_IS_FALSE(signedPack.empty());

        return signedPack;
    }

    std::filesystem::path WriteSampleLayout(_In_ std::filesystem::path const& folder)
    {
        std::error_code ignored{};
        std::filesystem::create_directories(folder, ignored);

        WriteAll(folder / L"Logo.png", Bytes("logo bytes"));

        glass::LayoutDocument document{};
        document.Name = L"Packed";
        document.BackgroundImage = L"Logo.png";

        midiapp::ContentProvenance provenance{};
        provenance.Id = L"0d9c8b7a-6f5e-4d3c-2b1a-098f7e6d5c4b";
        provenance.Author = L"Pat Example";

        document.Provenance = provenance;

        auto const path = folder / L"Packed.midilayout";

        VERIFY_IS_TRUE(glass::WriteLayoutFile(document, path.wstring()));

        return path;
    }
}

bool ContentPackTests::Setup()
{
    std::error_code ignored{};

    g_folder = std::filesystem::temp_directory_path(ignored) /
        (L"glass-packs-" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));

    std::filesystem::create_directories(g_folder, ignored);

    return true;
}

bool ContentPackTests::Cleanup()
{
    glass::UseSignedItemsFile({});

    std::error_code ignored{};
    std::filesystem::remove_all(g_folder, ignored);

    return true;
}

// ---------------------------------------------------------------- the zip

void ContentPackTests::AStoredZipRoundTrips()
{
    std::vector<StoredZipEntry> const entries{
        StoredZipEntry{ L"Gr\u00FC\u00DFe.png", Bytes("hello") },
        StoredZipEntry{ L"empty.txt", {} },
    };

    auto const zip = midiapp::BuildStoredZip(entries);

    std::vector<StoredZipEntry> read{};

    VERIFY_IS_TRUE(midiapp::ReadStoredZip(zip, 16, read) == StoredZipStatus::Read);
    VERIFY_ARE_EQUAL(size_t{ 2 }, read.size());
    VERIFY_ARE_EQUAL(entries[0].Name, read[0].Name);
    VERIFY_IS_TRUE(entries[0].Bytes == read[0].Bytes);
    VERIFY_ARE_EQUAL(entries[1].Name, read[1].Name);
    VERIFY_IS_TRUE(read[1].Bytes.empty());

    auto const hello = Bytes("hello");
    VERIFY_ARE_EQUAL(0x3610A686u, midiapp::ComputeCrc32(hello.data(), hello.size()));

    // The same files always make the same bytes.
    VERIFY_IS_TRUE(zip == midiapp::BuildStoredZip(entries));
}

void ContentPackTests::AChangedByteIsDamage()
{
    auto zip = midiapp::BuildStoredZip({ StoredZipEntry{ L"a.txt", Bytes("hello") } });

    std::string_view const hello{ "hello" };

    auto const at = std::search(zip.begin(), zip.end(), hello.begin(), hello.end());
    VERIFY_IS_TRUE(at != zip.end());

    *at = 'j';

    std::vector<StoredZipEntry> read{};
    VERIFY_IS_TRUE(midiapp::ReadStoredZip(zip, 16, read) == StoredZipStatus::Damaged);
    VERIFY_IS_TRUE(read.empty());
}

void ContentPackTests::ADirectoryThatDisagreesIsRefused()
{
    auto zip = midiapp::BuildStoredZip({ StoredZipEntry{ L"a.png", Bytes("picture") } });

    // The second copy of the name is the one in the central directory.
    std::string_view const name{ "a.png" };

    auto const first = std::search(zip.begin(), zip.end(), name.begin(), name.end());
    auto const second = std::search(first + 1, zip.end(), name.begin(), name.end());
    VERIFY_IS_TRUE(second != zip.end());

    *second = 'b';

    std::vector<StoredZipEntry> read{};
    VERIFY_IS_TRUE(midiapp::ReadStoredZip(zip, 16, read) == StoredZipStatus::DirectoryMismatch);
}

void ContentPackTests::ACompressedEntryIsNamedAsSuch()
{
    auto zip = midiapp::BuildStoredZip({ StoredZipEntry{ L"a.txt", Bytes("hello") } });

    // Method 8 is deflate.
    zip[8] = 8;

    std::vector<StoredZipEntry> read{};
    VERIFY_IS_TRUE(midiapp::ReadStoredZip(zip, 16, read) == StoredZipStatus::Compressed);
}

void ContentPackTests::TooManyEntriesAreRefused()
{
    std::vector<StoredZipEntry> entries{};

    for (int i = 0; i < 5; ++i)
    {
        entries.push_back(StoredZipEntry{ L"file" + std::to_wstring(i) + L".txt", Bytes("x") });
    }

    std::vector<StoredZipEntry> read{};
    VERIFY_IS_TRUE(midiapp::ReadStoredZip(midiapp::BuildStoredZip(entries), 4, read) == StoredZipStatus::TooManyEntries);
}

void ContentPackTests::SomethingElseIsNotAZip()
{
    std::vector<StoredZipEntry> read{};

    VERIFY_IS_TRUE(midiapp::ReadStoredZip(Bytes("this is just some text, not a zip at all"), 16, read) == StoredZipStatus::NotAZip);
    VERIFY_IS_TRUE(midiapp::ReadStoredZip({}, 16, read) == StoredZipStatus::NotAZip);
}

// ---------------------------------------------------------------- the pack

void ContentPackTests::APackRoundTrips()
{
    auto inputs = SampleInputs();

    midiapp::ContentProvenance credit{};
    credit.Author = L"A Photographer";
    credit.License = L"CC-BY-4.0";

    inputs[1].Provenance = credit;

    auto const pack = midiapp::BuildContentPack(ContentPackKind::GlassLayout, L"Layout.midilayout", inputs);
    VERIFY_IS_FALSE(pack.empty());

    auto const opened = midiapp::OpenContentPack(pack, ContentPackKind::GlassLayout, Offline());

    VERIFY_IS_TRUE(opened.Succeeded());
    VERIFY_IS_TRUE(opened.Manifest.Kind == ContentPackKind::GlassLayout);
    VERIFY_ARE_EQUAL(std::wstring{ L"Layout.midilayout" }, opened.Manifest.Primary);
    VERIFY_ARE_EQUAL(size_t{ 3 }, opened.Files.size());
    VERIFY_IS_TRUE(opened.Signatures.empty());
    VERIFY_IS_NULL(opened.TrustedSignature());
    VERIFY_ARE_EQUAL(size_t{ 64 }, opened.PackSha256.size());

    auto const picture = opened.FindListing(L"Picture.png");
    VERIFY_IS_NOT_NULL(picture);
    VERIFY_IS_TRUE(picture->Provenance.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"A Photographer" }, picture->Provenance->Author);

    auto const theme = opened.FindFile(L"theme/Mine.miditheme");
    VERIFY_IS_NOT_NULL(theme);
    VERIFY_IS_TRUE(theme->Bytes == inputs[2].Bytes);

    VERIFY_IS_TRUE(pack == midiapp::BuildContentPack(ContentPackKind::GlassLayout, L"Layout.midilayout", inputs));
}

void ContentPackTests::PackPathsAreBareAndSafe()
{
    VERIFY_IS_TRUE(midiapp::IsAllowedContentPackPath(L"Layout.midilayout"));
    VERIFY_IS_TRUE(midiapp::IsAllowedContentPackPath(L"theme/Deck.png"));

    for (auto const path : {
        L"", L"../Deck.png", L"a/b/c.png", L"/Deck.png", L"theme/", L"C:Deck.png", L"theme\\Deck.png",
        L"con.png", L"COM1.txt", L"Deck.png.", L" Deck.png", L"De\u202Eck.png", L"De?ck.png", L"theme/../x.png" })
    {
        Log::Comment(String().Format(L"Refused: [%s]", path));
        VERIFY_IS_FALSE(midiapp::IsAllowedContentPackPath(path));
    }
}

void ContentPackTests::AChangedFileIsCaught()
{
    auto const longer = Rebuild(SamplePack(), [](std::vector<StoredZipEntry>& entries)
        {
            for (auto& entry : entries)
            {
                if (entry.Name == L"Picture.png")
                {
                    entry.Bytes.push_back('!');
                }
            }
        });

    auto const sizeChanged = midiapp::OpenContentPack(longer, ContentPackKind::GlassLayout, Offline());
    VERIFY_IS_TRUE(sizeChanged.Problem == ContentPackProblem::SizeMismatch);
    VERIFY_ARE_EQUAL(std::wstring{ L"Picture.png" }, sizeChanged.ProblemFile);
    VERIFY_IS_TRUE(sizeChanged.Files.empty());

    auto const flipped = Rebuild(SamplePack(), [](std::vector<StoredZipEntry>& entries)
        {
            for (auto& entry : entries)
            {
                if (entry.Name == L"Picture.png")
                {
                    entry.Bytes[0] ^= 0x01;
                }
            }
        });

    auto const hashChanged = midiapp::OpenContentPack(flipped, ContentPackKind::GlassLayout, Offline());
    VERIFY_IS_TRUE(hashChanged.Problem == ContentPackProblem::HashMismatch);
    VERIFY_ARE_EQUAL(std::wstring{ L"Picture.png" }, hashChanged.ProblemFile);
}

void ContentPackTests::AFileTheListDoesNotNameIsRefused()
{
    auto const extra = Rebuild(SamplePack(), [](std::vector<StoredZipEntry>& entries)
        {
            entries.push_back(StoredZipEntry{ L"Extra.png", Bytes("not in the list") });
        });

    auto const opened = midiapp::OpenContentPack(extra, ContentPackKind::GlassLayout, Offline());
    VERIFY_IS_TRUE(opened.Problem == ContentPackProblem::UnlistedFile);
    VERIFY_ARE_EQUAL(std::wstring{ L"Extra.png" }, opened.ProblemFile);

    // The folder kept for C2PA doesn't have to be listed.
    auto const reserved = Rebuild(SamplePack(), [](std::vector<StoredZipEntry>& entries)
        {
            entries.push_back(StoredZipEntry{ L"META-INF/content_credential.c2pa", Bytes("later") });
        });

    VERIFY_IS_TRUE(midiapp::OpenContentPack(reserved, ContentPackKind::GlassLayout, Offline()).Succeeded());
}

void ContentPackTests::AFileTheListNamesMustBeThere()
{
    auto const missing = Rebuild(SamplePack(), [](std::vector<StoredZipEntry>& entries)
        {
            entries.erase(std::remove_if(entries.begin(), entries.end(),
                [](StoredZipEntry const& entry) { return entry.Name == L"Picture.png"; }), entries.end());
        });

    auto const opened = midiapp::OpenContentPack(missing, ContentPackKind::GlassLayout, Offline());
    VERIFY_IS_TRUE(opened.Problem == ContentPackProblem::MissingFile);
    VERIFY_ARE_EQUAL(std::wstring{ L"Picture.png" }, opened.ProblemFile);
}

void ContentPackTests::NamesThatDifferOnlyInCaseAreRefused()
{
    auto const twin = Rebuild(SamplePack(), [](std::vector<StoredZipEntry>& entries)
        {
            entries.push_back(StoredZipEntry{ L"PICTURE.PNG", Bytes("a twin") });
        });

    VERIFY_IS_TRUE(midiapp::OpenContentPack(twin, ContentPackKind::GlassLayout, Offline()).Problem ==
        ContentPackProblem::DuplicateName);

    // and a pack can't be built that way either
    auto inputs = SampleInputs();
    inputs.push_back(midiapp::ContentPackInput{ L"picture.png", Bytes("a twin"), std::nullopt });

    VERIFY_IS_TRUE(midiapp::BuildContentPack(ContentPackKind::GlassLayout, L"Layout.midilayout", inputs).empty());
}

void ContentPackTests::ANameThatClimbsOutIsRefused()
{
    auto const climbing = Rebuild(SamplePack(), [](std::vector<StoredZipEntry>& entries)
        {
            entries.push_back(StoredZipEntry{ L"../evil.png", Bytes("x") });
        });

    auto const opened = midiapp::OpenContentPack(climbing, ContentPackKind::GlassLayout, Offline());
    VERIFY_IS_TRUE(opened.Problem == ContentPackProblem::BadName);
    VERIFY_ARE_EQUAL(std::wstring{ L"../evil.png" }, opened.ProblemFile);
}

void ContentPackTests::TheKindIsChecked()
{
    VERIFY_IS_TRUE(midiapp::OpenContentPack(SamplePack(), ContentPackKind::GlassTheme, Offline()).Problem ==
        ContentPackProblem::WrongKind);

    VERIFY_IS_TRUE(midiapp::OpenContentPack(SamplePack(), ContentPackKind::Unknown, Offline()).Succeeded());
}

void ContentPackTests::ANewerFormatIsNamedAsSuch()
{
    auto const newer = Rebuild(SamplePack(), [](std::vector<StoredZipEntry>& entries)
        {
            for (auto& entry : entries)
            {
                if (entry.Name == L"manifest.json")
                {
                    std::string text{ entry.Bytes.begin(), entry.Bytes.end() };
                    std::string_view const version{ "\"formatVersion\": 1" };

                    auto const at = text.find(version);
                    VERIFY_IS_TRUE(at != std::string::npos);

                    text.replace(at, version.size(), "\"formatVersion\": 2");
                    entry.Bytes.assign(text.begin(), text.end());
                }
            }
        });

    VERIFY_IS_TRUE(midiapp::OpenContentPack(newer, ContentPackKind::GlassLayout, Offline()).Problem ==
        ContentPackProblem::NewerFormat);
}

// ---------------------------------------------------------------- signatures

void ContentPackTests::ASelfSignedPackIsNotTrusted()
{
    TestCertificate const certificate{ L"Glass Test Publisher" };
    VERIFY_IS_NOT_NULL(certificate.Get());

    auto const opened = midiapp::OpenContentPack(Signed(SamplePack(), certificate.Get()), ContentPackKind::GlassLayout, Offline());

    VERIFY_IS_TRUE(opened.Succeeded());
    VERIFY_ARE_EQUAL(size_t{ 1 }, opened.Signatures.size());
    VERIFY_IS_TRUE(opened.Signatures[0].Status == midiapp::PackSignatureStatus::Untrusted);
    VERIFY_ARE_EQUAL(std::wstring{ L"Glass Test Publisher" }, opened.Signatures[0].SignerName);
    VERIFY_IS_NULL(opened.TrustedSignature());
}

void ContentPackTests::ASignatureFromATrustedRootIsTrusted()
{
    TestCertificate const certificate{ L"Glass Test Publisher" };
    VERIFY_IS_NOT_NULL(certificate.Get());

    auto const pack = Signed(SamplePack(), certificate.Get());

    // Trusted the way this PC would trust a publisher whose root it has, without touching its stores.
    auto const store = ::CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, nullptr);
    VERIFY_IS_NOT_NULL(store);
    VERIFY_IS_TRUE(::CertAddCertificateContextToStore(store, certificate.Get(), CERT_STORE_ADD_ALWAYS, nullptr) != FALSE);

    auto options = Offline();
    options.ExclusiveRootStore = store;

    auto const opened = midiapp::OpenContentPack(pack, ContentPackKind::GlassLayout, options);

    ::CertCloseStore(store, 0);

    VERIFY_IS_TRUE(opened.Succeeded());

    auto const trusted = opened.TrustedSignature();
    VERIFY_IS_NOT_NULL(trusted);
    VERIFY_ARE_EQUAL(std::wstring{ L"Glass Test Publisher" }, trusted->SignerName);
    VERIFY_ARE_EQUAL(size_t{ 64 }, trusted->Thumbprint.size());
    VERIFY_ARE_EQUAL(0ll, trusted->SignedAt);
}

void ContentPackTests::ASignatureForAnotherPackIsBroken()
{
    TestCertificate const certificate{ L"Glass Test Publisher" };
    VERIFY_IS_NOT_NULL(certificate.Get());

    auto const first = Signed(SamplePack(), certificate.Get());

    std::vector<StoredZipEntry> entries{};
    VERIFY_IS_TRUE(midiapp::ReadStoredZip(first, 256, entries) == StoredZipStatus::Read);

    auto const signature = std::find_if(entries.begin(), entries.end(),
        [](StoredZipEntry const& entry) { return entry.Name.rfind(L"signatures/", 0) == 0; });
    VERIFY_IS_TRUE(signature != entries.end());

    auto inputs = SampleInputs();
    inputs[1].Bytes = Bytes("a different picture");

    auto const second = midiapp::BuildContentPack(ContentPackKind::GlassLayout, L"Layout.midilayout", inputs);
    auto const forged = midiapp::AddContentPackSignature(second, signature->Name, signature->Bytes);

    auto const opened = midiapp::OpenContentPack(forged, ContentPackKind::GlassLayout, Offline());

    VERIFY_IS_TRUE(opened.Problem == ContentPackProblem::BrokenSignature);
    VERIFY_ARE_EQUAL(signature->Name, opened.ProblemFile);
}

void ContentPackTests::ADamagedSignatureIsBroken()
{
    TestCertificate const certificate{ L"Glass Test Publisher" };
    VERIFY_IS_NOT_NULL(certificate.Get());

    auto const damaged = Rebuild(Signed(SamplePack(), certificate.Get()), [](std::vector<StoredZipEntry>& entries)
        {
            for (auto& entry : entries)
            {
                if (entry.Name.rfind(L"signatures/", 0) == 0)
                {
                    // Inside the signature value, which is near the end.
                    entry.Bytes[entry.Bytes.size() - 10] ^= 0xFF;
                }
            }
        });

    VERIFY_IS_TRUE(midiapp::OpenContentPack(damaged, ContentPackKind::GlassLayout, Offline()).Problem ==
        ContentPackProblem::BrokenSignature);
}

void ContentPackTests::AddingASignatureKeepsTheFiles()
{
    TestCertificate const certificate{ L"Glass Test Publisher" };
    VERIFY_IS_NOT_NULL(certificate.Get());

    auto const pack = SamplePack();
    auto const signedPack = Signed(pack, certificate.Get());

    auto const before = midiapp::OpenContentPack(pack, ContentPackKind::GlassLayout, Offline());
    auto const after = midiapp::OpenContentPack(signedPack, ContentPackKind::GlassLayout, Offline());

    VERIFY_IS_TRUE(before.Succeeded());
    VERIFY_IS_TRUE(after.Succeeded());
    VERIFY_IS_TRUE(before.ManifestBytes == after.ManifestBytes);
    VERIFY_ARE_EQUAL(before.Files.size(), after.Files.size());

    for (size_t i = 0; i < before.Files.size(); ++i)
    {
        VERIFY_ARE_EQUAL(before.Files[i].Name, after.Files[i].Name);
        VERIFY_IS_TRUE(before.Files[i].Bytes == after.Files[i].Bytes);
    }

    // Signed again with the same certificate, its signature is replaced rather than added to.
    auto const twice = midiapp::OpenContentPack(Signed(signedPack, certificate.Get()), ContentPackKind::GlassLayout, Offline());
    VERIFY_ARE_EQUAL(size_t{ 1 }, twice.Signatures.size());
}

// ---------------------------------------------------------------- layouts

void ContentPackTests::ALayoutPackCarriesItsPictures()
{
    auto const layout = WriteSampleLayout(g_folder / L"source");

    auto const built = glass::BuildLayoutPack(layout.wstring());

    VERIFY_IS_TRUE(built.Succeeded);
    VERIFY_ARE_EQUAL(2u, built.FileCount);

    auto const opened = midiapp::OpenContentPack(built.Bytes, ContentPackKind::GlassLayout, Offline());

    VERIFY_IS_TRUE(opened.Succeeded());
    VERIFY_ARE_EQUAL(std::wstring{ L"Packed.midilayout" }, opened.Manifest.Primary);
    VERIFY_IS_NOT_NULL(opened.FindFile(L"Logo.png"));

    auto const target = g_folder / L"target";
    auto const installed = glass::InstallLayoutPack(opened, target.wstring(), {});

    VERIFY_IS_TRUE(installed.Succeeded);
    VERIFY_ARE_EQUAL((target / L"Packed.midilayout").wstring(), installed.Path);
    VERIFY_ARE_EQUAL(size_t{ 2 }, installed.Files.size());

    // Both arrive exactly as they were packed.
    VERIFY_IS_TRUE(ReadAll(target / L"Packed.midilayout") == ReadAll(layout));
    VERIFY_IS_TRUE(ReadAll(target / L"Logo.png") == Bytes("logo bytes"));
}

void ContentPackTests::AnInstallNeverWritesOverAnotherPicture()
{
    auto const layout = WriteSampleLayout(g_folder / L"source");
    auto const built = glass::BuildLayoutPack(layout.wstring());
    auto const opened = midiapp::OpenContentPack(built.Bytes, ContentPackKind::GlassLayout, Offline());

    VERIFY_IS_TRUE(opened.Succeeded());

    // Somebody else's picture already has the name.
    auto const target = g_folder / L"target";

    std::error_code ignored{};
    std::filesystem::create_directories(target, ignored);
    WriteAll(target / L"Logo.png", Bytes("someone else's logo"));

    auto const installed = glass::InstallLayoutPack(opened, target.wstring(), {});

    VERIFY_IS_TRUE(installed.Succeeded);
    VERIFY_IS_TRUE(ReadAll(target / L"Logo.png") == Bytes("someone else's logo"));
    VERIFY_IS_TRUE(ReadAll(target / L"Logo 2.png") == Bytes("logo bytes"));

    // and the layout that arrived points at its own copy
    auto const read = glass::ReadLayoutFile(installed.Path);

    VERIFY_IS_TRUE(read.Succeeded);
    VERIFY_ARE_EQUAL(std::wstring{ L"Logo 2.png" }, read.Document.BackgroundImage);
    VERIFY_IS_TRUE(read.Document.Provenance.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"Pat Example" }, read.Document.Provenance->Author);
}

void ContentPackTests::ASignedItemIsOnlySignedWhileUnchanged()
{
    glass::UseSignedItemsFile((g_folder / L"SignedItems.json").wstring());

    auto const layout = g_folder / L"Signed.midilayout";
    auto const picture = g_folder / L"Signed.png";

    WriteAll(layout, Bytes(R"({ "name": "Signed" })"));
    WriteAll(picture, Bytes("signed picture"));

    glass::SignedItem item{};
    item.FilePath = layout.wstring();
    item.SignerName = L"Glass Test Publisher";

    glass::RememberSignedItem(item, { layout.wstring(), picture.wstring() });

    auto const found = glass::SignedItemFor(layout.wstring());

    VERIFY_IS_TRUE(found.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"Glass Test Publisher" }, found->SignerName);

    // A picture changed on this PC means it is no longer what was signed.
    WriteAll(picture, Bytes("Signed picture"));
    VERIFY_IS_FALSE(glass::SignedItemFor(layout.wstring()).has_value());

    // Put back, it is again.
    WriteAll(picture, Bytes("signed picture"));
    VERIFY_IS_TRUE(glass::SignedItemFor(layout.wstring()).has_value());

    glass::ForgetSignedItem(layout.wstring());
    VERIFY_IS_FALSE(glass::SignedItemFor(layout.wstring()).has_value());
}
