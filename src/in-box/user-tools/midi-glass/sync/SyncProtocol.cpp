// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Before windows.h, so the JSON projection is declared without the GetObject macro.
#include "ContentProvenance.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>

#include "SyncProtocol.h"
#include "SyncFileIndex.h"

#include <algorithm>
#include <map>
#include <set>

namespace glass::sync
{
    namespace
    {
        using midiapp::ContentPackProblem;
        using midiapp::DeviceLinkConnection;
        using midiapp::DeviceLinkFrame;
        using midiapp::DeviceLinkFrameStatus;
        using midiapp::DeviceLinkReader;
        using midiapp::DeviceLinkWriter;

        constexpr uint32_t ConnectTimeoutMilliseconds = 10'000;

        // Between frames while files are moving.
        constexpr uint32_t TransferTimeoutMilliseconds = 30'000;

        // Checking the pack can mean asking the network whether a signing certificate was revoked.
        constexpr uint32_t ResultTimeoutMilliseconds = 120'000;

        constexpr uint32_t ChunkBytes = midiapp::MaximumDeviceLinkFrameBytes;

        constexpr uint32_t OfferCap = MaximumManifestBytes + 4096;
        constexpr uint32_t SignatureCap = static_cast<uint32_t>(midiapp::MaximumContentPackSignatureBytes) + 1024;
        constexpr uint32_t NeedCap = 4 + midiapp::MaximumContentPackEntries * 32;
        constexpr uint32_t FileStartBytes = 32 + 8;
        constexpr uint32_t ResultBytes = 1 + 4;

        constexpr size_t MaximumSignatureNameLength = 128;

        // Both ends of an empty frame: the cap has to be at least 1 for the type to be allowed.
        constexpr uint32_t EmptyCap = 1;

        enum class ResultCode : uint8_t
        {
            Received = 0,
            Problem = 1,
        };

        midiapp::DeviceLinkFrameCap Allow(uint16_t type, uint32_t cap)
        {
            return [type, cap](uint16_t candidate) -> uint32_t { return candidate == type ? cap : 0u; };
        }

        std::optional<std::array<uint8_t, 32>> HashBytes(_In_ std::wstring const& hex) noexcept
        {
            return midiapp::DeviceKeyHashFromHex(hex);
        }

        bool WriteResult(_Inout_ DeviceLinkConnection& connection, _In_ ContentPackProblem problem) noexcept
        {
            try
            {
                DeviceLinkWriter writer{};
                writer.U8(static_cast<uint8_t>(problem == ContentPackProblem::None ? ResultCode::Received : ResultCode::Problem));
                writer.U32(static_cast<uint32_t>(problem));

                return connection.WriteFrame(SyncFrameType::Result, writer.Data());
            }
            catch (...)
            {
                return false;
            }
        }

        // A problem code from the other side, kept within the names this build knows.
        ContentPackProblem ProblemFrom(_In_ uint32_t value) noexcept
        {
            return value <= static_cast<uint32_t>(ContentPackProblem::RevokedSignature)
                ? static_cast<ContentPackProblem>(value)
                : ContentPackProblem::Damaged;
        }

        midiapp::DeviceLinkHello OwnHello(_In_ std::wstring const& name, _In_ midiapp::DeviceLinkPurpose purpose)
        {
            midiapp::DeviceLinkHello hello{};
            hello.App = SyncAppName;
            hello.Version = SyncProtocolVersion;
            hello.Purpose = purpose;
            hello.DeviceName = name;

            return hello;
        }

        bool IsOurs(_In_ midiapp::DeviceLinkHello const& hello) noexcept
        {
            return hello.App == SyncAppName && hello.Version == SyncProtocolVersion;
        }
    }

    // ------------------------------------------------------------------------ pending pairing

