// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// DEVELOPER DIAGNOSTIC. Not shipped, not installed, not a supported tool.
//
//   midicidump [--endpoint <text>] [--group <1-16>] [--timeout <ms>] [--out <file>] [--list]
//
// Connects to the first endpoint whose name contains <text>, runs Discovery, and prints every
// Property Exchange resource the device offers, as the raw JSON it sent. Writes the same thing to
// a file so it can be kept.
// ============================================================================

#include "pch.h"

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::CapabilityInquiry;
using namespace winrt::Windows::Devices::Midi2::Enumeration;

namespace json = winrt::Windows::Data::Json;

namespace
{
    std::wofstream g_file;

    void Line(std::wstring const& text = {})
    {
        std::wcout << text << std::endl;

        if (g_file.is_open())
        {
            g_file << text << std::endl;
        }
    }

    std::wstring Lower(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), ::towlower);
        return value;
    }

    std::wstring Hex(uint8_t value)
    {
        wchar_t text[8]{};
        swprintf_s(text, L"0x%02X", value);
        return text;
    }

    // Every request goes through here so the header and the reply are always shown together. A
    // resource that comes back empty is as interesting as one that does not.
    //
    // A negative offset or limit means the request does not paginate. M2-103-UM wants both
    // Properties present when it does, so they are written as a pair or not at all.
    MidiPropertyExchangeResponse Request(
        MidiCapabilityInquirySession const& session,
        MidiUniqueId const& muid,
        std::wstring const& resource,
        std::wstring const& resourceId,
        int32_t const offset = -1,
        int32_t const limit = -1)
    {
        json::JsonObject header;

        header.SetNamedValue(L"resource", json::JsonValue::CreateStringValue(winrt::hstring{ resource }));

        if (!resourceId.empty())
        {
            header.SetNamedValue(L"resId", json::JsonValue::CreateStringValue(winrt::hstring{ resourceId }));
        }

        if (offset >= 0 && limit >= 0)
        {
            header.SetNamedValue(L"offset", json::JsonValue::CreateNumberValue(offset));
            header.SetNamedValue(L"limit", json::JsonValue::CreateNumberValue(limit));
        }

        Line();
        Line(L"--- request: " + std::wstring{ header.Stringify() });

        auto const response = session.GetPropertyDataAsync(muid, header).get();

        if (response == nullptr)
        {
            Line(L"    no response object");
            return response;
        }

        Line(L"    status            " + std::to_wstring((int)response.Status()));
        Line(L"    resource status   " + std::to_wstring(response.ResourceStatus()));
        Line(L"    chunks            " + std::to_wstring(response.ChunkCount()));
        Line(L"    reply header      " + std::wstring{ response.HeaderText() });

        if (!response.NakStatusMessage().empty())
        {
            Line(L"    device said       " + std::wstring{ response.NakStatusMessage() });
        }

        auto const body = std::wstring{ response.BodyAsText() };

        Line(L"    body bytes        " + std::to_wstring(response.Body().Size()));

        if (!body.empty())
        {
            Line(L"    body");
            Line(body);
        }

        return response;
    }

    void DumpResourceList(MidiPropertyExchangeResponse const& response)
    {
        if (response == nullptr || response.BodyAsJson() == nullptr)
        {
            return;
        }

        MidiResourceList list{ nullptr };

        try
        {
            list = MidiResourceList::FromJson(response.BodyAsJson().GetArray());
        }
        catch (...)
        {
            Line(L"    (could not be read as a resource list)");
            return;
        }

        if (list == nullptr)
        {
            return;
        }

        Line();
        Line(L"    parsed resource list");

        for (auto const& entry : list.Entries())
        {
            Line(L"      " + std::wstring{ entry.Resource() } +
                L"  canGet=" + (entry.CanGet() ? L"true" : L"false") +
                L"  canSet=" + std::wstring{ entry.CanSet() } +
                L"  canSubscribe=" + (entry.CanSubscribe() ? L"true" : L"false") +
                L"  canPaginate=" + (entry.CanPaginate() ? L"true" : L"false") +
                L"  requireResId=" + (entry.RequireResourceId() ? L"true" : L"false"));
        }
    }

    // The links are the whole question: they are how a device says which program collections a
    // channel can actually select from.
    std::vector<std::wstring> DumpChannelList(MidiPropertyExchangeResponse const& response)
    {
        std::vector<std::wstring> resourceIds{};

        if (response == nullptr || response.BodyAsJson() == nullptr)
        {
            return resourceIds;
        }

        MidiChannelList list{ nullptr };

        try
        {
            list = MidiChannelList::FromJson(response.BodyAsJson().GetArray());
        }
        catch (...)
        {
            Line(L"    (could not be read as a channel list)");
            return resourceIds;
        }

        if (list == nullptr)
        {
            return resourceIds;
        }

        Line();
        Line(L"    parsed channel list");

        for (auto const& entry : list.Entries())
        {
            Line(L"      channel " + std::to_wstring(entry.Channel()) +
                L"  title='" + std::wstring{ entry.Title() } +
                L"'  program='" + std::wstring{ entry.ProgramTitle() } +
                L"'  bankPC=[" + std::to_wstring(entry.BankMsb()) + L"," +
                std::to_wstring(entry.BankLsb()) + L"," +
                std::to_wstring(entry.ProgramChange()) + L"]" +
                L"  links=" + std::to_wstring(entry.Links().Size()));

            for (auto const& link : entry.Links())
            {
                Line(L"        link resource='" + std::wstring{ link.Resource() } +
                    L"'  resId='" + std::wstring{ link.ResourceId() } +
                    L"'  title='" + std::wstring{ link.Title() } + L"'");
            }
        }

        // De-duplicated, which is what an application would ask for.
        for (auto const& link : list.GetProgramListLinks())
        {
            if (link != nullptr)
            {
                resourceIds.push_back(std::wstring{ link.ResourceId() });
            }
        }

        return resourceIds;
    }

    int Run(int argc, wchar_t** argv)
    {
        std::wstring endpointFilter{ L"loopback b" };
        std::wstring outputPath{ L"midi-ci-dump.txt" };
        uint8_t groupIndex{ 0 };
        uint32_t timeoutMilliseconds{ 5000 };
        bool listOnly{ false };

        // Set both to ask each program list for one page as well as the whole thing, which is how
        // a resource that declares "canPaginate" gets checked.
        int32_t pageOffset{ -1 };
        int32_t pageLimit{ -1 };

        for (int i = 1; i < argc; i++)
        {
            std::wstring const arg{ argv[i] };

            if (arg == L"--list") { listOnly = true; }
            else if (arg == L"--endpoint" && i + 1 < argc) { endpointFilter = Lower(argv[++i]); }
            else if (arg == L"--out" && i + 1 < argc) { outputPath = argv[++i]; }
            else if (arg == L"--group" && i + 1 < argc) { groupIndex = static_cast<uint8_t>(_wtoi(argv[++i]) - 1); }
            else if (arg == L"--timeout" && i + 1 < argc) { timeoutMilliseconds = static_cast<uint32_t>(_wtoi(argv[++i])); }
            else if (arg == L"--offset" && i + 1 < argc) { pageOffset = _wtoi(argv[++i]); }
            else if (arg == L"--limit" && i + 1 < argc) { pageLimit = _wtoi(argv[++i]); }
            else
            {
                std::wcout << L"midicidump [--endpoint <text>] [--group <1-16>] [--timeout <ms>] [--out <file>] [--list]"
                    << std::endl;
                return 1;
            }
        }

        if (!MidiApi::EnsureServiceAvailable())
        {
            std::wcout << L"The MIDI service is not available." << std::endl;
            return 2;
        }

        auto const endpoints = MidiEndpointDeviceInformation::FindAll();

        if (listOnly)
        {
            for (auto const& endpoint : endpoints)
            {
                std::wcout << L"  " << std::wstring{ endpoint.Name() } << std::endl;
                std::wcout << L"      " << std::wstring{ endpoint.EndpointDeviceId() } << std::endl;
            }

            return 0;
        }

        MidiEndpointDeviceInformation match{ nullptr };

        for (auto const& endpoint : endpoints)
        {
            if (Lower(std::wstring{ endpoint.Name() }).find(endpointFilter) != std::wstring::npos)
            {
                match = endpoint;
                break;
            }
        }

        if (match == nullptr)
        {
            std::wcout << L"No endpoint name contains '" << endpointFilter
                << L"'. Run with --list to see them." << std::endl;
            return 3;
        }

        g_file.open(outputPath.c_str(), std::ios::out | std::ios::trunc);

        Line(L"endpoint          " + std::wstring{ match.Name() });
        Line(L"endpoint id       " + std::wstring{ match.EndpointDeviceId() });
        Line(L"group             " + std::to_wstring(groupIndex + 1));
        Line(L"timeout           " + std::to_wstring(timeoutMilliseconds) + L" ms");

        auto midiSession = MidiSession::Create(L"MIDI CI Dump");

        if (midiSession == nullptr)
        {
            Line(L"Could not create a MIDI session.");
            return 4;
        }

        auto connection = midiSession.CreateEndpointConnection(match.EndpointDeviceId());

        if (connection == nullptr || !connection.Open())
        {
            Line(L"Could not open the endpoint.");
            return 5;
        }

        auto session = MidiCapabilityInquirySession::Create(connection);

        if (session == nullptr)
        {
            Line(L"Could not create a capability inquiry session.");
            return 6;
        }

        session.Group(MidiGroup(groupIndex));
        session.ResponseTimeoutMilliseconds(timeoutMilliseconds);

        Line(L"our identifier    " + std::to_wstring(session.SourceMuid().AsCombined28BitValue()));
        Line();
        Line(L"=== Discovery ===");

        auto const responders = session.DiscoverAsync().get();

        if (responders == nullptr || responders.Size() == 0)
        {
            Line(L"Nothing answered Discovery.");
            session.Close();
            midiSession.Close();
            return 7;
        }

        for (auto const& responder : responders)
        {
            Line();
            Line(L"responder identifier  " + std::to_wstring(responder.Muid().AsCombined28BitValue()));
            Line(L"  message version     " + Hex(responder.MessageVersion()));
            Line(L"  categories          " + Hex(static_cast<uint8_t>(responder.SupportedCategories())));
            Line(L"  property exchange   " + std::wstring(responder.SupportsPropertyExchange() ? L"yes" : L"no"));
            Line(L"  profiles            " + std::wstring(responder.SupportsProfiles() ? L"yes" : L"no"));
            Line(L"  process inquiry     " + std::wstring(responder.SupportsProcessInquiry() ? L"yes" : L"no"));
            Line(L"  max sysex in        " + std::to_wstring(responder.ReceivableMaximumSystemExclusiveSize()));
            Line(L"  output path id      " + Hex(responder.OutputPathId()));
            Line(L"  function block      " + Hex(responder.FunctionBlockNumber()));

            auto const identity = responder.Identity();

            if (identity != nullptr)
            {
                auto const sysExId = identity.SystemExclusiveId();

                Line(L"  manufacturer        " +
                    (sysExId.size() >= 3
                        ? Hex(sysExId[0]) + L" " + Hex(sysExId[1]) + L" " + Hex(sysExId[2])
                        : std::wstring{ L"(missing)" }));

                Line(L"  family              " + Hex(identity.DeviceFamilyLsb()) + L" " +
                    Hex(identity.DeviceFamilyMsb()));
                Line(L"  model               " + Hex(identity.DeviceFamilyModelNumberLsb()) + L" " +
                    Hex(identity.DeviceFamilyModelNumberMsb()));
            }

            if (!responder.SupportsPropertyExchange())
            {
                Line(L"  does not do property exchange, so there is nothing to ask it for");
                continue;
            }

            auto const muid = responder.Muid();

            // Asked explicitly so the outcome is visible. The session also does this by itself
            // before its first property request, but silently.
            Line();
            Line(L"=== Property Exchange Capabilities ===");

            auto const capabilities = session.RequestPropertyExchangeCapabilitiesAsync(muid).get();

            Line(L"    status            " + std::to_wstring((int)capabilities) +
                L"  (0 success, 1 no response, 2 negative acknowledgment)");
            Line(L"    declared limit    " +
                std::to_wstring(session.GetResponder(muid).MaximumSimultaneousPropertyRequests()));

            Line();
            Line(L"=== ResourceList ===");
            DumpResourceList(Request(session, muid, L"ResourceList", {}));

            Line();
            Line(L"=== DeviceInfo ===");
            (void)Request(session, muid, L"DeviceInfo", {});

            Line();
            Line(L"=== ChannelList ===");
            auto const resourceIds = DumpChannelList(Request(session, muid, L"ChannelList", {}));

            Line();
            Line(L"=== ProgramList, with no resource id ===");
            (void)Request(session, muid, L"ProgramList", {});

            for (auto const& resourceId : resourceIds)
            {
                if (resourceId.empty())
                {
                    continue;
                }

                Line();
                Line(L"=== ProgramList, resId '" + resourceId + L"' ===");
                (void)Request(session, muid, L"ProgramList", resourceId);

                if (pageOffset >= 0 && pageLimit >= 0)
                {
                    Line();
                    Line(L"=== ProgramList, resId '" + resourceId + L"', offset " +
                        std::to_wstring(pageOffset) + L" limit " + std::to_wstring(pageLimit) + L" ===");
                    (void)Request(session, muid, L"ProgramList", resourceId, pageOffset, pageLimit);
                }
            }

            Line();
            Line(L"  declared simultaneous requests  " +
                std::to_wstring(session.GetResponder(muid).MaximumSimultaneousPropertyRequests()));
        }
        Line();
        Line(L"done");

        session.Close();
        midiSession.Close();

        return 0;
    }
}

int wmain(int argc, wchar_t** argv)
{
    winrt::init_apartment();

    try
    {
        auto const result = Run(argc, argv);

        if (g_file.is_open())
        {
            g_file.close();
        }

        return result;
    }
    catch (winrt::hresult_error const& error)
    {
        std::wcout << L"Failed: " << std::wstring{ error.message() } << std::endl;
        return 10;
    }
    catch (...)
    {
        std::wcout << L"Failed." << std::endl;
        return 11;
    }
}
