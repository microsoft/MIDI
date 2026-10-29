// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "DeviceLinkTests.h"

// Before windows.h, so the JSON projection is declared without the GetObject macro.
#include "ContentProvenance.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>

#include "DeviceLinkChannel.h"
#include "DeviceLinkFrames.h"
#include "DeviceLinkIdentity.h"
#include "DeviceLinkPairing.h"
#include "PairedDevices.h"
#include "DeviceLinkTestKeys.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    using midiapp::DeviceLinkFrame;
    using midiapp::DeviceLinkFrameStatus;
    using midiapp::DeviceKeyHash;
    using midiapp::DeviceLinkNonce;

    std::unique_ptr<DeviceLinkTestKey> g_first{};
    std::unique_ptr<DeviceLinkTestKey> g_second{};

    std::filesystem::path g_folder{};

    // Bytes in memory, read the way a connection reads them.
    struct MemorySource
    {
        std::vector<uint8_t> Data{};
        size_t Offset{ 0 };

        midiapp::DeviceLinkReadExact Reader()
        {
            return [this](uint8_t* destination, size_t size, bool& closedCleanly)
                {
                    closedCleanly = false;

                    if (Data.size() - Offset < size)
                    {
                        closedCleanly = Offset == Data.size();
                        Offset = Data.size();
                        return false;
                    }

                    std::memcpy(destination, Data.data() + Offset, size);
                    Offset += size;

                    return true;
                };
        }
    };

    midiapp::DeviceLinkFrameCap Allow(uint16_t type, uint32_t cap)
    {
        return [type, cap](uint16_t candidate) -> uint32_t { return candidate == type ? cap : 0u; };
    }

    template <size_t Size>
    std::array<uint8_t, Size> RandomBytes()
    {
        std::array<uint8_t, Size> value{};
        VERIFY_IS_TRUE(midiapp::DeviceLinkRandom(value.data(), value.size()));
        return value;
    }
}

bool DeviceLinkTests::ClassSetup()
{
    g_first = std::make_unique<DeviceLinkTestKey>();
    g_second = std::make_unique<DeviceLinkTestKey>();

    if (g_first->Identity == nullptr || g_second->Identity == nullptr)
    {
        Log::Error(L"Could not make the test keys.");
        return false;
    }

    g_folder = std::filesystem::temp_directory_path() / (L"MidiDeviceLinkTests-" + std::to_wstring(::GetCurrentProcessId()));
    std::error_code ignored{};
    std::filesystem::create_directories(g_folder, ignored);

    return true;
}

bool DeviceLinkTests::ClassCleanup()
{
    g_first.reset();
    g_second.reset();

    std::error_code ignored{};
    std::filesystem::remove_all(g_folder, ignored);

    return true;
}

// ---------------------------------------------------------------------------- frames

void DeviceLinkTests::AFrameRoundTrips()
{
    std::vector<uint8_t> const payload{ 1, 2, 3, 4, 5 };

    MemorySource source{};
    source.Data = midiapp::EncodeDeviceLinkFrame(0x0123, payload);

    VERIFY_ARE_EQUAL(midiapp::DeviceLinkFrameHeaderBytes + payload.size(), source.Data.size());

    DeviceLinkFrame frame{};
    VERIFY_IS_TRUE(midiapp::ReadDeviceLinkFrame(source.Reader(), Allow(0x0123, 16), frame) == DeviceLinkFrameStatus::Read);
    VERIFY_ARE_EQUAL(static_cast<uint16_t>(0x0123), frame.Type);
    VERIFY_IS_TRUE(frame.Payload == payload);
}

