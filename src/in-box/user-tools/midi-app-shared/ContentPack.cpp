// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ContentPack.h"

#include <windows.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <wincrypt.h>

// windows.h defines this as GetObjectW, which turns IJsonValue::GetObject() into a compile error.
#undef GetObject

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>

namespace midiapp
{
    namespace
    {
        namespace mjson = winrt::Windows::Data::Json;

        constexpr DWORD MessageEncoding = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;

        constexpr wchar_t KeyFormat[] = L"format";
        constexpr wchar_t KeyFormatVersion[] = L"formatVersion";
        constexpr wchar_t KeyKind[] = L"kind";
        constexpr wchar_t KeyPrimary[] = L"primary";
        constexpr wchar_t KeyFiles[] = L"files";
        constexpr wchar_t KeyPath[] = L"path";
        constexpr wchar_t KeySize[] = L"size";
        constexpr wchar_t KeySha256[] = L"sha256";

        constexpr wchar_t KindGlassLayout[] = L"midiGlassLayout";
        constexpr wchar_t KindGlassTheme[] = L"midiGlassTheme";
        constexpr wchar_t KindPatchbayPatch[] = L"midiPatchbayPatch";

        constexpr size_t MaximumPackPathLength = 200;
        constexpr size_t MaximumSubjectLength = 1024;

        // RFC 5816 names the timestamp token attribute this; signtool writes the Microsoft OID instead.
        constexpr char TimestampTokenAttribute[] = "1.2.840.113549.1.9.16.2.14";

        constexpr DWORD TimestampRequestTimeoutMilliseconds = 30000;

        class MessageHandle
        {
        public:
            explicit MessageHandle(_In_opt_ HCRYPTMSG value) noexcept : m_value(value) {}
            ~MessageHandle() { if (m_value != nullptr) { ::CryptMsgClose(m_value); } }
            MessageHandle(MessageHandle const&) = delete;
            MessageHandle& operator=(MessageHandle const&) = delete;
            HCRYPTMSG Get() const noexcept { return m_value; }
        private:
            HCRYPTMSG m_value{ nullptr };
        };

        class StoreHandle
        {
        public:
            explicit StoreHandle(_In_opt_ HCERTSTORE value) noexcept : m_value(value) {}
            ~StoreHandle() { if (m_value != nullptr) { ::CertCloseStore(m_value, 0); } }
            StoreHandle(StoreHandle const&) = delete;
            StoreHandle& operator=(StoreHandle const&) = delete;
            HCERTSTORE Get() const noexcept { return m_value; }
        private:
            HCERTSTORE m_value{ nullptr };
        };

        class CertificateHandle
        {
        public:
            explicit CertificateHandle(_In_opt_ PCCERT_CONTEXT value) noexcept : m_value(value) {}
            ~CertificateHandle() { if (m_value != nullptr) { ::CertFreeCertificateContext(m_value); } }
            CertificateHandle(CertificateHandle const&) = delete;
            CertificateHandle& operator=(CertificateHandle const&) = delete;
            PCCERT_CONTEXT Get() const noexcept { return m_value; }
        private:
            PCCERT_CONTEXT m_value{ nullptr };
        };

        class ChainHandle
        {
        public:
            ChainHandle() noexcept = default;
            ~ChainHandle() { if (m_value != nullptr) { ::CertFreeCertificateChain(m_value); } }
            ChainHandle(ChainHandle const&) = delete;
            ChainHandle& operator=(ChainHandle const&) = delete;
            PCCERT_CHAIN_CONTEXT Get() const noexcept { return m_value; }
            PCCERT_CHAIN_CONTEXT* Put() noexcept { return &m_value; }
        private:
            PCCERT_CHAIN_CONTEXT m_value{ nullptr };
        };

        class ChainEngineHandle
        {
        public:
            ChainEngineHandle() noexcept = default;
            ~ChainEngineHandle() { if (m_value != nullptr) { ::CertFreeCertificateChainEngine(m_value); } }
            ChainEngineHandle(ChainEngineHandle const&) = delete;
            ChainEngineHandle& operator=(ChainEngineHandle const&) = delete;
            HCERTCHAINENGINE Get() const noexcept { return m_value; }
            HCERTCHAINENGINE* Put() noexcept { return &m_value; }
        private:
            HCERTCHAINENGINE m_value{ nullptr };
        };

        class TimestampHandle
        {
        public:
            TimestampHandle() noexcept = default;
            ~TimestampHandle() { if (m_value != nullptr) { ::CryptMemFree(m_value); } }
            TimestampHandle(TimestampHandle const&) = delete;
            TimestampHandle& operator=(TimestampHandle const&) = delete;
            PCRYPT_TIMESTAMP_CONTEXT Get() const noexcept { return m_value; }
            PCRYPT_TIMESTAMP_CONTEXT* Put() noexcept { return &m_value; }
        private:
            PCRYPT_TIMESTAMP_CONTEXT m_value{ nullptr };
        };

        class PrivateKeyHandle
        {
        public:
            PrivateKeyHandle() noexcept = default;
            ~PrivateKeyHandle()
            {
                if (m_value != 0 && m_callerFrees)
                {
                    if (m_keySpec == CERT_NCRYPT_KEY_SPEC)
                    {
                        ::NCryptFreeObject(m_value);
                    }
                    else
                    {
                        ::CryptReleaseContext(m_value, 0);
                    }
                }
            }
            PrivateKeyHandle(PrivateKeyHandle const&) = delete;
            PrivateKeyHandle& operator=(PrivateKeyHandle const&) = delete;

            bool Acquire(_In_ PCCERT_CONTEXT certificate) noexcept
            {
                BOOL callerFrees{ FALSE };

                // Cached on the certificate context, which also finds a key that was attached to
                // the context directly rather than through a key container.
                if (!::CryptAcquireCertificatePrivateKey(
                    certificate,
                    CRYPT_ACQUIRE_CACHE_FLAG | CRYPT_ACQUIRE_PREFER_NCRYPT_KEY_FLAG | CRYPT_ACQUIRE_COMPARE_KEY_FLAG,
                    nullptr,
                    &m_value,
                    &m_keySpec,
                    &callerFrees))
                {
                    m_value = 0;
                    return false;
                }

                m_callerFrees = callerFrees != FALSE;
                return true;
            }

            HCRYPTPROV_OR_NCRYPT_KEY_HANDLE Get() const noexcept { return m_value; }
            DWORD KeySpec() const noexcept { return m_keySpec; }

        private:
            HCRYPTPROV_OR_NCRYPT_KEY_HANDLE m_value{ 0 };
            DWORD m_keySpec{ 0 };
            bool m_callerFrees{ false };
        };

        HRESULT LastError() noexcept
        {
            auto const error = ::GetLastError();
            return error == ERROR_SUCCESS ? E_FAIL : HRESULT_FROM_WIN32(error);
        }

