// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <bcrypt.h>

// Linked here rather than per-project so that anything including this header, including the unit
// tests, gets the digest implementation without a project file change.
#pragma comment(lib, "bcrypt.lib")

// Network MIDI 2.0 authentication (spec 6.5, 6.6, and the digests in 6.9 and 6.10).
//
// The digest is implemented and tested. Credential STORAGE is not wired up yet.
// See https://github.com/microsoft/MIDI/issues/733
//
// Design constraints which shape everything in this file:
//
//  - midisrv runs as LocalService with network capability. It is not, and will not be,
//    LocalSystem. Whatever retrieves a secret has to work within that.
//  - The service cannot prompt. There is no interactive desktop and no user to answer, so a
//    secret can never be collected at connection time. It must already be stored.
//  - The Network MIDI 2.0 Setup app, running as the user, is what collects and stores the
//    secret. The service never writes a credential, only reads one, and is only ever handed an
//    *identifier* through configuration JSON, never the secret itself.
//  - Therefore the identifier arrives from a lower-trust source and is attacker-influenced in
//    the threat model where someone can write the config. It must never be usable to name an
//    arbitrary credential on the machine. MidiNetworkCredentialIdentifier exists solely to
//    enforce that: the resolver may only ever look inside our own namespace.
//
// There are two kinds of credential here and they are not symmetric. Treating them as one thing
// is the easiest way to get this wrong.
//
//  1. For an EXTERNAL host this PC connects to. Somebody else chose the secret, so we cannot
//     generate it, and the user may well have typed a password they use elsewhere. This is
//     where the password reuse risk actually lives. Write-only: nothing should read one back
//     out, and the settings app should offer to replace rather than to reveal.
//
//  2. For a host running ON this PC. We choose the secret, so the settings app can generate it,
//     which removes the reuse risk for this kind entirely. But the user has to be able to read
//     it back in order to type it into the other device, exactly like a hotspot password, so
//     this kind does need a reveal path. "password" mode is one secret per host; "user" mode is
//     a set of user and password pairs per host, of unbounded size.
//
// Both kinds have to be recoverable, because a host verifies by recomputing the digest exactly
// as a client proves by computing it. Neither can be stored one-way.
//
// The consequence for the design is that whether a secret may be revealed is a property of the
// stored ENTRY, recorded by the service when it writes it, and never a property of the
// identifier. An identifier is attacker-influenced, so it must not be able to claim to be kind
// 2 and thereby coax a reveal of a kind 1 secret.
//
// Only the host side has configuration keys today: authentication, globalPassword and userAuth
// all hang off the host definition. A client entry has none, so kind 1 needs new keys.
//
// What the storage options actually do, measured on a non-domain-joined machine, Aug 29 2026.
// These were tested rather than reasoned about, because the answer decides the whole design:
//
//  - DPAPI-NG with a SID= protection descriptor, which would have been the right answer, does
//    not work. NCryptCreateProtectionDescriptor("SID=S-1-5-19") succeeds, but protecting then
//    fails with NTE_ENCRYPTION_FAILURE (0x80090034). Those descriptors need the domain Key
//    Distribution Service, and Get-KdsRootKey fails with "domain could not be contacted". So we
//    cannot encrypt a secret such that only LocalService can recover it.
//  - DPAPI-NG "LOCAL=machine" and classic DPAPI with CRYPTPROTECT_LOCAL_MACHINE both work, and
//    both were round-tripped by an ordinary interactive user. They provide no boundary at all.
//
// So on the machines most of our users have, there is no encryption boundary available to an
// unattended service. Anything midisrv can decrypt with no user present, any local process that
// can read the blob can also decrypt. **The ACL on the store is the security boundary**, and
// encryption at rest only protects a copy taken off the machine.
//
// The per-user Credential Locker is separately ruled out, and not because of its size limit: an
// authenticated connection has to come up with no user signed in, so there is no token to
// impersonate at boot, and a per-machine config file cannot say whose locker to read anyway.
//
// Note also that spec 6.9 and 6.10 hash the password itself, so there is no password-equivalent
// we could store in place of it. The service must hold recoverable secrets.
//
// Generating the secret removes the reuse risk, but only for kind 2, where we own it. For kind 1
// the remote device decides, so the only defences are that we never reveal it, never log it, and
// make replacing it easy. The user has to be told plainly that these are not stored securely and
// that a credential used anywhere else must never be used here.
//
// That warning is not only about our storage. The digest is a single un-iterated SHA-256 with no
// key derivation, so anyone who passively captures one invitation exchange holds the nonce and
// the digest and can attack the secret offline as fast as their hardware allows. The entropy of
// the secret is the only thing in the way, which is why a generated one has to be long:
//
//      words    from 2048    from 7776      (search half the keyspace at 10^10 hashes/sec)
//        4       15 min       2.1 days
//        5       20 days      45 years
//        6       117 years    350,000 years
//
// Four words is not enough. Six, from a list of a few thousand, is the shape to aim for.
//
// There is no enumerable word list on Windows to build that from. Checked Aug 29 2026: no .dic
// files in the system locations, and the per-user spelling dictionary holds only the words that
// user added. Offering a passphrase means shipping a curated list in the setup app.
//
// The folder the configuration lives in cannot hold the store. Measured on
// C:\ProgramData\Microsoft\MIDI: Everyone has read and Authenticated Users has write, so a
// credential there would be readable by every account on the PC. It also grants create but not
// delete, so a file one user writes another cannot remove. The store needs its own folder with
// inheritance broken, readable by LocalService and writable only by administrators, which means
// the setup app has to elevate to add or change a credential. That elevation is also what gates
// the kind 2 reveal, so it costs nothing extra.
//
// The configuration file is meant to be portable between machines, so the store deliberately
// does not travel with it. On a second PC every identifier dangles and the user re-enters the
// credentials. That has to surface as exactly that, rather than as a connection failure;
// IDS_ERROR_MISSING_CREDENTIAL_IDENTIFIER already exists to say it.
//
// Still to be settled:
//
//  - Where the store lives, given the above rules out the configuration folder. A key under
//    HKLM is worth weighing against a file: the ACL story is the same, but it is not swept up
//    by file-based backup, nor by a user copying the configuration folder to another machine.
//  - Whether the identifier is generated by the setup app or derived from the entry it belongs
//    to. Either way IsWellFormed below is what keeps it inside our own namespace.
//  - How a secret is revoked and how a session is torn down when it is.
//
// These are low-value credentials, but they are still credentials, and a bug here would be a
// way to read secrets the service was never meant to see.