void DeviceLinkTests::AnOversizedFrameIsRefusedBeforeAllocating()
{
    // Says 4 GB is coming, and sends nothing after the header.
    MemorySource source{};
    source.Data = { 0xFF, 0xFF, 0xFF, 0xFF, 0x23, 0x01 };

    DeviceLinkFrame frame{};
    VERIFY_IS_TRUE(midiapp::ReadDeviceLinkFrame(source.Reader(), Allow(0x0123, 1024), frame) == DeviceLinkFrameStatus::TooLarge);
    VERIFY_IS_TRUE(frame.Payload.empty());

    // One byte over what the type allows.
    MemorySource over{};
    over.Data = midiapp::EncodeDeviceLinkFrame(0x0123, std::vector<uint8_t>(17, 0));

    VERIFY_IS_TRUE(midiapp::ReadDeviceLinkFrame(over.Reader(), Allow(0x0123, 16), frame) == DeviceLinkFrameStatus::TooLarge);

    // No frame may be written past the absolute cap either.
    std::vector<uint8_t> const huge(midiapp::MaximumDeviceLinkFrameBytes + 1, 0);
    VERIFY_IS_TRUE(midiapp::EncodeDeviceLinkFrame(0x0123, huge).empty());
}

void DeviceLinkTests::ATruncatedFrameIsNamedAsSuch()
{
    auto encoded = midiapp::EncodeDeviceLinkFrame(0x0123, std::vector<uint8_t>(10, 7));

    // Half a header.
    MemorySource header{};
    header.Data.assign(encoded.begin(), encoded.begin() + 3);

    DeviceLinkFrame frame{};
    VERIFY_IS_TRUE(midiapp::ReadDeviceLinkFrame(header.Reader(), Allow(0x0123, 16), frame) == DeviceLinkFrameStatus::Truncated);

    // A whole header and part of the payload.
    MemorySource body{};
    body.Data.assign(encoded.begin(), encoded.end() - 1);

    VERIFY_IS_TRUE(midiapp::ReadDeviceLinkFrame(body.Reader(), Allow(0x0123, 16), frame) == DeviceLinkFrameStatus::Truncated);
    VERIFY_IS_TRUE(frame.Payload.empty());
}

void DeviceLinkTests::AnUnexpectedTypeIsRefused()
{
    MemorySource source{};
    source.Data = midiapp::EncodeDeviceLinkFrame(0x0999, std::vector<uint8_t>(2, 0));

    DeviceLinkFrame frame{};
    VERIFY_IS_TRUE(midiapp::ReadDeviceLinkFrame(source.Reader(), Allow(0x0123, 16), frame) == DeviceLinkFrameStatus::UnexpectedType);
}

void DeviceLinkTests::ACleanCloseIsNotTruncation()
{
    MemorySource source{};

    DeviceLinkFrame frame{};
    VERIFY_IS_TRUE(midiapp::ReadDeviceLinkFrame(source.Reader(), Allow(0x0123, 16), frame) == DeviceLinkFrameStatus::Closed);
}

void DeviceLinkTests::TheReaderStopsAtTheEnd()
{
    midiapp::DeviceLinkWriter writer{};
    writer.U8(1);
    writer.U16(0x0203);
    writer.U32(0x04050607);
    writer.U64(0x08090A0B0C0D0E0Full);
    writer.Text(L"Studio PC");

    auto const data = writer.Take();

    {
        midiapp::DeviceLinkReader reader{ data };
        VERIFY_ARE_EQUAL(static_cast<uint8_t>(1), reader.U8());
        VERIFY_ARE_EQUAL(static_cast<uint16_t>(0x0203), reader.U16());
        VERIFY_ARE_EQUAL(static_cast<uint32_t>(0x04050607), reader.U32());
        VERIFY_ARE_EQUAL(static_cast<uint64_t>(0x08090A0B0C0D0E0Full), reader.U64());
        VERIFY_ARE_EQUAL(std::wstring{ L"Studio PC" }, reader.Text(64));
        VERIFY_IS_TRUE(reader.Ok());

        // Past the end fails, and stays failed.
        reader.U8();
        VERIFY_IS_TRUE(reader.Failed());
    }

    {
        // A length longer than what's left.
        midiapp::DeviceLinkWriter lying{};
        lying.U32(1000);
        lying.U8(1);

        auto const bytes = lying.Take();
        midiapp::DeviceLinkReader reader{ bytes };

        VERIFY_IS_TRUE(reader.Bytes(4096).empty());
        VERIFY_IS_TRUE(reader.Failed());
    }

    {
        // A length over the cap, even though the bytes are there.
        midiapp::DeviceLinkWriter big{};
        big.Bytes(std::vector<uint8_t>(100, 1));

        auto const bytes = big.Take();
        midiapp::DeviceLinkReader reader{ bytes };

        VERIFY_IS_TRUE(reader.Bytes(99).empty());
        VERIFY_IS_TRUE(reader.Failed());
    }

    {
        // Something left over isn't Ok either.
        midiapp::DeviceLinkReader reader{ data };
        reader.U8();
        VERIFY_IS_FALSE(reader.Ok());
        VERIFY_IS_FALSE(reader.Failed());
    }
}

