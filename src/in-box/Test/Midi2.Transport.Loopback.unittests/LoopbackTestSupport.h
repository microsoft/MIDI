// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// Creating, commanding and listing loopbacks through the service, for the tests which need one
// to carry MIDI. Key names are spelled out rather than taken from the transport's headers, so
// these stay an independent check of the wire shape.

namespace LoopbackTest
{
    // {942BF02D-93C0-4EA8-B03E-D51156CA75E1}
    inline constexpr GUID LoopbackTransportId
    {
        0x942BF02D, 0x93C0, 0x4EA8, { 0xB0, 0x3E, 0xD5, 0x11, 0x56, 0xCA, 0x75, 0xE1 }
    };

    inline constexpr wchar_t LoopbackTransportIdString[]{ L"{942BF02D-93C0-4EA8-B03E-D51156CA75E1}" };

    inline constexpr DWORD MessageWaitMilliseconds{ 5000 };


    inline TransportConfigTest::ServiceConfigResult SendLoopbackConfig(_In_ std::wstring const& json)
    {
        return TransportConfigTest::SendTransportConfig(LoopbackTransportId, LoopbackTransportIdString, json);
    }


    inline bool LoopbackAvailable()
    {
        return TransportConfigTest::IsTransportAvailable(LoopbackTransportId, LoopbackTransportIdString);
    }


    struct CreatedLoopback
    {
        std::wstring AssociationId{ };
        std::wstring EndpointA{ };
        std::wstring EndpointB{ };
        std::wstring Detail{ };

        bool Created() const noexcept { return !EndpointA.empty() && !EndpointB.empty(); }
    };

    // No two loopback endpoints may share a name, so each pair's names carry part of its unique id.
    inline CreatedLoopback CreateLoopback(_In_ std::wstring const& baseName)
    {
        CreatedLoopback created{ };
        created.AssociationId = TransportConfigTest::MakeGuidString();

        auto const uniqueId = TransportConfigTest::MakeUniqueIdString();
        auto const name = TransportConfigTest::EscapeJsonString(baseName + L" " + uniqueId.substr(0, 8));

        auto const result = SendLoopbackConfig(
            L"{\"create\":{\"" + created.AssociationId + L"\":{"
            L"\"endpointA\":{"
            L"\"name\":\"" + name + L" A\","
            L"\"description\":\"Service test endpoint A\","
            L"\"uniqueIdentifier\":\"" + uniqueId + L"\""
            L"},"
            L"\"endpointB\":{"
            L"\"name\":\"" + name + L" B\","
            L"\"description\":\"Service test endpoint B\","
            L"\"uniqueIdentifier\":\"" + uniqueId + L"\""
            L"}}}}");

        created.Detail = result.Message + L" " + result.ResponseJson;

        if (!result.IsSuccess())
        {
            return created;
        }

        winrt::Windows::Data::Json::JsonObject response{ nullptr };

        if (!winrt::Windows::Data::Json::JsonObject::TryParse(result.ResponseJson, response))
        {
            return created;
        }

        created.EndpointA = std::wstring{ response.GetNamedString(L"endpointA", L"") };
        created.EndpointB = std::wstring{ response.GetNamedString(L"endpointB", L"") };

        return created;
    }


    inline TransportConfigTest::ServiceConfigResult RemoveLoopback(_In_ std::wstring const& associationId)
    {
        return SendLoopbackConfig(L"{\"remove\":[\"" + TransportConfigTest::EscapeJsonString(associationId) + L"\"]}");
    }


    // moreArgumentsJson goes inside commandArguments, after the association id, comma first
    inline TransportConfigTest::ServiceConfigResult SendLoopbackCommand(
        _In_ std::wstring const& commandName,
        _In_ std::wstring const& associationId,
        _In_ std::wstring const& moreArgumentsJson = L"")
    {
        return SendLoopbackConfig(
            L"{\"transportCommand\":{\"commandName\":\"" + commandName + L"\",\"commandArguments\":{"
            L"\"associationId\":\"" + TransportConfigTest::EscapeJsonString(associationId) + L"\"" +
            moreArgumentsJson +
            L"}}}");
    }


