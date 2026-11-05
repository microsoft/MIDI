// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// "Sync to device" on the wire, on top of a device link connection (midi-app-shared).
//
// Receiver: listens only while its owner (the Receive screen) keeps it running. One connection
// at a time. A PC it isn't paired with is dropped right after the handshake, unless the person
// here has pairing open, and then it can only pair. A paired PC can offer one layout:
//
//   sender   -> Offer (layout id, name, manifest bytes, number of signatures), Signature...
//   receiver -> Need (the SHA-256 of each file it wants)
//   sender   -> FileStart + FileChunk... for each, then Done
//   receiver -> Result, after the whole pack is put back together and checked
//
// Every frame has a cap for its type, every read has a timeout, and the receiver accepts only
// the files it asked for, each at the size the manifest gives. Installing is not done here: the
// checked pack is handed to the app, which runs it through the same import as a pack file.
// Pch-free, so the unit tests run a real sender and receiver against each other.

#include <sal.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "DeviceLinkChannel.h"
#include "DeviceLinkPairing.h"
#include "PairedDevices.h"
#include "SyncPack.h"

namespace glass::sync
{
    constexpr wchar_t SyncAppName[] = L"midiglass";
    constexpr uint16_t SyncProtocolVersion = 1;
    constexpr wchar_t SyncServiceType[] = L"_midiglass._tcp.local";

    // A person has this long to compare the codes.
    constexpr uint32_t PairingAnswerTimeoutMilliseconds = 3 * 60 * 1000;

    namespace SyncFrameType
    {
        constexpr uint16_t Offer = 0x0100;
        constexpr uint16_t Signature = 0x0101;
        constexpr uint16_t Need = 0x0102;
        constexpr uint16_t FileStart = 0x0103;
        constexpr uint16_t FileChunk = 0x0104;
        constexpr uint16_t Done = 0x0105;
        constexpr uint16_t Result = 0x0106;
    }

    // ------------------------------------------------------------------------ pairing

    // A pairing waiting for the person on this PC to compare codes. Shared between the network
    // thread and the screen showing the code.
    class PendingPairing
    {
    public:
        std::wstring PeerName{};
        std::wstring Code{};
        midiapp::DeviceKeyHash PeerKey{};

        // From the screen. Only the first answer counts.
        void Answer(_In_ bool matched) noexcept;

        // From the network thread. Nothing when it timed out or was stopped.
        std::optional<bool> WaitForAnswer(_In_ std::stop_token stopToken, _In_ uint32_t timeoutMilliseconds) noexcept;

    private:
        std::mutex m_lock{};
        std::condition_variable_any m_answered{};
        std::optional<bool> m_answer{};
    };

    enum class PairOutcome
    {
        // Start: the code is ready to show.
        CodeReady,

        Paired,
        CouldNotConnect,

        // The other PC doesn't have pairing open.
        NotPairing,

        // Somebody said the codes didn't match, on either side.
        Declined,
        Failed,
        Canceled,
    };

    // The sending side of pairing. Connect, then show Code(), then Finish with the answer.
    class SyncPairingInitiator
    {
    public:
        PairOutcome Start(
            _In_ std::wstring const& host,
            _In_ uint16_t port,
            _In_ midiapp::DeviceIdentity const& identity,
            _In_ std::wstring const& ownName) noexcept;

        std::wstring const& Code() const noexcept { return m_code; }
        std::wstring const& PeerName() const noexcept { return m_peerName; }
        midiapp::DeviceKeyHash const& PeerKey() const noexcept { return m_peerKey; }

        // Tells the other PC what was decided here and hears its answer. Remembers it only when
        // both said yes.
        PairOutcome Finish(_In_ bool matched, _In_ midiapp::PairedDeviceList& paired) noexcept;

        // From any thread.
        void Cancel() noexcept;

    private:
        std::mutex m_lock{};
        std::shared_ptr<midiapp::DeviceLinkConnection> m_connection{};
        std::atomic<bool> m_canceled{ false };

        std::wstring m_code{};
        std::wstring m_peerName{};
        midiapp::DeviceKeyHash m_peerKey{};
    };

    // ------------------------------------------------------------------------ sending

    enum class SendOutcome
    {
        Sent,
        CouldNotConnect,

