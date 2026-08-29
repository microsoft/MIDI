// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "MidiNetworkCredentials.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    MidiNetworkSecret MakeSecret(_In_ std::string const& value)
    {
        MidiNetworkSecret secret;
        secret.Assign(reinterpret_cast<uint8_t const*>(value.data()), value.size());

        return secret;
    }

    std::wstring ToHex(_In_reads_bytes_(byteCount) uint8_t const* bytes, _In_ size_t const byteCount)
    {
        std::wstring text{};

        for (size_t i = 0; i < byteCount; i++)
        {
            wchar_t pair[3]{};
            swprintf_s(pair, L"%02X", bytes[i]);
            text += pair;
        }

        return text;
    }

    void VerifyDigest(
        _In_ std::string const& nonce,
        _In_ std::string const& sharedSecret,
        _In_ std::wstring const& expectedHex)
    {
        uint8_t digest[MidiNetworkAuthenticationDigestByteCount]{};

        auto const secret = MakeSecret(sharedSecret);

        VERIFY_SUCCEEDED(MidiNetworkComputeAuthenticationDigest(
            reinterpret_cast<uint8_t const*>(nonce.data()), nonce.size(),
            secret,
            digest, ARRAYSIZE(digest)));

        VERIFY_ARE_EQUAL(expectedHex, ToHex(digest, ARRAYSIZE(digest)));
    }
}


// Specification 6.9. hash = to_sha256_digest("<CryptoNonce><Shared Secret>")
void MidiNetworkAuthenticationDigestTests::SharedSecretDigestMatchesSpecExample()
{
    VerifyDigest(
        "nUWrn*@#$hjfwnkL",
        "5483",
        L"676EBE82587CECA8F82FC333D787951EEC2B00AD31613CC4DB18CF27373AFB82");
}


// Specification 6.10. hash = to_sha256_digest("<CryptoNonce><Username><Password>")
void MidiNetworkAuthenticationDigestTests::UserAuthenticationDigestMatchesSpecExample()
{
    std::string const nonce{ "XI|~=NNRVaD;XCPL" };

    uint8_t digest[MidiNetworkAuthenticationDigestByteCount]{};

    auto const password = MakeSecret("RPBqBno");

    VERIFY_SUCCEEDED(MidiNetworkComputeUserAuthenticationDigest(
        reinterpret_cast<uint8_t const*>(nonce.data()), nonce.size(),
        L"Rosa",
        password,
        digest, ARRAYSIZE(digest)));

    VERIFY_ARE_EQUAL(
        std::wstring{ L"3A2D5EBEEF92E8463535C27FB4B7B3E2D1ACF57A844C6D0808E041C102B7FF1A" },
        ToHex(digest, ARRAYSIZE(digest)));
}


// Nonce first, secret second. Reversing produces a digest which looks perfectly healthy and
// interoperates with nothing.
void MidiNetworkAuthenticationDigestTests::DigestIsOrderDependent()
{
    uint8_t forward[MidiNetworkAuthenticationDigestByteCount]{};
    uint8_t reversed[MidiNetworkAuthenticationDigestByteCount]{};

    std::string const nonce{ "0123456789ABCDEF" };
    std::string const secret{ "hunter2" };

    auto const a = MakeSecret(secret);

    VERIFY_SUCCEEDED(MidiNetworkComputeAuthenticationDigest(
        reinterpret_cast<uint8_t const*>(nonce.data()), nonce.size(), a,
        forward, ARRAYSIZE(forward)));

    auto const b = MakeSecret(nonce);

    VERIFY_SUCCEEDED(MidiNetworkComputeAuthenticationDigest(
        reinterpret_cast<uint8_t const*>(secret.data()), secret.size(), b,
        reversed, ARRAYSIZE(reversed)));

    VERIFY_ARE_NOT_EQUAL(
        ToHex(forward, ARRAYSIZE(forward)),
        ToHex(reversed, ARRAYSIZE(reversed)));
}


// Straight concatenation: no separator and no length prefix. If either were present, moving the
// boundary between the two parts would change the digest. It must not.
void MidiNetworkAuthenticationDigestTests::DigestHasNoSeparatorBetweenParts()
{
    uint8_t left[MidiNetworkAuthenticationDigestByteCount]{};
    uint8_t right[MidiNetworkAuthenticationDigestByteCount]{};

    auto const secretA = MakeSecret("CDEF");

    VERIFY_SUCCEEDED(MidiNetworkComputeAuthenticationDigest(
        reinterpret_cast<uint8_t const*>("AB"), 2, secretA, left, ARRAYSIZE(left)));

    auto const secretB = MakeSecret("BCDEF");

    VERIFY_SUCCEEDED(MidiNetworkComputeAuthenticationDigest(
        reinterpret_cast<uint8_t const*>("A"), 1, secretB, right, ARRAYSIZE(right)));

    VERIFY_ARE_EQUAL(
        ToHex(left, ARRAYSIZE(left)),
        ToHex(right, ARRAYSIZE(right)));
}


