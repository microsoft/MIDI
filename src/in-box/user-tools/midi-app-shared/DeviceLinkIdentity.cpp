// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "DeviceLinkIdentity.h"

#include <bcrypt.h>
#include <ncrypt.h>

#include <format>
#include <limits>
#include <vector>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace midiapp
{
    namespace
    {
        // Seen by nobody: a paired PC knows this one by its key, not by a name in a certificate.
        constexpr wchar_t CertificateSubject[] = L"CN=Windows MIDI Services device link";

        constexpr int32_t CertificateYears = 5;

        class ProviderHandle
        {
        public:
            ProviderHandle() noexcept = default;
            ~ProviderHandle() { if (Value != 0) { ::NCryptFreeObject(Value); } }
            ProviderHandle(ProviderHandle const&) = delete;
            ProviderHandle& operator=(ProviderHandle const&) = delete;

            NCRYPT_PROV_HANDLE Value{ 0 };
        };

        class KeyHandle
        {
        public:
            KeyHandle() noexcept = default;
            ~KeyHandle() { if (Value != 0) { ::NCryptFreeObject(Value); } }
            KeyHandle(KeyHandle const&) = delete;
            KeyHandle& operator=(KeyHandle const&) = delete;

            NCRYPT_KEY_HANDLE Value{ 0 };
        };

        HRESULT OpenOrCreateKey(
            _In_ NCRYPT_PROV_HANDLE provider,
            _In_ std::wstring const& keyName,
            _Out_ NCRYPT_KEY_HANDLE& key) noexcept
        {
            key = 0;

            auto opened = ::NCryptOpenKey(provider, &key, keyName.c_str(), 0, 0);

            if (opened == ERROR_SUCCESS)
            {
                return S_OK;
            }

            if (opened != NTE_BAD_KEYSET)
            {
                return opened;
            }

            // The signed-in user's own key: no NCRYPT_MACHINE_KEY_FLAG.
            auto created = ::NCryptCreatePersistedKey(provider, &key, BCRYPT_ECDSA_P256_ALGORITHM, keyName.c_str(), 0, 0);

            if (created == NTE_EXISTS)
            {
                // Made by another copy of the app a moment ago.
                return ::NCryptOpenKey(provider, &key, keyName.c_str(), 0, 0);
            }

            if (FAILED(created))
            {
                return created;
            }

            // Never exportable, in plain text or otherwise.
            DWORD exportPolicy{ 0 };
            auto result = ::NCryptSetProperty(key, NCRYPT_EXPORT_POLICY_PROPERTY,
                reinterpret_cast<PBYTE>(&exportPolicy), sizeof(exportPolicy), NCRYPT_PERSIST_FLAG);

            DWORD usage{ NCRYPT_ALLOW_SIGNING_FLAG };

            if (SUCCEEDED(result))
            {
                result = ::NCryptSetProperty(key, NCRYPT_KEY_USAGE_PROPERTY,
                    reinterpret_cast<PBYTE>(&usage), sizeof(usage), NCRYPT_PERSIST_FLAG);
            }

            if (SUCCEEDED(result))
            {
                result = ::NCryptFinalizeKey(key, 0);
            }

            if (FAILED(result))
            {
                ::NCryptDeleteKey(key, 0);
                key = 0;
                return result;
            }

            return S_OK;
        }

        std::vector<uint8_t> EncodedSubject()
        {
            DWORD size{ 0 };

            if (!::CertStrToNameW(X509_ASN_ENCODING, CertificateSubject, CERT_X500_NAME_STR, nullptr, nullptr, &size, nullptr))
            {
                return {};
            }

            std::vector<uint8_t> encoded(size);

            if (!::CertStrToNameW(X509_ASN_ENCODING, CertificateSubject, CERT_X500_NAME_STR, nullptr, encoded.data(), &size, nullptr))
            {
                return {};
            }

            encoded.resize(size);

            return encoded;
        }

        std::vector<uint8_t> EncodedUsages()
        {
            LPSTR usages[] = { const_cast<LPSTR>(szOID_PKIX_KP_SERVER_AUTH), const_cast<LPSTR>(szOID_PKIX_KP_CLIENT_AUTH) };

            CERT_ENHKEY_USAGE usage{};
            usage.cUsageIdentifier = static_cast<DWORD>(std::size(usages));
            usage.rgpszUsageIdentifier = usages;

            DWORD size{ 0 };

            if (!::CryptEncodeObjectEx(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &usage, 0, nullptr, nullptr, &size))
            {
                return {};
            }

            std::vector<uint8_t> encoded(size);

            if (!::CryptEncodeObjectEx(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &usage, 0, nullptr, encoded.data(), &size))
            {
                return {};
            }

            encoded.resize(size);

            return encoded;
        }

        SYSTEMTIME YearsFrom(_In_ SYSTEMTIME const& start, _In_ int32_t years) noexcept
        {
            auto end = start;
            end.wYear = static_cast<WORD>(end.wYear + years);

            // February 29 doesn't come every year.
            if (end.wMonth == 2 && end.wDay == 29)
            {
                end.wDay = 28;
            }

            return end;
        }

        // A day back, so a PC whose clock is a little behind still takes it.
        SYSTEMTIME YesterdayUtc() noexcept
        {
            FILETIME now{};
            ::GetSystemTimeAsFileTime(&now);

            ULARGE_INTEGER value{};
            value.LowPart = now.dwLowDateTime;
            value.HighPart = now.dwHighDateTime;
            value.QuadPart -= 24ull * 60 * 60 * 10'000'000;

            FILETIME yesterday{};
            yesterday.dwLowDateTime = value.LowPart;
            yesterday.dwHighDateTime = value.HighPart;

            SYSTEMTIME result{};
            ::FileTimeToSystemTime(&yesterday, &result);

            return result;
        }
    }

    DeviceIdentity::~DeviceIdentity()
    {
        if (m_certificate != nullptr)
        {
            ::CertFreeCertificateContext(m_certificate);
        }
    }

    _Use_decl_annotations_
    std::unique_ptr<DeviceIdentity> DeviceIdentity::OpenOrCreate(std::wstring const& keyName, HRESULT& error) noexcept
    {
        error = E_FAIL;

        try
        {
            if (keyName.empty())
            {
                error = E_INVALIDARG;
                return nullptr;
            }

            ProviderHandle provider{};

            if (error = ::NCryptOpenStorageProvider(&provider.Value, MS_KEY_STORAGE_PROVIDER, 0); FAILED(error))
            {
                return nullptr;
            }

            KeyHandle key{};

            if (error = OpenOrCreateKey(provider.Value, keyName, key.Value); FAILED(error))
            {
                return nullptr;
            }

            auto subject = EncodedSubject();
            auto usages = EncodedUsages();

            if (subject.empty() || usages.empty())
            {
                error = HRESULT_FROM_WIN32(::GetLastError());
                return nullptr;
            }

            CERT_NAME_BLOB subjectBlob{};
            subjectBlob.cbData = static_cast<DWORD>(subject.size());
            subjectBlob.pbData = subject.data();

            // Lets SChannel find the key by name, in whichever process it runs the handshake.
            CRYPT_KEY_PROV_INFO keyInfo{};
            keyInfo.pwszContainerName = const_cast<LPWSTR>(keyName.c_str());
            keyInfo.pwszProvName = const_cast<LPWSTR>(MS_KEY_STORAGE_PROVIDER);
            keyInfo.dwProvType = 0;
            keyInfo.dwKeySpec = 0;

            CRYPT_ALGORITHM_IDENTIFIER algorithm{};
            algorithm.pszObjId = const_cast<LPSTR>(szOID_ECDSA_SHA256);

            CERT_EXTENSION extension{};
            extension.pszObjId = const_cast<LPSTR>(szOID_ENHANCED_KEY_USAGE);
            extension.fCritical = FALSE;
            extension.Value.cbData = static_cast<DWORD>(usages.size());
            extension.Value.pbData = usages.data();

            CERT_EXTENSIONS extensions{};
            extensions.cExtension = 1;
            extensions.rgExtension = &extension;

            auto start = YesterdayUtc();
            auto end = YearsFrom(start, CertificateYears);

            auto certificate = ::CertCreateSelfSignCertificate(
                key.Value, &subjectBlob, 0, &keyInfo, &algorithm, &start, &end, &extensions);

            if (certificate == nullptr)
            {
                error = HRESULT_FROM_WIN32(::GetLastError());
                return nullptr;
            }

            std::unique_ptr<DeviceIdentity> identity{ new DeviceIdentity{} };
            identity->m_certificate = certificate;

            auto const hash = KeyHashOfCertificate(certificate);

            if (!hash)
            {
                error = E_FAIL;
                return nullptr;
            }

            identity->m_keyHash = *hash;
            error = S_OK;

            return identity;
        }
        catch (...)
        {
            error = E_OUTOFMEMORY;
            return nullptr;
        }
    }

    _Use_decl_annotations_
    HRESULT DeviceIdentity::DeleteKey(std::wstring const& keyName) noexcept
    {
        ProviderHandle provider{};

        if (auto const opened = ::NCryptOpenStorageProvider(&provider.Value, MS_KEY_STORAGE_PROVIDER, 0); FAILED(opened))
        {
            return opened;
        }

        NCRYPT_KEY_HANDLE key{ 0 };

        auto const found = ::NCryptOpenKey(provider.Value, &key, keyName.c_str(), 0, 0);

        if (found == NTE_BAD_KEYSET)
        {
            return S_OK;
        }

        if (FAILED(found))
        {
            return found;
        }

        // Frees the handle whether it works or not.
        return ::NCryptDeleteKey(key, 0);
    }

    _Use_decl_annotations_
    std::optional<DeviceKeyHash> KeyHashOfCertificate(PCCERT_CONTEXT certificate) noexcept
    {
        try
        {
            if (certificate == nullptr || certificate->pCertInfo == nullptr)
            {
                return std::nullopt;
            }

            DWORD size{ 0 };

            if (!::CryptEncodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
                &certificate->pCertInfo->SubjectPublicKeyInfo, 0, nullptr, nullptr, &size))
            {
                return std::nullopt;
            }

            std::vector<uint8_t> encoded(size);

            if (!::CryptEncodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
                &certificate->pCertInfo->SubjectPublicKeyInfo, 0, nullptr, encoded.data(), &size))
            {
                return std::nullopt;
            }

            encoded.resize(size);

            DeviceKeyHash hash{};

            if (!DeviceLinkSha256(encoded, hash))
            {
                return std::nullopt;
            }

            return hash;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    _Use_decl_annotations_
    std::wstring DeviceKeyHashToHex(DeviceKeyHash const& hash)
    {
        std::wstring hex{};
        hex.reserve(hash.size() * 2);

        for (auto const value : hash)
        {
            hex += std::format(L"{:02x}", value);
        }

        return hex;
    }

    _Use_decl_annotations_
    std::optional<DeviceKeyHash> DeviceKeyHashFromHex(std::wstring_view hex) noexcept
    {
        DeviceKeyHash hash{};

        if (hex.size() != hash.size() * 2)
        {
            return std::nullopt;
        }

        auto const digit = [](wchar_t c) -> int
            {
                if (c >= L'0' && c <= L'9') { return c - L'0'; }
                if (c >= L'a' && c <= L'f') { return c - L'a' + 10; }
                if (c >= L'A' && c <= L'F') { return c - L'A' + 10; }
                return -1;
            };

        for (size_t i = 0; i < hash.size(); ++i)
        {
            auto const high = digit(hex[i * 2]);
            auto const low = digit(hex[i * 2 + 1]);

            if (high < 0 || low < 0)
            {
                return std::nullopt;
            }

            hash[i] = static_cast<uint8_t>((high << 4) | low);
        }

        return hash;
    }

    _Use_decl_annotations_
    std::wstring ShortDeviceFingerprint(DeviceKeyHash const& hash)
    {
        std::wstring text{};

        for (size_t i = 0; i < 8; i += 2)
        {
            if (!text.empty())
            {
                text += L'-';
            }

            text += std::format(L"{:02X}{:02X}", hash[i], hash[i + 1]);
        }

        return text;
    }

    _Use_decl_annotations_
    bool SameDeviceKey(DeviceKeyHash const& first, DeviceKeyHash const& second) noexcept
    {
        uint8_t difference{ 0 };

        for (size_t i = 0; i < first.size(); ++i)
        {
            difference |= static_cast<uint8_t>(first[i] ^ second[i]);
        }

        return difference == 0;
    }

    _Use_decl_annotations_
    bool DeviceLinkSha256(std::span<uint8_t const> data, std::array<uint8_t, 32>& hash) noexcept
    {
        hash = {};

        if (data.size() > std::numeric_limits<ULONG>::max())
        {
            return false;
        }

        return BCRYPT_SUCCESS(::BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
            const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()),
            hash.data(), static_cast<ULONG>(hash.size())));
    }

    _Use_decl_annotations_
    bool DeviceLinkRandom(uint8_t* destination, size_t size) noexcept
    {
        if (size > std::numeric_limits<ULONG>::max())
        {
            return false;
        }

        return BCRYPT_SUCCESS(::BCryptGenRandom(nullptr, destination, static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG));
    }
}
