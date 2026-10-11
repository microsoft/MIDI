// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "GlassLayoutWriter.h"

#include "RemoteControl.h"

#include <algorithm>
#include <array>
#include <format>

namespace mjson = winrt::Windows::Data::Json;

namespace midisequencer
{
    namespace
    {
        constexpr double PageWidth = 1280;
        constexpr double PageHeight = 800;
        constexpr wchar_t DeviceName[] = L"Sequencer";

        // Page pixels, on Windows MIDI Glass's 8 pixel grid.
        constexpr double Margin = 24;
        constexpr double TransportY = 24;
        constexpr double ButtonHeight = 40;
        constexpr double SceneRowY = 96;
        constexpr double FirstTrackY = 152;
        constexpr double TrackPitch = 72;
        constexpr double TrackNameWidth = 120;
        constexpr double SwitchX = 152;
        constexpr double SwitchPitch = 64;
        constexpr double SwitchWidth = 56;
        constexpr double StopX = 344;
        constexpr double FirstPadX = 416;
        constexpr double PadPitch = 104;
        constexpr double PadWidth = 96;
        constexpr double PadHeight = 56;
        constexpr double TabY = 744;
        constexpr double TabPitch = 128;

        // The theme's color slots: 6 is its neutral.
        constexpr int32_t Neutral = 6;

        mjson::IJsonValue Text(std::wstring const& value)
        {
            return mjson::JsonValue::CreateStringValue(value);
        }

        mjson::IJsonValue Number(double value)
        {
            return mjson::JsonValue::CreateNumberValue(value);
        }

        mjson::IJsonValue Bool(bool value)
        {
            return mjson::JsonValue::CreateBooleanValue(value);
        }

        mjson::JsonObject Row(wchar_t const* trigger, wchar_t const* kind, uint8_t group, uint8_t channel, uint32_t number)
        {
            mjson::JsonObject row{};
            row.SetNamedValue(L"trigger", Text(trigger));
            row.SetNamedValue(L"kind", Text(kind));
            row.SetNamedValue(L"device", Text(DeviceName));
            row.SetNamedValue(L"group", Number(group));
            row.SetNamedValue(L"channel", Number(channel));
            row.SetNamedValue(L"number", Number(number));
            return row;
        }

        // A press sends the message and letting go sends its end, the way a new pad does. The
        // sequencer acts on the press.
        void Press(mjson::JsonObject const& control, wchar_t const* kind, uint8_t group, uint8_t channel, uint32_t number)
        {
            mjson::JsonArray messages{};
            messages.Append(Row(L"turnsOn", kind, group, channel, number));
            messages.Append(Row(L"turnsOff", kind, group, channel, number));
            control.SetNamedValue(L"messages", messages);
        }

        // Lit from what the sequencer sends back (RemoteFeedback).
        void Listen(mjson::JsonObject const& control, wchar_t const* kind, uint8_t group, uint8_t channel, uint32_t number)
        {
            mjson::JsonObject feedback{};
            feedback.SetNamedValue(L"enabled", Bool(true));
            feedback.SetNamedValue(L"mode", Text(L"message"));
            feedback.SetNamedValue(L"kind", Text(kind));
            feedback.SetNamedValue(L"device", Text(DeviceName));
            feedback.SetNamedValue(L"group", Number(group));
            feedback.SetNamedValue(L"channel", Number(channel));
            feedback.SetNamedValue(L"number", Number(number));
            control.SetNamedValue(L"feedback", feedback);
        }

        struct Controls
        {
            mjson::JsonArray List{};
            uint32_t Order{ 1 };

            mjson::JsonObject Add(std::wstring const& id, wchar_t const* kind, std::wstring const& label, double x, double y, double width, double height, int32_t hue)
            {
                mjson::JsonObject control{};
                control.SetNamedValue(L"id", Text(id));
                control.SetNamedValue(L"kind", Text(kind));
                control.SetNamedValue(L"label", Text(label));
                control.SetNamedValue(L"x", Number(x));
                control.SetNamedValue(L"y", Number(y));
                control.SetNamedValue(L"width", Number(width));
                control.SetNamedValue(L"height", Number(height));
                control.SetNamedValue(L"hueSlot", Number(hue));
                control.SetNamedValue(L"keyboardOrder", Number(Order++));
                List.Append(control);
                return control;
            }
        };

