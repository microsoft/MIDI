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

#include "DeviceLinkPairing.h"

#include <algorithm>
#include <format>
#include <vector>

namespace midiapp
{
    namespace
    {
        // Different labels keep a commitment from ever being mistaken for a code, or for
        // anything another protocol hashes.
        constexpr char CommitmentLabel[] = "Windows MIDI Services device link pairing commitment v1";
        constexpr char CodeLabel[] = "Windows MIDI Services device link pairing code v1";

        // Generous for a short greeting.
        constexpr uint32_t MaximumHelloBytes = 1024;

        template <size_t Size>
        void Append(_Inout_ std::vector<uint8_t>& data, _In_ std::array<uint8_t, Size> const& value)
        {
            data.insert(data.end(), value.begin(), value.end());
        }

        void AppendLabel(_Inout_ std::vector<uint8_t>& data, _In_z_ char const* label)
        {
            auto const length = std::char_traits<char>::length(label);
            data.insert(data.end(), reinterpret_cast<uint8_t const*>(label), reinterpret_cast<uint8_t const*>(label) + length);

            // Ends the label, so no label is a prefix of what follows another.
            data.push_back(0);
        }

        // A frame of exactly 32 bytes, of one type.
        bool ReadFixed(
            _Inout_ DeviceLinkConnection& connection,
            _In_ uint16_t type,
            _Out_ std::array<uint8_t, 32>& value) noexcept
        {
            value = {};

            DeviceLinkFrame frame{};

            auto const status = connection.ReadFrame(
                [type](uint16_t candidate) -> uint32_t { return candidate == type ? 32u : 0u; },
                frame);

            if (status != DeviceLinkFrameStatus::Read || frame.Payload.size() != value.size())
            {
                return false;
            }

            std::copy(frame.Payload.begin(), frame.Payload.end(), value.begin());

            return true;
        }
    }

    _Use_decl_annotations_
    std::wstring CleanDeviceLinkName(std::wstring_view name) noexcept
    {
        try
        {
            std::wstring spaced{};
            spaced.reserve(std::min<size_t>(name.size(), 1024));

            // Tabs and line breaks become spaces: a name is one line. Very long input is cut first,
            // so cleaning it costs little.
            for (auto const unit : name.substr(0, 1024))
            {
                spaced += (unit < L' ' || (unit >= 0x7F && unit < 0xA0)) ? L' ' : unit;
            }

            return SanitizeProvenanceText(spaced, MaximumDeviceLinkNameLength);
        }
        catch (...)
        {
            return {};
        }
    }

