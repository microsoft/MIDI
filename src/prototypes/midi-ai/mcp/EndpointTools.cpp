// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "EndpointTools.h"
#include "ToolText.h"

namespace midimcp
{
    namespace
    {
        std::once_flag g_catalogStarted{};

        std::wstring ShortIdTail(std::wstring const& id)
        {
            // The tail of the id is what differs between two of the same device.
            auto const hash = id.find_last_of(L'#');
            auto const text = hash != std::wstring::npos && hash > 8 ? id.substr(0, hash) : id;

            return text.size() > 6 ? L"..." + text.substr(text.size() - 6) : text;
        }

        std::wstring GroupsText(midiapp::LiveEndpoint const& endpoint, bool isSource)
        {
            std::vector<std::wstring> parts{};
            auto const& present = isSource ? endpoint.SourceGroups : endpoint.DestinationGroups;

            for (int32_t i = 0; i < midiapp::MaximumGroupCount; i++)
            {
                if (present[static_cast<size_t>(i)])
                {
                    parts.push_back(DescribeGroup(endpoint, i, isSource));
                }
            }

            return parts.empty() ? L"none" : Join(parts, L", ");
        }

        std::wstring NameOfEndpoint(std::vector<midiapp::LiveEndpoint> const& all, std::wstring const& id)
        {
            for (auto const& endpoint : all)
            {
                if (EqualsIgnoringCase(endpoint.EndpointDeviceId, id))
                {
                    return endpoint.Name;
                }
            }

            return {};
        }

        std::wstring LoopbackText(midiapp::LiveEndpoint const& endpoint, std::vector<midiapp::LiveEndpoint> const& all)
        {
            if (!endpoint.IsLoopback)
            {
                return {};
            }

            if (endpoint.LoopbackPartnerEndpointId.empty())
            {
                return L"Loopback: whatever is sent to it comes straight back out of it.";
            }

            auto const partner = NameOfEndpoint(all, endpoint.LoopbackPartnerEndpointId);

            return L"Loopback: whatever is sent to it comes out of \"" +
                (partner.empty() ? endpoint.LoopbackPartnerEndpointId : partner) +
                L"\". This is how another app, such as a DAW, joins a patch.";
        }
    }