        // Windows.Data.Json writes everything on one line. A layout someone may open in a text
        // editor reads better spread out.
        std::wstring Indented(std::wstring const& compact)
        {
            std::wstring out{};
            out.reserve(compact.size() * 2);

            size_t depth{ 0 };
            auto inString = false;
            auto escaped = false;

            auto const newline = [&]()
            {
                out += L"\n";
                out.append(depth * 2, L' ');
            };

            for (size_t i = 0; i < compact.size(); ++i)
            {
                auto const c = compact[i];

                if (inString)
                {
                    out += c;

                    if (escaped)
                    {
                        escaped = false;
                    }
                    else if (c == L'\\')
                    {
                        escaped = true;
                    }
                    else if (c == L'"')
                    {
                        inString = false;
                    }

                    continue;
                }

                switch (c)
                {
                case L'"':
                    inString = true;
                    out += c;
                    break;

                case L'{':
                case L'[':
                    out += c;

                    // An empty object or list stays on one line.
                    if (i + 1 < compact.size() && (compact[i + 1] == L'}' || compact[i + 1] == L']'))
                    {
                        out += compact[++i];
                        break;
                    }

                    ++depth;
                    newline();
                    break;

                case L'}':
                case L']':
                    depth = depth > 0 ? depth - 1 : 0;
                    newline();
                    out += c;
                    break;

                case L',':
                    out += c;
                    newline();
                    break;

                case L':':
                    out += L": ";
                    break;

                default:
                    out += c;
                    break;
                }
            }

            out += L"\n";
            return out;
        }

        size_t PagesFor(size_t count, size_t perPage) noexcept
        {
            return std::max<size_t>(1, (count + perPage - 1) / perPage);
        }
    }

    _Use_decl_annotations_
    std::pair<size_t, size_t> GlassLayoutCoverage(Sequence const& sequence) noexcept
    {
        try
        {
            auto const tracks = std::min(RemoteTrackOrder(sequence).size(), RemoteMaximumTracks);
            auto const scenes = std::min(sequence.Scenes.size(), RemoteMaximumScenes);
            auto const trackPages = PagesFor(tracks, GlassTracksPerPage);

            // Pages go down the tracks first, then on to the next scenes, until there are enough.
            if (trackPages >= GlassMaximumPages)
            {
                return { std::min(tracks, GlassMaximumPages * GlassTracksPerPage), std::min(scenes, GlassScenesPerPage) };
            }

            auto const sceneBlocks = GlassMaximumPages / trackPages;
            return { tracks, std::min(scenes, sceneBlocks * GlassScenesPerPage) };
        }
        catch (...)
        {
            return { 0, 0 };
        }
    }

