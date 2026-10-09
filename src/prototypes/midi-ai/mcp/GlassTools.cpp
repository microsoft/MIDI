// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "GlassTools.h"
#include "EndpointTools.h"
#include "ToolText.h"

#include "LayoutModel.h"
#include "LayoutSerializer.h"
#include "LayoutStore.h"
#include "MackieControl.h"
#include "PageTemplates.h"
#include "ThemeModel.h"
#include "ControlFactory.h"

namespace midimcp
{
    namespace
    {
        constexpr wchar_t LayoutFileExtension[] = L".midilayout";
        constexpr int32_t PageMargin = 32;
        constexpr int32_t ControlGap = 24;
        constexpr int32_t MinimumPageSide = 320;
        constexpr int32_t MaximumPageSide = 7680;
        constexpr size_t MaximumDraftControls = 256;
        constexpr DWORD PreviewTimeoutMilliseconds = 30000;
        constexpr int32_t PreviewWidth = 1280;

        // One line each, for the model. A shipping version would share the palette's own words.
        struct KindNote
        {
            wchar_t const* Name;
            wchar_t const* Use;
        };

        constexpr KindNote KindNotes[] =
        {
            { L"knob", L"A round control you turn, for anything continuous." },
            { L"fader", L"A slider, for levels and anything you ride." },
            { L"pad", L"A square to hit, for drums and clips. Plays a note while held." },
            { L"button", L"Momentary: on while pressed, off when let go." },
            { L"toggle", L"Latching: each press turns it on or off." },
            { L"xyPad", L"A square field with two values, across and up. Each axis sends its own message." },
            { L"meter", L"Shows a level the device sends back. Sends nothing." },
            { L"lamp", L"Lights when something arrives from the device. Sends nothing." },
            { L"readout", L"Shows a value as a number. Sends nothing." },
            { L"label", L"Text on the surface. Sends nothing." },
            { L"image", L"A picture or video. Sends nothing. The picture is chosen in the app." },
            { L"pageTab", L"Moves to another page. Set up in the app." },
            { L"panel", L"A frame that groups controls. Sends nothing." },
            { L"joystick", L"A stick with two values that springs back to the middle." },
            { L"ribbon", L"A long touch strip." },
            { L"pianoKeyboard", L"Mono keyboard: piano keys that play one note at a time. For chords, use notePads or hexPads." },
            { L"beatClock", L"Sends MIDI clock and shows the beat." },
            { L"timeDisplay", L"A stopwatch. Sends nothing." },
            { L"lfo", L"Moves a value up and down by itself while running." },
            { L"turntable", L"A platter you push forward and back, like a DJ deck." },
            { L"wheel", L"A pitch bend wheel. With springsBack false it is a modulation wheel." },
            { L"switch", L"A switch with several positions." },
            { L"steps", L"A step sequencer that plays a short pattern of notes." },
            { L"line", L"A line that divides the surface. Sends nothing." },
            { L"notePads", L"A grid of pads that play notes, colored by key." },
            { L"hexPads", L"A grid of hexagon pads laid out by interval." },
        };

        struct KindInfo
        {
            glass::ControlKind Kind{};
            std::wstring Name{};
            std::wstring Use{};
        };

        // The kind names, taken from the app's own writer rather than typed in again here: each
        // kind is written and read back, and only a name that comes back as the same kind counts.
        std::vector<KindInfo> const& Kinds()
        {
            static std::vector<KindInfo> const kinds = []
                {
                    std::vector<KindInfo> found{};

                    for (int32_t value = 0; value < 64; value++)
                    {
                        try
                        {
                            glass::LayoutDocument document{};
                            glass::Page page{};
                            glass::Control control{};

                            control.Id = L"k";
                            control.Kind = static_cast<glass::ControlKind>(value);
                            page.Id = L"p";
                            page.Controls.push_back(control);
                            document.Pages.push_back(page);

                            auto const text = glass::WriteLayoutToJson(document);
                            auto const back = glass::ReadLayoutFromJson(text);

                            if (!back.Succeeded || back.Document.Pages.empty() || back.Document.Pages[0].Controls.empty() ||
                                back.Document.Pages[0].Controls[0].Kind != control.Kind)
                            {
                                continue;
                            }

                            json::JsonObject root{ nullptr };

                            if (!json::JsonObject::TryParse(text, root))
                            {
                                continue;
                            }

                            auto const pages = ArrayOrNull(root, L"pages");
                            auto const controls = pages != nullptr && pages.Size() > 0
                                ? ArrayOrNull(pages.GetObjectAt(0), L"controls")
                                : nullptr;
                            auto const name = controls != nullptr && controls.Size() > 0
                                ? StringOrEmpty(controls.GetObjectAt(0), L"kind")
                                : std::wstring{};

                            if (name.empty() || std::any_of(found.begin(), found.end(), [&name](KindInfo const& k) { return k.Name == name; }))
                            {
                                continue;
                            }

                            KindInfo info{};
                            info.Kind = control.Kind;
                            info.Name = name;

                            for (auto const& note : KindNotes)
                            {
                                if (name == note.Name)
                                {
                                    info.Use = note.Use;
                                }
                            }

                            found.push_back(std::move(info));
                        }
                        catch (...)
                        {
                        }
                    }

                    return found;
                }();

            return kinds;
        }

        KindInfo const* FindKind(std::wstring const& name) noexcept
        {
            for (auto const& kind : Kinds())
            {
                if (EqualsIgnoringCase(kind.Name, name))
                {
                    return &kind;
                }
            }

            return nullptr;
        }

        std::wstring KindNameList()
        {
            std::vector<std::wstring> names{};

            for (auto const& kind : Kinds())
            {
                names.push_back(kind.Name);
            }

            return Join(names, L", ");
        }