    std::vector<midiapp::LiveEndpoint> LiveEndpoints() noexcept
    {
        try
        {
            std::call_once(g_catalogStarted, []
                {
                    midiapp::SetEndpointErrorHandler([](std::wstring_view message)
                        {
                            LogLine(message);
                        });

                    midiapp::EndpointCatalog::Current().Start();
                });

            return midiapp::EndpointCatalog::Current().Snapshot();
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    EndpointLookup FindEndpoint(std::vector<midiapp::LiveEndpoint> const& endpoints, std::wstring const& nameOrId)
    {
        EndpointLookup lookup{};

        if (nameOrId.empty())
        {
            return lookup;
        }

        for (auto const& endpoint : endpoints)
        {
            if (EqualsIgnoringCase(endpoint.EndpointDeviceId, nameOrId))
            {
                lookup.Found = endpoint;
                return lookup;
            }
        }

        auto const collect = [&](auto&& predicate)
            {
                std::vector<midiapp::LiveEndpoint> matches{};

                for (auto const& endpoint : endpoints)
                {
                    if (predicate(endpoint))
                    {
                        matches.push_back(endpoint);
                    }
                }

                return matches;
            };

        auto exact = collect([&](midiapp::LiveEndpoint const& e)
            {
                return EqualsIgnoringCase(e.Name, nameOrId) || EqualsIgnoringCase(e.TransportSuppliedName, nameOrId);
            });

        if (exact.size() == 1)
        {
            lookup.Found = exact.front();
            return lookup;
        }

        if (exact.size() > 1)
        {
            lookup.Candidates = std::move(exact);
            return lookup;
        }

        auto partial = collect([&](midiapp::LiveEndpoint const& e)
            {
                return ContainsIgnoringCase(e.Name, nameOrId) || ContainsIgnoringCase(e.TransportSuppliedName, nameOrId);
            });

        if (partial.size() == 1)
        {
            lookup.Found = partial.front();
            return lookup;
        }

        lookup.Candidates = std::move(partial);
        return lookup;
    }

    _Use_decl_annotations_
    std::wstring DescribeEndpoint(midiapp::LiveEndpoint const& endpoint, std::vector<midiapp::LiveEndpoint> const& all)
    {
        auto const sameName = std::count_if(all.begin(), all.end(),
            [&endpoint](midiapp::LiveEndpoint const& other) { return EqualsIgnoringCase(other.Name, endpoint.Name); });

        if (sameName > 1)
        {
            return endpoint.Name + L" (" + endpoint.TransportCode + L", " + ShortIdTail(endpoint.EndpointDeviceId) + L")";
        }

        return endpoint.Name;
    }

    _Use_decl_annotations_
    std::wstring DescribeLookupProblem(
        std::wstring const& role,
        std::wstring const& asked,
        EndpointLookup const& lookup,
        std::vector<midiapp::LiveEndpoint> const& all)
    {
        if (lookup.Candidates.empty())
        {
            return role + L" \"" + asked + L"\" is not an endpoint on this PC right now. Call list_midi_endpoints "
                L"and ask the customer which one they mean. It may be unplugged or switched off.";
        }

        std::vector<std::wstring> names{};

        for (auto const& candidate : lookup.Candidates)
        {
            names.push_back(L"\"" + DescribeEndpoint(candidate, all) + L"\" (id " + candidate.EndpointDeviceId + L")");
        }

        return role + L" \"" + asked + L"\" matches " + std::to_wstring(lookup.Candidates.size()) +
            L" endpoints: " + Join(names, L"; ") + L". Ask the customer which one, then pass its id.";
    }

    _Use_decl_annotations_
    std::wstring DescribeGroup(midiapp::LiveEndpoint const& endpoint, int32_t groupIndex, bool isSource)
    {
        if (groupIndex < 0)
        {
            return L"all groups";
        }

        auto const& name = endpoint.GroupName(groupIndex, isSource);

        // A block named after its own endpoint adds nothing but length.
        return name.empty() || EqualsIgnoringCase(name, endpoint.Name)
            ? L"group " + std::to_wstring(groupIndex + 1)
            : L"group " + std::to_wstring(groupIndex + 1) + L" \"" + name + L"\"";
    }

    ToolDefinition MakeListEndpointsTool()
    {
        ToolDefinition tool{};

        tool.Name = L"list_midi_endpoints";
        tool.Title = L"List MIDI endpoints";
        tool.Description =
            L"Lists the MIDI endpoints on this PC right now: hardware, network and Bluetooth devices, "
            L"loopbacks and virtual devices. For each one it gives the name the customer sees, its id, and "
            L"which groups it sends on and receives on, with the name of each group. Call this before "
            L"drafting anything, and use the names from here when you talk to the customer. Group numbers "
            L"are 1 to 16, the way the MIDI tools show them.";
        tool.InputSchema = LR"({
            "type": "object",
            "properties": {
                "nameContains": { "type": "string", "description": "Only endpoints whose name contains this text." },
                "includeLoopbacks": { "type": "boolean", "description": "Include loopback endpoints. Default true." }
            },
            "additionalProperties": false
        })";
        tool.Annotations = { true, false, true, false };

        tool.Handler = [](json::JsonObject const& arguments, CallContext const&)
            {
                auto const filter = StringOrEmpty(arguments, L"nameContains");
                auto const includeLoopbacks = OptionalBool(arguments, L"includeLoopbacks").value_or(true);

                auto const all = LiveEndpoints();

                std::wstring text{};
                size_t count{ 0 };

                for (auto const& endpoint : all)
                {
                    if (!filter.empty() && !ContainsIgnoringCase(endpoint.Name, filter) &&
                        !ContainsIgnoringCase(endpoint.TransportSuppliedName, filter))
                    {
                        continue;
                    }

                    if (!includeLoopbacks && endpoint.IsLoopback)
                    {
                        continue;
                    }

                    count++;

                    text += std::to_wstring(count) + L". " + DescribeEndpoint(endpoint, all);

                    if (!endpoint.TransportCode.empty())
                    {
                        text += L" [" + endpoint.TransportCode + L"]";
                    }

                    if (!endpoint.ManufacturerName.empty())
                    {
                        text += L", " + endpoint.ManufacturerName;
                    }

                    // Never a serial number or device instance id: they identify the hardware.
                    text += L"\n   id: " + endpoint.EndpointDeviceId;

                    if (!endpoint.Description.empty())
                    {
                        text += L"\n   description: " + endpoint.Description;
                    }

                    text += L"\n   sends on: " + GroupsText(endpoint, true);
                    text += L"\n   receives on: " + GroupsText(endpoint, false);

                    if (auto const loop = LoopbackText(endpoint, all); !loop.empty())
                    {
                        text += L"\n   " + loop;
                    }

                    text += L"\n";
                }

                auto const serviceRunning = midiapp::EndpointCatalog::Current().IsServiceAvailable();

                std::wstring header = std::to_wstring(count) + L" MIDI endpoint" + (count == 1 ? L"" : L"s") + L" on this PC. ";
                header += serviceRunning
                    ? L"The MIDI service is running.\n\n"
                    : L"The MIDI service is not running, so nothing can be listed or routed until it starts.\n\n";

                ToolResult result{};
                result.AddText(header + text);

                return result;
            };

        return tool;
    }
}
