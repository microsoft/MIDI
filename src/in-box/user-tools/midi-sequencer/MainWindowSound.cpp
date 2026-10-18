// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The sound a track asks its device for before it plays: a program change, with a bank when it
// has one, kept in the track's start-up messages. Track setup shows it, and the Sound dialog
// chooses it from the General MIDI names or from the device's own list. The device is asked for
// its list over MIDI-CI only when you select Ask, and never on a group Studio setup turned off
// (design section 26).

#include "pch.h"
#include "MainWindow.xaml.h"

#include <MidiDefs.h>

#include "BackgroundWork.h"
#include "DialogParts.h"
#include "GeneralMidi.h"
#include "LiveRegion.h"
#include "SequenceSerializer.h"
#include "StringResources.h"
#include "TrackSound.h"

namespace res = ::midisequencer::resources;
namespace midi2ci = ::winrt::Windows::Devices::Midi2::CapabilityInquiry;

namespace winrt::midisequencer::implementation
{
    using namespace dialogparts;

    // One sound in the dialog's list.
    struct SoundChoice
    {
        std::wstring Name{};

        // The General MIDI family or the device's category, for finding it.
        std::wstring Group{};

        seq::TrackSound Sound{};
    };

    // What the Sound dialog shows. The coroutine showing the dialog owns it; the controls'
    // handlers hold it weakly.
    struct SoundDialogState
    {
        std::wstring TrackId{};
        seq::TrackDestination Destination{};
        std::wstring DeviceName{};

        // The live endpoint, or empty when the device isn't here.
        std::wstring DeviceId{};

        // Which device list applies: the device, its group and the channel.
        std::wstring ListKey{};
        uint8_t Channel{ 0 };
        bool Drums{ false };
        bool TurnedOff{ false };

        seq::TrackSound Original{};
        std::wstring OriginalName{};
        seq::TrackSound Current{};
        std::wstring CurrentName{};
        bool Tried{ false };
        bool Asking{ false };
        bool Updating{ false };

        std::vector<SoundChoice> Choices{};
        bool FromDevice{ false };

        controls::ToggleSwitch Enabled{ nullptr };
        controls::StackPanel Body{ nullptr };
        controls::TextBox Find{ nullptr };
        controls::ListView List{ nullptr };
        controls::TextBlock Names{ nullptr };
        controls::NumberBox Program{ nullptr };
        controls::CheckBox UseBank{ nullptr };
        controls::NumberBox BankMsb{ nullptr };
        controls::NumberBox BankLsb{ nullptr };
        controls::Button Ask{ nullptr };
        controls::TextBlock Status{ nullptr };
    };

    namespace
    {
        constexpr double SoundDialogWidth = 540.0;
        constexpr double SoundListHeight = 260.0;

        std::wstring Lowered(std::wstring text)
        {
            if (!text.empty())
            {
                ::CharLowerBuffW(text.data(), static_cast<DWORD>(text.size()));
            }

            return text;
        }

        std::wstring Lowercase(std::wstring_view text)
        {
            return Lowered(std::wstring{ text });
        }

        std::wstring BankText(seq::TrackSound const& sound)
        {
            return sound.HasBank
                ? std::wstring{ res::FormatString(L"SoundBankFormat", static_cast<uint32_t>(sound.BankMsb), static_cast<uint32_t>(sound.BankLsb)) }
                : std::wstring{};
        }

        // General MIDI's name for a program, with its number as people count it: "81 Lead 1 (square)".
        // A drum channel picks kits, and General MIDI names only some of them.
        std::wstring GeneralMidiText(uint8_t const program, bool const drums)
        {
            auto const name = drums ? midiapp::GeneralMidiDrumKitName(program) : midiapp::GeneralMidiProgramName(program);

            if (name.empty())
            {
                return std::wstring{ res::FormatString(L"SoundProgramFormat", static_cast<uint32_t>(program) + 1) };
            }

            return std::wstring{ res::FormatString(L"SoundGeneralMidiFormat", static_cast<uint32_t>(program) + 1, name) };
        }