// Only the bytes handed over are hashed: the digest neither stops at an embedded null nor
// appends a terminator of its own. Either would still match our own implementation while
// interoperating with nothing.
void MidiNetworkAuthenticationDigestTests::DigestExcludesAnyStringTerminator()
{
    std::string const nonce{ "nUWrn*@#$hjfwnkL" };
    std::wstring const specDigest{ L"676EBE82587CECA8F82FC333D787951EEC2B00AD31613CC4DB18CF27373AFB82" };

    // Four bytes, no terminator, is what the specification's example hashes.
    VerifyDigest(nonce, "5483", specDigest);

    // The same four characters plus an explicit null is five bytes of secret, and must therefore
    // be a different digest. If these matched, we would be trimming or padding somewhere.
    uint8_t withNull[MidiNetworkAuthenticationDigestByteCount]{};

    auto const secret = MakeSecret(std::string("5483\0", 5));

    VERIFY_SUCCEEDED(MidiNetworkComputeAuthenticationDigest(
        reinterpret_cast<uint8_t const*>(nonce.data()), nonce.size(),
        secret,
        withNull, ARRAYSIZE(withNull)));

    VERIFY_ARE_NOT_EQUAL(specDigest, ToHex(withNull, ARRAYSIZE(withNull)));
}


// Not a defect in our code, but a property of the specification worth having written down: with
// no separator, the user name and password boundary is not authenticated. "Ro" + "saRPBqBno"
// yields exactly the digest the spec publishes for "Rosa" + "RPBqBno".
void MidiNetworkAuthenticationDigestTests::UserDigestIsNotAmbiguousAcrossTheUserNameBoundary()
{
    std::string const nonce{ "XI|~=NNRVaD;XCPL" };

    uint8_t digest[MidiNetworkAuthenticationDigestByteCount]{};

    auto const password = MakeSecret("saRPBqBno");

    VERIFY_SUCCEEDED(MidiNetworkComputeUserAuthenticationDigest(
        reinterpret_cast<uint8_t const*>(nonce.data()), nonce.size(),
        L"Ro",
        password,
        digest, ARRAYSIZE(digest)));

    VERIFY_ARE_EQUAL(
        std::wstring{ L"3A2D5EBEEF92E8463535C27FB4B7B3E2D1ACF57A844C6D0808E041C102B7FF1A" },
        ToHex(digest, ARRAYSIZE(digest)));
}


// The wide user name has to become UTF-8 before hashing. Comparing against the shared secret
// form fed the explicit UTF-8 bytes is what proves the conversion, rather than assuming it.
void MidiNetworkAuthenticationDigestTests::NonAsciiUserNameIsHashedAsUtf8()
{
    std::string const nonce{ "0123456789ABCDEF" };

    uint8_t viaUserPath[MidiNetworkAuthenticationDigestByteCount]{};

    auto const password = MakeSecret("pw");

    VERIFY_SUCCEEDED(MidiNetworkComputeUserAuthenticationDigest(
        reinterpret_cast<uint8_t const*>(nonce.data()), nonce.size(),
        L"Caf\u00e9",
        password,
        viaUserPath, ARRAYSIZE(viaUserPath)));

    // "Caf" + U+00E9 as UTF-8 (0xC3 0xA9) + "pw"
    uint8_t viaExplicitBytes[MidiNetworkAuthenticationDigestByteCount]{};

    auto const combined = MakeSecret(std::string("Caf\xC3\xA9pw"));

    VERIFY_SUCCEEDED(MidiNetworkComputeAuthenticationDigest(
        reinterpret_cast<uint8_t const*>(nonce.data()), nonce.size(),
        combined,
        viaExplicitBytes, ARRAYSIZE(viaExplicitBytes)));

    VERIFY_ARE_EQUAL(
        ToHex(viaExplicitBytes, ARRAYSIZE(viaExplicitBytes)),
        ToHex(viaUserPath, ARRAYSIZE(viaUserPath)));
}


void MidiNetworkAuthenticationDigestTests::RejectsBadArguments()
{
    uint8_t digest[MidiNetworkAuthenticationDigestByteCount]{};

    std::string const nonce{ "0123456789ABCDEF" };
    auto const secret = MakeSecret("secret");

    auto const noncePtr = reinterpret_cast<uint8_t const*>(nonce.data());

    VERIFY_ARE_EQUAL(E_INVALIDARG,
        MidiNetworkComputeAuthenticationDigest(nullptr, nonce.size(), secret, digest, ARRAYSIZE(digest)));

    VERIFY_ARE_EQUAL(E_INVALIDARG,
        MidiNetworkComputeAuthenticationDigest(noncePtr, 0, secret, digest, ARRAYSIZE(digest)));

    VERIFY_ARE_EQUAL(E_INVALIDARG,
        MidiNetworkComputeAuthenticationDigest(noncePtr, nonce.size(), secret, nullptr, ARRAYSIZE(digest)));

    // A short output buffer must be refused rather than partially filled.
    VERIFY_ARE_EQUAL(E_INVALIDARG,
        MidiNetworkComputeAuthenticationDigest(noncePtr, nonce.size(), secret, digest, 16));

    MidiNetworkSecret empty;

    VERIFY_ARE_EQUAL(E_INVALIDARG,
        MidiNetworkComputeAuthenticationDigest(noncePtr, nonce.size(), empty, digest, ARRAYSIZE(digest)));

    VERIFY_ARE_EQUAL(E_INVALIDARG,
        MidiNetworkComputeUserAuthenticationDigest(noncePtr, nonce.size(), L"", secret, digest, ARRAYSIZE(digest)));
}
