// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <mmsystem.h>
#include <vector>
#include <atomic>
#include <fstream>


// Sending group for the transfer. Set to something other than zero so we can verify
// that the sender actually applies it to the messages it generates.
#define SYSEX_TEST_DESTINATION_GROUP_INDEX      7

// Pause for this long after every SYSEX_TEST_TRANSFER_MESSAGE_COUNT messages
#define SYSEX_TEST_TRANSFER_MESSAGE_COUNT       100
#define SYSEX_TEST_TRANSFER_SPACING_MS          500

#define SYSEX_TEST_FILE_NAME                    L"7.syx"


// Resolves a file in the shared test-data folder.
//
// The test binaries build to vsfiles-sdk\out\tests\<platform>\<configuration>, and the
// test data lives with the test sources, so the path here is relative to the folder the
// test binary runs from. It is then made absolute because the WinRT storage APIs will
// not accept a relative path.
static winrt::hstring GetTestDataFilePath(_In_ std::wstring const& fileName)
{
    wchar_t modulePath[MAX_PATH]{ 0 };

    HMODULE thisModule{ nullptr };

    VERIFY_IS_TRUE(GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&GetTestDataFilePath),
        &thisModule) != FALSE);

    VERIFY_IS_TRUE(GetModuleFileNameW(thisModule, modulePath, ARRAYSIZE(modulePath)) > 0);

    std::filesystem::path path(modulePath);

    path = path.parent_path();
    path /= LR"(..\..\..\..\..\Test\WinRT Client\test-data)";
    path /= fileName;

    std::error_code ec{};
    auto absolutePath = std::filesystem::canonical(path, ec);

    if (ec)
    {
        std::wcout << L"Unable to resolve test data file: " << path.wstring() << std::endl;
        VERIFY_FAIL();
        return winrt::hstring{};
    }

    return winrt::hstring(absolutePath.wstring());
}