enum class MidiNetworkAuthenticationKind
{
    None = 0,
    SharedSecret,       // spec 6.5, invitation with authentication
    UserCredential,     // spec 6.6, invitation with user authentication
};


// Holds secret material and scrubs it on destruction so it does not linger in freed heap.
// Deliberately non-copyable: every copy is another place a secret can be left behind.
class MidiNetworkSecret
{
public:
    MidiNetworkSecret() = default;

    MidiNetworkSecret(_In_ MidiNetworkSecret const&) = delete;
    MidiNetworkSecret& operator=(_In_ MidiNetworkSecret const&) = delete;

    MidiNetworkSecret(_Inout_ MidiNetworkSecret&& other) noexcept
    {
        m_bytes = std::move(other.m_bytes);
        other.Clear();
    }

    MidiNetworkSecret& operator=(_Inout_ MidiNetworkSecret&& other) noexcept
    {
        if (this != &other)
        {
            Clear();
            m_bytes = std::move(other.m_bytes);
            other.Clear();
        }

        return *this;
    }

    ~MidiNetworkSecret()
    {
        Clear();
    }

    bool IsEmpty() const noexcept { return m_bytes.empty(); }
    size_t Size() const noexcept { return m_bytes.size(); }
    uint8_t const* Data() const noexcept { return m_bytes.data(); }

    // Takes the secret material as bytes. Callers converting from text must hand over UTF-8,
    // because that is what the digest is computed over.
    void Assign(_In_reads_bytes_opt_(byteCount) uint8_t const* bytes, _In_ size_t const byteCount)
    {
        Clear();

        if (bytes != nullptr && byteCount > 0)
        {
            m_bytes.assign(bytes, bytes + byteCount);
        }
    }

    void Clear() noexcept
    {
        if (!m_bytes.empty())
        {
            SecureZeroMemory(m_bytes.data(), m_bytes.size());
            m_bytes.clear();
        }
    }

private:
    std::vector<uint8_t> m_bytes{ };
};


// An opaque handle to a secret, not the secret. This is the only credential-related value which
// is ever allowed to appear in configuration JSON, in a device property, or in a log.
//
// Validation is the security boundary. A resolver must refuse anything which did not come
// through IsWellFormed, because the identifier is ultimately attacker-influenced and must not
// be able to address credentials outside the namespace this transport owns.
class MidiNetworkCredentialIdentifier
{
public:
    MidiNetworkCredentialIdentifier() = default;