        std::vector<SoundChoice> GeneralMidiChoices(bool const drums)
        {
            std::vector<SoundChoice> choices{};

            for (uint32_t program = 0; program < 128; ++program)
            {
                // A drum channel lists only the kits General MIDI names.
                if (drums && midiapp::GeneralMidiDrumKitName(static_cast<uint8_t>(program)).empty())
                {
                    continue;
                }

                SoundChoice choice{};
                choice.Name = GeneralMidiText(static_cast<uint8_t>(program), drums);
                choice.Group = drums ? std::wstring{} : midiapp::GeneralMidiFamilyName(static_cast<uint8_t>(program));
                choice.Sound.Set = true;
                choice.Sound.Program = static_cast<uint8_t>(program);
                choices.push_back(std::move(choice));
            }

            return choices;
        }

        std::vector<SoundChoice> DeviceChoices(std::vector<midiapp::ProgramListEntry> const& entries)
        {
            std::vector<SoundChoice> choices{};

            for (auto const& entry : entries)
            {
                SoundChoice choice{};
                choice.Name = entry.Title.substr(0, seq::MaximumSoundNameLength);

                // A program and its bank variation often share a title; the tags tell them apart.
                if (!entry.Tags.empty())
                {
                    choice.Name += L" (" + entry.Tags + L")";
                }

                for (auto const& category : entry.Categories)
                {
                    choice.Group += category + L" ";
                }

                choice.Group += entry.CollectionTitle;
                choice.Sound.Set = true;
                choice.Sound.Program = static_cast<uint8_t>(entry.ProgramChange & 0x7F);
                choice.Sound.HasBank = true;
                choice.Sound.BankMsb = static_cast<uint8_t>(entry.BankMsb & 0x7F);
                choice.Sound.BankLsb = static_cast<uint8_t>(entry.BankLsb & 0x7F);
                choices.push_back(std::move(choice));
            }

            return choices;
        }

        // A sound picked by its numbers takes the name of the choice with the same numbers.
        std::wstring NameFor(std::vector<SoundChoice> const& choices, seq::TrackSound const& sound, bool const fromDevice)
        {
            for (auto const& choice : choices)
            {
                auto const sameBank = choice.Sound.HasBank == sound.HasBank &&
                    (!sound.HasBank || (choice.Sound.BankMsb == sound.BankMsb && choice.Sound.BankLsb == sound.BankLsb));

                // General MIDI names are for the program alone, whatever bank goes with it.
                if (choice.Sound.Program == sound.Program && (!fromDevice || sameBank))
                {
                    return choice.Name;
                }
            }

            return {};
        }

        std::wstring PanText(uint32_t const value)
        {
            if (value == 64)
            {
                return std::wstring{ res::GetString(L"MixerPanCenter") };
            }

            return std::wstring{ res::FormatString(value < 64 ? L"MixerPanLeftFormat" : L"MixerPanRightFormat", value < 64 ? 64 - value : value - 64) };
        }

        // A start-up message in words, such as "Volume 100" or "Pan L24". Anything else shows as
        // its words in hex.
        std::wstring DescribeStartupMessage(seq::ClipEvent const& message)
        {
            auto const type = message.WordCount > 0 ? message.Words[0] >> 28 : 0;
            auto const word0 = message.Words[0];
            auto const status = (word0 >> 20) & 0x0F;
            auto const midi2 = type == 0x4 && message.WordCount >= 2;

            if (type == 0x2 || midi2)
            {
                if (status == 0xB)
                {
                    auto const controller = (word0 >> 8) & 0x7F;
                    auto const value = midi2 ? (message.Words[1] >> 25) : (word0 & 0x7F);

                    wchar_t const* key{ nullptr };

                    switch (controller)
                    {
                    case 1: key = L"ControllerModulation"; break;
                    case 7: key = L"MixerVolume"; break;
                    case 10: return std::wstring{ res::FormatString(L"StartupControllerFormat", res::GetString(L"MixerPan"), PanText(value)) };
                    case 11: key = L"MixerExpression"; break;
                    case 64: key = L"ControllerSustain"; break;
                    case 91: key = L"MixerReverb"; break;
                    case 93: key = L"MixerChorus"; break;
                    default: break;
                    }

                    if (key != nullptr)
                    {
                        return std::wstring{ res::FormatString(L"StartupControllerFormat", res::GetString(key), value) };
                    }

                    return std::wstring{ res::FormatString(L"StartupControllerNumberFormat", controller, value) };
                }

                if (status == 0xD)
                {
                    auto const value = midi2 ? (message.Words[1] >> 25) : ((word0 >> 8) & 0x7F);
                    return std::wstring{ res::FormatString(L"StartupPressureFormat", value) };
                }

                if (status == 0xE)
                {
                    // From the middle, which is where a bend rests.
                    auto const bend = midi2 ? static_cast<int64_t>(message.Words[1] >> 18) : static_cast<int64_t>(((word0 & 0x7F) << 7) | ((word0 >> 8) & 0x7F));
                    return std::wstring{ res::FormatString(L"StartupPitchBendFormat", bend - 8192) };
                }
            }

            if (type == 0x3)
            {
                return std::wstring{ res::GetString(L"StartupSystemExclusive") };
            }

            return seq::UmpToText(message);
        }
    }

