// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "SequenceSerializer.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr double InspectorWidth = 300.0;

        // The loopback pair Windows MIDI Services sets up. These are endpoint names, matched the
        // way every other saved endpoint is, not text shown by this app.
        constexpr wchar_t const* DefaultLoopbackA = L"Default App Loopback (A)";
        constexpr wchar_t const* DefaultLoopbackB = L"Default App Loopback (B)";

        struct RecordKind
        {
            uint8_t Bit{ 0 };
            wchar_t const* Key{ nullptr };
        };

        // System exclusive has its own flag rather than a bit in Record.
        constexpr uint8_t SystemExclusiveChip = 0;

        constexpr std::array<RecordKind, 6> RecordKindChoices{ {
            { seq::RecordNotes, L"RecordKindNotes" },
            { seq::RecordControllers, L"RecordKindControllers" },
            { seq::RecordPitchBend, L"RecordKindPitchBend" },
            { seq::RecordPressure, L"RecordKindPressure" },
            { seq::RecordProgram, L"RecordKindProgram" },
            { SystemExclusiveChip, L"RecordKindSystemExclusive" } } };

        int32_t FirstChannel(uint16_t channels) noexcept
        {
            if (channels == seq::AllChannels || channels == 0)
            {
                return -1;
            }

            for (int32_t channel = 0; channel < 16; ++channel)
            {
                if ((channels & (1u << channel)) != 0)
                {
                    return channel;
                }
            }

            return -1;
        }
    }

    _Use_decl_annotations_
    void MainWindow::ShowInspector(std::wstring const& trackId) noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr || track->IsFolder)
            {
                return;
            }

            m_inspectorTrackId = trackId;
            InspectorColumn().Width(xaml::GridLength{ InspectorWidth, xaml::GridUnitType::Pixel });
            TrackInspector().Visibility(xaml::Visibility::Visible);

            RefreshInspector();
            OnArrangeSizeChanged();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the track setup.")
    }

    void MainWindow::HideInspector() noexcept
    {
        try
        {
            m_inspectorTrackId.clear();
            TrackInspector().Visibility(xaml::Visibility::Collapsed);
            InspectorColumn().Width(xaml::GridLength{ 0, xaml::GridUnitType::Pixel });
            OnArrangeSizeChanged();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to hide the track setup.")
    }

    void MainWindow::RefreshInspector() noexcept
    {
        if (m_inspectorTrackId.empty())
        {
            return;
        }

        try
        {
            auto const track = seq::FindTrack(m_doc, m_inspectorTrackId);

            if (track == nullptr || track->IsFolder)
            {
                HideInspector();
                return;
            }

            m_inspectorUpdating = true;

            InspectorTitle().Text(winrt::hstring{ track->Name });

            // Don't fight the person typing.
            if (input::FocusManager::GetFocusedElement(Content().XamlRoot()) != TrackNameBox())
            {
                TrackNameBox().Text(winrt::hstring{ track->Name });
            }

            TrackPinToggle().IsChecked(track->Pinned);

            // Swatches
            TrackSwatches().Children().Clear();

            auto const& swatches = seq::TrackColorSwatches();

            for (size_t i = 0; i < swatches.size(); ++i)
            {
                auto const swatch = swatches[i];
                auto const selected = swatch == track->Color;

                controls::Button button{};
                button.Width(20);
                button.Height(20);
                button.MinWidth(0);
                button.MinHeight(0);
                button.Padding(xaml::Thickness{ 0, 0, 0, 0 });
                button.CornerRadius(xaml::CornerRadius{ 10, 10, 10, 10 });
                button.Background(BrushFor(seq::FromRgb(swatch)));
                button.BorderThickness(xaml::Thickness{ selected ? 2.0 : 1.0, selected ? 2.0 : 1.0, selected ? 2.0 : 1.0, selected ? 2.0 : 1.0 });
                button.BorderBrush(BrushFor(selected ? m_palette.Text : m_palette.StrokeStrong));

                auto const name = res::GetString(std::wstring{ L"SwatchName" } + std::to_wstring(i));
                automation::AutomationProperties::SetName(button, name);
                controls::ToolTipService::SetToolTip(button, winrt::box_value(name));

                button.Click([weak = get_weak(), swatch](auto&&, auto&&)
                {
                    auto strong = weak.get();

                    if (!strong)
                    {
                        return;
                    }

                    auto const id = strong->m_inspectorTrackId;

                    strong->EditTracks(std::wstring{ res::GetString(L"UndoColor") }, [&](seq::Sequence& doc)
                    {
                        if (auto t = seq::FindTrack(doc, id); t != nullptr)
                        {
                            t->Color = swatch;
                        }
                    });
                });

                TrackSwatches().Children().Append(button);
            }

            // Recording
            FillEndpointCombo(SourceEndpointCombo(), true, track->Source.Endpoint);
            FillGroupCombo(SourceGroupCombo(), true, track->Source.Endpoint, track->Source.Group, true);
            FillChannelCombo(SourceChannelCombo(), FirstChannel(track->Source.Channels), true);

            // Built once and then only updated, so a chip keeps keyboard focus when it's toggled.
            if (RecordKinds().Children().Size() == 0)
            {
                auto const style = RootGrid().Resources().Lookup(winrt::box_value(L"ChipToggleStyle")).as<xaml::Style>();

                for (auto const& kind : RecordKindChoices)
                {
                    primitives::ToggleButton chip{};
                    chip.Style(style);
                    chip.Content(winrt::box_value(res::GetString(kind.Key)));
                    chip.Tag(winrt::box_value(static_cast<int32_t>(kind.Bit)));

                    auto const changed = [weak = get_weak(), bit = kind.Bit](foundation::IInspectable const& sender, auto&&)
                    {
                        auto strong = weak.get();
                        auto const toggle = sender.try_as<primitives::ToggleButton>();

                        if (!strong || toggle == nullptr || strong->m_inspectorUpdating)
                        {
                            return;
                        }

                        auto const on = toggle.IsChecked() != nullptr && toggle.IsChecked().GetBoolean();
                        auto const id = strong->m_inspectorTrackId;

                        strong->EditTracks(std::wstring{ res::GetString(L"UndoChangeSource") }, [&](seq::Sequence& doc)
                        {
                            if (auto t = seq::FindTrack(doc, id); t != nullptr)
                            {
                                if (bit == SystemExclusiveChip)
                                {
                                    t->Source.SystemExclusive = on;
                                }
                                else
                                {
                                    t->Source.Record = on ? static_cast<uint8_t>(t->Source.Record | bit) : static_cast<uint8_t>(t->Source.Record & ~bit);
                                }
                            }
                        });
                    };

                    chip.Checked(changed);
                    chip.Unchecked(changed);
                    RecordKinds().Children().Append(chip);
                }
            }

            for (auto const& child : RecordKinds().Children())
            {
                if (auto const chip = child.try_as<primitives::ToggleButton>(); chip != nullptr)
                {
                    auto const bit = static_cast<uint8_t>(winrt::unbox_value_or<int32_t>(chip.Tag(), 0));
                    chip.IsChecked(bit == SystemExclusiveChip ? track->Source.SystemExclusive : (track->Source.Record & bit) != 0);
                }
            }

            EchoSwitch().IsOn(track->Source.Echo);

            // Playing
            FillEndpointCombo(DestinationEndpointCombo(), false, track->Destination.Endpoint);
            FillGroupCombo(DestinationGroupCombo(), false, track->Destination.Endpoint, track->Destination.Group, false);
            FillChannelCombo(DestinationChannelCombo(), track->Destination.Channel, true);

            UpdateProtocolNote();

            // Start-up messages, as their words, until there's an editor for them.
            StartupList().Children().Clear();

            if (track->Startup.empty())
            {
                controls::TextBlock none{};
                none.Text(res::GetString(L"StartupNone"));
                none.FontSize(12);
                none.Foreground(BrushFor(m_palette.Text3));
                StartupList().Children().Append(none);
            }
            else
            {
                for (auto const& message : track->Startup)
                {
                    controls::TextBlock line{};
                    line.Text(winrt::hstring{ seq::UmpToText(message) });
                    line.FontSize(12);
                    line.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
                    StartupList().Children().Append(line);
                }
            }

            m_inspectorUpdating = false;
        }
        catch (...)
        {
            m_inspectorUpdating = false;
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    void MainWindow::FillEndpointCombo(controls::ComboBox const& combo, bool sources, seq::EndpointRef const& selected)
    {
        auto const items = winrt::single_threaded_observable_vector<foundation::IInspectable>();

        items.Append(winrt::make<appshared::implementation::EndpointChoice>(res::GetString(sources ? L"NoSourceChoice" : L"NoDestinationChoice"), winrt::hstring{}, winrt::hstring{}));

        int32_t selectedIndex{ 0 };
        auto const selectedId = selected.IsEmpty() ? std::wstring{} : m_directory->ResolveId(selected);
        auto found = selected.IsEmpty();

        for (auto const& summary : m_directory->Snapshot())
        {
            auto const& live = summary.Live;
            auto const count = sources ? live.SourceGroupCount() : live.DestinationGroupCount();

            if (count <= 0)
            {
                continue;
            }

            if (!selectedId.empty() && midiapp::EndpointIdsMatch(winrt::hstring{ selectedId }, winrt::hstring{ live.EndpointDeviceId }))
            {
                selectedIndex = static_cast<int32_t>(items.Size());
                found = true;
            }

            items.Append(winrt::make<appshared::implementation::EndpointChoice>(winrt::hstring{ live.Name }, winrt::hstring{ live.EndpointDeviceId }, winrt::hstring{ live.ImagePath }));
        }

        // A saved device that isn't here still shows, so the choice isn't lost by looking at it.
        if (!found)
        {
            auto const name = selected.Name.empty() ? std::wstring{ res::GetString(L"UnknownEndpoint") } : selected.Name;
            selectedIndex = static_cast<int32_t>(items.Size());
            items.Append(winrt::make<appshared::implementation::EndpointChoice>(
                winrt::hstring{ name + L" " + std::wstring{ res::GetString(L"NotConnectedSuffix") } }, winrt::hstring{ selected.Id }, winrt::hstring{}));
        }

        combo.ItemsSource(items);
        combo.SelectedIndex(selectedIndex);
    }

    _Use_decl_annotations_
    void MainWindow::FillGroupCombo(controls::ComboBox const& combo, bool sources, seq::EndpointRef const& endpoint, int32_t selected, bool allowAny)
    {
        auto const items = winrt::single_threaded_observable_vector<foundation::IInspectable>();
        int32_t selectedIndex{ 0 };

        if (allowAny)
        {
            items.Append(winrt::make<appshared::implementation::NamedChoice>(res::GetString(L"AnyGroup"), -1));
        }

        auto const live = m_directory->Resolve(endpoint);

        for (int32_t group = 0; group < 16; ++group)
        {
            auto present = true;
            std::wstring name{};

            if (live.has_value())
            {
                present = sources ? live->Live.SourceGroups[group] : live->Live.DestinationGroups[group];
                name = live->Live.GroupName(group, sources);
            }

            // Only the groups the device has, and whatever was saved.
            if (!present && group != selected)
            {
                continue;
            }

            if (group == selected)
            {
                selectedIndex = static_cast<int32_t>(items.Size());
            }

            auto text = std::wstring{ res::FormatString(L"GroupChoiceFormat", group + 1) };

            if (!name.empty())
            {
                text += L" \u00B7 " + name;
            }

            items.Append(winrt::make<appshared::implementation::NamedChoice>(winrt::hstring{ text }, group));
        }

        combo.ItemsSource(items);
        combo.SelectedIndex(items.Size() > 0 ? selectedIndex : -1);
    }

    _Use_decl_annotations_
    void MainWindow::FillChannelCombo(controls::ComboBox const& combo, int32_t selected, bool allowAny)
    {
        auto const items = winrt::single_threaded_observable_vector<foundation::IInspectable>();
        int32_t selectedIndex{ 0 };

        // The destination's box shares a row with the group, so it gets the short names.
        auto const format = combo == DestinationChannelCombo() ? L"ChannelShortFormat" : L"ChannelChoiceFormat";

        if (allowAny)
        {
            auto const key = combo == SourceChannelCombo() ? L"AnyChannel" : L"KeepChannel";
            items.Append(winrt::make<appshared::implementation::NamedChoice>(res::GetString(key), -1));
        }

        for (int32_t channel = 0; channel < 16; ++channel)
        {
            if (channel == selected)
            {
                selectedIndex = static_cast<int32_t>(items.Size());
            }

            items.Append(winrt::make<appshared::implementation::NamedChoice>(res::FormatString(format, channel + 1), channel));
        }

        combo.ItemsSource(items);
        combo.SelectedIndex(selectedIndex);
    }

    void MainWindow::UpdateProtocolNote() noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, m_inspectorTrackId);

            if (track == nullptr || track->Destination.Endpoint.IsEmpty())
            {
                ProtocolNote().Text(L"");
                return;
            }

            if (!m_directory->Resolve(track->Destination.Endpoint).has_value())
            {
                ProtocolNote().Text(res::GetString(L"ProtocolNoteMissing"));
                return;
            }

            auto const info = m_directory->Lookup(track->Destination.Endpoint, track->Destination.Group);
            auto midi2 = info.SpeaksMidi2;

            if (track->Destination.Protocol == seq::ProtocolChoice::Midi1)
            {
                midi2 = false;
            }
            else if (track->Destination.Protocol == seq::ProtocolChoice::Midi2)
            {
                midi2 = true;
            }

            std::wstring note{ res::GetString(midi2 ? L"ProtocolNoteMidi2" : L"ProtocolNoteMidi1") };

            if (info.OffsetMicroseconds > 0)
            {
                auto milliseconds = std::format(L"{:.1f}", info.OffsetMicroseconds / 1000.0);

                if (milliseconds.ends_with(L".0"))
                {
                    milliseconds.resize(milliseconds.size() - 2);
                }

                note += L" ";
                note += res::FormatString(L"ProtocolNoteOffsetFormat", milliseconds);
            }

            ProtocolNote().Text(winrt::hstring{ note });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to describe the protocol.")
    }

    // ---------------------------------------------------------------- inspector events

    _Use_decl_annotations_
    void MainWindow::OnInspectorCloseClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        HideInspector();
    }

    _Use_decl_annotations_
    void MainWindow::OnTrackNameCommitted(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_inspectorUpdating || m_inspectorTrackId.empty())
            {
                return;
            }

            std::wstring text{ TrackNameBox().Text() };
            auto const first = text.find_first_not_of(L" \t");
            auto const last = text.find_last_not_of(L" \t");
            text = first == std::wstring::npos ? std::wstring{} : text.substr(first, last - first + 1);

            auto const track = seq::FindTrack(m_doc, m_inspectorTrackId);

            if (track == nullptr || text.empty() || text == track->Name)
            {
                if (track != nullptr)
                {
                    TrackNameBox().Text(winrt::hstring{ track->Name });
                }

                return;
            }

            auto const id = m_inspectorTrackId;

            EditTracks(std::wstring{ res::GetString(L"UndoRenameTrack") }, [&](seq::Sequence& doc)
            {
                if (auto t = seq::FindTrack(doc, id); t != nullptr)
                {
                    t->Name = text.substr(0, 100);
                }
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to rename the track.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTrackNameKeyDown(foundation::IInspectable const& sender, input::KeyRoutedEventArgs const& args)
    {
        if (args.Key() == winrt::Windows::System::VirtualKey::Enter)
        {
            OnTrackNameCommitted(sender, nullptr);
            args.Handled(true);
        }
        else if (args.Key() == winrt::Windows::System::VirtualKey::Escape)
        {
            if (auto const track = seq::FindTrack(m_doc, m_inspectorTrackId); track != nullptr)
            {
                TrackNameBox().Text(winrt::hstring{ track->Name });
            }

            args.Handled(true);
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnTrackPinClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_inspectorUpdating)
        {
            return;
        }

        auto const id = m_inspectorTrackId;
        auto const pinned = TrackPinToggle().IsChecked().GetBoolean();

        EditTracks(std::wstring{ res::GetString(L"UndoPin") }, [&](seq::Sequence& doc)
        {
            if (auto t = seq::FindTrack(doc, id); t != nullptr)
            {
                t->Pinned = pinned;
            }
        });
    }

    _Use_decl_annotations_
    void MainWindow::OnSourceChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_inspectorUpdating || m_inspectorTrackId.empty())
            {
                return;
            }

            auto const track = seq::FindTrack(m_doc, m_inspectorTrackId);

            if (track == nullptr)
            {
                return;
            }

            auto source = track->Source;

            if (auto const choice = SourceEndpointCombo().SelectedItem().try_as<appshared::EndpointChoice>(); choice != nullptr)
            {
                source.Endpoint = choice.EndpointDeviceId().empty()
                    ? seq::EndpointRef{}
                    : seq::EndpointRef{ std::wstring{ choice.DisplayName() }, std::wstring{ choice.EndpointDeviceId() } };

                // The not-connected entry keeps the saved name.
                if (!choice.EndpointDeviceId().empty() && !m_directory->Resolve(source.Endpoint).has_value())
                {
                    source.Endpoint = track->Source.Endpoint;
                }
            }

            // A new device starts on any group.
            if (sender == SourceEndpointCombo())
            {
                source.Group = -1;
            }
            else if (auto const group = SourceGroupCombo().SelectedItem().try_as<appshared::NamedChoice>(); group != nullptr)
            {
                source.Group = static_cast<int8_t>(std::clamp(group.Value(), -1, 15));
            }

            if (auto const channel = SourceChannelCombo().SelectedItem().try_as<appshared::NamedChoice>(); channel != nullptr)
            {
                source.Channels = channel.Value() < 0 ? seq::AllChannels : static_cast<uint16_t>(1u << std::clamp(channel.Value(), 0, 15));
            }

            if (source == track->Source)
            {
                return;
            }

            auto const id = m_inspectorTrackId;

            EditTracks(std::wstring{ res::GetString(L"UndoChangeSource") }, [&](seq::Sequence& doc)
            {
                if (auto t = seq::FindTrack(doc, id); t != nullptr)
                {
                    t->Source = source;
                }
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change where the track records from.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEchoToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_inspectorUpdating || m_inspectorTrackId.empty())
        {
            return;
        }

        auto const id = m_inspectorTrackId;
        auto const echo = EchoSwitch().IsOn();

        auto const track = seq::FindTrack(m_doc, id);

        if (track == nullptr || track->Source.Echo == echo)
        {
            return;
        }

        EditTracks(std::wstring{ res::GetString(L"UndoChangeSource") }, [&](seq::Sequence& doc)
        {
            if (auto t = seq::FindTrack(doc, id); t != nullptr)
            {
                t->Source.Echo = echo;
            }
        });
    }

    _Use_decl_annotations_
    void MainWindow::OnRecordFromKeyboardClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, m_inspectorTrackId);

            if (track == nullptr)
            {
                return;
            }

            // Record from side B, and play MIDI Keyboard into side A (design section 6).
            std::optional<midiapp::LiveEndpoint> sideA{};
            std::optional<midiapp::LiveEndpoint> sideB{};

            for (auto const& summary : m_directory->Snapshot())
            {
                if (_wcsicmp(summary.Live.Name.c_str(), DefaultLoopbackA) == 0)
                {
                    sideA = summary.Live;
                }
                else if (_wcsicmp(summary.Live.Name.c_str(), DefaultLoopbackB) == 0)
                {
                    sideB = summary.Live;
                }
            }

            if (!sideA.has_value() || !sideB.has_value())
            {
                ShowMessage(res::GetString(L"LoopbackMissing"));
                return;
            }

            auto const id = m_inspectorTrackId;
            auto const channel = track->Destination.Channel;

            EditTracks(std::wstring{ res::GetString(L"UndoChangeSource") }, [&](seq::Sequence& doc)
            {
                if (auto t = seq::FindTrack(doc, id); t != nullptr)
                {
                    t->Source.Endpoint = seq::EndpointDirectory::MakeRef(*sideB);
                    t->Source.Group = -1;
                    t->Source.Channels = seq::AllChannels;
                }
            });

            m_armed.insert(id);
            UpdateEchoRoutes();
            PrepareConnectionsAsync();
            RebuildHeaders();

            LaunchKeyboard(sideA->EndpointDeviceId, 0, channel);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to record from MIDI Keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::OnDestinationChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_inspectorUpdating || m_inspectorTrackId.empty())
            {
                return;
            }

            auto const track = seq::FindTrack(m_doc, m_inspectorTrackId);

            if (track == nullptr)
            {
                return;
            }

            auto destination = track->Destination;

            if (auto const choice = DestinationEndpointCombo().SelectedItem().try_as<appshared::EndpointChoice>(); choice != nullptr)
            {
                destination.Endpoint = choice.EndpointDeviceId().empty()
                    ? seq::EndpointRef{}
                    : seq::EndpointRef{ std::wstring{ choice.DisplayName() }, std::wstring{ choice.EndpointDeviceId() } };

                if (!choice.EndpointDeviceId().empty() && !m_directory->Resolve(destination.Endpoint).has_value())
                {
                    destination.Endpoint = track->Destination.Endpoint;
                }
            }

            if (sender == DestinationEndpointCombo())
            {
                // A new device starts on its first group, on a channel nothing else there uses.
                destination.Group = 0;

                if (auto const live = m_directory->Resolve(destination.Endpoint); live.has_value())
                {
                    for (uint8_t group = 0; group < 16; ++group)
                    {
                        if (live->Live.DestinationGroups[group])
                        {
                            destination.Group = group;
                            break;
                        }
                    }
                }

                if (!(destination.Endpoint == track->Destination.Endpoint))
                {
                    destination.Channel = destination.Endpoint.IsEmpty() ? 0 : seq::NextFreeChannel(m_doc, destination.Endpoint, destination.Group);
                }
            }
            else
            {
                if (auto const group = DestinationGroupCombo().SelectedItem().try_as<appshared::NamedChoice>(); group != nullptr)
                {
                    destination.Group = static_cast<uint8_t>(std::clamp(group.Value(), 0, 15));
                }

                if (auto const channel = DestinationChannelCombo().SelectedItem().try_as<appshared::NamedChoice>(); channel != nullptr)
                {
                    destination.Channel = static_cast<int8_t>(std::clamp(channel.Value(), -1, 15));
                }
            }

            if (destination == track->Destination)
            {
                return;
            }

            auto const id = m_inspectorTrackId;

            EditTracks(std::wstring{ res::GetString(L"UndoChangeDestination") }, [&](seq::Sequence& doc)
            {
                if (auto t = seq::FindTrack(doc, id); t != nullptr)
                {
                    t->Destination = destination;
                }
            });

            RefreshEndpoints();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change where the track plays.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTrySoundClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, m_inspectorTrackId);

            if (track == nullptr)
            {
                return;
            }

            LaunchKeyboard(m_directory->ResolveId(track->Destination.Endpoint), track->Destination.Group, track->Destination.Channel);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open MIDI Keyboard.")
    }
}