void DeviceLinkTests::TheReaderRefusesBadText()
{
    // An overlong encoding of '/', which a careless decoder turns into a path separator.
    midiapp::DeviceLinkWriter writer{};
    writer.Bytes(std::vector<uint8_t>{ 0xC0, 0xAF });

    auto const data = writer.Take();
    midiapp::DeviceLinkReader reader{ data };

    VERIFY_IS_TRUE(reader.Text(64).empty());
    VERIFY_IS_TRUE(reader.Failed());

    // Too many characters.
    midiapp::DeviceLinkWriter longer{};
    longer.Text(L"abcdef");

    auto const longData = longer.Take();
    midiapp::DeviceLinkReader longReader{ longData };

    VERIFY_IS_TRUE(longReader.Text(5).empty());
    VERIFY_IS_TRUE(longReader.Failed());
}

// ---------------------------------------------------------------------------- pairing

void DeviceLinkTests::BothSidesGetTheSameCode()
{
    auto const initiator = RandomBytes<32>();
    auto const responder = RandomBytes<32>();
    auto const initiatorNonce = RandomBytes<32>();
    auto const responderNonce = RandomBytes<32>();

    auto const commitment = midiapp::PairingCommitment(initiator, initiatorNonce);
    VERIFY_IS_TRUE(midiapp::PairingRevealMatches(commitment, initiator, initiatorNonce));

    // Each side computes it from its own point of view; the order of the inputs is fixed by role.
    auto const seenByInitiator = midiapp::PairingCode(initiator, responder, initiatorNonce, responderNonce);
    auto const seenByResponder = midiapp::PairingCode(initiator, responder, initiatorNonce, responderNonce);

    VERIFY_ARE_EQUAL(seenByInitiator, seenByResponder);

    // Swapping the roles is a different code: the roles are part of what's compared.
    VERIFY_ARE_NOT_EQUAL(seenByInitiator, midiapp::PairingCode(responder, initiator, responderNonce, initiatorNonce));
}