    // ---------------------------------------------------------------- Track setup

    _Use_decl_annotations_
    bool MainWindow::IsGeneralMidiDevice(seq::EndpointRef const& endpoint) const
    {
        auto const synth = m_directory->SynthEndpointId();
        auto const id = m_directory->ResolveId(endpoint);
        return !synth.empty() && !id.empty() && midiapp::EndpointIdsMatch(winrt::hstring{ synth }, winrt::hstring{ id });
    }

    _Use_decl_annotations_
    std::wstring MainWindow::SoundDisplayName(seq::Track const& track) const
    {
        auto const sound = seq::StartupSound(track.Startup);

        if (!sound.Set)
        {
            return {};
        }

        if (!track.SoundName.empty())
        {
            return track.SoundName;
        }

        // The built-in synth is General MIDI, so its names are known. Another device's aren't.
        auto const drums = track.Destination.Channel == static_cast<int8_t>(midiapp::PercussionChannelIndex);

        if (IsGeneralMidiDevice(track.Destination.Endpoint))
        {
            return GeneralMidiText(sound.Program, drums);
        }

        return std::wstring{ res::FormatString(L"SoundProgramFormat", static_cast<uint32_t>(sound.Program) + 1) };
    }

    void MainWindow::UpdateSoundSection() noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, m_inspectorTrackId);

            if (track == nullptr || track->IsFolder)
            {
                return;
            }

            auto const sound = seq::StartupSound(track->Startup);

            if (sound.Set)
            {
                SoundButtonTitle().Text(winrt::hstring{ SoundDisplayName(*track) });

                auto const bank = BankText(sound);
                SoundButtonDetail().Text(bank.empty() ? res::GetString(L"SoundChosenDetail") : res::FormatString(L"SoundChosenWithBankFormat", bank));
            }
            else
            {
                SoundButtonTitle().Text(res::GetString(L"SoundNotChosen"));
                SoundButtonDetail().Text(res::GetString(L"SoundNotChosenDetail"));
            }

            SoundButton().IsEnabled(!track->Destination.Endpoint.IsEmpty());

            // The rest of the start-up messages, in words. The sound is on the button above.
            StartupList().Children().Clear();

            for (auto const& message : track->Startup)
            {
                if (seq::IsSoundMessage(message))
                {
                    continue;
                }

                controls::TextBlock line{};
                line.Text(winrt::hstring{ DescribeStartupMessage(message) });
                line.FontSize(12);
                line.TextWrapping(xaml::TextWrapping::Wrap);
                StartupList().Children().Append(line);
            }

            if (StartupList().Children().Size() == 0)
            {
                controls::TextBlock none{};
                none.Text(res::GetString(L"StartupNone"));
                none.FontSize(12);
                none.TextWrapping(xaml::TextWrapping::Wrap);
                none.Foreground(BrushFor(m_palette.Text3));
                StartupList().Children().Append(none);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the track's sound.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSoundClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ShowSoundDialog();
    }

    // ---------------------------------------------------------------- the dialog

    _Use_decl_annotations_
    void MainWindow::FillSoundList(SoundDialogState& state)
    {
        auto const items = winrt::single_threaded_observable_vector<foundation::IInspectable>();
        auto const filter = Lowercase(std::wstring{ state.Find.Text() });
        int32_t selected{ -1 };

        for (size_t i = 0; i < state.Choices.size(); ++i)
        {
            auto const& choice = state.Choices[i];

            if (!filter.empty() && Lowered(choice.Name).find(filter) == std::wstring::npos && Lowered(choice.Group).find(filter) == std::wstring::npos)
            {
                continue;
            }

            if (selected < 0 && state.Current.Set && choice.Name == state.CurrentName)
            {
                selected = static_cast<int32_t>(items.Size());
            }

            items.Append(winrt::make<appshared::implementation::NamedChoice>(winrt::hstring{ choice.Name }, static_cast<int32_t>(i)));
        }

        state.Updating = true;
        state.List.ItemsSource(items);
        state.List.SelectedIndex(selected);
        state.Updating = false;

        if (selected >= 0)
        {
            state.List.ScrollIntoView(state.List.SelectedItem());
        }

        auto const names = state.FromDevice ? res::FormatString(L"SoundNamesDeviceFormat", state.DeviceName)
            : res::GetString(state.Drums ? L"SoundNamesDrums" : L"SoundNamesGeneralMidi");
        state.Names.Text(names);
    }

    _Use_decl_annotations_
    void MainWindow::ShowSoundNumbers(SoundDialogState& state)
    {
        state.Updating = true;
        state.Program.Value(static_cast<double>(state.Current.Program) + 1);
        state.UseBank.IsChecked(state.Current.HasBank);
        state.BankMsb.Value(state.Current.BankMsb);
        state.BankLsb.Value(state.Current.BankLsb);
        state.BankMsb.IsEnabled(state.Current.HasBank);
        state.BankLsb.IsEnabled(state.Current.HasBank);
        state.Updating = false;
    }

    _Use_decl_annotations_
    void MainWindow::TrySound(SoundDialogState& state)
    {
        // Sent now, so it can be heard from a keyboard or MIDI Keyboard. Never to a group Studio
        // setup turned off; the output wouldn't send it anyway.
        if (!state.Current.Set || state.TurnedOff || state.DeviceId.empty())
        {
            return;
        }

        auto const message = seq::SoundMessage(state.Current, state.Channel);
        SendNow(state.Destination, message.Words.data(), message.WordCount);
        state.Tried = true;
    }

    void MainWindow::ShowSoundDialog()
    {
        [](winrt::com_ptr<MainWindow> strong) -> winrt::fire_and_forget
        {
            try
            {
                auto const track = seq::FindTrack(strong->m_doc, strong->m_inspectorTrackId);

                if (track == nullptr || track->IsFolder || track->Destination.Endpoint.IsEmpty())
                {
                    co_return;
                }

                auto const state = std::make_shared<SoundDialogState>();
                state->TrackId = track->Id;
                state->Destination = track->Destination;
                state->Channel = static_cast<uint8_t>(std::max<int8_t>(0, track->Destination.Channel));
                state->Drums = state->Channel == midiapp::PercussionChannelIndex;

                auto const summary = strong->m_directory->Resolve(track->Destination.Endpoint);
                state->DeviceName = summary.has_value() ? summary->Live.Name : track->Destination.Endpoint.Name;
                state->DeviceId = summary.has_value() ? summary->Live.EndpointDeviceId : std::wstring{};
                state->TurnedOff = strong->DestinationTurnedOff(track->Destination);
                state->ListKey = strong->EndpointKeyOf(track->Destination.Endpoint) + L"#" + std::to_wstring(track->Destination.Group) + L"#" + std::to_wstring(state->Channel);

                state->Original = seq::StartupSound(track->Startup);
                state->OriginalName = strong->SoundDisplayName(*track);
                state->Current = state->Original;
                state->CurrentName = state->OriginalName;

                if (auto const known = strong->m_deviceSounds.find(state->ListKey); known != strong->m_deviceSounds.end())
                {
                    state->Choices = DeviceChoices(known->second);
                    state->FromDevice = true;
                }
                else
                {
                    state->Choices = GeneralMidiChoices(state->Drums);
                }

                auto const secondary = strong->BrushFor(strong->m_palette.Text2);
                auto const tertiary = strong->BrushFor(strong->m_palette.Text3);
                auto const namedTemplate = strong->RootGrid().Resources().Lookup(winrt::box_value(L"NamedChoiceTemplate")).as<xaml::DataTemplate>();
                std::weak_ptr<SoundDialogState> const weakState{ state };
                auto const weak = strong->get_weak();

                controls::StackPanel panel{};
                panel.Spacing(10);
                panel.Width(SoundDialogWidth);

                state->Enabled = controls::ToggleSwitch{};
                state->Enabled.Header(winrt::box_value(res::GetString(L"SoundEnabled")));
                state->Enabled.IsOn(state->Original.Set);
                panel.Children().Append(state->Enabled);
                panel.Children().Append(Note(res::GetString(L"SoundEnabledHelp"), secondary));

                state->Body = controls::StackPanel{};
                state->Body.Spacing(8);
                panel.Children().Append(state->Body);

                state->Names = Note(L"", tertiary);
                state->Body.Children().Append(state->Names);

                state->Find = controls::TextBox{};
                state->Find.PlaceholderText(res::GetString(L"SoundFindPlaceholder"));
                automation::AutomationProperties::SetName(state->Find, res::GetString(L"SoundFindName"));
                state->Body.Children().Append(state->Find);

                state->List = controls::ListView{};
                state->List.Height(SoundListHeight);
                state->List.SelectionMode(controls::ListViewSelectionMode::Single);
                state->List.ItemTemplate(namedTemplate);
                state->List.BorderThickness(xaml::Thickness{ 1, 1, 1, 1 });
                state->List.BorderBrush(strong->BrushFor(strong->m_palette.Divider));
                state->List.CornerRadius(xaml::CornerRadius{ 4, 4, 4, 4 });
                automation::AutomationProperties::SetName(state->List, res::GetString(L"SoundListName"));
                state->Body.Children().Append(state->List);

                // ---- the numbers, for a sound that isn't in the list ----

                controls::Grid numbers{};
                numbers.ColumnSpacing(10);

                for (auto const width : { xaml::GridLength{ 110, xaml::GridUnitType::Pixel }, xaml::GridLength{ 1, xaml::GridUnitType::Auto },
                    xaml::GridLength{ 96, xaml::GridUnitType::Pixel }, xaml::GridLength{ 96, xaml::GridUnitType::Pixel } })
                {
                    controls::ColumnDefinition column{};
                    column.Width(width);
                    numbers.ColumnDefinitions().Append(column);
                }

                auto const numberBox = [](wchar_t const* header, wchar_t const* name, double minimum, double maximum)
                {
                    controls::NumberBox box{};
                    box.Header(winrt::box_value(res::GetString(header)));
                    box.Minimum(minimum);
                    box.Maximum(maximum);
                    box.SmallChange(1);
                    box.LargeChange(8);
                    box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
                    box.ValidationMode(controls::NumberBoxValidationMode::InvalidInputOverwritten);
                    automation::AutomationProperties::SetName(box, res::GetString(name));
                    return box;
                };

                state->Program = numberBox(L"SoundProgramLabel", L"SoundProgramName", 1, 128);
                PlaceIn(numbers, state->Program, 0);

                state->UseBank = controls::CheckBox{};
                state->UseBank.Content(winrt::box_value(res::GetString(L"SoundBankLabel")));
                state->UseBank.VerticalAlignment(xaml::VerticalAlignment::Bottom);
                state->UseBank.Margin(xaml::Thickness{ 4, 0, 0, 2 });
                state->UseBank.MinWidth(0);
                PlaceIn(numbers, state->UseBank, 1);

                state->BankMsb = numberBox(L"SoundBankMsbLabel", L"SoundBankMsbName", 0, 127);
                PlaceIn(numbers, state->BankMsb, 2);

                state->BankLsb = numberBox(L"SoundBankLsbLabel", L"SoundBankLsbName", 0, 127);
                PlaceIn(numbers, state->BankLsb, 3);

                state->Body.Children().Append(numbers);
                state->Body.Children().Append(Note(res::GetString(L"SoundTryNote"), tertiary));

                // ---- the device's own list, only when asked for ----

                state->Ask = controls::Button{};
                state->Ask.Style(strong->RootGrid().Resources().Lookup(winrt::box_value(L"SmallButtonStyle")).as<xaml::Style>());
                state->Ask.Content(winrt::box_value(res::FormatString(L"SoundAskFormat", state->DeviceName)));
                state->Ask.Margin(xaml::Thickness{ 0, 6, 0, 0 });
                controls::ToolTipService::SetToolTip(state->Ask, winrt::box_value(res::FormatString(L"SoundAskHelpFormat", state->DeviceName)));
                state->Body.Children().Append(state->Ask);

                state->Status = Note(res::FormatString(L"SoundAskHelpFormat", state->DeviceName), tertiary);

                // A screen reader hears how the asking went, since focus stays on the button.
                midiapp::MakeLiveStatusRegion(state->Status);
                state->Body.Children().Append(state->Status);

                if (state->TurnedOff)
                {
                    state->Ask.IsEnabled(false);
                    state->Status.Text(res::GetString(L"SoundAskTurnedOff"));
                }
                else if (state->DeviceId.empty())
                {
                    state->Ask.IsEnabled(false);
                    state->Status.Text(res::FormatString(L"SoundAskNotConnectedFormat", state->DeviceName));
                }

                // ---- what each control does ----

                auto const enable = [weakState]()
                {
                    if (auto const current = weakState.lock())
                    {
                        auto const on = current->Enabled.IsOn();
                        current->Body.Opacity(on ? 1.0 : 0.5);
                        current->Find.IsEnabled(on);
                        current->List.IsEnabled(on);
                        current->Program.IsEnabled(on);
                        current->UseBank.IsEnabled(on);
                        current->BankMsb.IsEnabled(on && current->Current.HasBank);
                        current->BankLsb.IsEnabled(on && current->Current.HasBank);
                    }
                };

                state->Enabled.Toggled([weakState, enable](auto&&, auto&&)
                {
                    // Turning it on picks nothing yet, so nothing is sent until a sound is chosen.
                    if (auto const current = weakState.lock())
                    {
                        current->Current.Set = current->Enabled.IsOn();
                        enable();
                    }
                });

                state->Find.TextChanged([weak, weakState](auto&&, auto&&)
                {
                    auto const window = weak.get();
                    auto const current = weakState.lock();

                    if (window && current)
                    {
                        window->FillSoundList(*current);
                    }
                });

                state->List.SelectionChanged([weak, weakState](auto&&, auto&&)
                {
                    auto const window = weak.get();
                    auto const current = weakState.lock();

                    if (!window || !current || current->Updating)
                    {
                        return;
                    }

                    auto const item = current->List.SelectedItem().try_as<appshared::NamedChoice>();

                    if (item == nullptr || item.Value() < 0 || static_cast<size_t>(item.Value()) >= current->Choices.size())
                    {
                        return;
                    }

                    auto const& choice = current->Choices[static_cast<size_t>(item.Value())];
                    current->Current = choice.Sound;
                    current->CurrentName = choice.Name;
                    window->ShowSoundNumbers(*current);
                    window->TrySound(*current);
                });

                // Numbers typed in pick the sound with those numbers, named when the list has it.
                auto const numbersChanged = [weak, weakState]()
                {
                    auto const window = weak.get();
                    auto const current = weakState.lock();

                    if (!window || !current || current->Updating)
                    {
                        return;
                    }

                    auto const read = [](controls::NumberBox const& box, double fallback)
                    {
                        auto const value = box.Value();
                        return static_cast<uint8_t>(std::clamp(std::isnan(value) ? fallback : value, 0.0, 127.0));
                    };

                    current->Current.Set = true;
                    current->Current.Program = static_cast<uint8_t>(std::clamp(std::isnan(current->Program.Value()) ? 1.0 : current->Program.Value(), 1.0, 128.0) - 1);
                    current->Current.HasBank = IsChecked(current->UseBank);
                    current->Current.BankMsb = read(current->BankMsb, 0);
                    current->Current.BankLsb = read(current->BankLsb, 0);
                    current->CurrentName = NameFor(current->Choices, current->Current, current->FromDevice);

                    current->BankMsb.IsEnabled(current->Current.HasBank);
                    current->BankLsb.IsEnabled(current->Current.HasBank);

                    window->FillSoundList(*current);
                    window->TrySound(*current);
                };

                for (auto const& box : { state->Program, state->BankMsb, state->BankLsb })
                {
                    box.ValueChanged([numbersChanged](auto&&, auto&&) { numbersChanged(); });
                }

                state->UseBank.Checked([numbersChanged](auto&&, auto&&) { numbersChanged(); });
                state->UseBank.Unchecked([numbersChanged](auto&&, auto&&) { numbersChanged(); });

                state->Ask.Click([weak, weakState](auto&&, auto&&)
                {
                    auto const window = weak.get();
                    auto const current = weakState.lock();

                    if (window && current)
                    {
                        window->AskDeviceForSounds(current);
                    }
                });

                strong->FillSoundList(*state);
                strong->ShowSoundNumbers(*state);
                enable();

                controls::ScrollViewer scroller{};
                scroller.Content(panel);
                scroller.Padding(xaml::Thickness{ 0, 0, 14, 0 });
                scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Auto);

                controls::ContentDialog dialog{};
                dialog.XamlRoot(strong->Content().XamlRoot());
                dialog.Title(winrt::box_value(res::FormatString(L"SoundDialogTitleFormat", track->Name)));
                dialog.Content(scroller);

                // A dialog stops at 548 wide unless told otherwise, which would cut off the right edge.
                dialog.Resources().Insert(winrt::box_value(L"ContentDialogMaxWidth"), winrt::box_value(SoundDialogWidth + 80.0));
                dialog.PrimaryButtonText(res::GetString(L"DialogDone"));
                dialog.CloseButtonText(res::GetString(L"DialogCancel"));
                dialog.DefaultButton(controls::ContentDialogButton::Primary);
                dialog.RequestedTheme(strong->RootGrid().ActualTheme());

                strong->m_soundDialog = state;

                auto const result = co_await dialog.ShowAsync();

                strong->m_soundDialog.reset();

                if (strong->m_closing)
                {
                    co_return;
                }

                auto const chosen = state->Enabled.IsOn() ? state->Current : seq::TrackSound{};

                if (result == controls::ContentDialogResult::Primary)
                {
                    if (!(chosen == state->Original) || (chosen.Set && state->CurrentName != state->OriginalName))
                    {
                        auto const id = state->TrackId;
                        auto const channel = state->Channel;
                        auto const name = chosen.Set ? state->CurrentName : std::wstring{};

                        strong->EditTracks(std::wstring{ res::GetString(L"UndoSound") }, [&](seq::Sequence& doc)
                        {
                            if (auto t = seq::FindTrack(doc, id); t != nullptr)
                            {
                                seq::SetStartupSound(t->Startup, chosen, channel);

                                // The name it was picked by, from the device's list or General
                                // MIDI's. A number typed in that's in neither has none.
                                t->SoundName = name.substr(0, seq::MaximumSoundNameLength);
                            }
                        });
                    }
                }
                else if (state->Tried && state->Original.Set)
                {
                    // Cancel puts the device back on the sound it had.
                    auto const message = seq::SoundMessage(state->Original, state->Channel);
                    strong->SendNow(state->Destination, message.Words.data(), message.WordCount);
                }
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the sound dialog.")
        }(get_strong());
    }

    // ---------------------------------------------------------------- asking the device

    _Use_decl_annotations_
    midi2ci::MidiCapabilityInquirySession MainWindow::CiSessionFor(midi2::MidiSession const& midiSession, std::wstring const& endpointId, uint8_t group)
    {
        // Off the UI thread: opening a connection waits for the service.
        std::scoped_lock guard{ m_ciLock };

        auto const key = Lowercase(endpointId);
        auto found = m_ciLinks.find(key);

        if (found == m_ciLinks.end())
        {
            if (midiSession == nullptr)
            {
                return nullptr;
            }

            // A connection of its own, so the questions and answers stay apart from what plays.
            auto connection = midiSession.CreateEndpointConnection(winrt::hstring{ endpointId });

            if (connection == nullptr || !connection.Open())
            {
                return nullptr;
            }

            CiLink link{};
            link.Connection = connection;
            link.Presence = std::make_unique<midiapp::MidiCiPresence>(MIDI_DEVICE_FAMILY_MODEL_NUMBER_MIDI_SEQUENCER);

            // Nothing asks again by itself when a device turns up: the sequencer only asks when
            // the person selects Ask.
            link.Presence->Open(connection, group, nullptr);
            found = m_ciLinks.emplace(key, std::move(link)).first;
        }
        else
        {
            found->second.Presence->Group(group);
        }

        return found->second.Presence->Session();
    }

    void MainWindow::CloseCiLinks() noexcept
    {
        try
        {
            if (m_soundQuery != nullptr)
            {
                m_soundQuery->Cancel();
                m_soundQuery = nullptr;
            }

            std::map<std::wstring, CiLink> links{};

            {
                std::scoped_lock guard{ m_ciLock };
                links.swap(m_ciLinks);
            }

            for (auto& [key, link] : links)
            {
                if (link.Presence != nullptr)
                {
                    link.Presence->Close();
                }

                if (link.Connection != nullptr && m_session != nullptr)
                {
                    m_session.DisconnectEndpointConnection(link.Connection.ConnectionId());
                }
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to close the MIDI-CI connections.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::AskDeviceForSounds(std::shared_ptr<SoundDialogState> state)
    {
        auto strong = get_strong();

        try
        {
            if (state == nullptr || state->Asking || state->TurnedOff || state->DeviceId.empty())
            {
                co_return;
            }

            state->Asking = true;
            state->Ask.IsEnabled(false);
            state->Status.Text(res::FormatString(L"SoundAskingFormat", state->DeviceName));

            // The answer goes under the Ask button, which can be at the bottom of what shows.
            state->Status.StartBringIntoView();

            if (m_soundQuery != nullptr)
            {
                m_soundQuery->Cancel();
                m_soundQuery = nullptr;
            }

            auto const id = state->DeviceId;
            auto const group = state->Destination.Group;
            auto const midiSession = m_session;
            midi2ci::MidiCapabilityInquirySession session{ nullptr };

            co_await ::midisequencer::RunOnBackgroundAsync([&]() { session = CiSessionFor(midiSession, id, group); });

            if (m_closing)
            {
                co_return;
            }

            if (session == nullptr)
            {
                OnDeviceSounds(state->ListKey, midiapp::ProgramListResult::NoResponse, {});
                co_return;
            }

            auto const queue = DispatcherQueue();
            auto const weak = get_weak();
            auto const key = state->ListKey;

            // The answer arrives on a background thread and comes back to this one.
            m_soundQuery = midiapp::MidiCiProgramListQuery::Start(session, state->Channel,
                [weak, queue, key](midiapp::ProgramListResult result, std::vector<midiapp::ProgramListEntry> entries)
                {
                    if (queue == nullptr)
                    {
                        return;
                    }

                    queue.TryEnqueue([weak, key, result, entries = std::move(entries)]() mutable
                    {
                        if (auto window = weak.get())
                        {
                            window->OnDeviceSounds(key, result, std::move(entries));
                        }
                    });
                },
                nullptr);

            if (m_soundQuery == nullptr)
            {
                OnDeviceSounds(key, midiapp::ProgramListResult::NoResponse, {});
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to ask a device for its sounds.")
    }

    _Use_decl_annotations_
    void MainWindow::OnDeviceSounds(std::wstring const& key, midiapp::ProgramListResult result, std::vector<midiapp::ProgramListEntry> entries) noexcept
    {
        try
        {
            m_soundQuery = nullptr;

            if (result == midiapp::ProgramListResult::Success && !entries.empty())
            {
                m_deviceSounds[key] = entries;
            }

            auto const state = m_soundDialog.lock();

            if (state == nullptr || state->ListKey != key)
            {
                return;
            }

            state->Asking = false;
            state->Ask.IsEnabled(true);

            switch (result)
            {
            case midiapp::ProgramListResult::Success:
                if (!entries.empty())
                {
                    state->Choices = DeviceChoices(entries);
                    state->FromDevice = true;
                    state->CurrentName = NameFor(state->Choices, state->Current, true);
                    FillSoundList(*state);
                    state->Status.Text(res::FormatString(L"SoundFoundFormat", static_cast<uint32_t>(entries.size()), state->DeviceName));
                    break;
                }

                [[fallthrough]];

            case midiapp::ProgramListResult::Empty:
                state->Status.Text(res::FormatString(L"SoundEmptyFormat", state->DeviceName));
                break;

            case midiapp::ProgramListResult::NotSupported:
                state->Status.Text(res::FormatString(L"SoundNotSupportedFormat", state->DeviceName));
                break;

            default:
                state->Status.Text(res::FormatString(L"SoundNoResponseFormat", state->DeviceName));
                break;
            }

            state->Status.StartBringIntoView();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show a device's sounds.")
    }
}