    std::wstring ThisDeviceLinkName() noexcept
    {
        try
        {
            wchar_t name[256]{};
            DWORD size{ static_cast<DWORD>(std::size(name)) };

            if (::GetComputerNameExW(ComputerNameDnsHostname, name, &size))
            {
                return CleanDeviceLinkName(name);
            }

            return {};
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    bool NewDeviceLinkNonce(DeviceLinkNonce& nonce) noexcept
    {
        return DeviceLinkRandom(nonce.data(), nonce.size());
    }

    _Use_decl_annotations_
    DeviceLinkCommitment PairingCommitment(DeviceKeyHash const& initiatorKey, DeviceLinkNonce const& initiatorNonce) noexcept
    {
        DeviceLinkCommitment commitment{};

        try
        {
            std::vector<uint8_t> data{};
            AppendLabel(data, CommitmentLabel);
            Append(data, initiatorKey);
            Append(data, initiatorNonce);

            DeviceLinkSha256(data, commitment);
        }
        catch (...)
        {
            commitment = {};
        }

        return commitment;
    }

    _Use_decl_annotations_
    bool PairingRevealMatches(
        DeviceLinkCommitment const& commitment,
        DeviceKeyHash const& initiatorKey,
        DeviceLinkNonce const& initiatorNonce) noexcept
    {
        auto const expected = PairingCommitment(initiatorKey, initiatorNonce);

        // An all-zero commitment means the hash failed; never let that match.
        DeviceKeyHash const zero{};

        return !SameDeviceKey(expected, zero) && SameDeviceKey(expected, commitment);
    }

    _Use_decl_annotations_
    uint32_t PairingCode(
        DeviceKeyHash const& initiatorKey,
        DeviceKeyHash const& responderKey,
        DeviceLinkNonce const& initiatorNonce,
        DeviceLinkNonce const& responderNonce) noexcept
    {
        try
        {
            std::vector<uint8_t> data{};
            AppendLabel(data, CodeLabel);
            Append(data, initiatorKey);
            Append(data, responderKey);
            Append(data, initiatorNonce);
            Append(data, responderNonce);

            std::array<uint8_t, 32> hash{};

            if (!DeviceLinkSha256(data, hash))
            {
                return 0;
            }

            // 64 bits reduced to a million values: the bias is far below one in a billion.
            uint64_t value{ 0 };

            for (size_t i = 0; i < 8; ++i)
            {
                value = (value << 8) | hash[i];
            }

            return static_cast<uint32_t>(value % 1'000'000);
        }
        catch (...)
        {
            return 0;
        }
    }

    _Use_decl_annotations_
    std::wstring FormatPairingCode(uint32_t code)
    {
        code %= 1'000'000;

        return std::format(L"{:03} {:03}", code / 1000, code % 1000);
    }

    _Use_decl_annotations_
    bool WriteDeviceLinkHello(DeviceLinkConnection& connection, DeviceLinkHello const& hello) noexcept
    {
        try
        {
            DeviceLinkWriter writer{};
            writer.Text(hello.App.substr(0, MaximumDeviceLinkAppLength));
            writer.U16(hello.Version);
            writer.U8(static_cast<uint8_t>(hello.Purpose));
            writer.Text(CleanDeviceLinkName(hello.DeviceName));

            return connection.WriteFrame(DeviceLinkFrameType::Hello, writer.Data());
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    bool ReadDeviceLinkHello(DeviceLinkConnection& connection, DeviceLinkHello& hello, DeviceLinkRefusal& refusal) noexcept
    {
        hello = DeviceLinkHello{};
        refusal = DeviceLinkRefusal::None;

        try
        {
            DeviceLinkFrame frame{};

            auto const status = connection.ReadFrame(
                [](uint16_t type) -> uint32_t
                {
                    return type == DeviceLinkFrameType::Hello ? MaximumHelloBytes :
                        type == DeviceLinkFrameType::Refused ? 1u : 0u;
                },
                frame);

            if (status != DeviceLinkFrameStatus::Read)
            {
                return false;
            }

            DeviceLinkReader reader{ frame.Payload };

            if (frame.Type == DeviceLinkFrameType::Refused)
            {
                auto const reason = reader.U8();
                refusal = reader.Ok() && reason >= 1 && reason <= 5 ? static_cast<DeviceLinkRefusal>(reason) : DeviceLinkRefusal::WrongApp;
                return false;
            }

            hello.App = reader.Text(MaximumDeviceLinkAppLength);
            hello.Version = reader.U16();

            auto const purpose = reader.U8();
            hello.DeviceName = reader.Text(MaximumDeviceLinkNameLength);

            if (!reader.Ok() ||
                (purpose != static_cast<uint8_t>(DeviceLinkPurpose::Pair) && purpose != static_cast<uint8_t>(DeviceLinkPurpose::Use)))
            {
                hello = DeviceLinkHello{};
                return false;
            }

            hello.Purpose = static_cast<DeviceLinkPurpose>(purpose);
            hello.DeviceName = CleanDeviceLinkName(hello.DeviceName);

            return true;
        }
        catch (...)
        {
            hello = DeviceLinkHello{};
            return false;
        }
    }

    _Use_decl_annotations_
    bool WriteDeviceLinkRefusal(DeviceLinkConnection& connection, DeviceLinkRefusal refusal) noexcept
    {
        uint8_t const reason = static_cast<uint8_t>(refusal);
        return connection.WriteFrame(DeviceLinkFrameType::Refused, std::span<uint8_t const>{ &reason, 1 });
    }

    _Use_decl_annotations_
    std::optional<uint32_t> PairAsInitiator(DeviceLinkConnection& connection, DeviceKeyHash const& ownKey) noexcept
    {
        DeviceLinkNonce ownNonce{};

        if (!NewDeviceLinkNonce(ownNonce))
        {
            return std::nullopt;
        }

        auto const commitment = PairingCommitment(ownKey, ownNonce);

        if (!connection.WriteFrame(DeviceLinkFrameType::PairCommit, commitment))
        {
            return std::nullopt;
        }

        DeviceLinkNonce otherNonce{};

        if (!ReadFixed(connection, DeviceLinkFrameType::PairNonce, otherNonce))
        {
            return std::nullopt;
        }

        if (!connection.WriteFrame(DeviceLinkFrameType::PairReveal, ownNonce))
        {
            return std::nullopt;
        }

        return PairingCode(ownKey, connection.PeerKey(), ownNonce, otherNonce);
    }

    _Use_decl_annotations_
    std::optional<uint32_t> PairAsResponder(DeviceLinkConnection& connection, DeviceKeyHash const& ownKey) noexcept
    {
        DeviceLinkCommitment commitment{};

        if (!ReadFixed(connection, DeviceLinkFrameType::PairCommit, commitment))
        {
            return std::nullopt;
        }

        // Chosen only after the commitment arrived, and never reused.
        DeviceLinkNonce ownNonce{};

        if (!NewDeviceLinkNonce(ownNonce) || !connection.WriteFrame(DeviceLinkFrameType::PairNonce, ownNonce))
        {
            return std::nullopt;
        }

        DeviceLinkNonce otherNonce{};

        if (!ReadFixed(connection, DeviceLinkFrameType::PairReveal, otherNonce))
        {
            return std::nullopt;
        }

        if (!PairingRevealMatches(commitment, connection.PeerKey(), otherNonce))
        {
            return std::nullopt;
        }

        return PairingCode(connection.PeerKey(), ownKey, otherNonce, ownNonce);
    }

    _Use_decl_annotations_
    bool ExchangePairingAnswers(DeviceLinkConnection& connection, bool matched, bool& otherMatched) noexcept
    {
        otherMatched = false;

        uint8_t const answer = matched ? 1 : 0;

        if (!connection.WriteFrame(DeviceLinkFrameType::PairConfirm, std::span<uint8_t const>{ &answer, 1 }))
        {
            return false;
        }

        DeviceLinkFrame frame{};

        auto const status = connection.ReadFrame(
            [](uint16_t type) -> uint32_t { return type == DeviceLinkFrameType::PairConfirm ? 1u : 0u; },
            frame);

        if (status != DeviceLinkFrameStatus::Read || frame.Payload.size() != 1)
        {
            return false;
        }

        otherMatched = frame.Payload[0] == 1;

        return true;
    }
}
