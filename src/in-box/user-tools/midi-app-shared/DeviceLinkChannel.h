// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// A device link connection: TCP with TLS 1.3 from Windows (SChannel) on top, where both sides
// show a certificate. Neither certificate chains to anything, so SChannel's own checks are off
// and the caller decides by the SHA-256 of the other side's public key: a paired PC's key is
// compared with the one remembered when they paired, and an unknown one is either dropped or,
// while a person has pairing open, compared by eye with a code (DeviceLinkPairing.h).
//
// One thread uses a connection at a time. Abort() is the exception: any thread may call it to
// end a read or write that is waiting. Pch-free.

#include <sal.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "DeviceLinkFrames.h"
#include "DeviceLinkIdentity.h"

namespace midiapp
{
    // How long a connection waits for the other side, unless told otherwise.
    constexpr uint32_t DeviceLinkDefaultTimeoutMilliseconds = 15'000;

    class DeviceLinkConnection
    {
    public:
        ~DeviceLinkConnection();

        DeviceLinkConnection(DeviceLinkConnection const&) = delete;
        DeviceLinkConnection& operator=(DeviceLinkConnection const&) = delete;

        // Connects, shows `identity` and finishes the handshake. The other side's key is not
        // judged here: the caller compares PeerKey() with what it expects.
        static std::unique_ptr<DeviceLinkConnection> Connect(
            _In_ std::wstring const& host,
            _In_ uint16_t port,
            _In_ DeviceIdentity const& identity,
            _In_ uint32_t timeoutMilliseconds,
            _Out_ HRESULT& error) noexcept;

        // Takes the socket a listener accepted, whatever happens.
        static std::unique_ptr<DeviceLinkConnection> Accept(
            _In_ uintptr_t acceptedSocket,
            _In_ DeviceIdentity const& identity,
            _In_ uint32_t timeoutMilliseconds,
            _Out_ HRESULT& error) noexcept;

        DeviceKeyHash const& PeerKey() const noexcept { return m_peerKey; }

        // For every read and write from now on.
        void SetTimeout(_In_ uint32_t milliseconds) noexcept;

        bool WriteFrame(_In_ uint16_t type, _In_ std::span<uint8_t const> payload) noexcept;

        DeviceLinkFrameStatus ReadFrame(
            _In_ DeviceLinkFrameCap const& capFor,
            _Out_ DeviceLinkFrame& frame) noexcept;

        // Tells the other side it's over and closes. Safe to call more than once.
        void Close() noexcept;

        // From any thread: ends a wait now. The connection can't be used afterwards.
        void Abort() noexcept;

    private:
        struct State;

        DeviceLinkConnection() noexcept;

        HRESULT ClientHandshake() noexcept;
        HRESULT ServerHandshake() noexcept;
        HRESULT FinishHandshake() noexcept;

        bool SendAll(_In_reads_bytes_(size) uint8_t const* data, _In_ size_t size) noexcept;
        bool SendPlain(_In_reads_bytes_(size) uint8_t const* data, _In_ size_t size) noexcept;
        bool ReadPlain(_Out_writes_bytes_(size) uint8_t* destination, _In_ size_t size, _Out_ bool& closedCleanly) noexcept;
        HRESULT ReceiveMore() noexcept;
        HRESULT DecryptAvailable() noexcept;

        std::unique_ptr<State> m_state;
        DeviceKeyHash m_peerKey{};
    };

    // Listens on a TCP port for device link connections. Bound to every IPv4 address, or to the
    // loopback address only, for tests. Nothing listens until Start, and nothing after Stop.
    class DeviceLinkListener
    {
    public:
        DeviceLinkListener() noexcept = default;
        ~DeviceLinkListener();

        DeviceLinkListener(DeviceLinkListener const&) = delete;
        DeviceLinkListener& operator=(DeviceLinkListener const&) = delete;

        // Port 0 lets Windows choose one; Port() says which.
        HRESULT Start(_In_ uint16_t port, _In_ bool loopbackOnly) noexcept;

        uint16_t Port() const noexcept { return m_port; }

        // A connected socket, or 0 when none arrived in time or the listener stopped. The caller
        // hands it to DeviceLinkConnection::Accept.
        uintptr_t Accept(_In_ uint32_t timeoutMilliseconds) noexcept;

        void Stop() noexcept;

    private:
        std::atomic<uintptr_t> m_socket{ 0 };
        uint16_t m_port{ 0 };
    };
}
