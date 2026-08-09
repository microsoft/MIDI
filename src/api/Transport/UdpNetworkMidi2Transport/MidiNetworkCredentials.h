// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// Placeholders for Network MIDI 2.0 authentication (spec 6.5, 6.6 and Appendix B).
//
// NONE OF THIS IS WIRED UP YET. See https://github.com/microsoft/MIDI/issues/733
//
// Design constraints which shape everything in this file:
//
//  - midisrv runs as LocalService with network capability. It is not, and will not be,
//    LocalSystem. Whatever retrieves a secret has to work within that.
//  - The service cannot prompt. There is no interactive desktop and no user to answer, so a
//    secret can never be collected at connection time. It must already be stored.
//  - The MIDI Settings app, running as the user, is what collects and stores the secret. The
//    service is only ever handed an *identifier*, never the secret itself, through
//    configuration JSON.
//  - Therefore the identifier arrives from a lower-trust source and is attacker-influenced in
//    the threat model where someone can write the config. It must never be usable to name an
//    arbitrary credential on the machine. MidiNetworkCredentialIdentifier exists solely to
//    enforce that: the resolver may only ever look inside our own namespace.
//
// Open questions, to be settled before any of this is implemented:
//
//  - Which store. The per-user Credential Locker is the natural place for the Settings app to
//    write, but a LocalService process cannot read another user's locker without impersonating
//    that user, and holding an impersonation token in the service for the life of a session is
//    not something to do casually.
//  - If we impersonate, when and for how long. The narrowest version is impersonate, read one
//    named secret, revert, immediately in the config path, and never hold the token.
//  - Whether the secret should instead be DPAPI-protected by the Settings app and stored in a
//    service-readable location, so the service never touches the user's locker at all. This
//    avoids impersonation entirely but changes who can decrypt.
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
