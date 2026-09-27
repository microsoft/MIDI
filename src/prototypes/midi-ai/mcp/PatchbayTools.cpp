// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "PatchbayTools.h"
#include "EndpointTools.h"
#include "ToolText.h"

// The file this writes is MIDI Patchbay's own .midipatch, key for key: see PatchStore.cpp,
// MessageFilter.cpp and MessageTransform.cpp in src/in-box/user-tools/midi-patchbay. A shipping
// version would compile those files instead of repeating their shape here; they include the
// app's precompiled header, which is the only reason this prototype does not.

namespace midimcp
{
    namespace
    {
        constexpr wchar_t PatchFolderName[] = L"MIDI Patchbay";
        constexpr wchar_t PatchFileExtension[] = L".midipatch";

        // What the first builds of MIDI Patchbay wrote. It renames them when it starts.
        constexpr wchar_t LegacyPatchFileExtension[] = L".midipatch.json";

        constexpr int32_t AllGroups = -1;
        constexpr int32_t GroupCount = 16;
        constexpr size_t MaximumRoutes = 64;
        constexpr size_t MaximumTextLength = 1024;
        constexpr int32_t FullScaleHundredths = 10000;

        constexpr int32_t VelocityUnchanged = 0;
        constexpr int32_t VelocitySofter = 1;
        constexpr int32_t VelocityLouder = 2;
        constexpr int32_t VelocityFixed = 3;

        constexpr uint16_t Bit(int n) noexcept { return static_cast<uint16_t>(1u << n); }

        // Channel voice messages ride on both the MIDI 1.0 and the MIDI 2.0 message types.
        constexpr uint16_t VoiceTypes = Bit(0x2) | Bit(0x4);

        // The system messages MIDI Patchbay breaks out, in its own order:
        // F1 F2 F3 F6 F8 FA FB FC FE FF.
        constexpr size_t SystemEntryCount = 10;
        constexpr uint16_t AllSystemEntries = static_cast<uint16_t>((1u << SystemEntryCount) - 1);

        // What a customer means by "only notes" or "no clock", as switches in MIDI Patchbay's filter.
        struct MessageKindInfo
        {
            wchar_t const* Name;
            wchar_t const* Meaning;
            uint16_t Types;
            uint16_t VoiceStatuses;
            uint16_t SystemEntries;
        };

        constexpr MessageKindInfo MessageKinds[] =
        {
            { L"notes", L"note on and note off", VoiceTypes, static_cast<uint16_t>(Bit(0x8) | Bit(0x9)), 0 },
            { L"polyPressure", L"polyphonic (per key) aftertouch", VoiceTypes, Bit(0xA), 0 },
            { L"controlChanges", L"control change, including MIDI 1.0 RPN and NRPN", VoiceTypes, Bit(0xB), 0 },
            { L"programChanges", L"program change", VoiceTypes, Bit(0xC), 0 },
            { L"channelPressure", L"channel aftertouch", VoiceTypes, Bit(0xD), 0 },
            { L"pitchBend", L"pitch bend", VoiceTypes, Bit(0xE), 0 },
            { L"midi2Controllers", L"MIDI 2.0 registered and assignable controllers", Bit(0x4),
                static_cast<uint16_t>(Bit(0x2) | Bit(0x3) | Bit(0x4) | Bit(0x5)), 0 },
            { L"midi2PerNote", L"MIDI 2.0 per note controllers, per note pitch bend and per note management", Bit(0x4),
                static_cast<uint16_t>(Bit(0x0) | Bit(0x1) | Bit(0x6) | Bit(0xF)), 0 },
            { L"clock", L"timing clock", Bit(0x1), 0, Bit(4) },
            { L"transport", L"start, continue, stop, song position and song select", Bit(0x1), 0,
                static_cast<uint16_t>(Bit(1) | Bit(2) | Bit(5) | Bit(6) | Bit(7)) },
            { L"timecode", L"MIDI time code", Bit(0x1), 0, Bit(0) },
            { L"tuneRequest", L"tune request", Bit(0x1), 0, Bit(3) },
            { L"activeSensing", L"active sensing", Bit(0x1), 0, Bit(8) },
            { L"systemReset", L"system reset", Bit(0x1), 0, Bit(9) },
            { L"sysex", L"system exclusive", static_cast<uint16_t>(Bit(0x3) | Bit(0x5)), 0, 0 },
            { L"utility", L"UMP utility messages such as timestamps", Bit(0x0), 0, 0 },
            { L"flexData", L"flex data such as tempo and chord names", Bit(0xD), 0, 0 },
            { L"stream", L"UMP stream messages such as endpoint information", Bit(0xF), 0, 0 },
        };

        MessageKindInfo const* FindMessageKind(std::wstring const& name) noexcept
        {
            for (auto const& kind : MessageKinds)
            {
                if (EqualsIgnoringCase(kind.Name, name))
                {
                    return &kind;
                }
            }

            return nullptr;
        }

        std::wstring MessageKindNameList()
        {
            std::vector<std::wstring> names{};

            for (auto const& kind : MessageKinds)
            {
                names.push_back(kind.Name);
            }

            return Join(names, L", ");
        }

        struct RoutePoint
        {
            std::wstring Asked{};
            midiapp::LiveEndpoint Endpoint{};
            int32_t Group{ AllGroups };
            bool Resolved{ false };
        };

        struct Route
        {
            RoutePoint From{};
            RoutePoint To{};

            // What passes. All open unless the request narrowed it.
            std::array<bool, 16> Channels{};
            uint16_t Types{ 0xFFFF };
            uint16_t VoiceStatuses{ 0xFFFF };
            uint16_t SystemEntries{ AllSystemEntries };
            bool LimitNotes{ false };
            uint8_t LowestNote{ 0 };
            uint8_t HighestNote{ 127 };
            std::vector<std::wstring> OnlyKinds{};
            std::vector<std::wstring> BlockedKinds{};

            // What changes on the way through.
            int32_t Transpose{ 0 };
            std::array<int16_t, 16> ChannelMap{};
            std::array<int16_t, 128> NoteMap{};
            std::array<int16_t, 128> ControlMap{};
            std::array<int16_t, 128> ProgramMap{};
            int32_t VelocityCurve{ VelocityUnchanged };
            int32_t FixedVelocityHundredths{ 7874 };
            bool RescaleVelocity{ false };
            int32_t MinimumVelocityHundredths{ 0 };
            int32_t MaximumVelocityHundredths{ FullScaleHundredths };

            Route() noexcept
            {
                Channels.fill(true);
                ChannelMap.fill(-1);
                NoteMap.fill(-1);
                ControlMap.fill(-1);
                ProgramMap.fill(-1);
            }

            bool FilterExcludesAnything() const noexcept
            {
                return LimitNotes || Types != 0xFFFF || VoiceStatuses != 0xFFFF || SystemEntries != AllSystemEntries ||
                    std::any_of(Channels.begin(), Channels.end(), [](bool on) { return !on; });
            }

            bool TransformChangesAnything() const noexcept
            {
                auto const any = [](auto const& map) { return std::any_of(map.begin(), map.end(), [](int16_t v) { return v >= 0; }); };

                return Transpose != 0 || VelocityCurve != VelocityUnchanged || RescaleVelocity ||
                    any(ChannelMap) || any(NoteMap) || any(ControlMap) || any(ProgramMap);
            }
        };

        struct PatchRequest
        {
            std::wstring Name{};
            std::wstring Description{};
            std::wstring Request{};
            std::vector<Route> Routes{};
        };

        std::wstring Bounded(std::wstring text)
        {
            std::erase_if(text, [](wchar_t c) { return c < L' '; });

            if (text.size() > MaximumTextLength)
            {
                text.resize(MaximumTextLength);
            }

            return text;
        }