void DeviceLinkTests::APersonInTheMiddleGetsDifferentCodes()
{
    // Fixed values first, so this half of the test never depends on chance.
    DeviceKeyHash initiator{};
    DeviceKeyHash responder{};
    DeviceKeyHash attackerToInitiator{};
    DeviceKeyHash attackerToResponder{};
    DeviceLinkNonce initiatorNonce{};
    DeviceLinkNonce responderNonce{};
    DeviceLinkNonce attackerNonce{};

    initiator.fill(0x11);
    responder.fill(0x22);
    attackerToInitiator.fill(0x33);
    attackerToResponder.fill(0x44);
    initiatorNonce.fill(0x55);
    responderNonce.fill(0x66);
    attackerNonce.fill(0x77);

    // The attacker poses as the responder to the initiator, and as the initiator to the responder.
    auto const initiatorCode = midiapp::PairingCode(initiator, attackerToInitiator, initiatorNonce, attackerNonce);
    auto const responderCode = midiapp::PairingCode(attackerToResponder, responder, attackerNonce, responderNonce);

    VERIFY_ARE_NOT_EQUAL(initiatorCode, responderCode);

    // Then the attacker's best play, many times over. It has to commit its nonce to the responder
    // and send one to the initiator before the initiator reveals its own, so it can't steer the
    // initiator's code. Expect about one match in a million; four in a thousand tries means
    // something is badly wrong.
    int32_t matches{ 0 };

    for (int32_t trial = 0; trial < 1000; ++trial)
    {
        auto const committedNonce = RandomBytes<32>();
        auto const theResponderNonce = RandomBytes<32>();
        auto const nonceForInitiator = RandomBytes<32>();

        // Only now does the initiator reveal.
        auto const theInitiatorNonce = RandomBytes<32>();

        auto const a = midiapp::PairingCode(initiator, attackerToInitiator, theInitiatorNonce, nonceForInitiator);
        auto const b = midiapp::PairingCode(attackerToResponder, responder, committedNonce, theResponderNonce);

        if (a == b)
        {
            ++matches;
        }
    }

    VERIFY_IS_LESS_THAN(matches, 4);
}

void DeviceLinkTests::ANonceThatDoesNotMatchItsCommitmentIsRefused()
{
    auto const initiator = RandomBytes<32>();
    auto const other = RandomBytes<32>();
    auto const nonce = RandomBytes<32>();
    auto changed = nonce;
    changed[31] ^= 0x01;

    auto const commitment = midiapp::PairingCommitment(initiator, nonce);

    VERIFY_IS_TRUE(midiapp::PairingRevealMatches(commitment, initiator, nonce));
    VERIFY_IS_FALSE(midiapp::PairingRevealMatches(commitment, initiator, changed));
    VERIFY_IS_FALSE(midiapp::PairingRevealMatches(commitment, other, nonce));

    // An all-zero commitment never matches, whatever is revealed.
    midiapp::DeviceLinkCommitment const zero{};
    VERIFY_IS_FALSE(midiapp::PairingRevealMatches(zero, initiator, nonce));
}

void DeviceLinkTests::TheCodeIsSixDigits()
{
    VERIFY_ARE_EQUAL(std::wstring{ L"000 007" }, midiapp::FormatPairingCode(7));
    VERIFY_ARE_EQUAL(std::wstring{ L"042 917" }, midiapp::FormatPairingCode(42917));
    VERIFY_ARE_EQUAL(std::wstring{ L"999 999" }, midiapp::FormatPairingCode(999999));

    for (int32_t trial = 0; trial < 200; ++trial)
    {
        auto const code = midiapp::PairingCode(RandomBytes<32>(), RandomBytes<32>(), RandomBytes<32>(), RandomBytes<32>());
        VERIFY_IS_LESS_THAN(code, 1'000'000u);
    }
}

// ---------------------------------------------------------------------------- keys and the paired list

void DeviceLinkTests::AKeyHashRoundTripsThroughHex()
{
    auto const key = g_first->Identity->KeyHash();
    auto const hex = midiapp::DeviceKeyHashToHex(key);

    VERIFY_ARE_EQUAL(static_cast<size_t>(64), hex.size());

    auto const back = midiapp::DeviceKeyHashFromHex(hex);
    VERIFY_IS_TRUE(back.has_value());
    VERIFY_IS_TRUE(midiapp::SameDeviceKey(key, *back));

    VERIFY_IS_FALSE(midiapp::DeviceKeyHashFromHex(hex.substr(1)).has_value());
    VERIFY_IS_FALSE(midiapp::DeviceKeyHashFromHex(std::wstring(64, L'g')).has_value());

    VERIFY_ARE_EQUAL(static_cast<size_t>(19), midiapp::ShortDeviceFingerprint(key).size());

    // The same key opened again is the same key.
    HRESULT error{ E_FAIL };
    auto const again = midiapp::DeviceIdentity::OpenOrCreate(g_first->Name, error);
    VERIFY_SUCCEEDED(error);
    VERIFY_IS_TRUE(midiapp::SameDeviceKey(key, again->KeyHash()));
    VERIFY_IS_FALSE(midiapp::SameDeviceKey(key, g_second->Identity->KeyHash()));
}