    explicit MidiNetworkCredentialIdentifier(_In_ std::wstring const& value) : m_value(value) { }

    std::wstring const& Value() const noexcept { return m_value; }
    bool IsEmpty() const noexcept { return m_value.empty(); }

    // Conservative on purpose. No separators, no wildcards, no relative path characters, and a
    // bounded length, so the identifier can only ever name something inside our own namespace.
    bool IsWellFormed() const noexcept
    {
        if (m_value.empty() || m_value.size() > MaxLength)
        {
            return false;
        }

        for (auto const& ch : m_value)
        {
            bool allowed =
                (ch >= L'a' && ch <= L'z') ||
                (ch >= L'A' && ch <= L'Z') ||
                (ch >= L'0' && ch <= L'9') ||
                ch == L'-' || ch == L'_';

            if (!allowed)
            {
                return false;
            }
        }

        return true;
    }

    static constexpr size_t MaxLength{ 64 };

private:
    std::wstring m_value{ };
};


// The seam between the transport and wherever secrets actually live.
//
// Everything here is deliberately unimplemented. The point of the class existing now is that
// the transport can be written against a single, auditable choke point, so that when the
// storage question is settled there is exactly one place to change and one place to review.
class MidiNetworkCredentialResolver
{
public:
    // Resolve a shared secret for spec 6.5 authentication.
    static HRESULT ResolveSharedSecret(
        _In_ MidiNetworkCredentialIdentifier const& identifier,
        _Out_ MidiNetworkSecret& secret);

    // Resolve a user name and password for spec 6.6 authentication.
    static HRESULT ResolveUserCredential(
        _In_ MidiNetworkCredentialIdentifier const& identifier,
        _Out_ std::wstring& userName,
        _Out_ MidiNetworkSecret& password);

    // True when a secret is present for this identifier. Lets configuration validation report a
    // missing secret up front rather than failing every invitation at connection time.
    static bool CredentialExists(
        _In_ MidiNetworkCredentialIdentifier const& identifier);
};


// Fills a buffer with cryptographically random bytes for use as an invitation nonce.
HRESULT MidiNetworkGenerateCryptoNonce(
    _Out_writes_bytes_(byteCount) uint8_t* buffer,
    _In_ size_t const byteCount);


// Spec 6.9 and 6.10.
//
// The digest is SHA-256 over the raw CryptoNonce bytes followed by the UTF-8 bytes of the secret
// material: straight concatenation, no separator, no length prefix, no terminator. Both worked
// examples in the specification reproduce exactly under this reading, and neither reproduces
// under UTF-16 or with a trailing null, so the hash input is 8-bit and the wide strings this
// codebase carries everywhere must be converted before hashing. That conversion happens in here
// rather than at the call sites, so there is one place to get it right.
//
// The specification's examples are pure ASCII, so they cannot distinguish UTF-8 from any other
// 8-bit encoding. A non-ASCII password is therefore not interoperable by specification, only by
// convention. UTF-8 is the convention chosen here.
constexpr size_t MidiNetworkAuthenticationDigestByteCount{ 32 };
constexpr size_t MidiNetworkCryptoNonceByteCount{ 16 };