    // The list reports the association braced and lowercased, so this compares GUIDs. An entry
    // from a hand-edited configuration file need not be a GUID at all.
    inline bool SameAssociation(_In_ winrt::hstring const& reported, _In_ std::wstring const& wanted) noexcept
    {
        try
        {
            return !reported.empty() && winrt::guid{ reported } == winrt::guid{ wanted };
        }
        catch (...)
        {
            return false;
        }
    }


    struct ReportedState
    {
        bool Found{ false };
        bool Muted{ false };
        bool MutedForFeedback{ false };
    };

    inline ReportedState GetReportedState(_In_ std::wstring const& associationId)
    {
        ReportedState state{ };

        auto const result = SendLoopbackConfig(LR"({"transportCommand":{"commandName":"listEntries"}})");

        if (!result.IsSuccess())
        {
            return state;
        }

        winrt::Windows::Data::Json::JsonObject response{ nullptr };

        if (!winrt::Windows::Data::Json::JsonObject::TryParse(result.ResponseJson, response) ||
            response == nullptr ||
            !response.HasKey(L"entries"))
        {
            return state;
        }

        for (auto const& value : response.GetNamedArray(L"entries"))
        {
            if (value.ValueType() != winrt::Windows::Data::Json::JsonValueType::Object)
            {
                continue;
            }

            auto const entry = value.GetObject();

            if (!SameAssociation(entry.GetNamedString(L"associationIdentifier", L""), associationId))
            {
                continue;
            }

            state.Found = true;
            state.Muted = entry.GetNamedBoolean(L"muted", false);
            state.MutedForFeedback = entry.GetNamedBoolean(L"mutedForFeedback", false);

            break;
        }

        return state;
    }


    // Both ends of a loopback open in one session, as an app using it would have them.
    struct OpenLoopback
    {
        CreatedLoopback Loopback{ };
        MidiTest::ServiceSession Session{ };
        wil::com_ptr_nothrow<MidiTest::ReceivedMessages> ReceivedA{ };
        wil::com_ptr_nothrow<MidiTest::ReceivedMessages> ReceivedB{ };
        wil::com_ptr_nothrow<IMidiBidirectional> A{ };
        wil::com_ptr_nothrow<IMidiBidirectional> B{ };
    };

    // keepMessages false only counts what arrives, for loops which carry millions of messages
    inline void OpenBothSides(_In_ std::wstring const& baseName, _In_ bool const keepMessages, _Inout_ OpenLoopback& loopback)
    {
        loopback.Loopback = CreateLoopback(baseName);
        VERIFY_IS_TRUE(loopback.Loopback.Created(), loopback.Loopback.Detail.c_str());

        VERIFY_SUCCEEDED(loopback.Session.Open(L"Loopback service tests"));

        loopback.ReceivedA = MidiTest::ReceivedMessages::Create(keepMessages);
        loopback.ReceivedB = MidiTest::ReceivedMessages::Create(keepMessages);
        VERIFY_IS_NOT_NULL(loopback.ReceivedA.get());
        VERIFY_IS_NOT_NULL(loopback.ReceivedB.get());

        VERIFY_SUCCEEDED(loopback.Session.Connect(loopback.Loopback.EndpointA, loopback.ReceivedA.get(), loopback.A));
        VERIFY_SUCCEEDED(loopback.Session.Connect(loopback.Loopback.EndpointB, loopback.ReceivedB.get(), loopback.B));
    }

    // Closes both ends before removing the loopback, which keeps these tests off the path where a
    // loopback goes away under open connections. LoopbackTeardownTests covers that path.
    inline void CloseAndRemove(_Inout_ OpenLoopback& loopback) noexcept
    {
        if (loopback.ReceivedA)
        {
            loopback.ReceivedA->EchoThrough(nullptr);
        }

        if (loopback.ReceivedB)
        {
            loopback.ReceivedB->EchoThrough(nullptr);
        }

        if (!MidiTest::CloseConnectionWithHangGuard(loopback.A) ||
            !MidiTest::CloseConnectionWithHangGuard(loopback.B))
        {
            loopback.Session.Abandon();
            return;
        }

        loopback.Session.Close();

        if (!loopback.Loopback.AssociationId.empty())
        {
            try
            {
                RemoveLoopback(loopback.Loopback.AssociationId);
            }
            catch (...)
            {
            }
        }
    }
}
