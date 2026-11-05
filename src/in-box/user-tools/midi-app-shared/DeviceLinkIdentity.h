// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Who this PC is to the PCs it's paired with: an ECDSA P-256 key that belongs to the signed-in
// user, kept by the Microsoft Software Key Storage Provider and never exported. A paired PC
// remembers the SHA-256 of its public key, so the self-signed certificate wrapped around it for
// TLS is made fresh every time and nothing else needs storing. Pch-free.

#include <sal.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <windows.h>
#include <wincrypt.h>

namespace midiapp
{
    // SHA-256 of the DER SubjectPublicKeyInfo.
    using DeviceKeyHash = std::array<uint8_t, 32>;

    class DeviceIdentity
    {
    public:
        ~DeviceIdentity();

        DeviceIdentity(DeviceIdentity const&) = delete;
        DeviceIdentity& operator=(DeviceIdentity const&) = delete;

        // The key with this name for the signed-in user, made the first time. Null on failure.
        static std::unique_ptr<DeviceIdentity> OpenOrCreate(
            _In_ std::wstring const& keyName,
            _Out_ HRESULT& error) noexcept;

        // Forgets the key for good. Every PC paired with this one would have to pair again.
        static HRESULT DeleteKey(_In_ std::wstring const& keyName) noexcept;

        // A certificate with the private key reachable from it, for SChannel.
        PCCERT_CONTEXT Certificate() const noexcept { return m_certificate; }

        DeviceKeyHash const& KeyHash() const noexcept { return m_keyHash; }

    private:
        DeviceIdentity() noexcept = default;

        PCCERT_CONTEXT m_certificate{ nullptr };
        DeviceKeyHash m_keyHash{};
    };

    std::optional<DeviceKeyHash> KeyHashOfCertificate(_In_ PCCERT_CONTEXT certificate) noexcept;

    // Lowercase hex, 64 characters.
    std::wstring DeviceKeyHashToHex(_In_ DeviceKeyHash const& hash);
    std::optional<DeviceKeyHash> DeviceKeyHashFromHex(_In_ std::wstring_view hex) noexcept;

    // The first 8 bytes in groups, like "3F2A-91C0-0B7D-E412", for a person to compare by eye.
    std::wstring ShortDeviceFingerprint(_In_ DeviceKeyHash const& hash);

    // Constant time, so a comparison doesn't say how much of a key matched.
    bool SameDeviceKey(_In_ DeviceKeyHash const& first, _In_ DeviceKeyHash const& second) noexcept;

    bool DeviceLinkSha256(
        _In_ std::span<uint8_t const> data,
        _Out_ std::array<uint8_t, 32>& hash) noexcept;

    bool DeviceLinkRandom(_Out_writes_bytes_(size) uint8_t* destination, _In_ size_t size) noexcept;
}