void DeviceLinkTests::ThePairedListRoundTrips()
{
    auto const path = (g_folder / L"PairedRoundTrip.json").wstring();
    midiapp::PairedDeviceList list{ path };

    VERIFY_IS_TRUE(list.Load().empty());

    auto const first = g_first->Identity->KeyHash();
    auto const second = g_second->Identity->KeyHash();

    VERIFY_IS_TRUE(list.Remember(first, L"Surface"));
    VERIFY_IS_TRUE(list.Remember(second, L"Studio PC"));

    auto const loaded = list.Load();
    VERIFY_ARE_EQUAL(static_cast<size_t>(2), loaded.size());

    auto const found = list.Find(first);
    VERIFY_IS_TRUE(found.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"Surface" }, found->Name);
    VERIFY_IS_GREATER_THAN(found->PairedAt, 0ll);

    VERIFY_IS_TRUE(list.Touch(first, L"Surface Pro"));
    VERIFY_ARE_EQUAL(std::wstring{ L"Surface Pro" }, list.Find(first)->Name);

    // Remembering it again doesn't make a second entry.
    VERIFY_IS_TRUE(list.Remember(first, L"Surface"));
    VERIFY_ARE_EQUAL(static_cast<size_t>(2), list.Load().size());

    // A damaged file is an empty list, not a crash.
    {
        std::ofstream file{ std::filesystem::path{ path }, std::ios::binary | std::ios::trunc };
        file << "{ \"devices\": [ { \"key\": 5 }, \"x\", { \"key\": \"zz\" } ";
    }

    VERIFY_IS_TRUE(list.Load().empty());
}

void DeviceLinkTests::ThePairedListCleansNames()
{
    auto const path = (g_folder / L"PairedNames.json").wstring();
    midiapp::PairedDeviceList list{ path };

    // A right-to-left override would make "Surface\u202Egnp.exe" read differently on screen.
    VERIFY_IS_TRUE(list.Remember(g_first->Identity->KeyHash(), L"  Sur\u200Bface\u202E\tPC  "));

    auto const found = list.Find(g_first->Identity->KeyHash());
    VERIFY_IS_TRUE(found.has_value());
    VERIFY_ARE_EQUAL(std::wstring{ L"Surface PC" }, found->Name);

    VERIFY_ARE_EQUAL(midiapp::MaximumDeviceLinkNameLength, midiapp::CleanDeviceLinkName(std::wstring(500, L'x')).size());
}

void DeviceLinkTests::RemovingAPairingForgetsIt()
{
    auto const path = (g_folder / L"PairedForget.json").wstring();
    midiapp::PairedDeviceList list{ path };

    VERIFY_IS_TRUE(list.Remember(g_first->Identity->KeyHash(), L"Surface"));
    VERIFY_IS_TRUE(list.Forget(g_first->Identity->KeyHash()));
    VERIFY_IS_FALSE(list.Find(g_first->Identity->KeyHash()).has_value());

    // Forgetting one that isn't there is fine.
    VERIFY_IS_TRUE(list.Forget(g_second->Identity->KeyHash()));
}

// ---------------------------------------------------------------------------- the connection

