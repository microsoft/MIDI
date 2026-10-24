// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Pairing two PCs, and the greeting every device link connection starts with.
//
// Pairing is numeric comparison with commit-before-reveal, the way Bluetooth Secure Simple
// Pairing and ZRTP do it. It runs inside the TLS connection, after the handshake, so each side
// knows the key the other one used for TLS:
//
//   1. The initiator (the PC that connected) sends C = SHA-256(label, its key, its nonce).
//   2. The responder sends its own nonce.
//   3. The initiator sends its nonce, and the responder checks it against C.
//   4. Both show a 6-digit code made from both keys and both nonces, and a person on each side
//      says whether the two codes match.
//
// A person in the middle has a different TLS connection with each side, so the two sides put
// different keys into the code. The commitment fixes the initiator's nonce before it sees the
// other nonce, so the attacker can't try nonces until two codes agree: one guess, a one in a
// million chance. Nothing is ever typed, so there's nothing to guess offline either. Pch-free.

#include <sal.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "DeviceLinkChannel.h"

namespace midiapp
{
    using DeviceLinkNonce = std::array<uint8_t, 32>;
    using DeviceLinkCommitment = std::array<uint8_t, 32>;

    // Types below 0x100 belong to the device link itself. An app's own frames start at 0x100.
    namespace DeviceLinkFrameType
    {
        constexpr uint16_t Hello = 0x0001;
        constexpr uint16_t Refused = 0x0002;
        constexpr uint16_t PairCommit = 0x0010;
        constexpr uint16_t PairNonce = 0x0011;
        constexpr uint16_t PairReveal = 0x0012;
        constexpr uint16_t PairConfirm = 0x0013;

        constexpr uint16_t FirstAppType = 0x0100;
    }

    enum class DeviceLinkPurpose : uint8_t
    {
        Pair = 1,

        // Anything a paired PC does.
        Use = 2,
    };

    enum class DeviceLinkRefusal : uint8_t
    {
        None = 0,

        // This PC doesn't know the other one, and pairing isn't open.
        NotPaired = 1,

        // Pairing isn't open on this PC.
        NotPairing = 2,

        // Already busy with another PC.
        Busy = 3,

        // A different app, or a version this one can't talk to.
        WrongApp = 4,

        // The person said no, or the codes didn't match.
        Declined = 5,
    };

    constexpr size_t MaximumDeviceLinkNameLength = 64;
    constexpr size_t MaximumDeviceLinkAppLength = 32;

    struct DeviceLinkHello
    {
        // Which app this is, like "midiglass". Two different apps never pair.
        std::wstring App{};
        uint16_t Version{ 0 };
        DeviceLinkPurpose Purpose{ DeviceLinkPurpose::Use };

        // What a person calls this PC, already cleaned up for showing.
        std::wstring DeviceName{};
    };

    // Strips direction overrides, zero-width and control characters, and shortens it, so a name
    // can't pretend to be something else on the other screen.
    std::wstring CleanDeviceLinkName(_In_ std::wstring_view name) noexcept;

    // The computer's name, for the greeting.
    std::wstring ThisDeviceLinkName() noexcept;

    bool NewDeviceLinkNonce(_Out_ DeviceLinkNonce& nonce) noexcept;

    DeviceLinkCommitment PairingCommitment(
        _In_ DeviceKeyHash const& initiatorKey,
        _In_ DeviceLinkNonce const& initiatorNonce) noexcept;

    bool PairingRevealMatches(
        _In_ DeviceLinkCommitment const& commitment,
        _In_ DeviceKeyHash const& initiatorKey,
        _In_ DeviceLinkNonce const& initiatorNonce) noexcept;

    // 0 to 999999.
    uint32_t PairingCode(
        _In_ DeviceKeyHash const& initiatorKey,
        _In_ DeviceKeyHash const& responderKey,
        _In_ DeviceLinkNonce const& initiatorNonce,
        _In_ DeviceLinkNonce const& responderNonce) noexcept;

    // "042 917": six digits, leading zeros kept, in two groups.
    std::wstring FormatPairingCode(_In_ uint32_t code);

    // ------------------------------------------------------------------------ over a connection

    bool WriteDeviceLinkHello(_Inout_ DeviceLinkConnection& connection, _In_ DeviceLinkHello const& hello) noexcept;

    // Reads a greeting, or a refusal in its place.
    bool ReadDeviceLinkHello(
        _Inout_ DeviceLinkConnection& connection,
        _Out_ DeviceLinkHello& hello,
        _Out_ DeviceLinkRefusal& refusal) noexcept;

    bool WriteDeviceLinkRefusal(_Inout_ DeviceLinkConnection& connection, _In_ DeviceLinkRefusal refusal) noexcept;

    // Steps 1 to 3 above. The code, or nothing when the other side broke the protocol or left.
    std::optional<uint32_t> PairAsInitiator(
        _Inout_ DeviceLinkConnection& connection,
        _In_ DeviceKeyHash const& ownKey) noexcept;

    std::optional<uint32_t> PairAsResponder(
        _Inout_ DeviceLinkConnection& connection,
        _In_ DeviceKeyHash const& ownKey) noexcept;

    // Step 4: says what this side's person decided, and hears the other's. Both have to say yes
    // before either side remembers the other. Waits as long as the connection's timeout.
    bool ExchangePairingAnswers(
        _Inout_ DeviceLinkConnection& connection,
        _In_ bool matched,
        _Out_ bool& otherMatched) noexcept;
}