// Reads the whole test data file so we have something to compare the received data to.
static std::vector<uint8_t> ReadTestDataFileBytes(_In_ winrt::hstring const& filePath)
{
    std::ifstream file(filePath.c_str(), std::ios::binary);

    VERIFY_IS_TRUE(file.is_open());

    std::vector<uint8_t> bytes(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    file.close();

    return bytes;
}


// Collects the SysEx7 payload bytes arriving on a connection, and checks that every
// message is a SysEx7 message on the expected group.
struct SysExReceiveCollector
{
    wil::critical_section lock;

    std::vector<uint8_t> receivedDataBytes;

    uint32_t countMessagesReceived{ 0 };
    uint32_t countNonSysExMessages{ 0 };
    uint32_t countWrongGroupMessages{ 0 };

    void OnMessageReceived(_In_ MidiMessageReceivedEventArgs const& args)
    {
        auto word0 = args.PeekFirstWord();

        auto lockGuard = lock.lock();

        countMessagesReceived++;

        if (!MidiSystemExclusive7MessageHelper::MessageIsSystemExclusiveMessage(word0))
        {
            countNonSysExMessages++;
            return;
        }

        if (MidiMessageHelper::GetGroupFromMessageFirstWord(word0).Index() != SYSEX_TEST_DESTINATION_GROUP_INDEX)
        {
            countWrongGroupMessages++;
        }

        auto message = args.GetMessagePacket().as<MidiMessage64>();

        auto dataBytes = MidiSystemExclusive7MessageHelper::GetDataBytesFromSingleSystemExclusiveMessage(message);

        for (auto const& b : dataBytes)
        {
            receivedDataBytes.push_back(b);
        }
    }

    size_t ReceivedByteCount()
    {
        auto lockGuard = lock.lock();
        return receivedDataBytes.size();
    }
};


void MidiSysExTransferTests::TestSysExFileTransfer()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    auto filePath = GetTestDataFilePath(SYSEX_TEST_FILE_NAME);

    std::wcout << L"Test data file: " << filePath.c_str() << std::endl;

    auto fileBytes = ReadTestDataFileBytes(filePath);

    VERIFY_IS_TRUE(fileBytes.size() > 0);

    // The file is a complete bytestream SysEx message, so it starts with F0 and ends
    // with F7. Those framing bytes are not carried in the SysEx7 UMP payload, so the
    // data we expect to receive is everything in between.
    VERIFY_ARE_EQUAL(fileBytes.front(), (uint8_t)0xF0);
    VERIFY_ARE_EQUAL(fileBytes.back(), (uint8_t)0xF7);

    std::vector<uint8_t> expectedDataBytes(fileBytes.begin() + 1, fileBytes.end() - 1);

    std::cout << "File bytes:              " << std::dec << fileBytes.size() << std::endl;
    std::cout << "Expected payload bytes:  " << std::dec << expectedDataBytes.size() << std::endl;

    // set up the connections. We send on loopback A and receive on loopback B.
    auto session = MidiSession::Create(L"TestSysExFileTransfer");
    VERIFY_IS_NOT_NULL(session);

    auto connSend = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    VERIFY_IS_NOT_NULL(connSend);

    auto connReceive = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());
    VERIFY_IS_NOT_NULL(connReceive);

    SysExReceiveCollector collector;

    wil::unique_event_nothrow allDataReceived;
    allDataReceived.create();

    auto eventToken = connReceive.MessageReceived([&](IMidiMessageReceivedEventSource const&, MidiMessageReceivedEventArgs const& args)
        {
            collector.OnMessageReceived(args);

            if (collector.ReceivedByteCount() >= expectedDataBytes.size())
            {
                allDataReceived.SetEvent();
            }
        });

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());

    // open the file as an input stream for the sender
    auto file = storage::StorageFile::GetFileFromPathAsync(filePath).get();
    VERIFY_IS_NOT_NULL(file);

    auto stream = file.OpenReadAsync().get();
    VERIFY_IS_NOT_NULL(stream);

    MidiBytestreamToUmpMessageConverterState converterState;
    converterState.Tag(L"TestSysExFileTransfer");

    MidiGroup destinationGroup((uint8_t)SYSEX_TEST_DESTINATION_GROUP_INDEX);

    LOG_OUTPUT(L"Starting the SysEx transfer");

    std::atomic<uint64_t> lastReportedMessagesSent{ 0 };

    auto startTimestamp = MidiClock::Now();

    auto operation = MidiSystemExclusiveSender::SendBinarySysEx7ByteDataAsync(
        connSend,
        destinationGroup,
        stream,
        SYSEX_TEST_TRANSFER_MESSAGE_COUNT,
        SYSEX_TEST_TRANSFER_SPACING_MS,
        converterState);

    VERIFY_IS_NOT_NULL(operation);

    operation.Progress([&](auto const&, MidiSystemExclusiveSendProgress const& progress)
        {
            lastReportedMessagesSent = progress.CountMessagesSent();
        });

    auto sendSucceeded = operation.get();

    auto endTimestamp = MidiClock::Now();

    VERIFY_IS_TRUE(sendSucceeded);
    VERIFY_ARE_EQUAL(operation.Status(), foundation::AsyncStatus::Completed);

    LOG_OUTPUT(L"Transfer completed. Waiting for the remaining messages to arrive.");

    if (!allDataReceived.wait(30000))
    {
        std::cout << "Timed out waiting for the received data." << std::endl;
    }

    auto elapsedMilliseconds = MidiClock::ConvertTimestampTicksToMilliseconds(endTimestamp - startTimestamp);

    std::cout << "Messages sent:           " << std::dec << lastReportedMessagesSent.load() << std::endl;
    std::cout << "Messages received:       " << std::dec << collector.countMessagesReceived << std::endl;
    std::cout << "Transfer time:           " << std::fixed << elapsedMilliseconds << " ms" << std::endl;

    {
        auto lockGuard = collector.lock.lock();

        // every message must have been a SysEx7 message on the requested group
        VERIFY_ARE_EQUAL(collector.countNonSysExMessages, (uint32_t)0);
        VERIFY_ARE_EQUAL(collector.countWrongGroupMessages, (uint32_t)0);

        // and all of the data must have arrived, byte for byte
        VERIFY_ARE_EQUAL(collector.receivedDataBytes.size(), expectedDataBytes.size());
        VERIFY_IS_TRUE(collector.receivedDataBytes == expectedDataBytes);
    }

    // The sender pauses for transferSpacingMilliseconds after every
    // preferredSingleTransferMessageCount messages, so the transfer cannot have taken
    // less time than those pauses require.
    uint64_t expectedPauseCount = lastReportedMessagesSent.load() / SYSEX_TEST_TRANSFER_MESSAGE_COUNT;
    uint64_t minimumExpectedMilliseconds = expectedPauseCount * SYSEX_TEST_TRANSFER_SPACING_MS;

    std::cout << "Expected pauses:         " << std::dec << expectedPauseCount << std::endl;
    std::cout << "Minimum expected time:   " << std::dec << minimumExpectedMilliseconds << " ms" << std::endl;

    VERIFY_IS_TRUE(expectedPauseCount > 0);
    VERIFY_IS_TRUE(elapsedMilliseconds >= (double)minimumExpectedMilliseconds);

    connReceive.MessageReceived(eventToken);

    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());
    session.Close();
}