namespace MidiNetworkAuthenticationInternal
{
    // The digest is over 8-bit text, so a wide string has to be converted. UTF-8 specifically,
    // never the ANSI code page, or the same password would produce a different digest depending
    // on the machine's locale.
    inline HRESULT ToUtf8(_In_ std::wstring const& value, _Out_ std::string& utf8)
    {
        utf8.clear();

        if (value.empty())
        {
            return S_OK;
        }

        RETURN_HR_IF(E_INVALIDARG, value.size() > INT_MAX);

        auto const byteCount = ::WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()),
            nullptr, 0, nullptr, nullptr);

        RETURN_LAST_ERROR_IF(byteCount <= 0);

        utf8.resize(static_cast<size_t>(byteCount));

        RETURN_LAST_ERROR_IF(::WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()),
            utf8.data(), byteCount, nullptr, nullptr) <= 0);

        return S_OK;
    }

    struct HashPart
    {
        uint8_t const* Bytes;
        size_t ByteCount;
    };

    // Hashes the parts in order with nothing between them. The spec concatenates into one string
    // before hashing, which is the same bytes as feeding the pieces in sequence.
    inline HRESULT ComputeSha256(
        _In_reads_(partCount) HashPart const* parts,
        _In_ size_t const partCount,
        _Out_writes_bytes_(digestByteCount) uint8_t* digest,
        _In_ size_t const digestByteCount)
    {
        RETURN_HR_IF(E_INVALIDARG, digestByteCount != MidiNetworkAuthenticationDigestByteCount);

        BCRYPT_ALG_HANDLE algorithm{ nullptr };

        auto closeAlgorithm = wil::scope_exit([&]()
            {
                if (algorithm != nullptr) { BCryptCloseAlgorithmProvider(algorithm, 0); }
            });

        auto status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        RETURN_HR_IF(HRESULT_FROM_NT(status), !BCRYPT_SUCCESS(status));

        BCRYPT_HASH_HANDLE hash{ nullptr };

        auto destroyHash = wil::scope_exit([&]()
            {
                if (hash != nullptr) { BCryptDestroyHash(hash); }
            });

        status = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);
        RETURN_HR_IF(HRESULT_FROM_NT(status), !BCRYPT_SUCCESS(status));

        for (size_t i = 0; i < partCount; i++)
        {
            if (parts[i].ByteCount == 0)
            {
                continue;
            }

            RETURN_HR_IF(E_INVALIDARG, parts[i].ByteCount > ULONG_MAX);

            status = BCryptHashData(
                hash,
                const_cast<PUCHAR>(parts[i].Bytes),
                static_cast<ULONG>(parts[i].ByteCount),
                0);

            RETURN_HR_IF(HRESULT_FROM_NT(status), !BCRYPT_SUCCESS(status));
        }

        status = BCryptFinishHash(hash, digest, static_cast<ULONG>(digestByteCount), 0);
        RETURN_HR_IF(HRESULT_FROM_NT(status), !BCRYPT_SUCCESS(status));

        return S_OK;
    }
}

// SHA-256(CryptoNonce || SharedSecret). The secret holds UTF-8 bytes.
inline HRESULT MidiNetworkComputeAuthenticationDigest(
    _In_reads_bytes_(nonceByteCount) uint8_t const* nonce,
    _In_ size_t const nonceByteCount,
    _In_ MidiNetworkSecret const& sharedSecret,
    _Out_writes_bytes_(digestByteCount) uint8_t* digest,
    _In_ size_t const digestByteCount)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, nonce);
    RETURN_HR_IF_NULL(E_INVALIDARG, digest);
    RETURN_HR_IF(E_INVALIDARG, nonceByteCount == 0);
    RETURN_HR_IF(E_INVALIDARG, sharedSecret.IsEmpty());

    MidiNetworkAuthenticationInternal::HashPart const parts[]
    {
        { nonce, nonceByteCount },
        { sharedSecret.Data(), sharedSecret.Size() },
    };

    return MidiNetworkAuthenticationInternal::ComputeSha256(
        parts, ARRAYSIZE(parts), digest, digestByteCount);
}

// SHA-256(CryptoNonce || Username || Password). Kept separate from the shared secret form so
// that the order of the three parts is not something a caller has to remember.
inline HRESULT MidiNetworkComputeUserAuthenticationDigest(
    _In_reads_bytes_(nonceByteCount) uint8_t const* nonce,
    _In_ size_t const nonceByteCount,
    _In_ std::wstring const& userName,
    _In_ MidiNetworkSecret const& password,
    _Out_writes_bytes_(digestByteCount) uint8_t* digest,
    _In_ size_t const digestByteCount)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, nonce);
    RETURN_HR_IF_NULL(E_INVALIDARG, digest);
    RETURN_HR_IF(E_INVALIDARG, nonceByteCount == 0);
    RETURN_HR_IF(E_INVALIDARG, userName.empty());
    RETURN_HR_IF(E_INVALIDARG, password.IsEmpty());

    std::string userNameUtf8{};
    RETURN_IF_FAILED(MidiNetworkAuthenticationInternal::ToUtf8(userName, userNameUtf8));

    MidiNetworkAuthenticationInternal::HashPart const parts[]
    {
        { nonce, nonceByteCount },
        { reinterpret_cast<uint8_t const*>(userNameUtf8.data()), userNameUtf8.size() },
        { password.Data(), password.Size() },
    };

    return MidiNetworkAuthenticationInternal::ComputeSha256(
        parts, ARRAYSIZE(parts), digest, digestByteCount);
}
