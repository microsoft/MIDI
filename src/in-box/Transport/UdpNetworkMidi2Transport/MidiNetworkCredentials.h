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

// Network MIDI 2.0 authentication: spec 6.5 and 6.6 (the invitation commands), and 6.9 and 6.10
// (the digests). https://github.com/microsoft/MIDI/issues/733
//
// Windows does not support authentication yet. The digest below is implemented and tested
// against the specification's own worked examples. Nothing else is, because there is no decision
// yet on where the secrets would live. Until there is, authentication is refused cleanly:
//
//  - Configuration refuses to start a host set to "password" or "user" authentication
//    (ValidateHostDefinition in Midi2.NetworkMidiConfigurationManager.cpp).
//  - A host answers an authenticated invitation with Bye 0x41 No Matching Authentication Method
//    (MidiNetworkHostConnection::HandleIncomingInvitationWithAuthentication).
//  - A client answers a challenge with Bye 0x80 Invitation Canceled
//    (MidiNetworkClientConnection::HandleIncomingInvitationReplyAuthenticationRequired).
//
// Where authentication would be added:
//
//  1. A credential store, and a resolver that turns an identifier from the configuration into a
//     MidiNetworkSecret. The configuration file only ever holds the identifier, never a secret.
//  2. Configuration keys for that identifier, on host entries and on client entries. Before it
//     touches any store, the identifier must be checked to hold only letters, digits, '-' and
//     '_', 64 characters at most, so it can't name a credential outside our own namespace.
//  3. Host: MidiNetworkHostConnection::HandleIncomingInvitation answers with Invitation Reply:
//     Authentication Required and a fresh 16 byte nonce from BCryptGenRandom, before any other
//     decision. HandleIncomingInvitationWithAuthentication then recomputes the digest to decide.
//  4. Client: MidiNetworkClientConnection::HandleIncomingInvitationReplyAuthenticationRequired
//     computes the digest over the host's nonce and answers with an authenticated invitation.
//     SendInvitationCommand advertises support in the capability bits (spec Table 11).
//  5. MidiNetworkDataWriter writes the four authentication commands.
//
// What is known about storage, measured on a PC that is not joined to a domain (Aug 29 2026):
//
//  - midisrv runs as LocalService, can never prompt, and has to connect with nobody signed in.
//    So the secret must already be stored where the service can read it with no user present,
//    which rules out the per-user Credential Locker.
//  - Nothing encrypts a secret so that only LocalService can read it. DPAPI-NG with a SID=
//    descriptor needs a domain Key Distribution Service and fails with NTE_ENCRYPTION_FAILURE
//    without one. Machine-scope DPAPI of either kind was decrypted by an ordinary user. So the
//    ACL on the store is the security boundary, and encryption only protects a copy taken off
//    the PC.
//  - C:\ProgramData\Microsoft\MIDI can't hold it: Everyone can read it and Authenticated Users
//    can write to it.
//  - A store whose DACL grants read to NT SERVICE\midisrv and full control to Administrators
//    works: the user who created such a file could not read it back. The service SID is
//    S-1-5-80 plus the SHA-1 of the upper-cased service name, so it is the same on every PC.
//    The setup app, running elevated, would write it. A file in its own folder and a key under
//    HKLM would work equally well; that choice is still open.
//  - Both roles recompute the digest from the secret itself, so it can't be stored one way.
//  - The digest is a single SHA-256 with no key stretching, so one captured invitation exchange
//    allows an offline attack on the secret. At 10^10 guesses a second, four words from a list
//    of 7776 fall in about two days, and six take about 350,000 years. Recommend six.
//  - There are two kinds of secret. One for a remote host this PC connects to: someone else
//    chose it and it may be used elsewhere too, so it is never shown again, only replaced. One
//    for a host on this PC: the setup app can generate it, and the user must be able to read it
//    back to type into the other device. Whether a secret may be shown is recorded on the
//    stored entry, never implied by the identifier, which comes from the configuration.
//  - The configuration file can be copied to another PC and the store can't. There every
//    identifier points at nothing, and that has to be reported as missing credentials, not as
//    a failed connection.
//  - Ruled out: secrets in the configuration file; one-way storage; the Credential Locker; RPC
//    client impersonation, which lasts only for a live client call; a second service running
//    as the signed-in user, which isn't running when nobody is signed in; anything needing the
//    user present, such as a TPM or Windows Hello unlock; generating a passphrase, because
//    Windows has no word list to draw from. LSA private data was not measured. Reading it needs
//    policy rights LocalService doesn't have.
//  - Also open: how a secret is revoked, and how a session using it is ended when it is.
//  - A plugin loaded into midisrv can read anything the service can. If that matters, the store
//    should compute and check digests for the transport rather than hand it secrets.
//  - Remote management is being added to the specification. Re-check all of this if a
//    credential ever protects more than MIDI traffic.


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
