// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Compiled without a precompiled header, so it can be shared by projects whose headers differ.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include <string>
#include <string_view>

#include "SynthPropertyRequests.h"

namespace json = winrt::Windows::Data::Json;

namespace SoundFontSynth
{
    namespace
    {
        enum class ResourceLookup
        {
            Found,
            HeaderNotJson,
            UnknownResource,
        };

        struct ResourceRequest
        {
            std::vector<char> const* Blob{ nullptr };
            bool Cacheable{ true };

            bool IsProgramList{ false };
            std::string ResourceId{};
            size_t Offset{ 0 };
            size_t Limit{ SIZE_MAX };
        };

        // The header comes off the wire, so a key present with the wrong type is expected, not
        // exceptional. GetNamedString(key, default) throws on that, and an abandoned reply leaves
        // the initiator waiting out a timeout instead of getting an answer.
        std::wstring ReadString(_In_ json::JsonObject const& parsed, _In_ std::wstring_view key)
        {
            winrt::hstring const name{ key };

            if (!parsed.HasKey(name))
            {
                return {};
            }

            auto const found = parsed.Lookup(name);

            if (found == nullptr || found.ValueType() != json::JsonValueType::String)
            {
                return {};
            }

            return std::wstring{ found.GetString() };
        }

        // Negative and not-a-number are out of spec, so the default stands. An absurd but positive
        // value is clamped rather than ignored: an offset past the end has to stay past the end,
        // which is how an initiator paging forward learns it is done.
        size_t ReadCount(_In_ json::JsonObject const& parsed, _In_ std::wstring_view key, _In_ size_t fallback)
        {
            winrt::hstring const name{ key };

            if (!parsed.HasKey(name))
            {
                return fallback;
            }

            auto const found = parsed.Lookup(name);

            if (found == nullptr || found.ValueType() != json::JsonValueType::Number)
            {
                return fallback;
            }

            auto const value = found.GetNumber();

            if (!(value >= 0.0))
            {
                return fallback;
            }

            constexpr double ceiling = 1000000.0;

            return static_cast<size_t>((value > ceiling) ? ceiling : value);
        }

        // Resource and subscription ids are ASCII by definition. Anything else cannot match.
        std::string ToAscii(_In_ std::wstring const& text)
        {
            std::string result;
            result.reserve(text.size());

            for (auto const character : text)
            {
                result += (character > 0 && character < 0x80) ? static_cast<char>(character) : '?';
            }

            return result;
        }

        bool TryParseHeader(_In_ SynthDispatcher::PendingPropertyRequest const& request, _Out_ json::JsonObject& parsed)
        {
            parsed = nullptr;

            std::string_view const text(reinterpret_cast<char const*>(request.Header), request.HeaderByteCount);

            return json::JsonObject::TryParse(winrt::to_hstring(text), parsed);
        }

        ResourceLookup ResourceForHeader(
            _In_ SynthCore& core,
            _In_ SynthDispatcher::PendingPropertyRequest const& request,
            _Out_ ResourceRequest& result)
        {
            result = {};

            json::JsonObject parsed{ nullptr };

            if (!TryParseHeader(request, parsed))
            {
                return ResourceLookup::HeaderNotJson;
            }

            auto& properties = core.Properties();
            auto const resource = ReadString(parsed, L"resource");

            if (resource == L"ResourceList")
            {
                result.Blob = &properties.ResourceListJson();
                return ResourceLookup::Found;
            }

            if (resource == L"DeviceInfo")
            {
                result.Blob = &properties.DeviceInfoJson();
                return ResourceLookup::Found;
            }

            if (resource == L"ProgramList")
            {
                auto resourceId = ToAscii(ReadString(parsed, L"resId"));

                if (!SynthPropertySource::IsKnownProgramListResourceId(resourceId))
                {
                    return ResourceLookup::UnknownResource;
                }

                result.IsProgramList = true;
                result.ResourceId = std::move(resourceId);
                result.Offset = ReadCount(parsed, L"offset", 0);
                result.Limit = ReadCount(parsed, L"limit", SIZE_MAX);

                return ResourceLookup::Found;
            }

            // Rebuilt per request: it reflects what is selected right now, which is also why it is
            // the one resource sent without a cache time.
            if (resource == L"ChannelList")
            {
                result.Blob = &properties.RebuildChannelListJson(core.Engine(), core.Font());
                result.Cacheable = false;
                return ResourceLookup::Found;
            }

            return ResourceLookup::UnknownResource;
        }