void DeviceLinkTests::TwoKeysConnectOverTls()
{
    midiapp::DeviceLinkListener listener{};
    VERIFY_SUCCEEDED(listener.Start(0, true));

    HRESULT serverError{ E_FAIL };
    DeviceKeyHash serverSawKey{};
    std::vector<uint8_t> serverGot{};

    std::thread server([&]()
        {
            auto const socket = listener.Accept(10'000);
            auto connection = midiapp::DeviceLinkConnection::Accept(socket, *g_second->Identity, 10'000, serverError);

            if (connection == nullptr)
            {
                return;
            }

            serverSawKey = connection->PeerKey();

            DeviceLinkFrame frame{};

            if (connection->ReadFrame(Allow(0x0100, 64), frame) == DeviceLinkFrameStatus::Read)
            {
                serverGot = frame.Payload;
                std::vector<uint8_t> reply{ frame.Payload.rbegin(), frame.Payload.rend() };
                connection->WriteFrame(0x0101, reply);
            }

            // Waits for the client to finish before closing.
            connection->ReadFrame(Allow(0x0102, 1), frame);
        });

    HRESULT clientError{ E_FAIL };
    auto client = midiapp::DeviceLinkConnection::Connect(L"127.0.0.1", listener.Port(), *g_first->Identity, 10'000, clientError);

    Log::Comment(String().Format(L"Client 0x%08X", static_cast<uint32_t>(clientError)));

    std::vector<uint8_t> reply{};

    if (client != nullptr)
    {
        std::vector<uint8_t> const hello{ 'h', 'e', 'l', 'l', 'o' };
        VERIFY_IS_TRUE(client->WriteFrame(0x0100, hello));

        DeviceLinkFrame frame{};
        VERIFY_IS_TRUE(client->ReadFrame(Allow(0x0101, 64), frame) == DeviceLinkFrameStatus::Read);
        reply = frame.Payload;

        client->Close();
    }

    server.join();

    Log::Comment(String().Format(L"Server 0x%08X", static_cast<uint32_t>(serverError)));

    VERIFY_SUCCEEDED(clientError);
    VERIFY_SUCCEEDED(serverError);

    // Each side sees exactly the other's key, which is what pairing pins.
    VERIFY_IS_TRUE(midiapp::SameDeviceKey(client->PeerKey(), g_second->Identity->KeyHash()));
    VERIFY_IS_TRUE(midiapp::SameDeviceKey(serverSawKey, g_first->Identity->KeyHash()));

    VERIFY_IS_TRUE((serverGot == std::vector<uint8_t>{ 'h', 'e', 'l', 'l', 'o' }));
    VERIFY_IS_TRUE((reply == std::vector<uint8_t>{ 'o', 'l', 'l', 'e', 'h' }));
}

void DeviceLinkTests::PairingOverTheConnectionAgrees()
{
    midiapp::DeviceLinkListener listener{};
    VERIFY_SUCCEEDED(listener.Start(0, true));

    std::optional<uint32_t> responderCode{};
    bool responderHeardYes{ false };
    midiapp::DeviceLinkHello helloSeen{};

    std::thread server([&]()
        {
            HRESULT error{ E_FAIL };
            auto connection = midiapp::DeviceLinkConnection::Accept(listener.Accept(10'000), *g_second->Identity, 10'000, error);

            if (connection == nullptr)
            {
                return;
            }

            midiapp::DeviceLinkRefusal refusal{};

            if (!midiapp::ReadDeviceLinkHello(*connection, helloSeen, refusal))
            {
                return;
            }

            midiapp::DeviceLinkHello hello{};
            hello.App = L"test";
            hello.Version = 1;
            hello.Purpose = midiapp::DeviceLinkPurpose::Pair;
            hello.DeviceName = L"Responder";

            midiapp::WriteDeviceLinkHello(*connection, hello);

            responderCode = midiapp::PairAsResponder(*connection, g_second->Identity->KeyHash());

            if (responderCode)
            {
                midiapp::ExchangePairingAnswers(*connection, true, responderHeardYes);
            }
        });

    HRESULT error{ E_FAIL };
    auto client = midiapp::DeviceLinkConnection::Connect(L"127.0.0.1", listener.Port(), *g_first->Identity, 10'000, error);

    std::optional<uint32_t> initiatorCode{};
    bool initiatorHeardYes{ false };
    midiapp::DeviceLinkHello reply{};

    if (client != nullptr)
    {
        midiapp::DeviceLinkHello hello{};
        hello.App = L"test";
        hello.Version = 1;
        hello.Purpose = midiapp::DeviceLinkPurpose::Pair;
        hello.DeviceName = L"Initiator\u202E";

        midiapp::DeviceLinkRefusal refusal{};

        if (midiapp::WriteDeviceLinkHello(*client, hello) && midiapp::ReadDeviceLinkHello(*client, reply, refusal))
        {
            initiatorCode = midiapp::PairAsInitiator(*client, g_first->Identity->KeyHash());

            if (initiatorCode)
            {
                midiapp::ExchangePairingAnswers(*client, true, initiatorHeardYes);
            }
        }
    }

    server.join();

    VERIFY_SUCCEEDED(error);
    VERIFY_IS_TRUE(initiatorCode.has_value());
    VERIFY_IS_TRUE(responderCode.has_value());
    VERIFY_ARE_EQUAL(*initiatorCode, *responderCode);
    VERIFY_IS_TRUE(initiatorHeardYes);
    VERIFY_IS_TRUE(responderHeardYes);

    VERIFY_ARE_EQUAL(std::wstring{ L"Initiator" }, helloSeen.DeviceName);
    VERIFY_ARE_EQUAL(std::wstring{ L"Responder" }, reply.DeviceName);
    VERIFY_IS_TRUE(helloSeen.Purpose == midiapp::DeviceLinkPurpose::Pair);
}

void DeviceLinkTests::ALargeFrameCrossesTheConnection()
{
    midiapp::DeviceLinkListener listener{};
    VERIFY_SUCCEEDED(listener.Start(0, true));

    std::vector<uint8_t> payload(midiapp::MaximumDeviceLinkFrameBytes);

    for (size_t i = 0; i < payload.size(); ++i)
    {
        payload[i] = static_cast<uint8_t>(i * 31 + 7);
    }

    std::vector<uint8_t> received{};

    std::thread server([&]()
        {
            HRESULT error{ E_FAIL };
            auto connection = midiapp::DeviceLinkConnection::Accept(listener.Accept(10'000), *g_second->Identity, 10'000, error);

            if (connection == nullptr)
            {
                return;
            }

            for (int32_t i = 0; i < 3; ++i)
            {
                DeviceLinkFrame frame{};

                if (connection->ReadFrame(Allow(0x0100, midiapp::MaximumDeviceLinkFrameBytes), frame) != DeviceLinkFrameStatus::Read)
                {
                    return;
                }

                received.insert(received.end(), frame.Payload.begin(), frame.Payload.end());
            }

            // A clean close after the last frame is Closed, not a cut.
            DeviceLinkFrame last{};

            if (connection->ReadFrame(Allow(0x0100, 16), last) != DeviceLinkFrameStatus::Closed)
            {
                received.clear();
            }
        });

    HRESULT error{ E_FAIL };
    auto client = midiapp::DeviceLinkConnection::Connect(L"127.0.0.1", listener.Port(), *g_first->Identity, 10'000, error);

    VERIFY_SUCCEEDED(error);

    if (client != nullptr)
    {
        for (int32_t i = 0; i < 3; ++i)
        {
            VERIFY_IS_TRUE(client->WriteFrame(0x0100, payload));
        }

        client->Close();
    }

    server.join();

    VERIFY_ARE_EQUAL(payload.size() * 3, received.size());
    VERIFY_IS_TRUE(std::equal(payload.begin(), payload.end(), received.begin()));
    VERIFY_IS_TRUE(std::equal(payload.begin(), payload.end(), received.begin() + static_cast<ptrdiff_t>(payload.size() * 2)));
}