    _Use_decl_annotations_
    void PendingPairing::Answer(bool matched) noexcept
    {
        try
        {
            {
                std::scoped_lock guard{ m_lock };

                if (m_answer.has_value())
                {
                    return;
                }

                m_answer = matched;
            }

            m_answered.notify_all();
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    std::optional<bool> PendingPairing::WaitForAnswer(std::stop_token stopToken, uint32_t timeoutMilliseconds) noexcept
    {
        try
        {
            std::unique_lock lock{ m_lock };

            m_answered.wait_for(lock, stopToken, std::chrono::milliseconds{ timeoutMilliseconds },
                [this]() { return m_answer.has_value(); });

            return m_answer;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    // ------------------------------------------------------------------------ pairing, sending side

    _Use_decl_annotations_
    PairOutcome SyncPairingInitiator::Start(
        std::wstring const& host,
        uint16_t port,
        midiapp::DeviceIdentity const& identity,
        std::wstring const& ownName) noexcept
    {
        try
        {
            HRESULT error{ E_FAIL };
            std::shared_ptr<DeviceLinkConnection> connection{
                DeviceLinkConnection::Connect(host, port, identity, ConnectTimeoutMilliseconds, error) };

            if (connection == nullptr)
            {
                return m_canceled ? PairOutcome::Canceled : PairOutcome::CouldNotConnect;
            }

            {
                std::scoped_lock guard{ m_lock };
                m_connection = connection;
            }

            if (m_canceled)
            {
                connection->Abort();
                return PairOutcome::Canceled;
            }

            if (!midiapp::WriteDeviceLinkHello(*connection, OwnHello(ownName, midiapp::DeviceLinkPurpose::Pair)))
            {
                return m_canceled ? PairOutcome::Canceled : PairOutcome::Failed;
            }

            midiapp::DeviceLinkHello hello{};
            midiapp::DeviceLinkRefusal refusal{};

            if (!midiapp::ReadDeviceLinkHello(*connection, hello, refusal))
            {
                if (m_canceled)
                {
                    return PairOutcome::Canceled;
                }

                return refusal == midiapp::DeviceLinkRefusal::NotPairing ? PairOutcome::NotPairing : PairOutcome::Failed;
            }

            if (!IsOurs(hello) || hello.Purpose != midiapp::DeviceLinkPurpose::Pair)
            {
                return PairOutcome::Failed;
            }

            auto const code = midiapp::PairAsInitiator(*connection, identity.KeyHash());

            if (!code)
            {
                return m_canceled ? PairOutcome::Canceled : PairOutcome::Failed;
            }

            m_code = midiapp::FormatPairingCode(*code);
            m_peerName = hello.DeviceName;
            m_peerKey = connection->PeerKey();

            // The other side waits for a person too.
            connection->SetTimeout(PairingAnswerTimeoutMilliseconds + ConnectTimeoutMilliseconds);

            return PairOutcome::CodeReady;
        }
        catch (...)
        {
            return PairOutcome::Failed;
        }
    }

    _Use_decl_annotations_
    PairOutcome SyncPairingInitiator::Finish(bool matched, midiapp::PairedDeviceList& paired) noexcept
    {
        try
        {
            std::shared_ptr<DeviceLinkConnection> connection{};

            {
                std::scoped_lock guard{ m_lock };
                connection = m_connection;
            }

            if (connection == nullptr)
            {
                return PairOutcome::Failed;
            }

            bool otherMatched{ false };
            auto const exchanged = midiapp::ExchangePairingAnswers(*connection, matched, otherMatched);

            connection->Close();

            if (m_canceled)
            {
                return PairOutcome::Canceled;
            }

            if (!exchanged)
            {
                return matched ? PairOutcome::Failed : PairOutcome::Declined;
            }

            if (!matched || !otherMatched)
            {
                return PairOutcome::Declined;
            }

            return paired.Remember(m_peerKey, m_peerName) ? PairOutcome::Paired : PairOutcome::Failed;
        }
        catch (...)
        {
            return PairOutcome::Failed;
        }
    }

    void SyncPairingInitiator::Cancel() noexcept
    {
        m_canceled = true;

        try
        {
            std::scoped_lock guard{ m_lock };

            if (m_connection != nullptr)
            {
                m_connection->Abort();
            }
        }
        catch (...)
        {
        }
    }

    // ------------------------------------------------------------------------ sending

    _Use_decl_annotations_
    SendResult SyncSender::Send(
        std::wstring const& host,
        uint16_t port,
        midiapp::DeviceIdentity const& identity,
        std::wstring const& ownName,
        midiapp::DeviceKeyHash const& pairedKey,
        SyncSource const& source,
        SendProgress const& progress) noexcept
    {
        SendResult result{};

        try
        {
            auto const canceled = [this, &result]()
                {
                    if (m_canceled)
                    {
                        result.Outcome = SendOutcome::Canceled;
                    }

                    return result;
                };

            HRESULT error{ E_FAIL };
            std::shared_ptr<DeviceLinkConnection> connection{
                DeviceLinkConnection::Connect(host, port, identity, ConnectTimeoutMilliseconds, error) };

            result.Error = error;

            if (connection == nullptr)
            {
                result.Outcome = SendOutcome::CouldNotConnect;
                return canceled();
            }

            {
                std::scoped_lock guard{ m_lock };
                m_connection = connection;
            }

            if (m_canceled)
            {
                connection->Abort();
                return canceled();
            }

            // The handshake proved which key the other side holds. Nothing is sent to any other.
            if (!midiapp::SameDeviceKey(connection->PeerKey(), pairedKey))
            {
                result.Outcome = SendOutcome::NotThePairedDevice;
                connection->Close();
                return result;
            }

            connection->SetTimeout(TransferTimeoutMilliseconds);

            midiapp::DeviceLinkHello hello{};
            midiapp::DeviceLinkRefusal refusal{};

            if (!midiapp::WriteDeviceLinkHello(*connection, OwnHello(ownName, midiapp::DeviceLinkPurpose::Use)) ||
                !midiapp::ReadDeviceLinkHello(*connection, hello, refusal))
            {
                result.Outcome = refusal == midiapp::DeviceLinkRefusal::NotPaired ? SendOutcome::NotPaired :
                    refusal != midiapp::DeviceLinkRefusal::None ? SendOutcome::Refused : SendOutcome::Failed;
                return canceled();
            }

            if (!IsOurs(hello))
            {
                result.Outcome = SendOutcome::Refused;
                return result;
            }

            {
                DeviceLinkWriter offer{};
                offer.Text(source.Offer.LayoutId.substr(0, MaximumLayoutIdLength));
                offer.Text(source.Offer.LayoutName.substr(0, MaximumOfferNameLength));
                offer.Bytes(source.Offer.ManifestBytes);
                offer.U8(static_cast<uint8_t>(std::min(source.Offer.Signatures.size(), MaximumOfferSignatures)));

                if (!connection->WriteFrame(SyncFrameType::Offer, offer.Data()))
                {
                    return canceled();
                }

                for (size_t i = 0; i < source.Offer.Signatures.size() && i < MaximumOfferSignatures; ++i)
                {
                    DeviceLinkWriter signature{};
                    signature.Text(source.Offer.Signatures[i].EntryName);
                    signature.Bytes(source.Offer.Signatures[i].Bytes);

                    if (!connection->WriteFrame(SyncFrameType::Signature, signature.Data()))
                    {
                        return canceled();
                    }
                }
            }

            // Either what to send, or the other side's reason it won't take this.
            DeviceLinkFrame frame{};

            auto const needOrResult = [](uint16_t type) -> uint32_t
                {
                    return type == SyncFrameType::Need ? NeedCap : type == SyncFrameType::Result ? ResultBytes : 0u;
                };

            if (connection->ReadFrame(needOrResult, frame) != DeviceLinkFrameStatus::Read)
            {
                return canceled();
            }

            if (frame.Type == SyncFrameType::Result)
            {
                DeviceLinkReader reader{ frame.Payload };
                reader.U8();
                result.ReceiverProblem = ProblemFrom(reader.U32());
                result.Outcome = SendOutcome::PackProblem;
                return result;
            }

            std::vector<std::wstring> wanted{};

            {
                DeviceLinkReader reader{ frame.Payload };
                auto const count = reader.U32();

                if (count > midiapp::MaximumContentPackEntries)
                {
                    return result;
                }

                std::set<std::wstring> seen{};

                for (uint32_t i = 0; i < count; ++i)
                {
                    midiapp::DeviceKeyHash hash{};

                    if (!reader.Raw(hash.data(), hash.size()))
                    {
                        return result;
                    }

                    auto hex = midiapp::DeviceKeyHashToHex(hash);

                    // Only files in this pack, each once.
                    if (!source.FilesBySha256.contains(hex) || !seen.insert(hex).second)
                    {
                        return result;
                    }

                    wanted.push_back(std::move(hex));
                }

                if (!reader.Ok())
                {
                    return result;
                }
            }

            uint64_t total{ 0 };

            for (auto const& hex : wanted)
            {
                total += source.FilesBySha256.at(hex).size();
            }

            result.FilesSkipped = static_cast<uint32_t>(source.FilesBySha256.size() - wanted.size());

            if (progress)
            {
                progress(0, total);
            }

            for (auto const& hex : wanted)
            {
                auto const& bytes = source.FilesBySha256.at(hex);
                auto const hash = HashBytes(hex);

                DeviceLinkWriter start{};
                start.Raw(*hash);
                start.U64(bytes.size());

                if (!connection->WriteFrame(SyncFrameType::FileStart, start.Data()))
                {
                    return canceled();
                }

                for (size_t offset = 0; offset < bytes.size(); offset += ChunkBytes)
                {
                    auto const size = std::min<size_t>(ChunkBytes, bytes.size() - offset);

                    if (!connection->WriteFrame(SyncFrameType::FileChunk, std::span<uint8_t const>{ bytes.data() + offset, size }))
                    {
                        return canceled();
                    }

                    result.BytesSent += size;

                    if (progress)
                    {
                        progress(result.BytesSent, total);
                    }
                }

                result.FilesSent++;
            }

            if (!connection->WriteFrame(SyncFrameType::Done, {}))
            {
                return canceled();
            }

            connection->SetTimeout(ResultTimeoutMilliseconds);

            if (connection->ReadFrame(Allow(SyncFrameType::Result, ResultBytes), frame) != DeviceLinkFrameStatus::Read)
            {
                return canceled();
            }

            DeviceLinkReader reader{ frame.Payload };
            auto const code = reader.U8();
            auto const problem = ProblemFrom(reader.U32());

            if (!reader.Ok())
            {
                return result;
            }

            connection->Close();

            if (code == static_cast<uint8_t>(ResultCode::Received))
            {
                result.Outcome = SendOutcome::Sent;
            }
            else
            {
                result.Outcome = SendOutcome::PackProblem;
                result.ReceiverProblem = problem;
            }

            return result;
        }
        catch (...)
        {
            result.Outcome = m_canceled ? SendOutcome::Canceled : SendOutcome::Failed;
            return result;
        }
    }

    void SyncSender::Cancel() noexcept
    {
        m_canceled = true;

        try
        {
            std::scoped_lock guard{ m_lock };

            if (m_connection != nullptr)
            {
                m_connection->Abort();
            }
        }
        catch (...)
        {
        }
    }

    // ------------------------------------------------------------------------ receiving

    SyncReceiver::~SyncReceiver()
    {
        Stop();
    }

    _Use_decl_annotations_
    HRESULT SyncReceiver::Start(SyncReceiverOptions options) noexcept
    {
        try
        {
            Stop();

            if (options.Identity == nullptr || options.PairedListPath.empty())
            {
                return E_INVALIDARG;
            }

            m_options = std::move(options);

            if (auto const started = m_listener.Start(0, m_options.LoopbackOnly); FAILED(started))
            {
                return started;
            }

            m_worker = std::jthread([this](std::stop_token stopToken) { Work(stopToken); });

            return S_OK;
        }
        catch (...)
        {
            m_listener.Stop();
            return E_OUTOFMEMORY;
        }
    }

    void SyncReceiver::Stop() noexcept
    {
        if (m_worker.joinable())
        {
            m_worker.request_stop();

            try
            {
                std::scoped_lock guard{ m_lock };

                if (m_current != nullptr)
                {
                    m_current->Abort();
                }
            }
            catch (...)
            {
            }

            m_worker.join();
        }

        m_worker = std::jthread{};

        // Only after the thread is done with it.
        m_listener.Stop();
    }

    _Use_decl_annotations_
    void SyncReceiver::Work(std::stop_token stopToken) noexcept
    {
        std::stop_callback abortOnStop{ stopToken, [this]() noexcept
            {
                try
                {
                    std::scoped_lock guard{ m_lock };

                    if (m_current != nullptr)
                    {
                        m_current->Abort();
                    }
                }
                catch (...)
                {
                }
            } };

        while (!stopToken.stop_requested())
        {
            auto const socket = m_listener.Accept(250);

            if (socket != 0)
            {
                Handle(socket, stopToken);
            }
        }
    }

    _Use_decl_annotations_
    void SyncReceiver::Handle(uintptr_t socket, std::stop_token stopToken) noexcept
    {
        try
        {
            HRESULT error{ E_FAIL };
            std::shared_ptr<DeviceLinkConnection> connection{
                DeviceLinkConnection::Accept(socket, *m_options.Identity, ConnectTimeoutMilliseconds, error) };

            if (connection == nullptr)
            {
                return;
            }

            {
                std::scoped_lock guard{ m_lock };
                m_current = connection;
            }

            // Stop may have come between the accept and the line above.
            if (stopToken.stop_requested())
            {
                connection->Abort();
            }
            else
            {
                midiapp::DeviceLinkHello hello{};
                midiapp::DeviceLinkRefusal refusal{};

                if (midiapp::ReadDeviceLinkHello(*connection, hello, refusal))
                {
                    if (!IsOurs(hello))
                    {
                        midiapp::WriteDeviceLinkRefusal(*connection, midiapp::DeviceLinkRefusal::WrongApp);
                    }
                    else if (hello.Purpose == midiapp::DeviceLinkPurpose::Pair)
                    {
                        Pair(*connection, hello, stopToken);
                    }
                    else
                    {
                        Receive(*connection, hello);
                    }
                }
            }

            connection->Close();

            std::scoped_lock guard{ m_lock };
            m_current = nullptr;
        }
        catch (...)
        {
            try
            {
                std::scoped_lock guard{ m_lock };
                m_current = nullptr;
            }
            catch (...)
            {
            }
        }
    }

    _Use_decl_annotations_
    void SyncReceiver::Pair(DeviceLinkConnection& connection, midiapp::DeviceLinkHello const& hello, std::stop_token stopToken) noexcept
    {
        try
        {
            // Only while the person here has pairing open.
            if (!m_options.IsPairingOpen || !m_options.IsPairingOpen())
            {
                midiapp::WriteDeviceLinkRefusal(connection, midiapp::DeviceLinkRefusal::NotPairing);
                return;
            }

            if (!midiapp::WriteDeviceLinkHello(connection, OwnHello(m_options.OwnName, midiapp::DeviceLinkPurpose::Pair)))
            {
                return;
            }

            auto const code = midiapp::PairAsResponder(connection, m_options.Identity->KeyHash());

            if (!code)
            {
                return;
            }

            auto pending = std::make_shared<PendingPairing>();
            pending->PeerName = hello.DeviceName;
            pending->PeerKey = connection.PeerKey();
            pending->Code = midiapp::FormatPairingCode(*code);

            if (m_options.PairingAsked)
            {
                m_options.PairingAsked(pending);
            }

            auto const answer = pending->WaitForAnswer(stopToken, PairingAnswerTimeoutMilliseconds);

            connection.SetTimeout(PairingAnswerTimeoutMilliseconds + ConnectTimeoutMilliseconds);

            bool otherMatched{ false };
            auto const matched = answer.value_or(false);
            auto const exchanged = midiapp::ExchangePairingAnswers(connection, matched, otherMatched);

            auto paired = false;

            if (exchanged && matched && otherMatched && !stopToken.stop_requested())
            {
                midiapp::PairedDeviceList list{ m_options.PairedListPath };
                paired = list.Remember(pending->PeerKey, pending->PeerName);
            }

            if (m_options.PairingFinished)
            {
                m_options.PairingFinished(pending, paired);
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SyncReceiver::Receive(DeviceLinkConnection& connection, midiapp::DeviceLinkHello const& hello) noexcept
    {
        std::wstring layoutName{};

        auto const fail = [this, &connection, &hello, &layoutName](ContentPackProblem problem) noexcept
            {
                WriteResult(connection, problem);

                if (m_options.SyncFailed)
                {
                    try
                    {
                        m_options.SyncFailed(hello.DeviceName, layoutName, problem);
                    }
                    catch (...)
                    {
                    }
                }
            };

        try
        {
            midiapp::PairedDeviceList paired{ m_options.PairedListPath };

            // A PC this one doesn't know gets nothing but the refusal.
            if (!paired.Find(connection.PeerKey()).has_value())
            {
                midiapp::WriteDeviceLinkRefusal(connection, midiapp::DeviceLinkRefusal::NotPaired);
                return;
            }

            if (!midiapp::WriteDeviceLinkHello(connection, OwnHello(m_options.OwnName, midiapp::DeviceLinkPurpose::Use)))
            {
                return;
            }

            paired.Touch(connection.PeerKey(), hello.DeviceName);

            connection.SetTimeout(TransferTimeoutMilliseconds);

            ReceivedSync received{};
            received.Sender = connection.PeerKey();
            received.SenderName = hello.DeviceName;

            DeviceLinkFrame frame{};

            if (connection.ReadFrame(Allow(SyncFrameType::Offer, OfferCap), frame) != DeviceLinkFrameStatus::Read)
            {
                return;
            }

            size_t signatureCount{ 0 };

            {
                DeviceLinkReader reader{ frame.Payload };
                received.Offer.LayoutId = reader.Text(MaximumLayoutIdLength);
                received.Offer.LayoutName = midiapp::CleanDeviceLinkName(reader.Text(MaximumOfferNameLength));
                received.Offer.ManifestBytes = reader.Bytes(MaximumManifestBytes);
                signatureCount = reader.U8();

                if (!reader.Ok() || signatureCount > MaximumOfferSignatures)
                {
                    fail(ContentPackProblem::BadManifest);
                    return;
                }
            }

            layoutName = received.Offer.LayoutName;

            for (size_t i = 0; i < signatureCount; ++i)
            {
                if (connection.ReadFrame(Allow(SyncFrameType::Signature, SignatureCap), frame) != DeviceLinkFrameStatus::Read)
                {
                    return;
                }

                DeviceLinkReader reader{ frame.Payload };

                SyncSignature signature{};
                signature.EntryName = reader.Text(MaximumSignatureNameLength);
                signature.Bytes = reader.Bytes(midiapp::MaximumContentPackSignatureBytes);

                if (!reader.Ok() || !IsAllowedSyncSignature(signature))
                {
                    fail(ContentPackProblem::BadName);
                    return;
                }

                received.Offer.Signatures.push_back(std::move(signature));
            }

            if (m_options.SyncStarted)
            {
                m_options.SyncStarted(received.SenderName, layoutName);
            }

            // First the sizes, so only files that could match are hashed.
            auto const shape = ComputeSyncNeed(received.Offer.ManifestBytes, nullptr);

            if (shape.Problem != ContentPackProblem::None)
            {
                fail(shape.Problem);
                return;
            }

            std::vector<uint64_t> sizes{};

            for (auto const& [hash, size] : shape.Sizes)
            {
                sizes.push_back(size);
            }

            FileHashIndex index{ m_options.FileIndexPath };
            index.Refresh(m_options.Folders, &sizes);
            index.Save();

            auto const have = [&index](std::wstring const& sha256) { return index.Find(sha256); };

            auto const need = ComputeSyncNeed(received.Offer.ManifestBytes, have);

            if (need.Problem != ContentPackProblem::None)
            {
                fail(need.Problem);
                return;
            }

            {
                DeviceLinkWriter writer{};
                writer.U32(static_cast<uint32_t>(need.Needed.size()));

                for (auto const& hex : need.Needed)
                {
                    writer.Raw(*HashBytes(hex));
                }

                if (!connection.WriteFrame(SyncFrameType::Need, writer.Data()))
                {
                    return;
                }
            }

            std::set<std::wstring> outstanding{ need.Needed.begin(), need.Needed.end() };
            std::map<std::wstring, std::vector<uint8_t>> files{};

            auto const between = [](uint16_t type) -> uint32_t
                {
                    return type == SyncFrameType::FileStart ? FileStartBytes : type == SyncFrameType::Done ? EmptyCap : 0u;
                };

            for (;;)
            {
                if (connection.ReadFrame(between, frame) != DeviceLinkFrameStatus::Read)
                {
                    return;
                }

                if (frame.Type == SyncFrameType::Done)
                {
                    if (!frame.Payload.empty() || !outstanding.empty())
                    {
                        fail(ContentPackProblem::MissingFile);
                        return;
                    }

                    break;
                }

                midiapp::DeviceKeyHash hash{};
                DeviceLinkReader reader{ frame.Payload };
                reader.Raw(hash.data(), hash.size());
                auto const size = reader.U64();

                auto const hex = midiapp::DeviceKeyHashToHex(hash);

                // Only a file asked for, once, at the size the manifest gives it.
                if (!reader.Ok() || outstanding.erase(hex) == 0 || need.Sizes.at(hex) != size)
                {
                    fail(ContentPackProblem::UnlistedFile);
                    return;
                }

                std::vector<uint8_t> bytes{};
                bytes.reserve(static_cast<size_t>(size));

                while (bytes.size() < size)
                {
                    auto const remaining = size - bytes.size();
                    auto const cap = static_cast<uint32_t>(std::min<uint64_t>(remaining, ChunkBytes));

                    if (connection.ReadFrame(Allow(SyncFrameType::FileChunk, cap), frame) != DeviceLinkFrameStatus::Read ||
                        frame.Payload.empty())
                    {
                        return;
                    }

                    bytes.insert(bytes.end(), frame.Payload.begin(), frame.Payload.end());
                }

                // Caught here rather than at the end, so a wrong file stops the transfer early.
                if (midiapp::Sha256Hex(bytes.data(), bytes.size()) != hex)
                {
                    fail(ContentPackProblem::HashMismatch);
                    return;
                }

                files.emplace(hex, std::move(bytes));
            }

            received.FilesReceived = static_cast<uint32_t>(files.size());
            received.FilesReused = static_cast<uint32_t>(need.Sizes.size() - files.size());

            auto const assembled = AssembleSyncPack(received.Offer, files, have);

            files.clear();

            if (assembled.Problem != ContentPackProblem::None)
            {
                fail(assembled.Problem);
                return;
            }

            // The same checks a pack file gets before its import dialog.
            received.Pack = midiapp::OpenContentPack(assembled.Pack, midiapp::ContentPackKind::GlassLayout, m_options.Verify);

            if (!received.Pack.Succeeded())
            {
                fail(received.Pack.Problem);
                return;
            }

            WriteResult(connection, ContentPackProblem::None);

            if (m_options.SyncReceived)
            {
                m_options.SyncReceived(std::move(received));
            }
        }
        catch (...)
        {
            fail(ContentPackProblem::Damaged);
        }
    }
}