        void HandleSubscriptionRequest(
            _In_ SynthCore& core,
            _In_ ISysExSink& wire,
            _In_ SynthDispatcher::PendingPropertyRequest const& request)
        {
            auto& properties = core.Properties();
            auto const muid = core.Dispatcher().Muid();

            json::JsonObject parsed{ nullptr };

            if (!TryParseHeader(request, parsed))
            {
                properties.SendSubscriptionReply(wire, muid, request, 400, nullptr);
                return;
            }

            auto const command = ReadString(parsed, L"command");
            auto const resource = ReadString(parsed, L"resource");
            auto const subscribeId = ToAscii(ReadString(parsed, L"subscribeId"));

            if (command == L"start")
            {
                // ChannelList is the only resource here that changes while the synthesizer runs,
                // so it is the only one the resource list declares as subscribable.
                if (resource != L"ChannelList")
                {
                    properties.SendSubscriptionReply(wire, muid, request, 405, nullptr);
                    return;
                }

                auto const* const assigned = properties.AddChannelListSubscription(request.InitiatorMuid);

                // Out of room. 507 tells the initiator to keep polling instead.
                properties.SendSubscriptionReply(wire, muid, request, (assigned[0] == '\0') ? 507 : 200, assigned);
                return;
            }

            if (command == L"end")
            {
                (void)properties.RemoveSubscription(request.InitiatorMuid, subscribeId);
                properties.SendSubscriptionReply(wire, muid, request, 200, nullptr);
                return;
            }

            // An initiator does not send full, partial or notify to a responder. Answering rather
            // than ignoring keeps it from waiting out a timeout.
            properties.SendSubscriptionReply(wire, muid, request, 400, nullptr);
        }

        void ServicePropertyRequestsInner(_In_ SynthCore& core, _In_ ISysExSink& wire)
        {
            auto& properties = core.Properties();
            auto& dispatcher = core.Dispatcher();

            if (!properties.ReplyInProgress())
            {
                uint32_t withdrawn{ 0 };

                if (dispatcher.TakeInvalidatedInitiatorMuid(withdrawn))
                {
                    (void)properties.RemoveSubscription(withdrawn, {});
                }

                SynthDispatcher::PendingPropertyRequest request{};

                if (dispatcher.TakePendingPropertyRequest(request))
                {
                    if (request.IsSubscription)
                    {
                        HandleSubscriptionRequest(core, wire, request);
                        return;
                    }

                    ResourceRequest resourceRequest{};

                    auto const lookup = ResourceForHeader(core, request, resourceRequest);

                    if (lookup != ResourceLookup::Found || (resourceRequest.Blob == nullptr && !resourceRequest.IsProgramList))
                    {
                        properties.SendNotFound(wire, dispatcher.Muid(), request);
                        return;
                    }

                    if (resourceRequest.IsProgramList)
                    {
                        properties.BeginProgramListReply(
                            core.Font(), request, resourceRequest.ResourceId, resourceRequest.Offset, resourceRequest.Limit);
                    }
                    else
                    {
                        properties.BeginReply(request, *resourceRequest.Blob, resourceRequest.Cacheable);
                    }
                }
                else if (properties.HasSubscriptions())
                {
                    // Nothing was asked for, so this is the moment to tell subscribers what moved.
                    (void)properties.ChannelListChanged(core.Engine());

                    if (!properties.BeginNextSubscriptionNotification())
                    {
                        return;
                    }
                }
                else
                {
                    return;
                }
            }

            // One chunk per pass. A whole program list is far more packets than an endpoint
            // should be handed at once.
            (void)properties.SendNextChunk(wire, dispatcher.Muid());
        }
    }

    _Use_decl_annotations_
    void ServicePropertyRequests(SynthCore& core, ISysExSink& wire) noexcept
    {
        try
        {
            core.ReplaceMuidIfNeeded();

            ServicePropertyRequestsInner(core, wire);
        }
        catch (...)
        {
            // An abandoned reply is recoverable. A half sent one that never restarts is not.
            core.Properties().AbandonReply();
        }
    }
}