        // Something answered, but not with the key this PC paired with.
        NotThePairedDevice,

        // The other PC doesn't know this one any more.
        NotPaired,

        // Busy, or a different app or version.
        Refused,

        // The other PC couldn't use what arrived.
        PackProblem,

        Failed,
        Canceled,
    };

    struct SendResult
    {
        SendOutcome Outcome{ SendOutcome::Failed };
        HRESULT Error{ S_OK };
        midiapp::ContentPackProblem ReceiverProblem{ midiapp::ContentPackProblem::None };
        uint32_t FilesSent{ 0 };
        uint64_t BytesSent{ 0 };

        // How many of the pack's files the other PC already had.
        uint32_t FilesSkipped{ 0 };
    };

    // Bytes sent so far, and the total that will be sent.
    using SendProgress = std::function<void(uint64_t sent, uint64_t total)>;

    class SyncSender
    {
    public:
        SendResult Send(
            _In_ std::wstring const& host,
            _In_ uint16_t port,
            _In_ midiapp::DeviceIdentity const& identity,
            _In_ std::wstring const& ownName,
            _In_ midiapp::DeviceKeyHash const& pairedKey,
            _In_ SyncSource const& source,
            _In_ SendProgress const& progress) noexcept;

        // From any thread.
        void Cancel() noexcept;

    private:
        std::mutex m_lock{};
        std::shared_ptr<midiapp::DeviceLinkConnection> m_connection{};
        std::atomic<bool> m_canceled{ false };
    };

    // ------------------------------------------------------------------------ receiving

    struct ReceivedSync
    {
        midiapp::DeviceKeyHash Sender{};
        std::wstring SenderName{};

        SyncOffer Offer{};

        // Already checked, exactly as a pack file is checked before its import dialog.
        midiapp::OpenedContentPack Pack{};

        uint32_t FilesReceived{ 0 };
        uint32_t FilesReused{ 0 };
    };

    struct SyncReceiverOptions
    {
        std::shared_ptr<midiapp::DeviceIdentity> Identity{};
        std::wstring OwnName{};
        std::wstring PairedListPath{};

        // Where this PC's own copies of files might be: its layouts and themes folders.
        std::vector<std::wstring> Folders{};
        std::wstring FileIndexPath{};

        // Tests only.
        bool LoopbackOnly{ false };
        midiapp::ContentPackVerifyOptions Verify{};

        // Every callback runs on the receiver's own thread, and must return quickly.
        std::function<bool()> IsPairingOpen{};
        std::function<void(std::shared_ptr<PendingPairing> const&)> PairingAsked{};
        std::function<void(std::shared_ptr<PendingPairing> const&, bool)> PairingFinished{};
        std::function<void(std::wstring const& senderName, std::wstring const& layoutName)> SyncStarted{};
        std::function<void(ReceivedSync&&)> SyncReceived{};
        std::function<void(std::wstring const& senderName, std::wstring const& layoutName, midiapp::ContentPackProblem)> SyncFailed{};
    };

    class SyncReceiver
    {
    public:
        SyncReceiver() noexcept = default;
        ~SyncReceiver();

        SyncReceiver(SyncReceiver const&) = delete;
        SyncReceiver& operator=(SyncReceiver const&) = delete;

        HRESULT Start(_In_ SyncReceiverOptions options) noexcept;

        uint16_t Port() const noexcept { return m_listener.Port(); }

        // Stops listening and ends any connection in progress. Waits for the thread.
        void Stop() noexcept;

    private:
        void Work(_In_ std::stop_token stopToken) noexcept;
        void Handle(_In_ uintptr_t socket, _In_ std::stop_token stopToken) noexcept;
        void Pair(_In_ midiapp::DeviceLinkConnection& connection, _In_ midiapp::DeviceLinkHello const& hello, _In_ std::stop_token stopToken) noexcept;
        void Receive(_In_ midiapp::DeviceLinkConnection& connection, _In_ midiapp::DeviceLinkHello const& hello) noexcept;

        SyncReceiverOptions m_options{};
        midiapp::DeviceLinkListener m_listener{};

        std::mutex m_lock{};
        std::shared_ptr<midiapp::DeviceLinkConnection> m_current{};

        std::jthread m_worker{};
    };
}