        int64_t FileTimeValue(_In_ FILETIME const& time) noexcept
        {
            return static_cast<int64_t>((static_cast<uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime);
        }

        std::wstring Hex(_In_reads_bytes_(size) uint8_t const* data, _In_ size_t size)
        {
            static constexpr wchar_t digits[] = L"0123456789abcdef";

            std::wstring text{};
            text.reserve(size * 2);

            for (size_t i = 0; i < size; ++i)
            {
                text += digits[data[i] >> 4];
                text += digits[data[i] & 0x0F];
            }

            return text;
        }

        std::vector<uint8_t> ToUtf8Bytes(_In_ std::wstring_view text)
        {
            if (text.empty())
            {
                return {};
            }

            auto const needed = ::WideCharToMultiByte(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

            if (needed <= 0)
            {
                return {};
            }

            std::vector<uint8_t> bytes(static_cast<size_t>(needed));

            ::WideCharToMultiByte(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                reinterpret_cast<char*>(bytes.data()), needed, nullptr, nullptr);

            return bytes;
        }

        bool FromUtf8Bytes(_In_ std::vector<uint8_t> const& bytes, _Out_ std::wstring& text)
        {
            text.clear();

            size_t start{ 0 };

            if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
            {
                start = 3;
            }

            if (bytes.size() == start)
            {
                return false;
            }

            auto const data = reinterpret_cast<char const*>(bytes.data() + start);
            auto const length = static_cast<int>(bytes.size() - start);

            auto const needed = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, length, nullptr, 0);

            if (needed <= 0)
            {
                return false;
            }

            text.resize(static_cast<size_t>(needed));

            return ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, length, text.data(), needed) == needed;
        }

        bool EqualsIgnoreCase(_In_ std::wstring_view left, _In_ std::wstring_view right) noexcept
        {
            return ::CompareStringOrdinal(
                left.empty() ? L"" : left.data(), static_cast<int>(left.size()),
                right.empty() ? L"" : right.data(), static_cast<int>(right.size()),
                TRUE) == CSTR_EQUAL;
        }

        std::wstring_view FolderOf(_In_ std::wstring_view path) noexcept
        {
            auto const slash = path.find(L'/');
            return slash == std::wstring_view::npos ? std::wstring_view{} : path.substr(0, slash);
        }

        bool IsManifestEntry(_In_ std::wstring_view path) noexcept
        {
            return path == ContentPackManifestName;
        }

        bool IsSignatureEntry(_In_ std::wstring_view path) noexcept
        {
            return EqualsIgnoreCase(FolderOf(path), ContentPackSignaturesFolder);
        }

        bool IsReservedEntry(_In_ std::wstring_view path) noexcept
        {
            return EqualsIgnoreCase(FolderOf(path), ContentPackReservedFolder);
        }

        // Paths a manifest may list: not the manifest itself, and not where signatures or C2PA data live.
        bool IsListablePath(_In_ std::wstring_view path) noexcept
        {
            return IsAllowedContentPackPath(path) &&
                !EqualsIgnoreCase(path, ContentPackManifestName) &&
                !IsSignatureEntry(path) &&
                !IsReservedEntry(path);
        }

        bool IsAllowedSegment(_In_ std::wstring_view segment) noexcept
        {
            if (segment.empty() || segment == L"." || segment == L"..")
            {
                return false;
            }

            // Windows drops a trailing dot or space, so two different names could open one file.
            if (segment.front() == L' ' || segment.back() == L' ' || segment.back() == L'.')
            {
                return false;
            }

            for (auto const c : segment)
            {
                if (c < 0x20 || c == 0x7F)
                {
                    return false;
                }

                switch (c)
                {
                case L'\\':
                case L':':
                case L'*':
                case L'?':
                case L'"':
                case L'<':
                case L'>':
                case L'|':
                    return false;
                default:
                    break;
                }
            }

            if (SanitizeProvenanceText(segment, segment.size()) != segment)
            {
                return false;
            }

            // Device names stay devices whatever the extension.
            auto stem = segment.substr(0, segment.find(L'.'));

            while (!stem.empty() && stem.back() == L' ')
            {
                stem.remove_suffix(1);
            }

            static constexpr std::wstring_view devices[]{ L"CON", L"PRN", L"AUX", L"NUL", L"CONIN$", L"CONOUT$" };

            for (auto const device : devices)
            {
                if (EqualsIgnoreCase(stem, device))
                {
                    return false;
                }
            }

            if (stem.size() == 4 &&
                (EqualsIgnoreCase(stem.substr(0, 3), L"COM") || EqualsIgnoreCase(stem.substr(0, 3), L"LPT")))
            {
                auto const digit = stem[3];

                if ((digit >= L'0' && digit <= L'9') || digit == 0x00B9 || digit == 0x00B2 || digit == 0x00B3)
                {
                    return false;
                }
            }

            return true;
        }

        std::wstring TextOf(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key)
        {
            auto const value = object.TryLookup(winrt::hstring{ key });

            if (value == nullptr || value.ValueType() != mjson::JsonValueType::String)
            {
                return {};
            }

            return std::wstring{ value.GetString() };
        }

        bool ReadWholeNumber(
            _In_ mjson::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ double maximum,
            _Out_ uint64_t& number)
        {
            number = 0;

            auto const value = object.TryLookup(winrt::hstring{ key });

            if (value == nullptr || value.ValueType() != mjson::JsonValueType::Number)
            {
                return false;
            }

            auto const raw = value.GetNumber();

            if (!std::isfinite(raw) || raw < 0 || raw > maximum || raw != std::floor(raw))
            {
                return false;
            }

            number = static_cast<uint64_t>(raw);
            return true;
        }