void MidiSysExTransferTests::TestSysExFileTransferWithCancel()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    auto filePath = GetTestDataFilePath(SYSEX_TEST_FILE_NAME);

    auto fileBytes = ReadTestDataFileBytes(filePath);
    VERIFY_IS_TRUE(fileBytes.size() > 0);

    std::vector<uint8_t> expectedDataBytes(fileBytes.begin() + 1, fileBytes.end() - 1);

    auto session = MidiSession::Create(L"TestSysExFileTransferWithCancel");
    VERIFY_IS_NOT_NULL(session);

    auto connSend = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    VERIFY_IS_NOT_NULL(connSend);

    auto connReceive = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());
    VERIFY_IS_NOT_NULL(connReceive);

    SysExReceiveCollector collector;

    auto eventToken = connReceive.MessageReceived([&](IMidiMessageReceivedEventSource const&, MidiMessageReceivedEventArgs const& args)
        {
            collector.OnMessageReceived(args);
        });

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());

    auto file = storage::StorageFile::GetFileFromPathAsync(filePath).get();
    VERIFY_IS_NOT_NULL(file);

    auto stream = file.OpenReadAsync().get();
    VERIFY_IS_NOT_NULL(stream);

    MidiBytestreamToUmpMessageConverterState converterState;
    converterState.Tag(L"TestSysExFileTransferWithCancel");

    MidiGroup destinationGroup((uint8_t)SYSEX_TEST_DESTINATION_GROUP_INDEX);

    LOG_OUTPUT(L"Starting the SysEx transfer");

    auto startTimestamp = MidiClock::Now();

    auto operation = MidiSystemExclusiveSender::SendBinarySysEx7ByteDataAsync(
        connSend,
        destinationGroup,
        stream,
        SYSEX_TEST_TRANSFER_MESSAGE_COUNT,
        SYSEX_TEST_TRANSFER_SPACING_MS,
        converterState);

    VERIFY_IS_NOT_NULL(operation);

    // Let the transfer get properly under way, then cancel it. The spacing pauses make
    // this file take several seconds to send, so one second in we are mid-transfer.
    Sleep(1000);

    LOG_OUTPUT(L"Canceling the transfer");

    operation.Cancel();

    // A canceled operation reports Canceled and throws from the result accessor
    bool caughtCanceled{ false };

    try
    {
        operation.get();
    }
    catch (winrt::hresult_canceled const&)
    {
        caughtCanceled = true;
    }

    auto endTimestamp = MidiClock::Now();

    auto elapsedMilliseconds = MidiClock::ConvertTimestampTicksToMilliseconds(endTimestamp - startTimestamp);

    std::cout << "Operation status:        " << std::dec << (int)operation.Status() << std::endl;
    std::cout << "Threw hresult_canceled:  " << std::boolalpha << caughtCanceled << std::endl;
    std::cout << "Elapsed:                 " << std::fixed << elapsedMilliseconds << " ms" << std::endl;
    std::cout << "Messages received:       " << std::dec << collector.countMessagesReceived << std::endl;

    VERIFY_ARE_EQUAL(operation.Status(), foundation::AsyncStatus::Canceled);
    VERIFY_IS_TRUE(caughtCanceled);

    // give anything already queued a moment to land before we look at the totals
    Sleep(1000);

    {
        auto lockGuard = collector.lock.lock();

        std::cout << "Partial bytes received:  " << std::dec << collector.receivedDataBytes.size()
            << " of " << expectedDataBytes.size() << std::endl;

        // we should have received some data, but not the whole file
        VERIFY_IS_TRUE(collector.receivedDataBytes.size() > 0);
        VERIFY_IS_TRUE(collector.receivedDataBytes.size() < expectedDataBytes.size());

        // whatever did arrive must still have been correctly formed
        VERIFY_ARE_EQUAL(collector.countNonSysExMessages, (uint32_t)0);
        VERIFY_ARE_EQUAL(collector.countWrongGroupMessages, (uint32_t)0);

        // and it must match the beginning of the file
        for (size_t i = 0; i < collector.receivedDataBytes.size(); i++)
        {
            if (collector.receivedDataBytes[i] != expectedDataBytes[i])
            {
                std::cout << "Mismatch at byte " << std::dec << i << std::endl;
                VERIFY_FAIL();
                break;
            }
        }
    }

    connReceive.MessageReceived(eventToken);

    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());
    session.Close();
}