        std::vector<std::wstring> ThemeNames()
        {
            std::vector<std::wstring> names{};

            for (auto const& theme : glass::BuiltInThemes())
            {
                names.push_back(theme.Name);
            }

            return names;
        }

        std::wstring LayoutFolder(GlassToolOptions const& options)
        {
            return options.LayoutFolder.empty() ? glass::LayoutsFolder() : options.LayoutFolder;
        }

        std::wstring FindMidiGlass(GlassToolOptions const& options)
        {
            std::vector<std::filesystem::path> candidates{};

            if (!options.MidiGlassExe.empty())
            {
                candidates.emplace_back(options.MidiGlassExe);
            }

            wil::unique_cotaskmem_string programFiles{};

            if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, nullptr, &programFiles)) && programFiles)
            {
                candidates.push_back(std::filesystem::path{ programFiles.get() } / L"Windows MIDI Services" / L"Tools" / L"Glass" / L"midiglass.exe");
            }

            // Beside a developer build of this prototype, the app's own build output.
            wchar_t self[MAX_PATH]{};

            if (::GetModuleFileNameW(nullptr, self, ARRAYSIZE(self)) > 0)
            {
                auto const outDir = std::filesystem::path{ self }.parent_path();
                auto const platform = outDir.parent_path().filename();

                candidates.push_back(outDir.parent_path().parent_path().parent_path().parent_path().parent_path() /
                    L"in-box" / L"vsfiles-sdk" / L"out" / L"midiglass" / platform / L"Release" / L"midiglass.exe");
            }

            for (auto const& candidate : candidates)
            {
                std::error_code ec{};

                if (std::filesystem::exists(candidate, ec))
                {
                    return candidate.wstring();
                }
            }

            return {};
        }

        // ------------------------------------------------------------------------------------
        // Building a document from the request

        struct DeviceBinding
        {
            std::wstring Name{};
            midiapp::LiveEndpoint Endpoint{};
        };

        struct Send
        {
            glass::MessageKind Kind{ glass::MessageKind::ControlChange };
            std::wstring Device{};
            int32_t Channel{ 0 };
            int32_t Group{ 0 };
            uint32_t Number{ 0 };
        };

        bool ReadSend(
            json::JsonObject const& item,
            std::vector<DeviceBinding> const& devices,
            std::wstring const& role,
            Send& send,
            Problems& problems)
        {
            auto const kind = StringOrEmpty(item, L"kind");

            if (EqualsIgnoringCase(kind, L"controlChange") || kind.empty())
            {
                send.Kind = glass::MessageKind::ControlChange;
            }
            else if (EqualsIgnoringCase(kind, L"note"))
            {
                send.Kind = glass::MessageKind::Note;
            }
            else if (EqualsIgnoringCase(kind, L"pitchBend"))
            {
                send.Kind = glass::MessageKind::PitchBend;
            }
            else if (EqualsIgnoringCase(kind, L"channelPressure"))
            {
                send.Kind = glass::MessageKind::ChannelPressure;
            }
            else if (EqualsIgnoringCase(kind, L"rpn"))
            {
                send.Kind = glass::MessageKind::RegisteredController;
            }
            else if (EqualsIgnoringCase(kind, L"nrpn"))
            {
                send.Kind = glass::MessageKind::AssignedController;
            }
            else
            {
                problems.Errors.push_back(role + L": this prototype can draft controlChange, note, pitchBend, channelPressure, rpn and nrpn. \"" +
                    kind + L"\" has to be set up in Windows MIDI Glass itself.");
                return false;
            }

            send.Device = StringOrEmpty(item, L"device");

            if (send.Device.empty() && devices.size() == 1)
            {
                send.Device = devices.front().Name;
            }

            auto const known = std::find_if(devices.begin(), devices.end(),
                [&send](DeviceBinding const& d) { return EqualsIgnoringCase(d.Name, send.Device); });

            if (known == devices.end())
            {
                problems.Errors.push_back(role + L": \"" + send.Device + L"\" is not in the layout's devices. Name one of the entries in \"devices\".");
                return false;
            }

            send.Device = known->Name;

            auto const channel = OptionalInteger(item, L"channel").value_or(1);
            auto const group = OptionalInteger(item, L"group").value_or(1);
            auto number = OptionalInteger(item, L"number").value_or(0);

            if (channel < 1 || channel > 16 || group < 1 || group > 16)
            {
                problems.Errors.push_back(role + L": channel and group are 1 to 16.");
                return false;
            }

            auto const msb = OptionalInteger(item, L"msb");
            auto const lsb = OptionalInteger(item, L"lsb");

            if (glass::HasBankAndIndex(send.Kind))
            {
                if (msb || lsb)
                {
                    if (msb.value_or(0) < 0 || msb.value_or(0) > 127 || lsb.value_or(0) < 0 || lsb.value_or(0) > 127)
                    {
                        problems.Errors.push_back(role + L": msb and lsb are 0 to 127.");
                        return false;
                    }

                    auto const combined = static_cast<int64_t>(glass::ControllerNumber(
                        static_cast<uint32_t>(msb.value_or(0)), static_cast<uint32_t>(lsb.value_or(0))));

                    if (item.HasKey(L"number") && number != combined)
                    {
                        problems.Errors.push_back(role + L": give number or msb and lsb, not both.");
                        return false;
                    }

                    number = combined;
                }

                if (number < 0 || number > static_cast<int64_t>(glass::MaximumControllerNumber))
                {
                    problems.Errors.push_back(role + L": an rpn or nrpn number is 0 to 16383, which is msb x 128 + lsb.");
                    return false;
                }
            }
            else
            {
                if (msb || lsb)
                {
                    problems.Warnings.push_back(role + L": msb and lsb only apply to rpn and nrpn, so they were ignored.");
                }

                if (number < 0 || number > 127)
                {
                    problems.Errors.push_back(role + L": number is 0 to 127.");
                    return false;
                }
            }

            if (!known->Endpoint.DestinationGroups[static_cast<size_t>(group - 1)])
            {
                problems.Warnings.push_back(role + L": \"" + known->Endpoint.Name + L"\" does not say it receives on group " +
                    std::to_wstring(group) + L". It may ignore these messages.");
            }

            send.Channel = static_cast<int32_t>(channel - 1);
            send.Group = static_cast<int32_t>(group - 1);
            send.Number = static_cast<uint32_t>(number);

            return true;
        }

        // A button-like control plays a note while held, and sends a controller at full on press
        // and at zero on release. Two axis controls give each axis its own row. Everything else
        // sends each row whenever it moves.
        void ApplySends(glass::Control& control, std::vector<Send> const& sends)
        {
            auto const buttonLike = control.Kind == glass::ControlKind::Pad ||
                control.Kind == glass::ControlKind::Button || control.Kind == glass::ControlKind::Toggle;

            auto const twoAxis = control.Kind == glass::ControlKind::XYPad || control.Kind == glass::ControlKind::Joystick;

            auto const template_ = control.Messages.empty() ? glass::ControlMessage{} : control.Messages.front();

            control.Messages.clear();

            for (size_t i = 0; i < sends.size(); i++)
            {
                auto const& send = sends[i];

                auto row = template_;
                row.Kind = send.Kind;
                row.DeviceName = send.Device;
                row.ChannelIndex = send.Channel;
                row.GroupIndex = send.Group;
                row.Number = send.Number;
                row.Axis = twoAxis && i == 1 ? glass::ValueAxis::Y : glass::ValueAxis::X;

                if (buttonLike && send.Kind == glass::MessageKind::Note)
                {
                    row.Trigger = glass::MessageTrigger::TurnsOn;

                    auto off = row;
                    off.Trigger = glass::MessageTrigger::TurnsOff;

                    control.Messages.push_back(row);
                    control.Messages.push_back(off);
                }
                else
                {
                    row.Trigger = glass::MessageTrigger::Changes;
                    control.Messages.push_back(row);
                }
            }
        }

        struct Draft
        {
            glass::LayoutDocument Document{};
            std::vector<DeviceBinding> Devices{};
            std::wstring Request{};
        };

        struct Placed
        {
            double X{};
            double Y{};
            double Width{};
            double Height{};
        };

        bool Overlaps(Placed const& a, Placed const& b) noexcept
        {
            return a.X < b.X + b.Width && b.X < a.X + a.Width && a.Y < b.Y + b.Height && b.Y < a.Y + a.Height;
        }

        void ReadPage(
            json::JsonObject const& item,
            std::wstring const& pageName,
            Draft& draft,
            Problems& problems)
        {
            auto& document = draft.Document;

            glass::Page page{};
            page.Id = glass::LayoutDocument::NewId();
            page.Name = pageName;

            auto const controls = ArrayOrNull(item, L"controls");

            if (controls == nullptr || controls.Size() == 0)
            {
                problems.Errors.push_back(L"Page \"" + pageName + L"\" needs at least one control in \"controls\".");
                return;
            }

            if (controls.Size() > MaximumDraftControls)
            {
                problems.Errors.push_back(L"A page in a draft can hold at most " + std::to_wstring(MaximumDraftControls) + L" controls.");
                return;
            }

            auto const firstDevice = draft.Devices.empty() ? std::wstring{} : draft.Devices.front().Name;

            // Controls with no position flow left to right in rows, the way somebody would lay out a
            // strip of faders by hand.
            double cursorX = PageMargin;
            double cursorY = PageMargin;
            double rowHeight = 0;
            std::vector<Placed> placed{};
            size_t number{ 0 };

            for (auto const& value : controls)
            {
                number++;
                auto const role = L"Page \"" + pageName + L"\" control " + std::to_wstring(number);

                if (value.ValueType() != json::JsonValueType::Object)
                {
                    problems.Errors.push_back(role + L" is not an object.");
                    continue;
                }

                auto const entry = value.GetObject();
                auto const kindName = StringOrEmpty(entry, L"kind");
                auto const kind = FindKind(kindName);

                if (kind == nullptr)
                {
                    problems.Errors.push_back(role + L": \"" + kindName + L"\" is not a control kind. Use one of: " + KindNameList() + L".");
                    continue;
                }

                auto control = glass::MakeNewControl(kind->Kind, 0, 0, document.PageWidth, document.PageHeight, firstDevice, page);

                if (auto const label = StringOrEmpty(entry, L"label"); !label.empty())
                {
                    control.Label = glass::SanitizeStoredString(label);
                }

                if (auto const hue = entry.HasKey(L"hue") ? entry.GetNamedValue(L"hue") : nullptr; hue != nullptr)
                {
                    auto const slot = IntegerFromValue(hue);

                    if (hue.ValueType() == json::JsonValueType::String && EqualsIgnoringCase(hue.GetString(), L"neutral"))
                    {
                        control.HueSlot = glass::NeutralSlot;
                    }
                    else if (slot && *slot >= 1 && *slot <= glass::HueSlotCount)
                    {
                        control.HueSlot = static_cast<int32_t>(*slot - 1);
                    }
                    else
                    {
                        problems.Errors.push_back(role + L": hue is 1 to 6, one of the theme's six colors, or \"neutral\".");
                    }
                }

                auto const width = OptionalNumber(entry, L"width");
                auto const height = OptionalNumber(entry, L"height");

                if (width)
                {
                    control.Width = glass::QuantizePixels(*width);
                }

                if (height)
                {
                    control.Height = glass::QuantizePixels(*height);
                }

                if (auto const springs = OptionalBool(entry, L"springsBack"); springs.has_value())
                {
                    control.ReturnsToDefault = *springs;
                }

                auto const x = OptionalNumber(entry, L"x");
                auto const y = OptionalNumber(entry, L"y");

                if (x.has_value() != y.has_value())
                {
                    problems.Errors.push_back(role + L": give both x and y, or neither and let the layout flow.");
                }

                if (x && y)
                {
                    control.X = glass::QuantizePixels(*x);
                    control.Y = glass::QuantizePixels(*y);
                }
                else
                {
                    if (OptionalBool(entry, L"newRow").value_or(false) && rowHeight > 0)
                    {
                        cursorX = PageMargin;
                        cursorY += rowHeight + ControlGap;
                        rowHeight = 0;
                    }

                    if (cursorX + control.Width > document.PageWidth - PageMargin && cursorX > PageMargin)
                    {
                        cursorX = PageMargin;
                        cursorY += rowHeight + ControlGap;
                        rowHeight = 0;
                    }

                    control.X = cursorX;
                    control.Y = cursorY;
                    cursorX += control.Width + ControlGap;
                    rowHeight = std::max(rowHeight, control.Height);
                }

                if (auto const sends = ArrayOrNull(entry, L"sends"); sends != nullptr)
                {
                    if (!glass::SendsAnything(kind->Kind))
                    {
                        problems.Errors.push_back(role + L": a " + kind->Name + L" does not send anything.");
                    }
                    else
                    {
                        std::vector<Send> list{};
                        bool good{ true };

                        for (auto const& sendValue : sends)
                        {
                            Send send{};

                            if (sendValue.ValueType() != json::JsonValueType::Object ||
                                !ReadSend(sendValue.GetObject(), draft.Devices, role, send, problems))
                            {
                                good = false;
                                break;
                            }

                            list.push_back(send);
                        }

                        if (good && !list.empty())
                        {
                            ApplySends(control, list);
                        }
                    }
                }
                else if (glass::SendsAnything(kind->Kind) && firstDevice.empty())
                {
                    problems.Errors.push_back(role + L": a " + kind->Name + L" sends MIDI, so the layout needs a device in \"devices\".");
                }

                Placed box{ control.X, control.Y, control.Width, control.Height };

                for (size_t other = 0; other < placed.size(); other++)
                {
                    if (Overlaps(box, placed[other]) && kind->Kind != glass::ControlKind::Panel)
                    {
                        problems.Warnings.push_back(role + L" overlaps control " + std::to_wstring(other + 1) + L" on the same page.");
                        break;
                    }
                }

                placed.push_back(box);
                page.Controls.push_back(std::move(control));
            }

            document.Pages.push_back(std::move(page));
        }

        std::optional<Draft> ReadDraft(json::JsonObject const& arguments, std::vector<midiapp::LiveEndpoint> const& live, Problems& problems)
        {
            Draft draft{};
            auto& document = draft.Document;

            document.Name = glass::SanitizeStoredString(StringOrEmpty(arguments, L"name"));
            document.Description = glass::SanitizeStoredString(StringOrEmpty(arguments, L"description"));
            draft.Request = glass::SanitizeStoredString(StringOrEmpty(arguments, L"request"));

            if (document.Name.empty())
            {
                problems.Errors.push_back(L"Give the layout a name, the way the customer would say it.");
            }

            if (auto const page = ObjectOrNull(arguments, L"page"); page != nullptr)
            {
                auto const width = OptionalInteger(page, L"width").value_or(document.PageWidth);
                auto const height = OptionalInteger(page, L"height").value_or(document.PageHeight);

                if (width < MinimumPageSide || width > MaximumPageSide || height < MinimumPageSide || height > MaximumPageSide)
                {
                    problems.Errors.push_back(L"Page width and height are " + std::to_wstring(MinimumPageSide) + L" to " +
                        std::to_wstring(MaximumPageSide) + L" pixels. The usual sizes are 1280 x 800, 1920 x 1080 and 1024 x 768.");
                }
                else
                {
                    document.PageWidth = glass::QuantizePixels(static_cast<double>(width));
                    document.PageHeight = glass::QuantizePixels(static_cast<double>(height));
                }
            }

            document.CanvasWidth = document.PageWidth;
            document.CanvasHeight = document.PageHeight;

            if (auto const theme = StringOrEmpty(arguments, L"theme"); !theme.empty())
            {
                auto const current = glass::CurrentThemeName(theme);

                if (glass::FindBuiltInTheme(current) == nullptr)
                {
                    problems.Errors.push_back(L"\"" + theme + L"\" is not a Windows MIDI Glass theme. Use one of: " + Join(ThemeNames(), L", ") + L".");
                }
                else
                {
                    document.ThemeName = glass::FindBuiltInTheme(current)->Name;
                }
            }

            if (auto const devices = ArrayOrNull(arguments, L"devices"); devices != nullptr)
            {
                for (auto const& value : devices)
                {
                    if (value.ValueType() != json::JsonValueType::Object)
                    {
                        continue;
                    }

                    auto const item = value.GetObject();
                    auto const name = glass::SanitizeStoredString(StringOrEmpty(item, L"name"));
                    auto const asked = StringOrEmpty(item, L"endpoint");

                    if (asked.empty())
                    {
                        problems.Errors.push_back(L"Each device names an endpoint, for example { \"name\": \"Synth\", \"endpoint\": \"Pro 3\" }.");
                        continue;
                    }

                    auto const lookup = FindEndpoint(live, asked);

                    if (!lookup.Found)
                    {
                        problems.Errors.push_back(DescribeLookupProblem(L"Device", asked, lookup, live));
                        continue;
                    }

                    DeviceBinding binding{};
                    binding.Name = name.empty() ? lookup.Found->Name : name;
                    binding.Endpoint = *lookup.Found;

                    if (std::any_of(draft.Devices.begin(), draft.Devices.end(),
                        [&binding](DeviceBinding const& d) { return EqualsIgnoringCase(d.Name, binding.Name); }))
                    {
                        problems.Errors.push_back(L"Two devices are both called \"" + binding.Name + L"\". Give each a different name.");
                        continue;
                    }

                    glass::DeviceEntry entry{};
                    entry.Name = binding.Name;
                    entry.Match = binding.Endpoint.BuildMatch();
                    entry.MatchMode = midiapp::EndpointMatchMode::EndpointDeviceId;

                    document.Devices.push_back(entry);
                    draft.Devices.push_back(std::move(binding));
                }
            }

            if (auto const pages = ArrayOrNull(arguments, L"pages"); pages != nullptr)
            {
                size_t number{ 0 };

                for (auto const& value : pages)
                {
                    number++;

                    if (value.ValueType() != json::JsonValueType::Object)
                    {
                        continue;
                    }

                    auto const item = value.GetObject();
                    auto name = glass::SanitizeStoredString(StringOrEmpty(item, L"name"));

                    ReadPage(item, name.empty() ? L"Page " + std::to_wstring(number) : name, draft, problems);
                }
            }
            else if (arguments.HasKey(L"controls"))
            {
                ReadPage(arguments, L"Page 1", draft, problems);
            }

            if (document.Pages.empty() && !problems.HasErrors())
            {
                problems.Errors.push_back(L"A layout needs controls, either in \"controls\" or in \"pages\".");
            }

            if (problems.HasErrors())
            {
                return std::nullopt;
            }

            // The app's own check: what it would refuse to run.
            for (auto const& issue : glass::Validate(document))
            {
                problems.Errors.push_back(issue.Detail);
            }

            for (auto const outside : document.ControlsOutsidePage())
            {
                problems.Errors.push_back(L"\"" + (outside->Label.empty() ? outside->Id : outside->Label) +
                    L"\" is outside the page. Make it smaller, move it, or make the page bigger.");
            }

            auto const now = CurrentUnixSeconds();
            document.CreatedTimestamp = now;
            document.ModifiedTimestamp = now;

            // What a future app would read to show a review bar. The app keeps fields it does not
            // know when it saves, so this survives until the app itself decides to clear it.
            json::JsonObject marker{};
            marker.SetNamedValue(L"request", json::JsonValue::CreateStringValue(draft.Request));
            marker.SetNamedValue(L"created", json::JsonValue::CreateNumberValue(static_cast<double>(now)));

            document.Unknown = json::JsonObject{};
            document.Unknown.SetNamedValue(L"_draft", marker);

            return draft;
        }

        std::wstring DescribeMessage(glass::ControlMessage const& message)
        {
            // A function is sent the way Mackie Control says, so its channel is not the row's.
            if (message.Kind == glass::MessageKind::MackieControl)
            {
                auto const name = glass::MackieFunctionFileName(message.Number);

                return L"Mackie Control " + (name.empty() ? std::wstring{ L"(no function yet)" } : name) +
                    L" to " + message.DeviceName;
            }

            std::wstring what{};

            switch (message.Kind)
            {
            case glass::MessageKind::Note: what = L"note " + NoteName(static_cast<uint8_t>(message.Number & 0x7F)); break;
            case glass::MessageKind::ControlChange: what = L"CC " + std::to_wstring(message.Number); break;
            case glass::MessageKind::PitchBend: what = L"pitch bend"; break;
            case glass::MessageKind::ChannelPressure: what = L"channel pressure"; break;
            case glass::MessageKind::RegisteredController: what = L"RPN " + glass::FormatMessageNumber(message.Kind, message.Number); break;
            case glass::MessageKind::AssignedController: what = L"NRPN " + glass::FormatMessageNumber(message.Kind, message.Number); break;
            case glass::MessageKind::RawUmp: what = L"clock"; break;
            default: what = L"a message"; break;
            }

            return what + L" on channel " + std::to_wstring(message.ChannelIndex + 1) + L" to " + message.DeviceName;
        }

        std::wstring DescribeDraft(Draft const& draft)
        {
            auto const& document = draft.Document;

            std::wstring text = L"Layout \"" + document.Name + L"\", page " + std::to_wstring(document.PageWidth) + L" x " +
                std::to_wstring(document.PageHeight) + L", theme " + (document.ThemeName.empty() ? L"the app's default" : document.ThemeName) + L".\n";

            for (auto const& device : draft.Devices)
            {
                text += L"Device \"" + device.Name + L"\" is " + device.Endpoint.Name + L".\n";
            }

            for (auto const& page : document.Pages)
            {
                text += L"Page \"" + page.Name + L"\":\n";

                for (auto const& control : page.Controls)
                {
                    auto const kind = std::find_if(Kinds().begin(), Kinds().end(), [&control](KindInfo const& k) { return k.Kind == control.Kind; });

                    text += L"- " + (kind != Kinds().end() ? kind->Name : std::wstring{ L"control" });

                    if (!control.Label.empty())
                    {
                        text += L" \"" + control.Label + L"\"";
                    }

                    text += std::format(L" at {},{} size {} x {}", control.X, control.Y, control.Width, control.Height);

                    std::vector<std::wstring> sends{};

                    for (auto const& message : control.Messages)
                    {
                        if (message.Trigger == glass::MessageTrigger::TurnsOff)
                        {
                            continue;
                        }

                        sends.push_back(DescribeMessage(message));
                    }

                    if (!sends.empty())
                    {
                        text += L", sends " + Join(sends, L" and ");
                    }

                    if (control.ReturnsToDefault)
                    {
                        text += L", springs back";
                    }

                    text += L"\n";
                }
            }

            return text;
        }

        // ------------------------------------------------------------------------------------
        // Files

        std::optional<std::wstring> WriteNewLayout(std::wstring const& folder, glass::LayoutDocument const& document)
        {
            std::error_code ec{};
            std::filesystem::create_directories(folder, ec);

            auto const bytes = WideToUtf8(glass::WriteLayoutToJson(document));

            for (int attempt = 0; attempt < 16; attempt++)
            {
                auto const path = glass::MakeUnusedLayoutPath(folder, document.Name);

                if (path.empty())
                {
                    return std::nullopt;
                }

                // CREATE_NEW, so a file that appears after the name was chosen is never replaced.
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

        std::optional<std::vector<uint8_t>> ReadAllBytes(std::wstring const& path)
        {
            std::error_code ec{};
            auto const size = std::filesystem::file_size(path, ec);

            if (ec || size == 0 || size > 32 * 1024 * 1024)
            {
                return std::nullopt;
            }

            wil::unique_hfile file{ ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) };

            if (!file)
            {
                return std::nullopt;
            }

            std::vector<uint8_t> bytes(static_cast<size_t>(size));
            DWORD read{ 0 };

            if (!::ReadFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr))
            {
                return std::nullopt;
            }

            bytes.resize(read);
            return bytes;
        }

        // The app's own renderer, in its headless mode: no window, no device, exits when done.
        std::optional<std::vector<uint8_t>> RenderPreview(std::wstring const& midiGlass, glass::LayoutDocument const& document, std::wstring& failure)
        {
            wchar_t tempRoot[MAX_PATH]{};

            if (::GetTempPathW(ARRAYSIZE(tempRoot), tempRoot) == 0)
            {
                failure = L"No temporary folder.";
                return std::nullopt;
            }

            auto const folder = std::filesystem::path{ tempRoot } / L"midi-mcp-spike";
            std::error_code ec{};
            std::filesystem::create_directories(folder, ec);

            auto const stem = NewGuidText();
            auto const layoutPath = (folder / (stem + LayoutFileExtension)).wstring();
            auto const picturePath = (folder / (stem + L".png")).wstring();

            auto const cleanup = wil::scope_exit([&]
                {
                    std::error_code ignored{};
                    std::filesystem::remove(layoutPath, ignored);
                    std::filesystem::remove(picturePath, ignored);
                });

            if (!glass::WriteLayoutFile(document, layoutPath))
            {
                failure = L"The preview layout could not be written.";
                return std::nullopt;
            }

            auto commandLine = L"\"" + midiGlass + L"\" --thumbnail \"" + layoutPath + L"\" \"" + picturePath + L"\" " + std::to_wstring(PreviewWidth);

            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};

            // No inherited handles: this process's stdout is the MCP channel, and nothing but
            // protocol messages may ever reach it.
            if (!::CreateProcessW(midiGlass.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
            {
                failure = L"Windows MIDI Glass could not be started to draw the preview.";
                return std::nullopt;
            }

            wil::unique_handle processHandle{ process.hProcess };
            wil::unique_handle threadHandle{ process.hThread };

            if (::WaitForSingleObject(processHandle.get(), PreviewTimeoutMilliseconds) != WAIT_OBJECT_0)
            {
                ::TerminateProcess(processHandle.get(), 1);
                failure = L"Windows MIDI Glass took too long to draw the preview.";
                return std::nullopt;
            }

            DWORD exitCode{ 1 };
            ::GetExitCodeProcess(processHandle.get(), &exitCode);

            if (exitCode != 0)
            {
                failure = L"Windows MIDI Glass could not draw the preview (exit code " + std::to_wstring(exitCode) + L").";
                return std::nullopt;
            }

            auto bytes = ReadAllBytes(picturePath);

            if (!bytes)
            {
                failure = L"Windows MIDI Glass drew nothing.";
            }

            return bytes;
        }

        // ------------------------------------------------------------------------------------
        // Tools

        constexpr wchar_t LayoutSchema[] = LR"({
            "type": "object",
            "properties": {
                "name": { "type": "string", "description": "Name for the layout, the way the customer would say it." },
                "description": { "type": "string" },
                "request": { "type": "string", "description": "What the customer asked for, in their own words." },
                "theme": { "type": "string", "description": "One of: THEMES. Leave out for the app's default." },
                "page": {
                    "type": "object",
                    "description": "Page size in pixels. Default 1280 x 800. A page never stretches: it scales to fit the screen.",
                    "properties": { "width": { "type": "integer" }, "height": { "type": "integer" } }
                },
                "devices": {
                    "type": "array",
                    "description": "The layout's own names for the endpoints it sends to. Controls name these, not endpoints.",
                    "items": {
                        "type": "object",
                        "properties": {
                            "name": { "type": "string", "description": "Short name used by controls, for example Synth." },
                            "endpoint": { "type": "string", "description": "Endpoint name or id from list_midi_endpoints." }
                        },
                        "required": ["endpoint"]
                    }
                },
                "controls": { "type": "array", "description": "Controls for a one page layout. Or use pages.", "items": { "$comment": "same as pages[].controls[]" } },
                "pages": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "properties": {
                            "name": { "type": "string" },
                            "controls": {
                                "type": "array",
                                "items": {
                                    "type": "object",
                                    "properties": {
                                        "kind": { "type": "string", "description": "One of: KINDS. See list_glass_controls." },
                                        "label": { "type": "string" },
                                        "hue": { "type": ["integer", "string"], "description": "1 to 6, one of the theme's six colors, or \"neutral\". Default walks through the six." },
                                        "x": { "type": "number" }, "y": { "type": "number" },
                                        "width": { "type": "number" }, "height": { "type": "number" },
                                        "newRow": { "type": "boolean", "description": "Start a new row before this control when positions are left out." },
                                        "springsBack": { "type": "boolean", "description": "Returns to its resting value when let go, like a pitch wheel." },
                                        "sends": {
                                            "type": "array",
                                            "description": "What the control sends. Leave out to take the app's default for the kind on the first device. An XY pad or joystick takes two, one per axis.",
                                            "items": {
                                                "type": "object",
                                                "properties": {
                                                    "kind": { "type": "string", "enum": ["controlChange", "note", "pitchBend", "channelPressure", "rpn", "nrpn"], "description": "rpn and nrpn go out as one MIDI 2.0 registered or assignable controller message. Windows turns that into CC 101/100 (RPN) or CC 99/98 (NRPN), then CC 6 and CC 38, for a MIDI 1.0 device." },
                                                    "device": { "type": "string", "description": "A name from devices. May be left out when there is one device." },
                                                    "channel": { "type": "integer", "minimum": 1, "maximum": 16 },
                                                    "group": { "type": "integer", "minimum": 1, "maximum": 16 },
                                                    "number": { "type": "integer", "minimum": 0, "maximum": 16383, "description": "Controller or note number, 0 to 127. For rpn and nrpn, the parameter number, 0 to 16383, which is msb x 128 + lsb. Or give msb and lsb instead." },
                                                    "msb": { "type": "integer", "minimum": 0, "maximum": 127, "description": "rpn and nrpn only. The parameter number's MSB, the value of CC 101 (RPN) or CC 99 (NRPN). MIDI 2.0 calls it the bank." },
                                                    "lsb": { "type": "integer", "minimum": 0, "maximum": 127, "description": "rpn and nrpn only. The parameter number's LSB, the value of CC 100 (RPN) or CC 98 (NRPN). MIDI 2.0 calls it the index." }
                                                },
                                                "required": ["kind"]
                                            }
                                        }
                                    },
                                    "required": ["kind"]
                                }
                            }
                        },
                        "required": ["controls"]
                    }
                }
            },
            "required": ["name"]
        })";

        std::wstring LayoutSchemaText()
        {
            std::wstring text{ LayoutSchema };

            auto const replace = [&text](std::wstring_view token, std::wstring const& value)
                {
                    if (auto const at = text.find(token); at != std::wstring::npos)
                    {
                        text.replace(at, token.size(), value);
                    }
                };

            replace(L"THEMES", Join(ThemeNames(), L", "));
            replace(L"KINDS", KindNameList());

            return text;
        }
    }

    _Use_decl_annotations_
    std::vector<ToolDefinition> MakeGlassTools(GlassToolOptions const& options)
    {
        std::vector<ToolDefinition> tools{};

        {
            ToolDefinition tool{};
            tool.Name = L"list_glass_controls";
            tool.Title = L"List Windows MIDI Glass controls and themes";
            tool.Description =
                L"Lists every kind of control a Windows MIDI Glass layout can hold, what each is for, its starting size on a "
                L"1280 x 800 page and what it sends when it is first placed, plus the themes and the usual page sizes. "
                L"Read this before designing a layout.";
            tool.InputSchema = LR"({ "type": "object", "additionalProperties": false })";
            tool.Annotations = { true, false, true, false };
            tool.Handler = [](json::JsonObject const&, CallContext const&)
                {
                    std::wstring text = L"Control kinds (name: use; default size on 1280 x 800; default send):\n";

                    glass::Page empty{};
                    empty.Id = L"p";

                    for (auto const& kind : Kinds())
                    {
                        auto const size = glass::DefaultControlSize(kind.Kind, 1280, 800);
                        auto const sample = glass::MakeNewControl(kind.Kind, 0, 0, 1280, 800, L"Device", empty);

                        std::wstring sends = glass::SendsAnything(kind.Kind) && !sample.Messages.empty()
                            ? DescribeMessage(sample.Messages.front())
                            : L"nothing";

                        text += L"- " + kind.Name + L": " + (kind.Use.empty() ? L"(no note)" : kind.Use) +
                            L" " + std::to_wstring(size.Width) + L" x " + std::to_wstring(size.Height) + L"; " + sends +
                            (sample.ReturnsToDefault ? L"; springs back" : L"") + L"\n";
                    }

                    text += L"\nThemes: " + Join(ThemeNames(), L", ") + L".\n";

                    std::vector<std::wstring> pages{};
                    for (auto const& page : glass::PageTemplates())
                    {
                        if (page.Width > 0 && page.Height > 0)
                        {
                            pages.push_back(std::to_wstring(page.Width) + L" x " + std::to_wstring(page.Height));
                        }
                    }

                    text += L"Page sizes: " + Join(pages, L", ") + L". Positions and sizes are in page pixels, snapped to 4.\n";
                    text += L"Each control uses one of the theme's six colors (hue 1 to 6), so a new theme recolors the whole surface.\n";
                    text += L"A control can send control changes, notes, pitch bend, channel pressure, RPN and NRPN. An RPN or NRPN goes "
                        L"out as one MIDI 2.0 message, and Windows turns it into the CC 101/100 or 99/98, CC 6 and CC 38 a MIDI 1.0 device expects.\n";

                    ToolResult result{};
                    result.AddText(text);

                    return result;
                };

            tools.push_back(std::move(tool));
        }

        {
            ToolDefinition tool{};
            tool.Name = L"list_glass_layouts";
            tool.Title = L"List Windows MIDI Glass layouts";
            tool.Description = L"Lists the layouts saved in Windows MIDI Glass: name, description, page size, theme, devices, and how many controls each has.";
            tool.InputSchema = LR"({ "type": "object", "additionalProperties": false })";
            tool.Annotations = { true, false, true, false };
            tool.Handler = [options](json::JsonObject const&, CallContext const&)
                {
                    auto const folder = LayoutFolder(options);
                    std::wstring text{};
                    size_t count{ 0 };
                    std::error_code ec{};

                    if (!folder.empty() && std::filesystem::exists(folder, ec))
                    {
                        for (auto const& entry : std::filesystem::directory_iterator{ folder, ec })
                        {
                            auto const name = entry.path().filename().wstring();

                            if (ec || count >= 256)
                            {
                                break;
                            }

                            // Either extension: MIDI Glass renames old files when it starts, but it
                            // may not have been started since.
                            if (!entry.is_regular_file(ec) || !glass::IsLayoutFileName(name))
                            {
                                continue;
                            }

                            auto const read = glass::ReadLayoutFile(entry.path().wstring());

                            if (!read.Succeeded)
                            {
                                continue;
                            }

                            auto const& document = read.Document;
                            count++;

                            std::vector<std::wstring> devices{};
                            for (auto const& device : document.Devices)
                            {
                                devices.push_back(device.Name);
                            }

                            auto const isDraft = document.Unknown != nullptr && document.Unknown.HasKey(L"_draft");

                            text += L"- \"" + document.Name + L"\"" + (isDraft ? L" [draft, not reviewed]" : L"") + L", " +
                                std::to_wstring(document.PageWidth) + L" x " + std::to_wstring(document.PageHeight) + L", " +
                                std::to_wstring(document.Pages.size()) + L" page(s), " + std::to_wstring(document.ControlCount()) +
                                L" controls, theme " + (document.ThemeName.empty() ? L"default" : document.ThemeName) +
                                L", devices: " + (devices.empty() ? L"none" : Join(devices, L", ")) +
                                (document.Description.empty() ? L"" : L". " + document.Description) + L"\n";
                        }
                    }

                    ToolResult result{};
                    result.AddText(std::to_wstring(count) + L" saved layout" + (count == 1 ? L"" : L"s") + L".\n" + text);

                    return result;
                };

            tools.push_back(std::move(tool));
        }

        {
            ToolDefinition tool{};
            tool.Name = L"preview_layout";
            tool.Title = L"Preview a Windows MIDI Glass layout";
            tool.Description =
                L"Builds a layout without saving it and returns a picture drawn by Windows MIDI Glass itself, plus what each "
                L"control sends and anything that is wrong. The picture shows where controls sit, their sizes and their "
                L"colors; labels are listed in the text, not drawn. Look at the picture and fix overlaps, crowding and "
                L"wasted space before showing it to the customer. When a device name matches more than one endpoint the "
                L"answer lists them so you can ask. Nothing is sent to any device.";
            tool.InputSchema = LayoutSchemaText();
            tool.Annotations = { true, false, true, false };
            tool.Handler = [options](json::JsonObject const& arguments, CallContext const&)
                {
                    Problems problems{};
                    auto const live = LiveEndpoints();
                    auto const draft = ReadDraft(arguments, live, problems);

                    ToolResult result{};

                    if (!draft)
                    {
                        std::wstring text = L"This layout cannot be drawn yet.\n";
                        problems.AppendTo(text);
                        return ToolResult::Error(text);
                    }

                    std::wstring text = DescribeDraft(*draft);
                    problems.AppendTo(text);

                    auto const midiGlass = FindMidiGlass(options);
                    std::wstring failure{};

                    if (midiGlass.empty())
                    {
                        text += L"\nNo picture: Windows MIDI Glass is not installed on this PC.\n";
                    }
                    else if (auto const picture = RenderPreview(midiGlass, draft->Document, failure); picture)
                    {
                        result.AddImage(Base64(*picture), L"image/png");
                    }
                    else
                    {
                        text += L"\nNo picture: " + failure + L"\n";
                    }

                    result.AddText(text);
                    result.IsError = problems.HasErrors();

                    return result;
                };

            tools.push_back(std::move(tool));
        }

        {
            ToolDefinition tool{};
            tool.Name = L"save_layout_draft";
            tool.Title = L"Save a Windows MIDI Glass draft";
            tool.Description =
                L"Saves the layout as a new draft in Windows MIDI Glass. Nothing is sent to any device until the customer opens it and "
                L"runs it. It never replaces an existing layout. Call preview_layout first, look at the picture, and get the "
                L"customer's agreement.";
            tool.InputSchema = LayoutSchemaText();
            tool.Annotations = { false, false, false, false };
            tool.Handler = [options](json::JsonObject const& arguments, CallContext const& context)
                {
                    Problems problems{};
                    auto const live = LiveEndpoints();
                    auto draft = ReadDraft(arguments, live, problems);

                    if (!draft || problems.HasErrors())
                    {
                        std::wstring text = L"Nothing was saved.\n";
                        problems.AppendTo(text);
                        return ToolResult::Error(text);
                    }

                    auto marker = draft->Document.Unknown.GetNamedObject(L"_draft");
                    marker.SetNamedValue(L"by", json::JsonValue::CreateStringValue(
                        context.ClientName.empty() ? L"an AI assistant" : glass::SanitizeStoredString(context.ClientName)));

                    auto const folder = LayoutFolder(options);
                    auto const path = folder.empty() ? std::nullopt : WriteNewLayout(folder, draft->Document);

                    if (!path)
                    {
                        return ToolResult::Error(L"The draft could not be written to the MIDI Layouts folder.");
                    }

                    std::wstring text = L"Saved a draft: " + DisplayPathUnderDocuments(*path) + L"\n\n" + DescribeDraft(*draft) +
                        L"\nThe customer finds it in the Windows MIDI Glass library, where Edit opens it for changes and Run starts it.\n";
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