    _Use_decl_annotations_
    std::wstring MakeGlassLayout(Sequence const& sequence, GlassLayoutText const& text)
    {
        auto const order = RemoteTrackOrder(sequence);
        auto const [trackCount, sceneCount] = GlassLayoutCoverage(sequence);

        struct Plan
        {
            size_t FirstTrack{ 0 };
            size_t TrackCount{ 0 };
            size_t FirstScene{ 0 };
            size_t SceneCount{ 0 };
            std::wstring Id{};
        };

        std::vector<Plan> plans{};

        for (size_t scenePage = 0; scenePage < PagesFor(sceneCount, GlassScenesPerPage); ++scenePage)
        {
            for (size_t trackPage = 0; trackPage < PagesFor(trackCount, GlassTracksPerPage); ++trackPage)
            {
                Plan plan{};
                plan.FirstTrack = trackPage * GlassTracksPerPage;
                plan.TrackCount = trackCount > plan.FirstTrack ? std::min(GlassTracksPerPage, trackCount - plan.FirstTrack) : 0;
                plan.FirstScene = scenePage * GlassScenesPerPage;
                plan.SceneCount = sceneCount > plan.FirstScene ? std::min(GlassScenesPerPage, sceneCount - plan.FirstScene) : 0;
                plan.Id = std::format(L"page-{}", plans.size() + 1);
                plans.push_back(std::move(plan));
            }
        }

        auto const pageName = [&text](Plan const& plan)
        {
            return text.PageName
                ? text.PageName(plan.FirstTrack, plan.FirstTrack + std::max<size_t>(plan.TrackCount, 1) - 1, plan.FirstScene, plan.FirstScene + std::max<size_t>(plan.SceneCount, 1) - 1)
                : plan.Id;
        };

        // The band shows on every page: the transport, and a tab for each page.
        Controls band{};

        struct TransportButton
        {
            std::wstring const* Label{ nullptr };
            uint8_t Note{ 0 };
            bool Lights{ false };
            int32_t Hue{ Neutral };
            double Width{ PadWidth };
        };

        std::array<TransportButton, 8> const transport{ {
            { &text.Play, RemoteNotePlay, true, 1, PadWidth },
            { &text.Stop, RemoteNoteStopTransport, false, Neutral, PadWidth },
            { &text.Record, RemoteNoteRecord, true, 3, PadWidth },
            { &text.Loop, RemoteNoteLoop, true, 0, PadWidth },
            { &text.Metronome, RemoteNoteMetronome, true, 0, PadWidth },
            { &text.GoToStart, RemoteNoteGoToStart, false, Neutral, PadWidth },
            { &text.StopAll, RemoteNoteStop, false, Neutral, PadWidth },
            { &text.BackToTimeline, RemoteNoteToTimeline, false, Neutral, 136 },
        } };

        auto x = Margin;

        for (auto const& button : transport)
        {
            auto const control = band.Add(std::format(L"transport-{}", button.Note), L"button", *button.Label, x, TransportY, button.Width, ButtonHeight, button.Hue);
            control.SetNamedValue(L"labelPlaced", Text(L"inside"));
            Press(control, L"note", 0, RemoteSequenceChannel, button.Note);

            if (button.Lights)
            {
                Listen(control, L"note", 0, RemoteSequenceChannel, button.Note);
            }

            x += button.Width + 8;
        }

        if (plans.size() > 1)
        {
            for (size_t i = 0; i < plans.size(); ++i)
            {
                auto const tabName = text.PageTabName ? text.PageTabName(i) : pageName(plans[i]);
                auto const tab = band.Add(std::format(L"tab-{}", i + 1), L"pageTab", tabName, Margin + static_cast<double>(i) * TabPitch, TabY, 120, 36, Neutral);

                mjson::JsonObject go{};
                go.SetNamedValue(L"trigger", Text(L"turnsOn"));
                go.SetNamedValue(L"kind", Text(L"goToPage"));
                go.SetNamedValue(L"targetPage", Text(plans[i].Id));

                mjson::JsonArray messages{};
                messages.Append(go);
                tab.SetNamedValue(L"messages", messages);
            }
        }

        mjson::JsonArray pages{};

        for (auto const& plan : plans)
        {
            Controls page{};
            page.Order = band.Order;

            for (size_t s = 0; s < plan.SceneCount; ++s)
            {
                auto const scene = plan.FirstScene + s;
                auto const name = scene < sequence.Scenes.size() && !sequence.Scenes[scene].Name.empty()
                    ? sequence.Scenes[scene].Name
                    : (text.SceneName ? text.SceneName(scene) : std::to_wstring(scene + 1));

                auto const control = page.Add(std::format(L"{}-scene-{}", plan.Id, scene + 1), L"button", name,
                    FirstPadX + static_cast<double>(s) * PadPitch, SceneRowY, PadWidth, ButtonHeight, Neutral);
                control.SetNamedValue(L"labelPlaced", Text(L"inside"));
                Press(control, L"note", 0, RemoteSequenceChannel, static_cast<uint32_t>(scene));
            }

            for (size_t r = 0; r < plan.TrackCount; ++r)
            {
                auto const index = plan.FirstTrack + r;

                if (index >= order.size())
                {
                    break;
                }

                auto const track = FindTrack(sequence, order[index]);
                auto const [group, channel] = RemoteTrackAddress(index);
                auto const y = FirstTrackY + static_cast<double>(r) * TrackPitch;
                auto const hue = static_cast<int32_t>(index % 6);
                auto const prefix = std::format(L"{}-track-{}", plan.Id, index + 1);

                auto const name = track != nullptr && !track->Name.empty()
                    ? track->Name
                    : (text.TrackName ? text.TrackName(index) : std::to_wstring(index + 1));

                auto const label = page.Add(prefix + L"-name", L"label", name, Margin, y, TrackNameWidth, PadHeight, hue);
                label.SetNamedValue(L"style", Text(L"bare"));
                label.SetNamedValue(L"labelPlaced", Text(L"insideCenter"));

                std::array<std::pair<std::wstring const*, uint8_t>, 3> const switches{ {
                    { &text.Mute, RemoteControllerMute },
                    { &text.Solo, RemoteControllerSolo },
                    { &text.Arm, RemoteControllerArm },
                } };

                for (size_t k = 0; k < switches.size(); ++k)
                {
                    auto const toggle = page.Add(std::format(L"{}-switch-{}", prefix, switches[k].second), L"toggle", *switches[k].first,
                        SwitchX + static_cast<double>(k) * SwitchPitch, y + 8, SwitchWidth, ButtonHeight, Neutral);
                    toggle.SetNamedValue(L"labelPlaced", Text(L"inside"));
                    Press(toggle, L"controlChange", group, channel, switches[k].second);
                    Listen(toggle, L"controlChange", group, channel, switches[k].second);
                }

                auto const stop = page.Add(prefix + L"-stop", L"pad", text.StopTrack, StopX, y, SwitchWidth, PadHeight, Neutral);
                stop.SetNamedValue(L"labelPlaced", Text(L"inside"));
                Press(stop, L"note", group, channel, RemoteNoteStop);

                for (size_t s = 0; s < plan.SceneCount; ++s)
                {
                    auto const scene = plan.FirstScene + s;
                    auto const clipId = track != nullptr && scene < track->Slots.size() ? track->Slots[scene] : std::wstring{};
                    auto const clip = clipId.empty() ? nullptr : FindClip(sequence, clipId);
                    auto const padName = clip != nullptr && !clip->Name.empty() ? clip->Name : text.EmptySlot;

                    auto const pad = page.Add(std::format(L"{}-slot-{}", prefix, scene + 1), L"pad", padName,
                        FirstPadX + static_cast<double>(s) * PadPitch, y, PadWidth, PadHeight, clip != nullptr ? hue : Neutral);
                    pad.SetNamedValue(L"labelPlaced", Text(L"inside"));
                    Press(pad, L"note", group, channel, static_cast<uint32_t>(scene));
                    Listen(pad, L"note", group, channel, static_cast<uint32_t>(scene));
                }
            }

            mjson::JsonObject pageObject{};
            pageObject.SetNamedValue(L"id", Text(plan.Id));
            pageObject.SetNamedValue(L"name", Text(pageName(plan)));
            pageObject.SetNamedValue(L"controls", page.List);
            pages.Append(pageObject);
        }

        // Last, so the layout opens on its first real page.
        mjson::JsonObject bandPage{};
        bandPage.SetNamedValue(L"id", Text(L"band"));
        bandPage.SetNamedValue(L"name", Text(text.Name));
        bandPage.SetNamedValue(L"sharedBand", Bool(true));
        bandPage.SetNamedValue(L"controls", band.List);
        pages.Append(bandPage);

        mjson::JsonObject provenance{};
        provenance.SetNamedValue(L"id", Text(text.Id));
        provenance.SetNamedValue(L"version", Text(L"1.0"));

        if (!text.Author.empty())
        {
            provenance.SetNamedValue(L"author", Text(text.Author));
        }

        if (!text.License.empty())
        {
            provenance.SetNamedValue(L"license", Text(text.License));
        }

        provenance.SetNamedValue(L"created", Text(text.Created));
        provenance.SetNamedValue(L"tool", Text(text.Tool));

        // Made by a program from the sequence, with no AI.
        provenance.SetNamedValue(L"digitalSourceType", Text(L"algorithmicMedia"));

        mjson::JsonObject basedOn{};
        basedOn.SetNamedValue(L"name", Text(sequence.Name));

        if (sequence.Provenance.has_value())
        {
            if (!sequence.Provenance->Author.empty())
            {
                basedOn.SetNamedValue(L"author", Text(sequence.Provenance->Author));
            }

            if (!sequence.Provenance->Id.empty())
            {
                basedOn.SetNamedValue(L"id", Text(sequence.Provenance->Id));
            }

            if (!sequence.Provenance->Version.empty())
            {
                basedOn.SetNamedValue(L"version", Text(sequence.Provenance->Version));
            }
        }

        provenance.SetNamedValue(L"basedOn", basedOn);

        mjson::JsonObject match{};
        match.SetNamedValue(L"transportSuppliedEndpointName", Text(text.EndpointName));

        mjson::JsonObject device{};
        device.SetNamedValue(L"name", Text(DeviceName));
        device.SetNamedValue(L"match", match);
        device.SetNamedValue(L"matchMode", Text(L"endpointName"));
        device.SetNamedValue(L"protocol", Text(L"midi1"));

        mjson::JsonArray devices{};
        devices.Append(device);

        mjson::JsonObject layout{};
        layout.SetNamedValue(L"fileVersion", Number(1));
        layout.SetNamedValue(L"name", Text(text.Name));
        layout.SetNamedValue(L"description", Text(text.Description));
        layout.SetNamedValue(L"provenance", provenance);
        layout.SetNamedValue(L"pageWidth", Number(PageWidth));
        layout.SetNamedValue(L"pageHeight", Number(PageHeight));
        layout.SetNamedValue(L"canvasWidth", Number(PageWidth));
        layout.SetNamedValue(L"canvasHeight", Number(PageHeight));
        layout.SetNamedValue(L"theme", Text(L"Studio Dark"));
        layout.SetNamedValue(L"scaleMode", Text(L"fitToScreen"));
        layout.SetNamedValue(L"devices", devices);
        layout.SetNamedValue(L"pages", pages);
        layout.SetNamedValue(L"sequences", mjson::JsonArray{});

        return Indented(std::wstring{ layout.Stringify() });
    }
}