        bool IsSha256Hex(_In_ std::wstring_view text) noexcept
        {
            return text.size() == 64 && std::all_of(text.begin(), text.end(),
                [](wchar_t c) { return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'); });
        }

        std::wstring LowerAscii(_In_ std::wstring_view text)
        {
            std::wstring lower{ text };

            for (auto& c : lower)
            {
                if (c >= L'A' && c <= L'Z')
                {
                    c = static_cast<wchar_t>(c - L'A' + L'a');
                }
            }

            return lower;
        }

        std::vector<uint8_t> MessageParam(_In_ HCRYPTMSG message, _In_ DWORD type, _In_ DWORD index)
        {
            DWORD size{ 0 };

            if (!::CryptMsgGetParam(message, type, index, nullptr, &size) || size == 0)
            {
                return {};
            }

            std::vector<uint8_t> buffer(size);

            if (!::CryptMsgGetParam(message, type, index, buffer.data(), &size))
            {
                return {};
            }

            buffer.resize(size);
            return buffer;
        }

        std::wstring CertificateName(_In_ PCCERT_CONTEXT certificate, _In_ DWORD flags)
        {
            wchar_t buffer[512]{};

            auto const length = ::CertGetNameStringW(
                certificate, CERT_NAME_SIMPLE_DISPLAY_TYPE, flags, nullptr, buffer, ARRAYSIZE(buffer));

            if (length <= 1)
            {
                return {};
            }

            return SanitizeProvenanceText(buffer, MaximumProvenanceNameLength);
        }

        std::wstring CertificateSubject(_In_ PCCERT_CONTEXT certificate)
        {
            auto const needed = ::CertNameToStrW(
                X509_ASN_ENCODING, &certificate->pCertInfo->Subject, CERT_X500_NAME_STR, nullptr, 0);

            if (needed <= 1)
            {
                return {};
            }

            std::wstring subject(needed, L'\0');

            ::CertNameToStrW(
                X509_ASN_ENCODING, &certificate->pCertInfo->Subject, CERT_X500_NAME_STR, subject.data(), needed);

            subject.resize(needed - 1);

            return SanitizeProvenanceText(subject, MaximumSubjectLength);
        }

        enum class ChainVerdict
        {
            Trusted,
            TrustedRevocationUnknown,
            Untrusted,
            Revoked,
        };

        ChainVerdict EvaluateChain(
            _In_opt_ HCERTCHAINENGINE engine,
            _In_ PCCERT_CONTEXT certificate,
            _In_opt_ FILETIME* at,
            _In_opt_ HCERTSTORE additionalStore,
            _In_ LPCSTR usage,
            _In_ bool checkRevocation,
            _Out_ HRESULT& error) noexcept
        {
            error = S_OK;

            LPSTR usages[]{ const_cast<LPSTR>(usage) };

            CERT_CHAIN_PARA chainPara{};
            chainPara.cbSize = sizeof(chainPara);
            chainPara.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
            chainPara.RequestedUsage.Usage.cUsageIdentifier = 1;
            chainPara.RequestedUsage.Usage.rgpszUsageIdentifier = usages;

            // With no timeout of our own, the accumulative flag caps every revocation download at 20 seconds in total.
            DWORD const flags = checkRevocation
                ? (CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT | CERT_CHAIN_REVOCATION_ACCUMULATIVE_TIMEOUT)
                : 0;

            ChainHandle chain{};

            if (!::CertGetCertificateChain(engine, certificate, at, additionalStore, &chainPara, flags, nullptr, chain.Put()) ||
                chain.Get() == nullptr)
            {
                error = LastError();
                return ChainVerdict::Untrusted;
            }

            CERT_CHAIN_POLICY_PARA policyPara{};
            policyPara.cbSize = sizeof(policyPara);
            policyPara.dwFlags = CERT_CHAIN_POLICY_IGNORE_ALL_REV_UNKNOWN_FLAGS;

            CERT_CHAIN_POLICY_STATUS policyStatus{};
            policyStatus.cbSize = sizeof(policyStatus);

            if (!::CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_BASE, chain.Get(), &policyPara, &policyStatus))
            {
                error = LastError();
                return ChainVerdict::Untrusted;
            }

            if (policyStatus.dwError != ERROR_SUCCESS)
            {
                error = static_cast<HRESULT>(policyStatus.dwError);

                return policyStatus.dwError == static_cast<DWORD>(CRYPT_E_REVOKED)
                    ? ChainVerdict::Revoked
                    : ChainVerdict::Untrusted;
            }

            auto const status = chain.Get()->TrustStatus.dwErrorStatus;

            if (!checkRevocation ||
                (status & (CERT_TRUST_REVOCATION_STATUS_UNKNOWN | CERT_TRUST_IS_OFFLINE_REVOCATION)) != 0)
            {
                return ChainVerdict::TrustedRevocationUnknown;
            }

            return ChainVerdict::Trusted;
        }

        // The time from an RFC 3161 token over this signature, if Windows trusts the timestamping authority.
        int64_t VerifyTimestamp(
            _In_ CRYPT_ATTR_BLOB const& token,
            _In_ CRYPT_DATA_BLOB const& signatureValue,
            _In_opt_ HCERTCHAINENGINE engine,
            _In_ bool checkRevocation) noexcept
        {
            TimestampHandle context{};
            PCCERT_CONTEXT signerValue{ nullptr };
            HCERTSTORE storeValue{ nullptr };

            auto const verified = ::CryptVerifyTimeStampSignature(
                token.pbData, token.cbData,
                signatureValue.pbData, signatureValue.cbData,
                nullptr,
                context.Put(),
                &signerValue,
                &storeValue);

            CertificateHandle const signer{ signerValue };
            StoreHandle const store{ storeValue };

            if (!verified || context.Get() == nullptr || context.Get()->pTimeStamp == nullptr || signer.Get() == nullptr)
            {
                return 0;
            }

            FILETIME at = context.Get()->pTimeStamp->ftTime;
            HRESULT error{ S_OK };

            auto const verdict = EvaluateChain(
                engine, signer.Get(), &at, store.Get(), szOID_PKIX_KP_TIMESTAMP_SIGNING, checkRevocation, error);

            if (verdict != ChainVerdict::Trusted && verdict != ChainVerdict::TrustedRevocationUnknown)
            {
                return 0;
            }

            return FileTimeValue(at);
        }

        bool IsStrongHash(_In_opt_ LPCSTR oid) noexcept
        {
            return oid != nullptr &&
                (std::strcmp(oid, szOID_NIST_sha256) == 0 ||
                    std::strcmp(oid, szOID_NIST_sha384) == 0 ||
                    std::strcmp(oid, szOID_NIST_sha512) == 0);
        }

        PackSignature VerifyManifestSignature(
            _In_ std::vector<uint8_t> const& manifest,
            _In_ std::vector<uint8_t> const& signatureBytes,
            _In_opt_ HCERTCHAINENGINE engine,
            _In_ bool checkRevocation)
        {
            PackSignature result{};
            result.Status = PackSignatureStatus::Broken;

            if (signatureBytes.empty() || signatureBytes.size() > MaximumContentPackSignatureBytes ||
                manifest.empty() || manifest.size() > MAXDWORD)
            {
                result.ChainError = CRYPT_E_BAD_MSG;
                return result;
            }

            CRYPT_VERIFY_MESSAGE_PARA verifyPara{};
            verifyPara.cbSize = sizeof(verifyPara);
            verifyPara.dwMsgAndCertEncodingType = MessageEncoding;

            BYTE const* content[]{ manifest.data() };
            DWORD contentSizes[]{ static_cast<DWORD>(manifest.size()) };

            PCCERT_CONTEXT signerValue{ nullptr };

            auto const verified = ::CryptVerifyDetachedMessageSignature(
                &verifyPara,
                0,
                signatureBytes.data(),
                static_cast<DWORD>(signatureBytes.size()),
                1,
                content,
                contentSizes,
                &signerValue);

            auto const verifyError = LastError();

            CertificateHandle const signer{ signerValue };

            if (!verified || signer.Get() == nullptr)
            {
                result.ChainError = verifyError;
                return result;
            }

            result.SignerName = CertificateName(signer.Get(), 0);
            result.IssuerName = CertificateName(signer.Get(), CERT_NAME_ISSUER_FLAG);
            result.SignerSubject = CertificateSubject(signer.Get());
            result.Thumbprint = Sha256Hex(signer.Get()->pbCertEncoded, signer.Get()->cbCertEncoded);
            result.ValidFrom = FileTimeValue(signer.Get()->pCertInfo->NotBefore);
            result.ValidTo = FileTimeValue(signer.Get()->pCertInfo->NotAfter);

            MessageHandle const message{ ::CryptMsgOpenToDecode(MessageEncoding, CMSG_DETACHED_FLAG, 0, 0, nullptr, nullptr) };

            if (message.Get() == nullptr ||
                !::CryptMsgUpdate(message.Get(), signatureBytes.data(), static_cast<DWORD>(signatureBytes.size()), TRUE))
            {
                result.ChainError = LastError();
                return result;
            }

            auto const signerInfoBuffer = MessageParam(message.Get(), CMSG_SIGNER_INFO_PARAM, 0);

            if (signerInfoBuffer.size() < sizeof(CMSG_SIGNER_INFO))
            {
                result.ChainError = CRYPT_E_BAD_MSG;
                return result;
            }

            auto const signerInfo = reinterpret_cast<CMSG_SIGNER_INFO const*>(signerInfoBuffer.data());

            // A SHA-1 signature can be forged, so it proves nothing about who made the pack.
            if (!IsStrongHash(signerInfo->HashAlgorithm.pszObjId))
            {
                result.Status = PackSignatureStatus::Untrusted;
                result.ChainError = NTE_BAD_ALGID;
                return result;
            }

            for (DWORD i = 0; i < signerInfo->UnauthAttrs.cAttr; ++i)
            {
                auto const& attribute = signerInfo->UnauthAttrs.rgAttr[i];

                if (attribute.pszObjId == nullptr || attribute.cValue == 0 ||
                    (std::strcmp(attribute.pszObjId, szOID_RFC3161_counterSign) != 0 &&
                        std::strcmp(attribute.pszObjId, TimestampTokenAttribute) != 0))
                {
                    continue;
                }

                result.SignedAt = VerifyTimestamp(attribute.rgValue[0], signerInfo->EncryptedHash, engine, checkRevocation);

                if (result.SignedAt != 0)
                {
                    break;
                }
            }

            StoreHandle const messageStore{ ::CertOpenStore(CERT_STORE_PROV_MSG, MessageEncoding, 0, 0, message.Get()) };

            FILETIME signedAt{};
            signedAt.dwLowDateTime = static_cast<DWORD>(static_cast<uint64_t>(result.SignedAt) & 0xFFFFFFFFu);
            signedAt.dwHighDateTime = static_cast<DWORD>(static_cast<uint64_t>(result.SignedAt) >> 32);

            // A trusted timestamp lets a signature outlive its certificate's expiry date.
            HRESULT chainError{ S_OK };

            auto const verdict = EvaluateChain(
                engine,
                signer.Get(),
                result.SignedAt != 0 ? &signedAt : nullptr,
                messageStore.Get(),
                szOID_PKIX_KP_CODE_SIGNING,
                checkRevocation,
                chainError);

            result.ChainError = chainError;

            switch (verdict)
            {
            case ChainVerdict::Trusted:
                result.Status = PackSignatureStatus::Trusted;
                break;
            case ChainVerdict::TrustedRevocationUnknown:
                result.Status = PackSignatureStatus::TrustedRevocationUnknown;
                break;
            case ChainVerdict::Revoked:
                result.Status = PackSignatureStatus::Revoked;
                break;
            default:
                result.Status = PackSignatureStatus::Untrusted;
                break;
            }

            return result;
        }

        std::vector<uint8_t> AddTimestamp(
            _In_ std::vector<uint8_t> const& signature,
            _In_ std::wstring const& timestampUrl,
            _Out_ HRESULT& error)
        {
            error = E_FAIL;

            MessageHandle const message{ ::CryptMsgOpenToDecode(MessageEncoding, CMSG_DETACHED_FLAG, 0, 0, nullptr, nullptr) };

            if (message.Get() == nullptr ||
                !::CryptMsgUpdate(message.Get(), signature.data(), static_cast<DWORD>(signature.size()), TRUE))
            {
                error = LastError();
                return {};
            }

            auto const signerInfoBuffer = MessageParam(message.Get(), CMSG_SIGNER_INFO_PARAM, 0);

            if (signerInfoBuffer.size() < sizeof(CMSG_SIGNER_INFO))
            {
                error = CRYPT_E_BAD_MSG;
                return {};
            }

            auto const signerInfo = reinterpret_cast<CMSG_SIGNER_INFO const*>(signerInfoBuffer.data());

            uint8_t nonce[8]{};

            if (!BCRYPT_SUCCESS(::BCryptGenRandom(nullptr, nonce, sizeof(nonce), BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
            {
                error = NTE_FAIL;
                return {};
            }

            CRYPT_TIMESTAMP_PARA request{};
            request.fRequestCerts = TRUE;
            request.Nonce.cbData = sizeof(nonce);
            request.Nonce.pbData = nonce;

            TimestampHandle context{};

            if (!::CryptRetrieveTimeStamp(
                timestampUrl.c_str(),
                TIMESTAMP_VERIFY_CONTEXT_SIGNATURE,
                TimestampRequestTimeoutMilliseconds,
                szOID_NIST_sha256,
                &request,
                signerInfo->EncryptedHash.pbData,
                signerInfo->EncryptedHash.cbData,
                context.Put(),
                nullptr,
                nullptr) ||
                context.Get() == nullptr)
            {
                error = LastError();
                return {};
            }

            CRYPT_ATTR_BLOB token{};
            token.cbData = context.Get()->cbEncoded;
            token.pbData = context.Get()->pbEncoded;

            CRYPT_ATTRIBUTE attribute{};
            attribute.pszObjId = const_cast<LPSTR>(szOID_RFC3161_counterSign);
            attribute.cValue = 1;
            attribute.rgValue = &token;

            DWORD encodedSize{ 0 };

            if (!::CryptEncodeObject(MessageEncoding, PKCS_ATTRIBUTE, &attribute, nullptr, &encodedSize) || encodedSize == 0)
            {
                error = LastError();
                return {};
            }

            std::vector<uint8_t> encoded(encodedSize);

            if (!::CryptEncodeObject(MessageEncoding, PKCS_ATTRIBUTE, &attribute, encoded.data(), &encodedSize))
            {
                error = LastError();
                return {};
            }

            CMSG_CTRL_ADD_SIGNER_UNAUTH_ATTR_PARA add{};
            add.cbSize = sizeof(add);
            add.dwSignerIndex = 0;
            add.blob.cbData = encodedSize;
            add.blob.pbData = encoded.data();

            if (!::CryptMsgControl(message.Get(), 0, CMSG_CTRL_ADD_SIGNER_UNAUTH_ATTR, &add))
            {
                error = LastError();
                return {};
            }

            auto stamped = MessageParam(message.Get(), CMSG_ENCODED_MESSAGE, 0);

            error = stamped.empty() ? LastError() : S_OK;
            return stamped;
        }

        bool HasCodeSigningUsage(_In_ PCCERT_CONTEXT certificate)
        {
            DWORD size{ 0 };

            if (!::CertGetEnhancedKeyUsage(certificate, CERT_FIND_EXT_ONLY_ENHKEY_USAGE_FLAG, nullptr, &size) || size == 0)
            {
                return false;
            }

            std::vector<uint8_t> buffer(size);
            auto const usage = reinterpret_cast<CERT_ENHKEY_USAGE*>(buffer.data());

            if (!::CertGetEnhancedKeyUsage(certificate, CERT_FIND_EXT_ONLY_ENHKEY_USAGE_FLAG, usage, &size))
            {
                return false;
            }

            for (DWORD i = 0; i < usage->cUsageIdentifier; ++i)
            {
                if (usage->rgpszUsageIdentifier[i] != nullptr &&
                    std::strcmp(usage->rgpszUsageIdentifier[i], szOID_PKIX_KP_CODE_SIGNING) == 0)
                {
                    return true;
                }
            }

            return false;
        }

        HCERTSTORE OpenPersonalStore() noexcept
        {
            return ::CertOpenStore(
                CERT_STORE_PROV_SYSTEM_W,
                0,
                0,
                CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_READONLY_FLAG | CERT_STORE_OPEN_EXISTING_FLAG,
                L"MY");
        }

        ContentPackProblem ProblemFor(_In_ StoredZipStatus status) noexcept
        {
            switch (status)
            {
            case StoredZipStatus::Read:
                return ContentPackProblem::None;
            case StoredZipStatus::NotAZip:
                return ContentPackProblem::NotAPack;
            case StoredZipStatus::Compressed:
                return ContentPackProblem::Compressed;
            case StoredZipStatus::TooManyEntries:
                return ContentPackProblem::TooManyEntries;
            default:
                return ContentPackProblem::Damaged;
            }
        }
    }

    _Use_decl_annotations_
    std::wstring ContentPackKindName(ContentPackKind kind) noexcept
    {
        switch (kind)
        {
        case ContentPackKind::GlassLayout:
            return KindGlassLayout;
        case ContentPackKind::GlassTheme:
            return KindGlassTheme;
        case ContentPackKind::PatchbayPatch:
            return KindPatchbayPatch;
        default:
            return {};
        }
    }

    _Use_decl_annotations_
    ContentPackKind ContentPackKindFromName(std::wstring_view name) noexcept
    {
        if (name == KindGlassLayout)
        {
            return ContentPackKind::GlassLayout;
        }

        if (name == KindGlassTheme)
        {
            return ContentPackKind::GlassTheme;
        }

        if (name == KindPatchbayPatch)
        {
            return ContentPackKind::PatchbayPatch;
        }

        return ContentPackKind::Unknown;
    }

    _Use_decl_annotations_
    std::wstring Sha256Hex(uint8_t const* data, size_t size) noexcept
    {
        try
        {
            BCRYPT_ALG_HANDLE algorithm{ nullptr };

            if (!BCRYPT_SUCCESS(::BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
            {
                return {};
            }

            BCRYPT_HASH_HANDLE hash{ nullptr };
            bool succeeded = BCRYPT_SUCCESS(::BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0));

            size_t offset{ 0 };

            while (succeeded && offset < size)
            {
                auto const chunk = static_cast<ULONG>(std::min<size_t>(size - offset, 0x40000000));
                succeeded = BCRYPT_SUCCESS(::BCryptHashData(hash, const_cast<PUCHAR>(data + offset), chunk, 0));
                offset += chunk;
            }

            uint8_t digest[32]{};
            succeeded = succeeded && BCRYPT_SUCCESS(::BCryptFinishHash(hash, digest, sizeof(digest), 0));

            if (hash != nullptr)
            {
                ::BCryptDestroyHash(hash);
            }

            ::BCryptCloseAlgorithmProvider(algorithm, 0);

            return succeeded ? Hex(digest, sizeof(digest)) : std::wstring{};
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    bool IsAllowedContentPackPath(std::wstring_view path) noexcept
    {
        try
        {
            if (path.empty() || path.size() > MaximumPackPathLength)
            {
                return false;
            }

            auto const slash = path.find(L'/');

            if (slash == std::wstring_view::npos)
            {
                return IsAllowedSegment(path);
            }

            if (path.find(L'/', slash + 1) != std::wstring_view::npos)
            {
                return false;
            }

            return IsAllowedSegment(path.substr(0, slash)) && IsAllowedSegment(path.substr(slash + 1));
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    std::wstring WriteContentPackManifest(ContentPackManifest const& manifest) noexcept
    {
        try
        {
            auto const quote = [](std::wstring_view value)
                {
                    return std::wstring{ mjson::JsonValue::CreateStringValue(winrt::hstring{ value }).Stringify() };
                };

            std::wstring text{ L"{\n" };

            text += std::format(L"  \"{}\": {},\n", KeyFormat, quote(ContentPackFormatName));
            text += std::format(L"  \"{}\": {},\n", KeyFormatVersion, manifest.FormatVersion);
            text += std::format(L"  \"{}\": {},\n", KeyKind, quote(ContentPackKindName(manifest.Kind)));
            text += std::format(L"  \"{}\": {},\n", KeyPrimary, quote(manifest.Primary));
            text += std::format(L"  \"{}\": [", KeyFiles);

            for (size_t i = 0; i < manifest.Files.size(); ++i)
            {
                auto const& file = manifest.Files[i];

                text += i == 0 ? L"\n" : L",\n";
                text += L"    {\n";
                text += std::format(L"      \"{}\": {},\n", KeyPath, quote(file.Path));
                text += std::format(L"      \"{}\": {},\n", KeySize, file.Size);
                text += std::format(L"      \"{}\": {}", KeySha256, quote(file.Sha256));

                if (file.Provenance.has_value() && !file.Provenance->IsEmpty())
                {
                    text += std::format(L",\n      \"{}\": ", ProvenanceKey);
                    text += ProvenanceToJsonText(*file.Provenance, 3);
                }

                text += L"\n    }";
            }

            text += manifest.Files.empty() ? L"]\n" : L"\n  ]\n";
            text += L"}\n";

            return text;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::optional<ContentPackManifest> ReadContentPackManifest(std::wstring_view json, ContentPackProblem& problem) noexcept
    {
        problem = ContentPackProblem::BadManifest;

        try
        {
            mjson::JsonObject root{ nullptr };

            if (!mjson::JsonObject::TryParse(winrt::hstring{ json }, root) || root == nullptr)
            {
                return std::nullopt;
            }

            if (TextOf(root, KeyFormat) != ContentPackFormatName)
            {
                return std::nullopt;
            }

            uint64_t formatVersion{ 0 };

            if (!ReadWholeNumber(root, KeyFormatVersion, 1.0e6, formatVersion) || formatVersion == 0)
            {
                return std::nullopt;
            }

            if (formatVersion > ContentPackFormatVersion)
            {
                problem = ContentPackProblem::NewerFormat;
                return std::nullopt;
            }

            ContentPackManifest manifest{};
            manifest.FormatVersion = static_cast<uint32_t>(formatVersion);
            manifest.Kind = ContentPackKindFromName(TextOf(root, KeyKind));
            manifest.Primary = TextOf(root, KeyPrimary);

            auto const files = root.TryLookup(KeyFiles);

            if (files == nullptr || files.ValueType() != mjson::JsonValueType::Array)
            {
                return std::nullopt;
            }

            for (auto const& item : files.GetArray())
            {
                if (item.ValueType() != mjson::JsonValueType::Object ||
                    manifest.Files.size() >= MaximumContentPackEntries)
                {
                    return std::nullopt;
                }

                auto const object = item.GetObject();

                ContentPackFile file{};
                file.Path = TextOf(object, KeyPath);
                file.Sha256 = LowerAscii(TextOf(object, KeySha256));

                if (file.Path.empty() || !IsSha256Hex(file.Sha256) ||
                    !ReadWholeNumber(object, KeySize, static_cast<double>(MaximumContentPackBytes), file.Size))
                {
                    return std::nullopt;
                }

                file.Provenance = ReadProvenance(object);

                manifest.Files.push_back(std::move(file));
            }

            problem = ContentPackProblem::None;
            return manifest;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    PackSignature const* OpenedContentPack::TrustedSignature() const noexcept
    {
        for (auto const& signature : Signatures)
        {
            if (signature.IsTrusted())
            {
                return &signature;
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    StoredZipEntry const* OpenedContentPack::FindFile(std::wstring_view path) const noexcept
    {
        for (auto const& file : Files)
        {
            if (file.Name == path)
            {
                return &file;
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    ContentPackFile const* OpenedContentPack::FindListing(std::wstring_view path) const noexcept
    {
        for (auto const& file : Manifest.Files)
        {
            if (file.Path == path)
            {
                return &file;
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    OpenedContentPack OpenContentPack(
        std::vector<uint8_t> const& pack,
        ContentPackKind expectedKind,
        ContentPackVerifyOptions const& options) noexcept
    {
        OpenedContentPack opened{};

        try
        {
            auto const fail = [&opened](ContentPackProblem problem, std::wstring_view file)
                {
                    opened.Problem = problem;
                    opened.ProblemFile = std::wstring{ file };
                    opened.Files.clear();
                };

            if (pack.size() > MaximumContentPackBytes)
            {
                fail(ContentPackProblem::TooBig, {});
                return opened;
            }

            std::vector<StoredZipEntry> entries{};

            auto const zipProblem = ProblemFor(ReadStoredZip(pack, MaximumContentPackEntries, entries));

            if (zipProblem != ContentPackProblem::None)
            {
                fail(zipProblem, {});
                return opened;
            }

            for (size_t i = 0; i < entries.size(); ++i)
            {
                auto const& name = entries[i].Name;

                if (!IsAllowedContentPackPath(name))
                {
                    fail(ContentPackProblem::BadName, name);
                    return opened;
                }

                for (size_t j = 0; j < i; ++j)
                {
                    if (EqualsIgnoreCase(entries[j].Name, name))
                    {
                        fail(ContentPackProblem::DuplicateName, name);
                        return opened;
                    }
                }
            }

            auto const manifestEntry = std::find_if(entries.begin(), entries.end(),
                [](StoredZipEntry const& entry) { return IsManifestEntry(entry.Name); });

            if (manifestEntry == entries.end())
            {
                fail(ContentPackProblem::NoManifest, {});
                return opened;
            }

            std::wstring manifestText{};

            if (!FromUtf8Bytes(manifestEntry->Bytes, manifestText))
            {
                fail(ContentPackProblem::BadManifest, ContentPackManifestName);
                return opened;
            }

            ContentPackProblem manifestProblem{ ContentPackProblem::BadManifest };
            auto manifest = ReadContentPackManifest(manifestText, manifestProblem);

            if (!manifest.has_value())
            {
                fail(manifestProblem, ContentPackManifestName);
                return opened;
            }

            if (expectedKind != ContentPackKind::Unknown && manifest->Kind != expectedKind)
            {
                fail(ContentPackProblem::WrongKind, {});
                return opened;
            }

            for (size_t i = 0; i < manifest->Files.size(); ++i)
            {
                auto const& path = manifest->Files[i].Path;

                if (!IsListablePath(path))
                {
                    fail(ContentPackProblem::BadManifest, path);
                    return opened;
                }

                for (size_t j = 0; j < i; ++j)
                {
                    if (EqualsIgnoreCase(manifest->Files[j].Path, path))
                    {
                        fail(ContentPackProblem::BadManifest, path);
                        return opened;
                    }
                }
            }

            auto const listingFor = [&manifest](std::wstring_view path) -> ContentPackFile const*
                {
                    for (auto const& file : manifest->Files)
                    {
                        if (file.Path == path)
                        {
                            return &file;
                        }
                    }

                    return nullptr;
                };

            for (auto const& entry : entries)
            {
                if (IsManifestEntry(entry.Name) || IsSignatureEntry(entry.Name) || IsReservedEntry(entry.Name))
                {
                    continue;
                }

                if (listingFor(entry.Name) == nullptr)
                {
                    fail(ContentPackProblem::UnlistedFile, entry.Name);
                    return opened;
                }
            }

            for (auto const& file : manifest->Files)
            {
                auto const entry = std::find_if(entries.begin(), entries.end(),
                    [&file](StoredZipEntry const& candidate) { return candidate.Name == file.Path; });

                if (entry == entries.end())
                {
                    fail(ContentPackProblem::MissingFile, file.Path);
                    return opened;
                }

                if (entry->Bytes.size() != file.Size)
                {
                    fail(ContentPackProblem::SizeMismatch, file.Path);
                    return opened;
                }

                if (Sha256Hex(entry->Bytes.data(), entry->Bytes.size()) != file.Sha256)
                {
                    fail(ContentPackProblem::HashMismatch, file.Path);
                    return opened;
                }
            }

            if (manifest->Primary.empty() ||
                manifest->Primary.find(L'/') != std::wstring::npos ||
                listingFor(manifest->Primary) == nullptr)
            {
                fail(ContentPackProblem::NoPrimary, manifest->Primary);
                return opened;
            }

            opened.ManifestBytes = manifestEntry->Bytes;

            ChainEngineHandle testEngine{};

            if (options.ExclusiveRootStore != nullptr)
            {
                CERT_CHAIN_ENGINE_CONFIG config{};
                config.cbSize = sizeof(config);
                config.hExclusiveRoot = options.ExclusiveRootStore;

                if (!::CertCreateCertificateChainEngine(&config, testEngine.Put()))
                {
                    fail(ContentPackProblem::BrokenSignature, {});
                    return opened;
                }
            }

            for (auto const& entry : entries)
            {
                if (!IsSignatureEntry(entry.Name))
                {
                    continue;
                }

                auto signature = VerifyManifestSignature(
                    opened.ManifestBytes, entry.Bytes, testEngine.Get(), options.CheckRevocation);

                signature.EntryName = entry.Name;
                opened.Signatures.push_back(std::move(signature));
            }

            for (auto const& signature : opened.Signatures)
            {
                if (signature.Status == PackSignatureStatus::Broken)
                {
                    fail(ContentPackProblem::BrokenSignature, signature.EntryName);
                    return opened;
                }

                if (signature.Status == PackSignatureStatus::Revoked)
                {
                    fail(ContentPackProblem::RevokedSignature, signature.EntryName);
                    return opened;
                }
            }

            opened.PackSha256 = Sha256Hex(pack.data(), pack.size());
            opened.Manifest = std::move(*manifest);

            for (auto const& file : opened.Manifest.Files)
            {
                auto const entry = std::find_if(entries.begin(), entries.end(),
                    [&file](StoredZipEntry const& candidate) { return candidate.Name == file.Path; });

                opened.Files.push_back(std::move(*entry));
            }

            opened.Problem = ContentPackProblem::None;
            return opened;
        }
        catch (...)
        {
            opened.Problem = ContentPackProblem::Damaged;
            opened.Files.clear();
            return opened;
        }
    }

    _Use_decl_annotations_
    std::vector<uint8_t> BuildContentPack(
        ContentPackKind kind,
        std::wstring const& primary,
        std::vector<ContentPackInput> const& files) noexcept
    {
        try
        {
            if (files.size() + 1 > MaximumContentPackEntries || primary.find(L'/') != std::wstring::npos)
            {
                return {};
            }

            ContentPackManifest manifest{};
            manifest.Kind = kind;
            manifest.Primary = primary;

            std::vector<StoredZipEntry> entries{};
            entries.reserve(files.size() + 1);
            entries.push_back(StoredZipEntry{ ContentPackManifestName, {} });

            uint64_t total{ 0 };
            bool primaryListed{ false };

            for (auto const& file : files)
            {
                if (!IsListablePath(file.Path))
                {
                    return {};
                }

                for (auto const& listed : manifest.Files)
                {
                    if (EqualsIgnoreCase(listed.Path, file.Path))
                    {
                        return {};
                    }
                }

                total += file.Bytes.size();

                if (total > MaximumContentPackBytes)
                {
                    return {};
                }

                primaryListed = primaryListed || file.Path == primary;

                ContentPackFile listing{};
                listing.Path = file.Path;
                listing.Size = file.Bytes.size();
                listing.Sha256 = Sha256Hex(file.Bytes.data(), file.Bytes.size());
                listing.Provenance = file.Provenance;

                if (listing.Sha256.empty())
                {
                    return {};
                }

                manifest.Files.push_back(std::move(listing));
                entries.push_back(StoredZipEntry{ file.Path, file.Bytes });
            }

            if (!primaryListed)
            {
                return {};
            }

            entries[0].Bytes = ToUtf8Bytes(WriteContentPackManifest(manifest));

            if (entries[0].Bytes.empty())
            {
                return {};
            }

            auto zip = BuildStoredZip(entries);

            if (zip.size() > MaximumContentPackBytes)
            {
                return {};
            }

            return zip;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::vector<uint8_t> ReadContentPackManifestBytes(std::vector<uint8_t> const& pack) noexcept
    {
        try
        {
            std::vector<StoredZipEntry> entries{};

            if (ReadStoredZip(pack, MaximumContentPackEntries, entries) != StoredZipStatus::Read)
            {
                return {};
            }

            for (auto& entry : entries)
            {
                if (IsManifestEntry(entry.Name))
                {
                    return std::move(entry.Bytes);
                }
            }

            return {};
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::vector<uint8_t> SignContentPackManifest(
        std::vector<uint8_t> const& manifestBytes,
        PCCERT_CONTEXT certificate,
        std::wstring const& timestampUrl,
        HRESULT& error) noexcept
    {
        error = E_FAIL;

        try
        {
            if (certificate == nullptr || manifestBytes.empty() || manifestBytes.size() > MAXDWORD)
            {
                error = E_INVALIDARG;
                return {};
            }

            PrivateKeyHandle key{};

            if (!key.Acquire(certificate))
            {
                error = LastError();
                return {};
            }

            // The chain goes in with the signature, so a PC that only has the root can still check it.
            ChainHandle chain{};
            std::vector<CERT_BLOB> certificates{};

            CERT_CHAIN_PARA chainPara{};
            chainPara.cbSize = sizeof(chainPara);

            if (::CertGetCertificateChain(nullptr, certificate, nullptr, nullptr, &chainPara, 0, nullptr, chain.Put()) &&
                chain.Get() != nullptr && chain.Get()->cChain > 0)
            {
                auto const simple = chain.Get()->rgpChain[0];

                for (DWORD i = 0; i < simple->cElement; ++i)
                {
                    auto const element = simple->rgpElement[i];

                    if (i > 0 && (element->TrustStatus.dwInfoStatus & CERT_TRUST_IS_SELF_SIGNED) != 0)
                    {
                        break;
                    }

                    certificates.push_back(CERT_BLOB{
                        element->pCertContext->cbCertEncoded,
                        element->pCertContext->pbCertEncoded });
                }
            }

            if (certificates.empty())
            {
                certificates.push_back(CERT_BLOB{ certificate->cbCertEncoded, certificate->pbCertEncoded });
            }

            FILETIME now{};
            ::GetSystemTimeAsFileTime(&now);

            DWORD signingTimeSize{ 0 };
            std::vector<uint8_t> signingTime{};

            if (::CryptEncodeObject(X509_ASN_ENCODING, szOID_RSA_signingTime, &now, nullptr, &signingTimeSize) &&
                signingTimeSize > 0)
            {
                signingTime.resize(signingTimeSize);

                if (!::CryptEncodeObject(X509_ASN_ENCODING, szOID_RSA_signingTime, &now, signingTime.data(), &signingTimeSize))
                {
                    signingTime.clear();
                }
            }

            CRYPT_ATTR_BLOB signingTimeValue{ static_cast<DWORD>(signingTime.size()), signingTime.data() };

            CRYPT_ATTRIBUTE signingTimeAttribute{};
            signingTimeAttribute.pszObjId = const_cast<LPSTR>(szOID_RSA_signingTime);
            signingTimeAttribute.cValue = 1;
            signingTimeAttribute.rgValue = &signingTimeValue;

            CMSG_SIGNER_ENCODE_INFO signer{};
            signer.cbSize = sizeof(signer);
            signer.pCertInfo = certificate->pCertInfo;
            signer.hCryptProv = key.Get();
            signer.dwKeySpec = key.KeySpec();
            signer.HashAlgorithm.pszObjId = const_cast<LPSTR>(szOID_NIST_sha256);

            if (!signingTime.empty())
            {
                signer.cAuthAttr = 1;
                signer.rgAuthAttr = &signingTimeAttribute;
            }

            CMSG_SIGNED_ENCODE_INFO signedInfo{};
            signedInfo.cbSize = sizeof(signedInfo);
            signedInfo.cSigners = 1;
            signedInfo.rgSigners = &signer;
            signedInfo.cCertEncoded = static_cast<DWORD>(certificates.size());
            signedInfo.rgCertEncoded = certificates.data();

            std::vector<uint8_t> signature{};

            {
                MessageHandle const message{ ::CryptMsgOpenToEncode(
                    MessageEncoding, CMSG_DETACHED_FLAG, CMSG_SIGNED, &signedInfo, nullptr, nullptr) };

                if (message.Get() == nullptr ||
                    !::CryptMsgUpdate(message.Get(), manifestBytes.data(), static_cast<DWORD>(manifestBytes.size()), TRUE))
                {
                    error = LastError();
                    return {};
                }

                signature = MessageParam(message.Get(), CMSG_CONTENT_PARAM, 0);
            }

            if (signature.empty())
            {
                error = LastError();
                return {};
            }

            if (!timestampUrl.empty())
            {
                signature = AddTimestamp(signature, timestampUrl, error);

                if (signature.empty())
                {
                    return {};
                }
            }

            error = S_OK;
            return signature;
        }
        catch (...)
        {
            error = E_OUTOFMEMORY;
            return {};
        }
    }

    _Use_decl_annotations_
    std::vector<uint8_t> AddContentPackSignature(
        std::vector<uint8_t> const& pack,
        std::wstring const& signatureFileName,
        std::vector<uint8_t> const& signature) noexcept
    {
        try
        {
            if (!IsAllowedContentPackPath(signatureFileName) || !IsSignatureEntry(signatureFileName) ||
                signature.empty() || signature.size() > MaximumContentPackSignatureBytes)
            {
                return {};
            }

            std::vector<StoredZipEntry> entries{};

            if (ReadStoredZip(pack, MaximumContentPackEntries, entries) != StoredZipStatus::Read)
            {
                return {};
            }

            entries.erase(std::remove_if(entries.begin(), entries.end(),
                [&signatureFileName](StoredZipEntry const& entry) { return EqualsIgnoreCase(entry.Name, signatureFileName); }),
                entries.end());

            if (entries.size() >= MaximumContentPackEntries)
            {
                return {};
            }

            // Before META-INF, which a C2PA manifest store expects to stay last.
            auto const reserved = std::find_if(entries.begin(), entries.end(),
                [](StoredZipEntry const& entry) { return IsReservedEntry(entry.Name); });

            entries.insert(reserved, StoredZipEntry{ signatureFileName, signature });

            return BuildStoredZip(entries);
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring ContentPackSignatureFileName(PCCERT_CONTEXT certificate) noexcept
    {
        try
        {
            if (certificate == nullptr)
            {
                return {};
            }

            auto const thumbprint = Sha256Hex(certificate->pbCertEncoded, certificate->cbCertEncoded);

            if (thumbprint.size() < 16)
            {
                return {};
            }

            return std::wstring{ ContentPackSignaturesFolder } + L"/" + thumbprint.substr(0, 16) + L".p7s";
        }
        catch (...)
        {
            return {};
        }
    }

    std::vector<SigningCertificate> ListSigningCertificates() noexcept
    {
        std::vector<SigningCertificate> certificates{};

        try
        {
            StoreHandle const store{ OpenPersonalStore() };

            if (store.Get() == nullptr)
            {
                return certificates;
            }

            PCCERT_CONTEXT certificate{ nullptr };

            while ((certificate = ::CertEnumCertificatesInStore(store.Get(), certificate)) != nullptr)
            {
                if (!HasCodeSigningUsage(certificate) ||
                    ::CertVerifyTimeValidity(nullptr, certificate->pCertInfo) != 0)
                {
                    continue;
                }

                DWORD keyInfoSize{ 0 };

                if (!::CertGetCertificateContextProperty(certificate, CERT_KEY_PROV_INFO_PROP_ID, nullptr, &keyInfoSize))
                {
                    continue;
                }

                std::vector<uint8_t> thumbprint(20);
                DWORD thumbprintSize{ static_cast<DWORD>(thumbprint.size()) };

                if (!::CertGetCertificateContextProperty(certificate, CERT_HASH_PROP_ID, thumbprint.data(), &thumbprintSize))
                {
                    continue;
                }

                thumbprint.resize(thumbprintSize);

                SigningCertificate item{};
                item.Name = CertificateName(certificate, 0);
                item.Issuer = CertificateName(certificate, CERT_NAME_ISSUER_FLAG);
                item.ValidTo = FileTimeValue(certificate->pCertInfo->NotAfter);
                item.Sha1Thumbprint = std::move(thumbprint);

                certificates.push_back(std::move(item));
            }
        }
        catch (...)
        {
        }

        return certificates;
    }

    _Use_decl_annotations_
    std::vector<uint8_t> SignContentPackWithStoreCertificate(
        std::vector<uint8_t> const& pack,
        std::vector<uint8_t> const& sha1Thumbprint,
        std::wstring const& timestampUrl,
        HRESULT& error) noexcept
    {
        error = E_FAIL;

        try
        {
            StoreHandle const store{ OpenPersonalStore() };

            if (store.Get() == nullptr)
            {
                error = LastError();
                return {};
            }

            CRYPT_HASH_BLOB hash{};
            hash.cbData = static_cast<DWORD>(sha1Thumbprint.size());
            hash.pbData = const_cast<BYTE*>(sha1Thumbprint.data());

            CertificateHandle const certificate{ ::CertFindCertificateInStore(
                store.Get(), MessageEncoding, 0, CERT_FIND_HASH, &hash, nullptr) };

            if (certificate.Get() == nullptr)
            {
                error = CRYPT_E_NOT_FOUND;
                return {};
            }

            auto const manifest = ReadContentPackManifestBytes(pack);

            if (manifest.empty())
            {
                error = CRYPT_E_BAD_MSG;
                return {};
            }

            auto const signature = SignContentPackManifest(manifest, certificate.Get(), timestampUrl, error);

            if (signature.empty())
            {
                return {};
            }

            auto signedPack = AddContentPackSignature(pack, ContentPackSignatureFileName(certificate.Get()), signature);

            error = signedPack.empty() ? E_FAIL : S_OK;
            return signedPack;
        }
        catch (...)
        {
            error = E_OUTOFMEMORY;
            return {};
        }
    }
}
