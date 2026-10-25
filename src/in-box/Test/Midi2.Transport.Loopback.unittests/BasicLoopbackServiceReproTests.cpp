// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <thread>

#include "BasicLoopbackServiceReproTests.h"
#include "..\..\Transport\BasicLoopbackMidiTransport\basic_loopback_transport_error_codes.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    // {10088473-9478-4E62-850B-3D2315E135B8}
    constexpr GUID BasicLoopbackTransportId
    {
        0x10088473, 0x9478, 0x4E62, { 0x85, 0x0B, 0x3D, 0x23, 0x15, 0xE1, 0x35, 0xB8 }
    };

    constexpr wchar_t BasicLoopbackTransportIdString[]{ L"{10088473-9478-4E62-850B-3D2315E135B8}" };

    bool ShouldRun()
    {
        bool confirmed{ false };

        if (FAILED(RuntimeParameters::TryGetValue(L"ConfirmFixedTransport", confirmed)) || !confirmed)
        {
            Log::Result(TestResults::Skipped,
                L"Skipped: against the old transport this takes midisrv down. Pass /p:\"ConfirmFixedTransport=true\" once the fixed transport is deployed.");
            return false;
        }

        if (!TransportConfigTest::IsTransportAvailable(BasicLoopbackTransportId, BasicLoopbackTransportIdString))
        {
            Log::Result(TestResults::Skipped, L"Skipped: the Basic Loopback transport isn't answering.");
            return false;
        }

        return true;
    }

    struct Reply
    {
        bool Answered{ false };
        bool Success{ false };
        uint32_t ErrorCode{ 0 };
        std::wstring Json{};
    };

    Reply Send(_In_ std::wstring const& json)
    {
        Reply reply{};

        auto const result = TransportConfigTest::SendTransportConfig(BasicLoopbackTransportId, BasicLoopbackTransportIdString, json);

        reply.Answered = result.CallSucceeded;
        reply.Success = result.IsSuccess();
        reply.Json = result.ResponseJson + L" " + result.Message;

        winrt::Windows::Data::Json::JsonObject parsed{ nullptr };

        if (winrt::Windows::Data::Json::JsonObject::TryParse(result.ResponseJson, parsed))
        {
            reply.ErrorCode = static_cast<uint32_t>(parsed.GetNamedNumber(L"errorCode", 0));
        }

        return reply;
    }

    std::wstring CreateJson(
        _In_ std::wstring const& associationId,
        _In_ std::wstring const& uniqueId,
        _In_ std::wstring const& name)
    {
        return L"{\"create\":{\"" + associationId + L"\":{\"endpoint\":{\"name\":\"" + name +
            L"\",\"uniqueIdentifier\":\"" + uniqueId + L"\"}}}}";
    }

    Reply Remove(_In_ std::wstring const& associationId)
    {
        return Send(L"{\"remove\":[\"" + associationId + L"\"]}");
    }

    bool ServiceStillAnswers()
    {
        return Send(LR"({"transportCommand":{"commandName":"listEntries"}})").Success;
    }
}


void BasicLoopbackServiceReproTests::TestReusedAssociationIdIsRefusedByTheService()
{
    if (!ShouldRun()) return;

    auto const association = TransportConfigTest::MakeGuidString();
    auto const otherAssociation = TransportConfigTest::MakeGuidString();
    auto const uniqueX = TransportConfigTest::MakeUniqueIdString();
    auto const uniqueY = TransportConfigTest::MakeUniqueIdString();

    auto const first = Send(CreateJson(association, uniqueX, L"B1 repro " + uniqueX.substr(0, 8)));
    Log::Comment(first.Json.c_str());
    VERIFY_IS_TRUE(first.Success);

    auto const second = Send(CreateJson(association, uniqueY, L"B1 repro " + uniqueY.substr(0, 8)));
    Log::Comment(second.Json.c_str());
    VERIFY_IS_TRUE(second.Answered);
    VERIFY_IS_FALSE(second.Success);
    VERIFY_ARE_EQUAL(second.ErrorCode, static_cast<uint32_t>(BASIC_LOOPBACK_ERROR_CODE_INVALID_ASSOCIATION_ID));

    auto const third = Send(CreateJson(otherAssociation, uniqueX, L"B1 repro again " + uniqueX.substr(0, 8)));
    Log::Comment(third.Json.c_str());
    VERIFY_IS_TRUE(third.Answered, L"the service answered, so it is still running");
    VERIFY_IS_FALSE(third.Success);
    VERIFY_ARE_EQUAL(third.ErrorCode, static_cast<uint32_t>(BASIC_LOOPBACK_ERROR_CODE_DUPLICATE_UNIQUE_ID));

    VERIFY_IS_TRUE(ServiceStillAnswers());

    auto const removed = Remove(association);
    VERIFY_IS_TRUE(removed.Success, removed.Json.c_str());
}


void BasicLoopbackServiceReproTests::TestConcurrentCreatesOfOneUniqueIdThroughTheService()
{
    if (!ShouldRun()) return;

    constexpr size_t CreatorCount{ 6 };

    auto const uniqueId = TransportConfigTest::MakeUniqueIdString();

    std::vector<std::wstring> associations;
    for (size_t i = 0; i < CreatorCount; i++)
    {
        associations.push_back(TransportConfigTest::MakeGuidString());
    }

    std::vector<Reply> replies(CreatorCount);
    std::vector<std::thread> creators;

    for (size_t i = 0; i < CreatorCount; i++)
    {
        creators.emplace_back([&, i]()
            {
                winrt::init_apartment();

                replies[i] = Send(CreateJson(associations[i], uniqueId, L"B1 race " + uniqueId.substr(0, 8) + L" " + std::to_wstring(i)));

                winrt::uninit_apartment();
            });
    }

    for (auto& creator : creators)
    {
        creator.join();
    }

    size_t created{ 0 };
    size_t refused{ 0 };

    for (size_t i = 0; i < CreatorCount; i++)
    {
        Log::Comment(replies[i].Json.c_str());

        if (replies[i].Success)
        {
            created++;

            auto const removed = Remove(associations[i]);
            VERIFY_IS_TRUE(removed.Success, removed.Json.c_str());
        }
        else if (replies[i].ErrorCode == BASIC_LOOPBACK_ERROR_CODE_DUPLICATE_UNIQUE_ID)
        {
            refused++;
        }
    }

    VERIFY_ARE_EQUAL(created, static_cast<size_t>(1));
    VERIFY_ARE_EQUAL(refused, CreatorCount - 1);
    VERIFY_IS_TRUE(ServiceStillAnswers());
}