        std::wstring PatchFolder(PatchbayToolOptions const& options)
        {
            if (!options.PatchFolder.empty())
            {
                return options.PatchFolder;
            }

            wil::unique_cotaskmem_string documents{};

            if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents)) && documents)
            {
                return (std::filesystem::path{ documents.get() } / PatchFolderName).wstring();
            }

            return {};
        }

        // ------------------------------------------------------------------------------------
        // Reading the request

        bool ReadPoint(
            json::JsonObject const& route,
            wchar_t const* key,
            bool isSource,
            std::vector<midiapp::LiveEndpoint> const& live,
            size_t routeNumber,
            RoutePoint& point,
            Problems& problems)
        {
            auto const role = std::wstring{ L"Route " } + std::to_wstring(routeNumber) + (isSource ? L" from" : L" to");

            json::JsonObject object{ nullptr };
            std::wstring asked{};
            json::IJsonValue groupValue{ nullptr };

            if (auto const value = route.HasKey(key) ? route.GetNamedValue(key) : nullptr; value != nullptr)
            {
                if (value.ValueType() == json::JsonValueType::String)
                {
                    asked = std::wstring{ value.GetString() };
                }
                else if (value.ValueType() == json::JsonValueType::Object)
                {
                    object = value.GetObject();
                    asked = StringOrEmpty(object, L"endpoint");
                    groupValue = object.HasKey(L"group") ? object.GetNamedValue(L"group") : nullptr;
                }
            }

            if (asked.empty())
            {
                problems.Errors.push_back(role + L": name the endpoint, for example { \"endpoint\": \"Launchkey 49\", \"group\": 1 }.");
                return false;
            }

            point.Asked = asked;

            auto const lookup = FindEndpoint(live, asked);

            if (!lookup.Found)
            {
                problems.Errors.push_back(DescribeLookupProblem(role, asked, lookup, live));
                return false;
            }

            point.Endpoint = *lookup.Found;
            point.Resolved = true;

            auto const& present = isSource ? point.Endpoint.SourceGroups : point.Endpoint.DestinationGroups;
            auto const anyGroup = std::any_of(present.begin(), present.end(), [](bool on) { return on; });

            if (!anyGroup)
            {
                problems.Errors.push_back(role + L": \"" + point.Endpoint.Name + (isSource
                    ? L"\" does not send MIDI, so it cannot be the start of a route."
                    : L"\" does not receive MIDI, so it cannot be the end of a route."));
                return false;
            }

            point.Group = AllGroups;

            if (groupValue != nullptr && groupValue.ValueType() != json::JsonValueType::Null)
            {
                auto const isAll = groupValue.ValueType() == json::JsonValueType::String &&
                    EqualsIgnoringCase(groupValue.GetString(), L"all");

                if (!isAll)
                {
                    auto const number = IntegerFromValue(groupValue);

                    if (!number || *number < 1 || *number > GroupCount)
                    {
                        problems.Errors.push_back(role + L": group must be 1 to 16, or \"all\".");
                        return false;
                    }

                    auto const index = static_cast<int32_t>(*number - 1);

                    if (!present[static_cast<size_t>(index)])
                    {
                        std::vector<std::wstring> offered{};

                        for (int32_t i = 0; i < GroupCount; i++)
                        {
                            if (present[static_cast<size_t>(i)])
                            {
                                offered.push_back(DescribeGroup(point.Endpoint, i, isSource));
                            }
                        }

                        problems.Errors.push_back(role + L": \"" + point.Endpoint.Name + L"\" does not " +
                            (isSource ? L"send" : L"receive") + L" on group " + std::to_wstring(*number) +
                            L". It " + (isSource ? L"sends" : L"receives") + L" on " + Join(offered, L", ") + L".");
                        return false;
                    }

                    point.Group = index;
                }
            }

            return true;
        }

        bool ReadIntegerPairs(
            json::JsonArray const& pairs,
            wchar_t const* what,
            int32_t low,
            int32_t high,
            int32_t offset,
            bool notes,
            int16_t* map,
            size_t size,
            std::wstring const& role,
            Problems& problems)
        {
            if (pairs == nullptr)
            {
                return true;
            }

            for (auto const& item : pairs)
            {
                if (item.ValueType() != json::JsonValueType::Object)
                {
                    problems.Errors.push_back(role + L": each " + what + L" entry is { \"from\": ..., \"to\": ... }.");
                    return false;
                }

                auto const entry = item.GetObject();
                std::optional<int64_t> from{};
                std::optional<int64_t> to{};

                if (notes)
                {
                    auto const f = ParseNote(entry.HasKey(L"from") ? entry.GetNamedValue(L"from") : nullptr);
                    auto const t = ParseNote(entry.HasKey(L"to") ? entry.GetNamedValue(L"to") : nullptr);

                    if (f) from = *f;
                    if (t) to = *t;
                }
                else
                {
                    from = OptionalInteger(entry, L"from");
                    to = OptionalInteger(entry, L"to");
                }

                if (!from || !to || *from < low || *from > high || *to < low || *to > high)
                {
                    problems.Errors.push_back(role + L": " + what + L" values must be " + std::to_wstring(low) +
                        L" to " + std::to_wstring(high) + (notes ? L", or note names such as " + NoteName(60) : L"") + L".");
                    return false;
                }

                auto const index = static_cast<size_t>(*from - offset);

                if (index < size)
                {
                    map[index] = static_cast<int16_t>(*to - offset);
                }
            }

            return true;
        }

        void ReadFilter(json::JsonObject const& item, Route& route, std::wstring const& role, Problems& problems)
        {
            if (auto const channels = ArrayOrNull(item, L"channels"); channels != nullptr)
            {
                route.Channels.fill(false);

                for (auto const& value : channels)
                {
                    auto const channel = IntegerFromValue(value);

                    if (!channel || *channel < 1 || *channel > 16)
                    {
                        problems.Errors.push_back(role + L": channels are 1 to 16.");
                        return;
                    }

                    route.Channels[static_cast<size_t>(*channel - 1)] = true;
                }

                if (std::none_of(route.Channels.begin(), route.Channels.end(), [](bool on) { return on; }))
                {
                    problems.Errors.push_back(role + L": an empty channel list would pass nothing. Leave it out to pass every channel.");
                }
            }

            auto const only = ArrayOrNull(item, L"messages");
            auto const block = ArrayOrNull(item, L"block");

            if (only != nullptr && block != nullptr)
            {
                problems.Errors.push_back(role + L": use \"messages\" (only these) or \"block\" (all but these), not both.");
                return;
            }

            auto const readKinds = [&](json::JsonArray const& list, std::vector<MessageKindInfo const*>& kinds)
                {
                    for (auto const& value : list)
                    {
                        auto const name = value.ValueType() == json::JsonValueType::String ? std::wstring{ value.GetString() } : std::wstring{};
                        auto const kind = FindMessageKind(name);

                        if (kind == nullptr)
                        {
                            problems.Errors.push_back(role + L": \"" + name + L"\" is not a message kind. Use: " + MessageKindNameList() + L".");
                            return false;
                        }

                        kinds.push_back(kind);
                    }

                    return true;
                };

            if (only != nullptr)
            {
                std::vector<MessageKindInfo const*> kinds{};

                if (!readKinds(only, kinds))
                {
                    return;
                }

                if (kinds.empty())
                {
                    problems.Errors.push_back(role + L": an empty \"messages\" list would pass nothing.");
                    return;
                }

                route.Types = 0;
                route.VoiceStatuses = 0;
                route.SystemEntries = 0;

                for (auto const kind : kinds)
                {
                    route.Types |= kind->Types;
                    route.VoiceStatuses |= kind->VoiceStatuses;
                    route.SystemEntries |= kind->SystemEntries;
                    route.OnlyKinds.push_back(kind->Name);
                }
            }

            if (block != nullptr)
            {
                std::vector<MessageKindInfo const*> kinds{};

                if (!readKinds(block, kinds))
                {
                    return;
                }

                for (auto const kind : kinds)
                {
                    // A kind that is part of a message type narrows the type; a kind that is the
                    // whole type switches the type off.
                    if (kind->VoiceStatuses != 0 || kind->SystemEntries != 0)
                    {
                        route.VoiceStatuses &= static_cast<uint16_t>(~kind->VoiceStatuses);
                        route.SystemEntries &= static_cast<uint16_t>(~kind->SystemEntries);
                    }
                    else
                    {
                        route.Types &= static_cast<uint16_t>(~kind->Types);
                    }

                    route.BlockedKinds.push_back(kind->Name);
                }
            }

            if (auto const range = ObjectOrNull(item, L"noteRange"); range != nullptr)
            {
                auto const lowest = ParseNote(range.HasKey(L"lowest") ? range.GetNamedValue(L"lowest") : nullptr);
                auto const highest = ParseNote(range.HasKey(L"highest") ? range.GetNamedValue(L"highest") : nullptr);

                if ((range.HasKey(L"lowest") && !lowest) || (range.HasKey(L"highest") && !highest))
                {
                    problems.Errors.push_back(role + L": note range ends are 0 to 127 or note names such as " + NoteName(60) +
                        L" (note 60 is " + NoteName(60) + L" in the MIDI tools).");
                    return;
                }

                route.LimitNotes = true;
                route.LowestNote = lowest.value_or(0);
                route.HighestNote = highest.value_or(127);

                if (route.LowestNote > route.HighestNote)
                {
                    std::swap(route.LowestNote, route.HighestNote);
                    problems.Warnings.push_back(role + L": the note range was given high to low, so it was turned around.");
                }
            }
        }

        void ReadTransform(json::JsonObject const& item, Route& route, std::wstring const& role, Problems& problems)
        {
            if (auto const transpose = OptionalInteger(item, L"transposeSemitones"); transpose.has_value())
            {
                if (*transpose < -48 || *transpose > 48)
                {
                    problems.Errors.push_back(role + L": transposeSemitones must be -48 to 48.");
                    return;
                }

                route.Transpose = static_cast<int32_t>(*transpose);
            }
            else if (item.HasKey(L"transposeSemitones"))
            {
                problems.Errors.push_back(role + L": transposeSemitones must be a whole number.");
                return;
            }

            if (!ReadIntegerPairs(ArrayOrNull(item, L"channelMap"), L"channelMap", 1, 16, 1, false,
                route.ChannelMap.data(), route.ChannelMap.size(), role, problems) ||
                !ReadIntegerPairs(ArrayOrNull(item, L"noteMap"), L"noteMap", 0, 127, 0, true,
                    route.NoteMap.data(), route.NoteMap.size(), role, problems) ||
                !ReadIntegerPairs(ArrayOrNull(item, L"controllerMap"), L"controllerMap", 0, 127, 0, false,
                    route.ControlMap.data(), route.ControlMap.size(), role, problems) ||
                !ReadIntegerPairs(ArrayOrNull(item, L"programMap"), L"programMap", 1, 128, 1, false,
                    route.ProgramMap.data(), route.ProgramMap.size(), role, problems))
            {
                return;
            }

            if (auto const velocity = ObjectOrNull(item, L"velocity"); velocity != nullptr)
            {
                auto const curve = StringOrEmpty(velocity, L"curve");

                if (curve.empty() || EqualsIgnoringCase(curve, L"unchanged"))
                {
                    route.VelocityCurve = VelocityUnchanged;
                }
                else if (EqualsIgnoringCase(curve, L"softer"))
                {
                    route.VelocityCurve = VelocitySofter;
                }
                else if (EqualsIgnoringCase(curve, L"louder"))
                {
                    route.VelocityCurve = VelocityLouder;
                }
                else if (EqualsIgnoringCase(curve, L"fixed"))
                {
                    route.VelocityCurve = VelocityFixed;
                }
                else
                {
                    problems.Errors.push_back(role + L": velocity curve is one of unchanged, softer, louder, fixed.");
                    return;
                }

                auto const readPercent = [&](wchar_t const* key, int32_t& target) -> bool
                    {
                        if (!velocity.HasKey(key))
                        {
                            return true;
                        }

                        auto const percent = OptionalNumber(velocity, key);

                        if (!percent || *percent < 0.0 || *percent > 100.0)
                        {
                            problems.Errors.push_back(role + L": velocity " + key + L" is a percentage, 0 to 100.");
                            return false;
                        }

                        target = static_cast<int32_t>(std::lround(*percent * 100.0));
                        return true;
                    };

                if (route.VelocityCurve == VelocityFixed)
                {
                    if (!velocity.HasKey(L"fixedPercent"))
                    {
                        problems.Errors.push_back(role + L": a fixed velocity needs fixedPercent, 1 to 100.");
                        return;
                    }

                    if (!readPercent(L"fixedPercent", route.FixedVelocityHundredths))
                    {
                        return;
                    }
                }

                if (velocity.HasKey(L"minimumPercent") || velocity.HasKey(L"maximumPercent"))
                {
                    if (!readPercent(L"minimumPercent", route.MinimumVelocityHundredths) ||
                        !readPercent(L"maximumPercent", route.MaximumVelocityHundredths))
                    {
                        return;
                    }

                    if (route.MinimumVelocityHundredths > route.MaximumVelocityHundredths)
                    {
                        std::swap(route.MinimumVelocityHundredths, route.MaximumVelocityHundredths);
                    }

                    route.RescaleVelocity = true;

                    if (route.VelocityCurve == VelocityFixed)
                    {
                        problems.Warnings.push_back(role + L": a fixed velocity ignores the velocity range.");
                    }
                }
            }
        }

        constexpr std::wstring_view KnownRouteKeys[] =
        {
            L"from", L"to", L"channels", L"messages", L"block", L"noteRange", L"transposeSemitones",
            L"channelMap", L"noteMap", L"controllerMap", L"programMap", L"velocity",
        };

        PatchRequest ReadRequest(json::JsonObject const& arguments, std::vector<midiapp::LiveEndpoint> const& live, Problems& problems)
        {
            PatchRequest request{};

            request.Name = Bounded(StringOrEmpty(arguments, L"name"));
            request.Description = Bounded(StringOrEmpty(arguments, L"description"));
            request.Request = Bounded(StringOrEmpty(arguments, L"request"));

            while (!request.Name.empty() && request.Name.back() == L' ')
            {
                request.Name.pop_back();
            }

            if (request.Name.empty())
            {
                problems.Errors.push_back(L"Give the patch a name, the way the customer would say it.");
            }

            auto const routes = ArrayOrNull(arguments, L"routes");

            if (routes == nullptr || routes.Size() == 0)
            {
                problems.Errors.push_back(L"A patch needs at least one route in \"routes\".");
                return request;
            }

            if (routes.Size() > MaximumRoutes)
            {
                problems.Errors.push_back(L"A draft can hold at most " + std::to_wstring(MaximumRoutes) + L" routes.");
                return request;
            }

            size_t number{ 0 };

            for (auto const& value : routes)
            {
                number++;
                auto const role = L"Route " + std::to_wstring(number);

                if (value.ValueType() != json::JsonValueType::Object)
                {
                    problems.Errors.push_back(role + L" is not an object.");
                    continue;
                }

                auto const item = value.GetObject();

                for (auto const& pair : item)
                {
                    auto const key = std::wstring{ pair.Key() };

                    if (std::find(std::begin(KnownRouteKeys), std::end(KnownRouteKeys), key) == std::end(KnownRouteKeys))
                    {
                        problems.Warnings.push_back(role + L": \"" + key + L"\" is not something a route can do, so it was ignored.");
                    }
                }

                Route route{};

                auto const fromOk = ReadPoint(item, L"from", true, live, number, route.From, problems);
                auto const toOk = ReadPoint(item, L"to", false, live, number, route.To, problems);

                ReadFilter(item, route, role, problems);
                ReadTransform(item, route, role, problems);

                if (fromOk && toOk &&
                    EqualsIgnoringCase(route.From.Endpoint.EndpointDeviceId, route.To.Endpoint.EndpointDeviceId) &&
                    route.From.Group == route.To.Group)
                {
                    problems.Errors.push_back(role + L" goes from \"" + route.From.Endpoint.Name + L"\" back into the same group of itself. "
                        L"The two ends of a route on one endpoint need different groups.");
                }

                for (size_t earlier = 0; earlier < request.Routes.size(); earlier++)
                {
                    auto const& other = request.Routes[earlier];

                    if (EqualsIgnoringCase(other.From.Endpoint.EndpointDeviceId, route.From.Endpoint.EndpointDeviceId) &&
                        EqualsIgnoringCase(other.To.Endpoint.EndpointDeviceId, route.To.Endpoint.EndpointDeviceId) &&
                        other.From.Group == route.From.Group && other.To.Group == route.To.Group && fromOk && toOk)
                    {
                        problems.Errors.push_back(role + L" repeats route " + std::to_wstring(earlier + 1) +
                            L". Put both sets of filters and changes on one route, or use different groups.");
                    }
                }

                request.Routes.push_back(std::move(route));
            }

            return request;
        }

        // ------------------------------------------------------------------------------------
        // Loops. Loopbacks are the only endpoints known for certain to echo what they receive, so
        // they are the only way a loop can be proven from here. A vertex is (endpoint, side, group);
        // without the side a plain two way link between a keyboard and a synth would read as a loop.

        struct Edge
        {
            size_t From{};
            size_t To{};
            bool FromDraft{ false };
            size_t RouteIndex{ 0 };
            std::wstring OtherPatch{};
        };

        class LoopGraph
        {
        public:
            explicit LoopGraph(std::vector<midiapp::LiveEndpoint> const& live)
            {
                for (auto const& endpoint : live)
                {
                    IndexOf(endpoint.EndpointDeviceId);
                }

                // What goes into a loopback comes out of its partner, or out of itself.
                for (auto const& endpoint : live)
                {
                    if (!endpoint.IsLoopback)
                    {
                        continue;
                    }

                    auto const echo = endpoint.LoopbackPartnerEndpointId.empty()
                        ? endpoint.EndpointDeviceId
                        : endpoint.LoopbackPartnerEndpointId;

                    for (int32_t group = 0; group < GroupCount; group++)
                    {
                        Add(Vertex(endpoint.EndpointDeviceId, false, group), Vertex(echo, true, group), Edge{});
                    }
                }
            }

            void AddRoute(std::wstring const& fromId, int32_t fromGroup, std::wstring const& toId, int32_t toGroup, Edge const& tag)
            {
                for (int32_t group = 0; group < GroupCount; group++)
                {
                    if (fromGroup != AllGroups && group != fromGroup)
                    {
                        continue;
                    }

                    // All groups to one group gathers them; one group to all groups keeps its number.
                    auto const target = toGroup == AllGroups ? group : toGroup;

                    Add(Vertex(fromId, true, group), Vertex(toId, false, target), tag);
                }
            }

            // Every draft route that closes a circle, once each.
            std::vector<Edge> DraftEdgesInLoops() const
            {
                std::vector<Edge> found{};
                std::set<size_t> reported{};

                for (auto const& edge : m_edges)
                {
                    if (!edge.FromDraft || reported.count(edge.RouteIndex) != 0)
                    {
                        continue;
                    }

                    if (Reaches(edge.To, edge.From))
                    {
                        found.push_back(edge);
                        reported.insert(edge.RouteIndex);
                    }
                }

                return found;
            }

            // Whether a loop found for this route needs another saved patch to close it.
            bool LoopUsesOtherPatch(Edge const& start, std::wstring& otherPatch) const
            {
                std::vector<bool> seen(m_adjacency.size(), false);
                std::vector<std::pair<size_t, std::wstring>> stack{ { start.To, std::wstring{} } };

                while (!stack.empty())
                {
                    auto [vertex, via] = stack.back();
                    stack.pop_back();

                    if (vertex == start.From)
                    {
                        otherPatch = via;
                        return !via.empty();
                    }

                    if (seen[vertex])
                    {
                        continue;
                    }

                    seen[vertex] = true;

                    for (auto const edgeIndex : m_adjacency[vertex])
                    {
                        auto const& edge = m_edges[edgeIndex];
                        stack.emplace_back(edge.To, via.empty() ? edge.OtherPatch : via);
                    }
                }

                return false;
            }

        private:
            size_t IndexOf(std::wstring const& id)
            {
                std::wstring key{ id };
                std::transform(key.begin(), key.end(), key.begin(), [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

                auto const found = m_endpoints.find(key);

                if (found != m_endpoints.end())
                {
                    return found->second;
                }

                auto const index = m_endpoints.size();
                m_endpoints.emplace(key, index);
                m_adjacency.resize((index + 1) * 2 * GroupCount);

                return index;
            }

            size_t Vertex(std::wstring const& id, bool isOut, int32_t group)
            {
                return (IndexOf(id) * 2 + (isOut ? 1 : 0)) * GroupCount + static_cast<size_t>(group);
            }

            void Add(size_t from, size_t to, Edge tag)
            {
                tag.From = from;
                tag.To = to;

                m_adjacency.resize(std::max(m_adjacency.size(), std::max(from, to) + 1));
                m_adjacency[from].push_back(m_edges.size());
                m_edges.push_back(std::move(tag));
            }

            bool Reaches(size_t from, size_t to) const
            {
                std::vector<bool> seen(m_adjacency.size(), false);
                std::vector<size_t> stack{ from };

                while (!stack.empty())
                {
                    auto const vertex = stack.back();
                    stack.pop_back();

                    if (vertex == to)
                    {
                        return true;
                    }

                    if (seen[vertex])
                    {
                        continue;
                    }

                    seen[vertex] = true;

                    for (auto const edgeIndex : m_adjacency[vertex])
                    {
                        stack.push_back(m_edges[edgeIndex].To);
                    }
                }

                return false;
            }

            std::map<std::wstring, size_t> m_endpoints{};
            std::vector<std::vector<size_t>> m_adjacency{};
            std::vector<Edge> m_edges{};
        };

        // ------------------------------------------------------------------------------------
        // Saved patches

        struct SavedPatch
        {
            std::wstring FileName{};
            std::wstring Name{};
            std::wstring Description{};
            bool ActivateAtStartup{ true };
            bool IsDraft{ false };
            std::wstring DraftedBy{};
            std::vector<std::wstring> EndpointNames{};
            std::vector<std::wstring> MissingEndpointNames{};
            size_t ConnectionCount{ 0 };

            // Resolved to live endpoint ids through the saved device id, for the loop check.
            struct Connection
            {
                std::wstring FromId{};
                int32_t FromGroup{ AllGroups };
                std::wstring ToId{};
                int32_t ToGroup{ AllGroups };
            };

            std::vector<Connection> Connections{};
        };

        std::optional<json::JsonObject> ReadJsonFile(std::filesystem::path const& path)
        {
            std::error_code ec{};
            auto const size = std::filesystem::file_size(path, ec);

            // The same bound the app itself uses for a patch file.
            if (ec || size == 0 || size > 4 * 1024 * 1024)
            {
                return std::nullopt;
            }

            wil::unique_hfile file{ ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) };

            if (!file)
            {
                return std::nullopt;
            }

            std::string bytes(static_cast<size_t>(size), '\0');
            DWORD read{ 0 };

            if (!::ReadFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr))
            {
                return std::nullopt;
            }

            bytes.resize(read);

            if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
                static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF)
            {
                bytes.erase(0, 3);
            }

            json::JsonObject root{ nullptr };

            if (!json::JsonObject::TryParse(Utf8ToWide(bytes), root) || root == nullptr)
            {
                return std::nullopt;
            }

            return root;
        }

        int32_t GroupFromFile(json::JsonObject const& item, wchar_t const* key)
        {
            auto const value = OptionalInteger(item, key);

            return value && *value >= 0 && *value < GroupCount ? static_cast<int32_t>(*value) : AllGroups;
        }

        std::vector<SavedPatch> ReadSavedPatches(std::wstring const& folder, std::vector<midiapp::LiveEndpoint> const& live)
        {
            std::vector<SavedPatch> patches{};
            std::error_code ec{};

            if (folder.empty() || !std::filesystem::exists(folder, ec))
            {
                return patches;
            }

            for (auto const& entry : std::filesystem::directory_iterator{ folder, ec })
            {
                if (ec || patches.size() >= 256)
                {
                    break;
                }

                auto const name = entry.path().filename().wstring();

                auto const endsWith = [&name](std::wstring_view extension)
                    {
                        return name.size() > extension.size() &&
                            EqualsIgnoringCase(std::wstring_view{ name }.substr(name.size() - extension.size()), extension);
                    };

                size_t extensionLength{ 0 };

                if (endsWith(PatchFileExtension))
                {
                    extensionLength = std::size(PatchFileExtension) - 1;
                }
                else if (endsWith(LegacyPatchFileExtension))
                {
                    extensionLength = std::size(LegacyPatchFileExtension) - 1;
                }

                if (!entry.is_regular_file(ec) || extensionLength == 0)
                {
                    continue;
                }

                auto const root = ReadJsonFile(entry.path());

                if (!root)
                {
                    continue;
                }

                SavedPatch patch{};
                patch.FileName = name;
                patch.Name = Bounded(StringOrEmpty(*root, L"name"));
                patch.Description = Bounded(StringOrEmpty(*root, L"description"));
                patch.ActivateAtStartup = OptionalBool(*root, L"activateAtStartup").value_or(true);

                if (auto const draft = ObjectOrNull(*root, L"_draft"); draft != nullptr)
                {
                    patch.IsDraft = true;
                    patch.DraftedBy = Bounded(StringOrEmpty(draft, L"by"));
                }

                if (patch.Name.empty())
                {
                    patch.Name = name.substr(0, name.size() - extensionLength);
                }

                // Node id in the file to the live endpoint id it was saved against.
                std::map<std::wstring, std::wstring> nodes{};

                if (auto const endpoints = ArrayOrNull(*root, L"endpoints"); endpoints != nullptr)
                {
                    for (auto const& value : endpoints)
                    {
                        if (value.ValueType() != json::JsonValueType::Object)
                        {
                            continue;
                        }

                        auto const item = value.GetObject();
                        auto const displayName = Bounded(StringOrEmpty(item, L"displayName"));
                        auto const deviceId = StringOrEmpty(ObjectOrNull(item, L"match"), L"endpointDeviceId");

                        nodes[StringOrEmpty(item, L"id")] = deviceId;
                        patch.EndpointNames.push_back(displayName);

                        auto const present = std::any_of(live.begin(), live.end(),
                            [&deviceId](midiapp::LiveEndpoint const& e) { return EqualsIgnoringCase(e.EndpointDeviceId, deviceId); });

                        if (!present)
                        {
                            patch.MissingEndpointNames.push_back(displayName);
                        }
                    }
                }

                if (auto const connections = ArrayOrNull(*root, L"connections"); connections != nullptr)
                {
                    for (auto const& value : connections)
                    {
                        if (value.ValueType() != json::JsonValueType::Object)
                        {
                            continue;
                        }

                        auto const item = value.GetObject();

                        patch.ConnectionCount++;

                        if (OptionalBool(item, L"muted").value_or(false))
                        {
                            continue;
                        }

                        SavedPatch::Connection connection{};
                        connection.FromId = nodes[StringOrEmpty(item, L"sourceEndpointId")];
                        connection.ToId = nodes[StringOrEmpty(item, L"destinationEndpointId")];
                        connection.FromGroup = GroupFromFile(item, L"sourceGroup");
                        connection.ToGroup = GroupFromFile(item, L"destinationGroup");

                        if (!connection.FromId.empty() && !connection.ToId.empty())
                        {
                            patch.Connections.push_back(std::move(connection));
                        }
                    }
                }

                patches.push_back(std::move(patch));
            }

            std::sort(patches.begin(), patches.end(), [](SavedPatch const& a, SavedPatch const& b) { return a.Name < b.Name; });

            return patches;
        }

        void CheckLoops(
            PatchRequest const& request,
            std::vector<midiapp::LiveEndpoint> const& live,
            std::vector<SavedPatch> const& saved,
            Problems& problems)
        {
            LoopGraph graph{ live };

            for (size_t i = 0; i < request.Routes.size(); i++)
            {
                auto const& route = request.Routes[i];

                if (!route.From.Resolved || !route.To.Resolved)
                {
                    continue;
                }

                Edge tag{};
                tag.FromDraft = true;
                tag.RouteIndex = i;

                graph.AddRoute(route.From.Endpoint.EndpointDeviceId, route.From.Group,
                    route.To.Endpoint.EndpointDeviceId, route.To.Group, tag);
            }

            // Patches that route when MIDI Patchbay starts run beside this one, so a loop can be
            // closed by two patches together.
            for (auto const& patch : saved)
            {
                if (!patch.ActivateAtStartup)
                {
                    continue;
                }

                for (auto const& connection : patch.Connections)
                {
                    Edge tag{};
                    tag.OtherPatch = patch.Name;

                    graph.AddRoute(connection.FromId, connection.FromGroup, connection.ToId, connection.ToGroup, tag);
                }
            }

            for (auto const& edge : graph.DraftEdgesInLoops())
            {
                auto const& route = request.Routes[edge.RouteIndex];
                std::wstring other{};

                if (graph.LoopUsesOtherPatch(edge, other))
                {
                    problems.Errors.push_back(L"Route " + std::to_wstring(edge.RouteIndex + 1) + L" (\"" + route.From.Endpoint.Name +
                        L"\" to \"" + route.To.Endpoint.Name + L"\") makes a feedback loop together with the saved patch \"" + other +
                        L"\", which routes when MIDI Patchbay starts. Messages would circle forever.");
                }
                else
                {
                    problems.Errors.push_back(L"Route " + std::to_wstring(edge.RouteIndex + 1) + L" (\"" + route.From.Endpoint.Name +
                        L"\" to \"" + route.To.Endpoint.Name + L"\") makes a feedback loop through a loopback. Messages would circle forever.");
                }
            }
        }

        // ------------------------------------------------------------------------------------
        // Plain English, written by this tool from what it resolved, never by the model. This is
        // what the customer should be shown, because it describes what will actually happen.

        std::wstring DescribeKinds(std::vector<std::wstring> const& names)
        {
            std::vector<std::wstring> meanings{};

            for (auto const& name : names)
            {
                if (auto const kind = FindMessageKind(name))
                {
                    meanings.push_back(kind->Meaning);
                }
            }

            return Join(meanings, L", ");
        }

        std::wstring DescribeRoute(Route const& route, std::vector<midiapp::LiveEndpoint> const& live)
        {
            std::wstring text = L"\"" + DescribeEndpoint(route.From.Endpoint, live) + L"\" " +
                DescribeGroup(route.From.Endpoint, route.From.Group, true) + L" to \"" +
                DescribeEndpoint(route.To.Endpoint, live) + L"\" " +
                (route.To.Group == AllGroups && route.From.Group != AllGroups
                    ? std::wstring{ L"(same group number)" }
                    : DescribeGroup(route.To.Endpoint, route.To.Group, false));

            std::vector<std::wstring> parts{};

            if (std::any_of(route.Channels.begin(), route.Channels.end(), [](bool on) { return !on; }))
            {
                std::vector<std::wstring> channels{};

                for (size_t i = 0; i < route.Channels.size(); i++)
                {
                    if (route.Channels[i])
                    {
                        channels.push_back(std::to_wstring(i + 1));
                    }
                }

                parts.push_back((channels.size() == 1 ? L"only channel " : L"only channels ") + Join(channels, L", "));
            }

            if (!route.OnlyKinds.empty())
            {
                parts.push_back(L"only " + DescribeKinds(route.OnlyKinds));
            }

            if (!route.BlockedKinds.empty())
            {
                parts.push_back(L"no " + DescribeKinds(route.BlockedKinds));
            }

            if (route.LimitNotes)
            {
                parts.push_back(L"only notes " + NoteName(route.LowestNote) + L" to " + NoteName(route.HighestNote) +
                    L" (" + std::to_wstring(route.LowestNote) + L" to " + std::to_wstring(route.HighestNote) + L")");
            }

            if (route.Transpose != 0)
            {
                parts.push_back(std::wstring{ route.Transpose > 0 ? L"transposed up " : L"transposed down " } +
                    std::to_wstring(std::abs(route.Transpose)) + L" semitones");
            }

            auto const describeMap = [&parts](auto const& map, wchar_t const* what, int32_t offset, bool notes)
                {
                    std::vector<std::wstring> pairs{};

                    for (size_t i = 0; i < map.size(); i++)
                    {
                        if (map[i] < 0)
                        {
                            continue;
                        }

                        pairs.push_back(notes
                            ? NoteName(static_cast<uint8_t>(i)) + L" to " + NoteName(static_cast<uint8_t>(map[i]))
                            : std::to_wstring(static_cast<int32_t>(i) + offset) + L" to " + std::to_wstring(map[i] + offset));
                    }

                    if (!pairs.empty())
                    {
                        parts.push_back(std::wstring{ what } + L" " + Join(pairs, L", "));
                    }
                };

            describeMap(route.ChannelMap, L"channel", 1, false);
            describeMap(route.NoteMap, L"note", 0, true);
            describeMap(route.ControlMap, L"controller", 0, false);
            describeMap(route.ProgramMap, L"program", 1, false);

            auto const percent = [](int32_t hundredths) { return std::format(L"{:g}%", hundredths / 100.0); };

            switch (route.VelocityCurve)
            {
            case VelocitySofter:
                parts.push_back(L"velocity on a softer curve, so it takes a harder hit to play loudly");
                break;
            case VelocityLouder:
                parts.push_back(L"velocity on a louder curve, so soft playing comes out louder");
                break;
            case VelocityFixed:
                parts.push_back(L"every note at the same velocity, " + percent(route.FixedVelocityHundredths));
                break;
            default:
                break;
            }

            if (route.RescaleVelocity && route.VelocityCurve != VelocityFixed)
            {
                parts.push_back(L"velocity kept between " + percent(route.MinimumVelocityHundredths) + L" and " +
                    percent(route.MaximumVelocityHundredths));
            }

            if (!parts.empty())
            {
                text += L": " + Join(parts, L"; ");
            }

            return text + L".";
        }

        std::wstring DescribeRequest(PatchRequest const& request, std::vector<midiapp::LiveEndpoint> const& live)
        {
            std::wstring text = L"Patch \"" + request.Name + L"\"";

            if (!request.Description.empty())
            {
                text += L" - " + request.Description;
            }

            text += L"\n";

            for (size_t i = 0; i < request.Routes.size(); i++)
            {
                auto const& route = request.Routes[i];

                if (route.From.Resolved && route.To.Resolved)
                {
                    text += std::to_wstring(i + 1) + L". " + DescribeRoute(route, live) + L"\n";
                }
            }

            return text;
        }

        // ------------------------------------------------------------------------------------
        // Writing MIDI Patchbay's file

        uint32_t PackFlags16(uint16_t flags, size_t count) noexcept
        {
            return static_cast<uint32_t>(flags) & ((count >= 32) ? 0xFFFFFFFFu : ((1u << count) - 1));
        }

        uint32_t PackChannels(std::array<bool, 16> const& channels) noexcept
        {
            uint32_t packed{ 0 };

            for (size_t i = 0; i < channels.size(); i++)
            {
                if (channels[i])
                {
                    packed |= 1u << i;
                }
            }

            return packed;
        }

        json::JsonObject FilterJson(Route const& route)
        {
            json::JsonObject filter{};

            filter.SetNamedValue(L"active", json::JsonValue::CreateBooleanValue(true));
            filter.SetNamedValue(L"messageTypes", json::JsonValue::CreateNumberValue(PackFlags16(route.Types, 16)));
            filter.SetNamedValue(L"channels", json::JsonValue::CreateNumberValue(PackChannels(route.Channels)));
            filter.SetNamedValue(L"channelVoiceStatuses", json::JsonValue::CreateNumberValue(PackFlags16(route.VoiceStatuses, 16)));
            filter.SetNamedValue(L"systemMessages", json::JsonValue::CreateNumberValue(PackFlags16(route.SystemEntries, SystemEntryCount)));
            filter.SetNamedValue(L"limitNoteRange", json::JsonValue::CreateBooleanValue(route.LimitNotes));
            filter.SetNamedValue(L"lowestNote", json::JsonValue::CreateNumberValue(route.LowestNote));
            filter.SetNamedValue(L"highestNote", json::JsonValue::CreateNumberValue(route.HighestNote));

            return filter;
        }

        template <size_t N>
        json::JsonArray MapJson(std::array<int16_t, N> const& map)
        {
            json::JsonArray array{};

            for (size_t i = 0; i < N; i++)
            {
                if (map[i] < 0 || map[i] == static_cast<int16_t>(i))
                {
                    continue;
                }

                json::JsonObject entry{};
                entry.SetNamedValue(L"from", json::JsonValue::CreateNumberValue(static_cast<double>(i)));
                entry.SetNamedValue(L"to", json::JsonValue::CreateNumberValue(map[i]));
                array.Append(entry);
            }

            return array;
        }

        int32_t SevenBitFromHundredths(int32_t hundredths) noexcept
        {
            auto const clamped = std::clamp(hundredths, 0, FullScaleHundredths);
            return (clamped * 127 + FullScaleHundredths / 2) / FullScaleHundredths;
        }

        json::JsonObject TransformJson(Route const& route)
        {
            json::JsonObject transform{};

            transform.SetNamedValue(L"active", json::JsonValue::CreateBooleanValue(true));
            transform.SetNamedValue(L"valueScale", json::JsonValue::CreateStringValue(L"percent"));
            transform.SetNamedValue(L"transposeSemitones", json::JsonValue::CreateNumberValue(route.Transpose));
            transform.SetNamedValue(L"ignoreExactPitchNotes", json::JsonValue::CreateBooleanValue(false));
            transform.SetNamedValue(L"velocityCurve", json::JsonValue::CreateNumberValue(route.VelocityCurve));
            transform.SetNamedValue(L"rescaleVelocity", json::JsonValue::CreateBooleanValue(route.RescaleVelocity));
            transform.SetNamedValue(L"fixedVelocityPercent", json::JsonValue::CreateNumberValue(route.FixedVelocityHundredths / 100.0));
            transform.SetNamedValue(L"minimumVelocityPercent", json::JsonValue::CreateNumberValue(route.MinimumVelocityHundredths / 100.0));
            transform.SetNamedValue(L"maximumVelocityPercent", json::JsonValue::CreateNumberValue(route.MaximumVelocityHundredths / 100.0));

            // The app writes the old 0 to 127 pair beside the percentages for older previews.
            transform.SetNamedValue(L"minimumVelocity", json::JsonValue::CreateNumberValue(std::max(1, SevenBitFromHundredths(route.MinimumVelocityHundredths))));
            transform.SetNamedValue(L"maximumVelocity", json::JsonValue::CreateNumberValue(std::max(1, SevenBitFromHundredths(route.MaximumVelocityHundredths))));

            transform.SetNamedValue(L"channelMap", MapJson(route.ChannelMap));
            transform.SetNamedValue(L"noteMap", MapJson(route.NoteMap));
            transform.SetNamedValue(L"controlMap", MapJson(route.ControlMap));
            transform.SetNamedValue(L"programMap", MapJson(route.ProgramMap));
            transform.SetNamedValue(L"bankMsbMap", json::JsonArray{});
            transform.SetNamedValue(L"bankLsbMap", json::JsonArray{});
            transform.SetNamedValue(L"controlValueShapes", json::JsonArray{});

            json::JsonObject aftertouch{};
            aftertouch.SetNamedValue(L"curve", json::JsonValue::CreateStringValue(L"linear"));
            aftertouch.SetNamedValue(L"inputMinimumPercent", json::JsonValue::CreateNumberValue(0));
            aftertouch.SetNamedValue(L"inputMaximumPercent", json::JsonValue::CreateNumberValue(100));
            aftertouch.SetNamedValue(L"outputMinimumPercent", json::JsonValue::CreateNumberValue(0));
            aftertouch.SetNamedValue(L"outputMaximumPercent", json::JsonValue::CreateNumberValue(100));
            transform.SetNamedValue(L"aftertouchShape", aftertouch);

            return transform;
        }

        json::JsonObject PatchJson(PatchRequest const& request, CallContext const& context)
        {
            auto const now = static_cast<double>(CurrentUnixSeconds());

            json::JsonObject root{};
            root.SetNamedValue(L"_comment", json::JsonValue::CreateStringValue(
                L"Windows MIDI Patchbay. Drafted by an AI assistant through the MIDI MCP prototype. "
                L"It does not route until the customer turns routing on in MIDI Patchbay."));
            root.SetNamedValue(L"fileVersion", json::JsonValue::CreateNumberValue(1));
            root.SetNamedValue(L"name", json::JsonValue::CreateStringValue(request.Name));
            root.SetNamedValue(L"description", json::JsonValue::CreateStringValue(request.Description));
            root.SetNamedValue(L"created", json::JsonValue::CreateNumberValue(now));
            root.SetNamedValue(L"modified", json::JsonValue::CreateNumberValue(now));

            // A draft never routes by itself. This one switch is what makes it a draft to today's app.
            root.SetNamedValue(L"activateAtStartup", json::JsonValue::CreateBooleanValue(false));

            // What a future app would read to show a review bar. Today's app ignores it.
            json::JsonObject draft{};
            draft.SetNamedValue(L"by", json::JsonValue::CreateStringValue(context.ClientName.empty() ? L"an AI assistant" : Bounded(context.ClientName)));
            draft.SetNamedValue(L"request", json::JsonValue::CreateStringValue(request.Request));
            draft.SetNamedValue(L"created", json::JsonValue::CreateNumberValue(now));
            root.SetNamedValue(L"_draft", draft);

            // One node per endpoint, sources on the left and destinations on the right.
            std::vector<std::wstring> order{};
            std::map<std::wstring, std::pair<bool, bool>> roles{};
            std::map<std::wstring, midiapp::LiveEndpoint const*> byId{};

            auto const note = [&](midiapp::LiveEndpoint const& endpoint, bool isSource)
                {
                    std::wstring key{ endpoint.EndpointDeviceId };
                    std::transform(key.begin(), key.end(), key.begin(), [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

                    if (byId.find(key) == byId.end())
                    {
                        order.push_back(key);
                        byId[key] = &endpoint;
                    }

                    (isSource ? roles[key].first : roles[key].second) = true;
                    return key;
                };

            for (auto const& route : request.Routes)
            {
                note(route.From.Endpoint, true);
                note(route.To.Endpoint, false);
            }

            std::map<std::wstring, std::wstring> nodeIds{};
            std::array<int, 3> rows{};
            json::JsonArray endpoints{};

            for (auto const& key : order)
            {
                auto const& endpoint = *byId[key];
                auto const [isSource, isDestination] = roles[key];
                auto const column = isSource && isDestination ? 1 : (isSource ? 0 : 2);

                auto const nodeId = NewGuidText();
                nodeIds[key] = nodeId;

                json::JsonObject item{};
                item.SetNamedValue(L"id", json::JsonValue::CreateStringValue(nodeId));
                item.SetNamedValue(L"displayName", json::JsonValue::CreateStringValue(endpoint.Name));
                item.SetNamedValue(L"transportCode", json::JsonValue::CreateStringValue(endpoint.TransportCode));
                item.SetNamedValue(L"match", midiapp::MatchToJson(endpoint.BuildMatch()));
                item.SetNamedValue(L"matchMode", json::JsonValue::CreateStringValue(L"endpointDeviceId"));
                item.SetNamedValue(L"x", json::JsonValue::CreateNumberValue(60.0 + column * 400.0));
                item.SetNamedValue(L"y", json::JsonValue::CreateNumberValue(60.0 + rows[static_cast<size_t>(column)]++ * 220.0));
                item.SetNamedValue(L"showAllGroups", json::JsonValue::CreateBooleanValue(false));

                endpoints.Append(item);
            }

            root.SetNamedValue(L"endpoints", endpoints);

            json::JsonArray connections{};

            for (auto const& route : request.Routes)
            {
                auto const lower = [](std::wstring text)
                    {
                        std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
                        return text;
                    };

                json::JsonObject item{};
                item.SetNamedValue(L"id", json::JsonValue::CreateStringValue(NewGuidText()));
                item.SetNamedValue(L"sourceEndpointId", json::JsonValue::CreateStringValue(nodeIds[lower(route.From.Endpoint.EndpointDeviceId)]));
                item.SetNamedValue(L"sourceGroup", json::JsonValue::CreateNumberValue(route.From.Group));
                item.SetNamedValue(L"destinationEndpointId", json::JsonValue::CreateStringValue(nodeIds[lower(route.To.Endpoint.EndpointDeviceId)]));
                item.SetNamedValue(L"destinationGroup", json::JsonValue::CreateNumberValue(route.To.Group));
                item.SetNamedValue(L"muted", json::JsonValue::CreateBooleanValue(false));

                // Only when it does something, as the app does, so the file stays readable.
                if (route.FilterExcludesAnything())
                {
                    item.SetNamedValue(L"filter", FilterJson(route));
                }

                if (route.TransformChangesAnything())
                {
                    item.SetNamedValue(L"transform", TransformJson(route));
                }

                connections.Append(item);
            }

            root.SetNamedValue(L"connections", connections);

            return root;
        }

        // The app's own rules for turning a patch name into a file name.
        std::wstring SafeFileStem(std::wstring const& name)
        {
            std::wstring result{};

            for (auto const ch : name)
            {
                auto const reserved = ch < L' ' || ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' ||
                    ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*';

                result.push_back(reserved ? L'_' : ch);
            }

            while (!result.empty() && (result.back() == L'.' || result.back() == L' '))
            {
                result.pop_back();
            }

            if (result.size() > 96)
            {
                result.resize(96);
            }

            if (result.empty())
            {
                result = L"Patch";
            }

            static constexpr std::wstring_view devices[] =
            {
                L"CON", L"PRN", L"AUX", L"NUL", L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7",
                L"COM8", L"COM9", L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9",
            };

            for (auto const device : devices)
            {
                if (EqualsIgnoringCase(result, device))
                {
                    result.insert(result.begin(), L'_');
                    break;
                }
            }

            return result;
        }

        // Never replaces a file. CREATE_NEW makes "is this name free" and "take it" one step.
        std::optional<std::wstring> WriteNewFile(std::wstring const& folder, std::wstring const& stem, std::string const& bytes)
        {
            std::error_code ec{};
            std::filesystem::create_directories(folder, ec);

            for (int suffix = 0; suffix < 1000; suffix++)
            {
                auto const path = (std::filesystem::path{ folder } /
                    (suffix == 0 ? stem + PatchFileExtension : stem + L" (" + std::to_wstring(suffix) + L")" + PatchFileExtension)).wstring();

                wil::unique_hfile file{ ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr) };

                if (!file)
                {
                    if (::GetLastError() == ERROR_FILE_EXISTS)
                    {
                        continue;
                    }

                    return std::nullopt;
                }

                DWORD written{ 0 };

                if (!::WriteFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) || written != bytes.size())
                {
                    file.reset();
                    ::DeleteFileW(path.c_str());
                    return std::nullopt;
                }

                return path;
            }

            return std::nullopt;
        }

        // ------------------------------------------------------------------------------------
        // Tools

        constexpr wchar_t PatchSchema[] = LR"({
            "type": "object",
            "properties": {
                "name": { "type": "string", "description": "Name for the patch, the way the customer would say it." },
                "description": { "type": "string", "description": "One line about what the patch is for." },
                "request": { "type": "string", "description": "What the customer asked for, in their own words. Stored with the draft so the app can show it while they review." },
                "routes": {
                    "type": "array",
                    "minItems": 1,
                    "maxItems": 64,
                    "description": "Each route sends what arrives on one endpoint to another. Filters and changes apply to that route only.",
                    "items": {
                        "type": "object",
                        "properties": {
                            "from": {
                                "type": "object",
                                "description": "Where messages come from. Must send on the group given.",
                                "properties": {
                                    "endpoint": { "type": "string", "description": "Endpoint name or id from list_midi_endpoints." },
                                    "group": { "type": ["integer", "string"], "description": "Group 1 to 16, or \"all\". Default all." }
                                },
                                "required": ["endpoint"]
                            },
                            "to": {
                                "type": "object",
                                "description": "Where messages go. Must receive on the group given.",
                                "properties": {
                                    "endpoint": { "type": "string", "description": "Endpoint name or id from list_midi_endpoints." },
                                    "group": { "type": ["integer", "string"], "description": "Group 1 to 16, or \"all\". Default all, which keeps each message's own group." }
                                },
                                "required": ["endpoint"]
                            },
                            "channels": { "type": "array", "items": { "type": "integer", "minimum": 1, "maximum": 16 }, "description": "Only pass these channels. Leave out to pass every channel." },
                            "messages": { "type": "array", "items": { "type": "string" }, "description": "Only pass these kinds of message. Kinds: MESSAGE_KINDS." },
                            "block": { "type": "array", "items": { "type": "string" }, "description": "Pass everything except these kinds. Same kinds as messages. Not together with messages." },
                            "noteRange": {
                                "type": "object",
                                "description": "Only pass notes in this range, for example to split a keyboard. Ends are included.",
                                "properties": {
                                    "lowest": { "type": ["integer", "string"], "description": "0 to 127, or a note name. NOTE_CONVENTION" },
                                    "highest": { "type": ["integer", "string"], "description": "0 to 127, or a note name." }
                                }
                            },
                            "transposeSemitones": { "type": "integer", "minimum": -48, "maximum": 48 },
                            "channelMap": { "type": "array", "items": { "type": "object", "properties": { "from": { "type": "integer" }, "to": { "type": "integer" } }, "required": ["from", "to"] }, "description": "Move messages from one channel to another, 1 to 16." },
                            "noteMap": { "type": "array", "items": { "type": "object", "properties": { "from": { "type": ["integer", "string"] }, "to": { "type": ["integer", "string"] } }, "required": ["from", "to"] }, "description": "Send one note as another. A listed note ignores the transpose." },
                            "controllerMap": { "type": "array", "items": { "type": "object", "properties": { "from": { "type": "integer" }, "to": { "type": "integer" } }, "required": ["from", "to"] }, "description": "Send one controller number as another, 0 to 127." },
                            "programMap": { "type": "array", "items": { "type": "object", "properties": { "from": { "type": "integer" }, "to": { "type": "integer" } }, "required": ["from", "to"] }, "description": "Send one program as another, 1 to 128 as the customer counts them." },
                            "velocity": {
                                "type": "object",
                                "description": "Change note on velocity.",
                                "properties": {
                                    "curve": { "type": "string", "enum": ["unchanged", "softer", "louder", "fixed"], "description": "softer: a harder hit is needed to play loudly. louder: soft playing comes out louder. fixed: every note the same." },
                                    "fixedPercent": { "type": "number", "minimum": 1, "maximum": 100 },
                                    "minimumPercent": { "type": "number", "minimum": 0, "maximum": 100 },
                                    "maximumPercent": { "type": "number", "minimum": 0, "maximum": 100 }
                                }
                            }
                        },
                        "required": ["from", "to"]
                    }
                }
            },
            "required": ["name", "routes"]
        })";

        std::wstring ReplaceAll(std::wstring text, std::wstring_view token, std::wstring const& value)
        {
            for (auto at = text.find(token); at != std::wstring::npos; at = text.find(token, at + value.size()))
            {
                text.replace(at, token.size(), value);
            }

            return text;
        }

        std::wstring PatchSchemaText()
        {
            auto text = ReplaceAll(PatchSchema, L"MESSAGE_KINDS", MessageKindNameList());
            return ReplaceAll(text, L"NOTE_CONVENTION", L"Names follow the MIDI tools: note 60 is " + NoteName(60) + L".");
        }

        ToolResult Prepare(
            json::JsonObject const& arguments,
            PatchbayToolOptions const& options,
            PatchRequest& request,
            Problems& problems,
            std::vector<midiapp::LiveEndpoint>& live)
        {
            live = LiveEndpoints();

            if (live.empty())
            {
                return ToolResult::Error(L"No MIDI endpoints are visible. The MIDI service may not be running.");
            }

            request = ReadRequest(arguments, live, problems);

            if (!problems.HasErrors())
            {
                CheckLoops(request, live, ReadSavedPatches(PatchFolder(options), live), problems);
            }

            return {};
        }
    }

    _Use_decl_annotations_
    std::vector<ToolDefinition> MakePatchbayTools(PatchbayToolOptions const& options)
    {
        std::vector<ToolDefinition> tools{};

        {
            ToolDefinition tool{};
            tool.Name = L"list_patches";
            tool.Title = L"List MIDI Patchbay patches";
            tool.Description =
                L"Lists the patches saved in MIDI Patchbay: name, description, the endpoints each one uses, how many routes "
                L"it has, whether it routes when MIDI Patchbay starts, and which of its endpoints are missing right now. "
                L"Drafts written by an assistant are marked.";
            tool.InputSchema = LR"({ "type": "object", "additionalProperties": false })";
            tool.Annotations = { true, false, true, false };
            tool.Handler = [options](json::JsonObject const&, CallContext const&)
                {
                    auto const live = LiveEndpoints();
                    auto const patches = ReadSavedPatches(PatchFolder(options), live);

                    std::wstring text = std::to_wstring(patches.size()) + L" saved patch" + (patches.size() == 1 ? L"" : L"es") + L".\n";

                    for (auto const& patch : patches)
                    {
                        text += L"- \"" + patch.Name + L"\"";

                        if (patch.IsDraft)
                        {
                            text += L" [draft" + (patch.DraftedBy.empty() ? std::wstring{} : L" by " + patch.DraftedBy) + L", not reviewed]";
                        }

                        text += patch.ActivateAtStartup ? L", routes at startup" : L", does not route at startup";
                        text += L", " + std::to_wstring(patch.ConnectionCount) + L" route" + (patch.ConnectionCount == 1 ? L"" : L"s");
                        text += L", endpoints: " + Join(patch.EndpointNames, L", ");

                        if (!patch.MissingEndpointNames.empty())
                        {
                            text += L". Missing now: " + Join(patch.MissingEndpointNames, L", ");
                        }

                        if (!patch.Description.empty())
                        {
                            text += L". " + patch.Description;
                        }

                        text += L"\n";
                    }

                    ToolResult result{};
                    result.AddText(text);

                    return result;
                };

            tools.push_back(std::move(tool));
        }

        {
            ToolDefinition tool{};
            tool.Name = L"preview_patch";
            tool.Title = L"Check a MIDI Patchbay patch";
            tool.Description =
                L"Checks a patch without saving anything, and says in plain words what it would do. Use it to confirm the "
                L"plan with the customer, and to find out what is still missing: when an endpoint name matches more than one "
                L"device, or a device does not have the group asked for, the answer says so and lists the choices, so you can "
                L"ask the customer. It also finds feedback loops through loopbacks, including loops closed by patches that "
                L"already route. Nothing is sent to any device.";
            tool.InputSchema = PatchSchemaText();
            tool.Annotations = { true, false, true, false };
            tool.Handler = [options](json::JsonObject const& arguments, CallContext const&)
                {
                    PatchRequest request{};
                    Problems problems{};
                    std::vector<midiapp::LiveEndpoint> live{};

                    if (auto early = Prepare(arguments, options, request, problems, live); early.IsError)
                    {
                        return early;
                    }

                    std::wstring text = problems.HasErrors()
                        ? L"This patch cannot be saved yet.\n"
                        : L"This patch is ready to save as a draft. Show the customer this summary first.\n\n";

                    text += DescribeRequest(request, live);
                    problems.AppendTo(text);

                    ToolResult result{};
                    result.AddText(text);

                    return result;
                };

            tools.push_back(std::move(tool));
        }

        {
            ToolDefinition tool{};
            tool.Name = L"save_patch_draft";
            tool.Title = L"Save a MIDI Patchbay draft";
            tool.Description =
                L"Saves the patch as a new draft in MIDI Patchbay. A draft never routes by itself: the customer opens it "
                L"in MIDI Patchbay, looks it over and turns routing on. It never replaces an existing patch; to change one, "
                L"save a new draft beside it. Call preview_patch first and get the customer's agreement.";
            tool.InputSchema = PatchSchemaText();
            tool.Annotations = { false, false, false, false };
            tool.Handler = [options](json::JsonObject const& arguments, CallContext const& context)
                {
                    PatchRequest request{};
                    Problems problems{};
                    std::vector<midiapp::LiveEndpoint> live{};

                    if (auto early = Prepare(arguments, options, request, problems, live); early.IsError)
                    {
                        return early;
                    }

                    if (problems.HasErrors())
                    {
                        std::wstring text = L"Nothing was saved.\n";
                        problems.AppendTo(text);

                        return ToolResult::Error(text);
                    }

                    auto const folder = PatchFolder(options);
                    auto const bytes = WideToUtf8(std::wstring{ PatchJson(request, context).Stringify() });
                    auto const path = folder.empty() ? std::nullopt : WriteNewFile(folder, SafeFileStem(request.Name), bytes);

                    if (!path)
                    {
                        return ToolResult::Error(L"The draft could not be written to the MIDI Patchbay folder.");
                    }

                    std::wstring text = L"Saved a draft: " + DisplayPathUnderDocuments(*path) + L"\n\n" + DescribeRequest(request, live) +
                        L"\nIt does not route yet. The customer opens MIDI Patchbay, picks \"" + request.Name +
                        L"\" in the patch list, checks it and turns routing on. MIDI Patchbay reads new patches when it starts, "
                        L"so if it is already open it shows this one the next time it starts.\n";
                    problems.AppendTo(text);

                    ToolResult result{};
                    result.AddText(text);

                    return result;
                };

            tools.push_back(std::move(tool));
        }

        return tools;
    }
}
