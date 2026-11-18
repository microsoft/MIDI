// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "HarmonyFollow.h"
#include "RemoteControl.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;
namespace lpx = ::midisequencer::launchpad;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        // Often enough that a pressed pad lights while it's held, and a launch shows on the beat.
        constexpr auto LaunchpadInterval = std::chrono::milliseconds{ 50 };

        // The scales Note mode can play in, in midiapp::ScaleKind order.
        constexpr wchar_t const* ScaleNameKeys[]{
            L"ScaleMajor", L"ScaleMinor", L"ScaleDorian", L"ScalePhrygian", L"ScaleLydian", L"ScaleMixolydian",
            L"ScaleHarmonicMinor", L"ScaleMajorPentatonic", L"ScaleMinorPentatonic", L"ScaleBlues", L"ScaleChromatic",
        };

        static_assert(std::size(ScaleNameKeys) == midiapp::ScaleKindCount);
        static_assert(seq::AppSettings::LaunchpadScaleCount == midiapp::ScaleKindCount);

        lpx::EndpointCandidate ToCandidate(midiapp::LiveEndpoint const& live)
        {
            lpx::EndpointCandidate candidate{};
            candidate.EndpointId = live.EndpointDeviceId;
            candidate.Name = live.Name;
            candidate.UsbVendorId = live.UsbVendorId;
            candidate.UsbProductId = live.UsbProductId;

            for (size_t group = 0; group < candidate.SourceGroups.size() && group < live.SourceGroups.size(); ++group)
            {
                candidate.SourceGroups[group] = live.SourceGroups[group];
                candidate.DestinationGroups[group] = live.DestinationGroups[group];
                candidate.SourcePortNames[group] = live.SourcePortNames[group];
                candidate.DestinationPortNames[group] = live.DestinationPortNames[group];
                candidate.SourceGroupNames[group] = live.SourceGroupNames[group];
                candidate.DestinationGroupNames[group] = live.DestinationGroupNames[group];
            }

            return candidate;
        }
    }

    // ---------------------------------------------------------------- the connection

    winrt::fire_and_forget MainWindow::UpdateLaunchpadConnectionAsync()
    {
        auto strong = get_strong();

        try
        {
            if (m_session == nullptr || m_closing)
            {
                co_return;
            }

            // One at a time. A change while connecting is looked at again when it's done.
            if (m_launchpadConnecting)
            {
                m_launchpadAgain = true;
                co_return;
            }

            auto& settings = seq::AppSettings::Current();
            std::optional<lpx::LaunchpadPort> port{};
            std::wstring name{};

            if (settings.LaunchpadEnabled())
            {
                std::vector<lpx::EndpointCandidate> candidates{};

                for (auto const& summary : m_directory->Snapshot())
                {
                    candidates.push_back(ToCandidate(summary.Live));
                }

                port = lpx::FindLaunchpadPort(candidates, settings.LaunchpadEndpointId());

                if (port.has_value())
                {
                    auto const found = std::find_if(candidates.begin(), candidates.end(), [&port](lpx::EndpointCandidate const& c) { return c.EndpointId == port->EndpointId; });
                    name = found != candidates.end() ? found->Name : std::wstring{};
                }
            }

            // Still on the one it should be.
            if (m_launchpad != nullptr && m_launchpadEndpoint != nullptr && port.has_value() &&
                m_launchpadEndpoint->EndpointId() == port->EndpointId && m_launchpad->Group() == port->Group)
            {
                co_return;
            }

            if (m_launchpad != nullptr)
            {
                StopLaunchpad(true);
            }

            if (!port.has_value())
            {
                co_return;
            }

            m_launchpadConnecting = true;

            // The connection closed last has to be gone first: the device takes one at a time.
            if (auto closing = std::exchange(m_launchpadClosing, nullptr); closing != nullptr)
            {
                try
                {
                    co_await closing;
                }
                catch (...)
                {
                }
            }

            auto const endpoint = std::make_shared<::midisequencer::LaunchpadXEndpoint>(m_session);
            auto const weak = get_weak();
            auto const queue = DispatcherQueue();

            auto const controller = std::make_shared<lpx::LaunchpadXController>(
                endpoint,
                port->Group,
                [weak](lpx::SurfaceAction const& action, uint64_t timestamp)
                {
                    if (auto window = weak.get())
                    {
                        window->OnLaunchpadNote(action, timestamp);
                    }
                },
                [weak, queue](std::vector<lpx::SurfaceAction> actions)
                {
                    queue.TryEnqueue([weak, actions = std::move(actions)]()
                    {
                        if (auto window = weak.get())
                        {
                            window->OnLaunchpadActions(actions);
                        }
                    });
                });

            controller->SetMode(settings.LaunchpadNoteMode() ? lpx::ControllerMode::Note : lpx::ControllerMode::Clip);
            controller->SetNoteConfig(LaunchpadNoteLayout());

            auto const snapshot = CurrentLaunchpadState();
            auto const endpointId = port->EndpointId;
            auto opened = false;

            co_await ::midisequencer::RunOnBackgroundAsync([&]()
            {
                opened = endpoint->Open(endpointId, [controller](uint32_t const* words, uint8_t count, uint64_t timestamp)
                {
                    controller->OnInput(words, count, timestamp);
                });

                if (opened && !controller->Start(snapshot))
                {
                    endpoint->Close();
                    opened = false;
                }
            });

            m_launchpadConnecting = false;

            // Turned off, or closing, while it was connecting.
            if (m_closing || !settings.LaunchpadEnabled())
            {
                if (opened)
                {
                    m_launchpadClosing = ::midisequencer::RunOnBackgroundAsync([controller, endpoint]()
                    {
                        controller->Stop();
                        endpoint->Close();
                    });
                }

                co_return;
            }

            if (!opened)
            {
                ShowMessage(res::GetString(L"LaunchpadUnavailable"));
            }
            else
            {
                m_launchpadEndpoint = endpoint;
                m_launchpad = controller;
                m_launchpadName = name;

                {
                    std::scoped_lock guard{ m_inputLock };
                    m_launchpadSourceId = endpointId;
                    m_launchpadGroup = port->Group;
                }

                UpdateLaunchpadTarget();

                if (m_launchpadTimer == nullptr)
                {
                    m_launchpadTimer = xaml::DispatcherTimer{};
                    m_launchpadTimer.Interval(LaunchpadInterval);
                    m_launchpadTimer.Tick([weak](auto&&, auto&&)
                    {
                        if (auto window = weak.get())
                        {
                            window->UpdateLaunchpadFeedback();
                        }
                    });
                }

                m_launchpadTimer.Start();
                UpdateLaunchpadModeButton();
                ShowMessage(res::FormatString(L"LaunchpadOnFormat", m_launchpadName));
            }

            if (std::exchange(m_launchpadAgain, false))
            {
                UpdateLaunchpadConnectionAsync();
            }
        }
        catch (...)
        {
            m_launchpadConnecting = false;
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    void MainWindow::StopLaunchpad(bool inBackground) noexcept
    {
        try
        {
            if (m_launchpadTimer != nullptr)
            {
                m_launchpadTimer.Stop();
            }

            auto controller = std::exchange(m_launchpad, nullptr);
            auto endpoint = std::exchange(m_launchpadEndpoint, nullptr);
            m_launchpadName.clear();

            if (!m_closing)
            {
                UpdateLaunchpadModeButton();
            }

            {
                std::scoped_lock guard{ m_inputLock };
                m_launchpadSourceId.clear();
                m_launchpadTarget.reset();
            }

            if (controller == nullptr && endpoint == nullptr)
            {
                return;
            }

            // Stopping lets go of held notes, which still go out to where they were played.
            auto const close = [controller, endpoint]()
            {
                if (controller != nullptr)
                {
                    controller->Stop();
                }

                if (endpoint != nullptr)
                {
                    endpoint->Close();
                }
            };

            if (inBackground)
            {
                m_launchpadClosing = ::midisequencer::RunOnBackgroundAsync(close);
            }
            else
            {
                close();
            }
        }
        catch (...)
        {
        }
    }

    // ---------------------------------------------------------------- what the pads ask for

    _Use_decl_annotations_
    void MainWindow::OnLaunchpadActions(std::vector<lpx::SurfaceAction> const& actions) noexcept
    {
        try
        {
            if (m_closing || m_launchpad == nullptr)
            {
                return;
            }

            auto const order = seq::RemoteTrackOrder(m_doc);
            auto& settings = seq::AppSettings::Current();

            for (auto const& action : actions)
            {
                switch (action.kind)
                {
                case lpx::SurfaceActionKind::TriggerOrStopSlot:
                {
                    if (action.trackIndex >= order.size())
                    {
                        break;
                    }

                    auto const& trackId = order[action.trackIndex];
                    auto const track = seq::FindTrack(m_doc, trackId);
                    size_t const scene = action.sceneIndex;

                    if (track == nullptr)
                    {
                        break;
                    }

                    // The same as pressing the slot in the launcher.
                    auto const empty = scene >= track->Slots.size() || track->Slots[scene].empty();

                    if (m_slotTake.has_value() && m_slotTake->TrackId == trackId && m_slotTake->Scene == scene)
                    {
                        FinishSlotRecording(SlotTakeEnd::Early);
                    }
                    else if (empty && m_armed.contains(trackId) && !track->IsFolder)
                    {
                        StartSlotRecording(trackId, scene);
                    }
                    else
                    {
                        LaunchSlot(trackId, scene);
                    }

                    break;
                }

                case lpx::SurfaceActionKind::LaunchScene:
                    LaunchScene(action.sceneIndex);
                    break;

                case lpx::SurfaceActionKind::StopAll:
                    Stop();
                    break;

                case lpx::SurfaceActionKind::ToggleRecord:
                    if (m_slotRecording)
                    {
                        FinishSlotRecording(SlotTakeEnd::Early);
                    }
                    else if (m_recording)
                    {
                        StopRecording();
                    }
                    else
                    {
                        StartRecording();
                    }
                    break;

                case lpx::SurfaceActionKind::SelectTrack:
                    if (action.trackIndex < order.size())
                    {
                        SelectTrack(order[action.trackIndex]);
                    }
                    break;

                case lpx::SurfaceActionKind::SetViewport:
                    // Keep the launcher's view on the scenes the pads show.
                    if (action.sceneIndex < m_doc.Scenes.size())
                    {
                        ShowScene(action.sceneIndex);
                    }
                    break;

                case lpx::SurfaceActionKind::SwitchMode:
                    settings.LaunchpadNoteMode(action.targetMode == lpx::ControllerMode::Note);
                    UpdateLaunchpadModeButton();
                    break;

                case lpx::SurfaceActionKind::ShiftOctave:
                    settings.LaunchpadOctave(m_launchpad->NoteConfig().octave);
                    break;

                default:
                    break;
                }
            }

            UpdateLaunchpadFeedback();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to act on the Launchpad X.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLaunchpadNote(lpx::SurfaceAction const& action, uint64_t timestamp) noexcept
    {
        // On the service's thread for a pad, or the UI thread for a note let go by a mode change.
        // Quick, and nothing here touches XAML.
        try
        {
            uint32_t words[2]{};
            auto const count = lpx::LaunchpadXController::BuildNoteWords(action, words);

            if (count == 0)
            {
                return;
            }

            auto const when = timestamp != 0 ? timestamp : midi2::MidiClock::Now();
            auto const tick = m_engine != nullptr && m_recording ? m_engine->TickAtTime(when) : -1;
            auto const slotTick = m_engine != nullptr && m_slotRecording ? m_engine->UnwrappedTickAtTime(when) : -1;

            std::scoped_lock guard{ m_inputLock };

            LaunchpadTarget target{};

            if (action.kind == lpx::SurfaceActionKind::EmitNoteOn)
            {
                if (!m_launchpadTarget.has_value())
                {
                    return;
                }

                target = *m_launchpadTarget;
                m_launchpadSounding[action.midiNote] = target;
            }
            else
            {
                // A note-off, or pressure, goes where its note-on went.
                auto const sounding = m_launchpadSounding.find(action.midiNote);

                if (sounding == m_launchpadSounding.end())
                {
                    return;
                }

                target = sounding->second;

                if (action.kind == lpx::SurfaceActionKind::EmitNoteOff)
                {
                    m_launchpadSounding.erase(sounding);
                }
            }

            // The track's channel, like the notes it plays. SendNow moves it to the track's group.
            auto const channel = static_cast<uint32_t>(std::max<int8_t>(0, target.Destination.Channel) & 0x0F);
            words[0] = (words[0] & 0xFFF0FFFFu) | (channel << 16);

            SendNow(target.Destination, words, count);

            if (target.Armed)
            {
                RecordHeard(target.TrackId, tick, slotTick, when, words, count);
            }
        }
        catch (...)
        {
        }
    }

    // ---------------------------------------------------------------- what goes back

    lpx::NoteLayoutConfig MainWindow::LaunchpadNoteLayout() const
    {
        auto const& settings = seq::AppSettings::Current();

        lpx::NoteLayoutConfig config{};
        config.octave = settings.LaunchpadOctave();
        config.rootNote = settings.LaunchpadRoot();
        config.scale = static_cast<midiapp::ScaleKind>(settings.LaunchpadScale());
        config.inScaleOnly = settings.LaunchpadInScaleOnly();

        // The song's key where the playhead is, so the pads play in key as the song moves on. A
        // song with no key keeps the one picked in the dialog.
        if (settings.LaunchpadFollowSongKey())
        {
            if (auto const key = seq::SongKeyAt(m_doc, m_position); key.has_value())
            {
                config.rootNote = static_cast<uint8_t>(key->Root % 12);
                config.scale = key->Scale;
            }
        }

        return config;
    }

    lpx::SurfaceStateSnapshot MainWindow::CurrentLaunchpadState() const
    {
        std::vector<seq::TrackLaunchView> views{};
        views.reserve(m_launchViews.size());

        for (auto const& [id, view] : m_launchViews)
        {
            views.push_back(view);
        }

        auto const slotRecording = m_slotTake.has_value();

        return lpx::BuildSurfaceSnapshot(
            m_doc,
            views,
            m_selectedTrackId,
            m_selectedSlot != SIZE_MAX ? static_cast<uint32_t>(m_selectedSlot) : 0u,
            m_engine != nullptr && m_engine->IsPlaying(),
            m_recording || m_slotRecording,
            slotRecording,
            slotRecording ? std::wstring_view{ m_slotTake->TrackId } : std::wstring_view{},
            slotRecording ? static_cast<uint32_t>(m_slotTake->Scene) : 0u,
            LaunchpadNoteLayout(),
            m_armed.contains(m_selectedTrackId));
    }

    void MainWindow::UpdateLaunchpadTarget() noexcept
    {
        try
        {
            std::optional<LaunchpadTarget> target{};

            if (auto const track = seq::FindTrack(m_doc, m_selectedTrackId); track != nullptr && !track->IsFolder && !track->Destination.Endpoint.IsEmpty())
            {
                target = LaunchpadTarget{ track->Id, track->Destination, m_armed.contains(track->Id) };
            }

            std::scoped_lock guard{ m_inputLock };
            m_launchpadTarget = std::move(target);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MainWindow::UpdateLaunchpadFeedback(bool forceFullRefresh) noexcept
    {
        try
        {
            if (m_launchpad == nullptr || m_closing)
            {
                return;
            }

            // The key and layout from the settings, or the song. The octave is the pads' own, from
            // Up and Down.
            auto layout = LaunchpadNoteLayout();
            layout.octave = m_launchpad->NoteConfig().octave;
            m_launchpad->SetNoteConfig(layout);

            UpdateLaunchpadTarget();
            m_launchpad->Update(CurrentLaunchpadState(), forceFullRefresh);
        }
        catch (...)
        {
        }
    }

    // ---------------------------------------------------------------- the mode button

    void MainWindow::UpdateLaunchpadModeButton() noexcept
    {
        try
        {
            if (m_launchpad == nullptr)
            {
                LaunchpadModeButton().Visibility(xaml::Visibility::Collapsed);
                return;
            }

            auto const text = res::GetString(m_launchpad->Mode() == lpx::ControllerMode::Note ? L"LaunchpadModeNote" : L"LaunchpadModeClip");
            LaunchpadModeText().Text(text);
            automation::AutomationProperties::SetName(LaunchpadModeButton(), text);
            LaunchpadModeButton().Visibility(xaml::Visibility::Visible);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnLaunchpadModeClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_launchpad == nullptr)
            {
                return;
            }

            // The same as the Session and Note buttons on the device. Notes held in Note mode stop.
            auto const note = m_launchpad->Mode() != lpx::ControllerMode::Note;
            m_launchpad->SetMode(note ? lpx::ControllerMode::Note : lpx::ControllerMode::Clip);
            seq::AppSettings::Current().LaunchpadNoteMode(note);

            UpdateLaunchpadModeButton();
            UpdateLaunchpadFeedback();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to switch the Launchpad X mode.")
    }

    // ---------------------------------------------------------------- the dialog

    _Use_decl_annotations_
    void MainWindow::OnLaunchpadClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            ShowLaunchpadDialog();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the Launchpad X settings.")
    }

    void MainWindow::ShowLaunchpadDialog()
    {
        [](winrt::com_ptr<MainWindow> strong) -> winrt::fire_and_forget
        {
            try
            {
                auto const secondary = strong->BrushFor(strong->m_palette.Text2);

                auto const note = [&secondary](winrt::hstring const& text)
                {
                    controls::TextBlock block{};
                    block.Text(text);
                    block.TextWrapping(xaml::TextWrapping::Wrap);
                    block.FontSize(12);
                    block.Foreground(secondary);
                    return block;
                };

                controls::StackPanel panel{};
                panel.Spacing(10);
                panel.MaxWidth(560);

                controls::TextBlock intro{};
                intro.Text(res::GetString(L"LaunchpadDialogIntro"));
                intro.TextWrapping(xaml::TextWrapping::Wrap);
                panel.Children().Append(intro);

                controls::ToggleSwitch enabled{};
                enabled.Header(winrt::box_value(res::GetString(L"LaunchpadSwitchHeader")));
                enabled.IsOn(seq::AppSettings::Current().LaunchpadEnabled());
                panel.Children().Append(enabled);

                auto status = note(strong->m_launchpad != nullptr
                    ? res::FormatString(L"LaunchpadStatusConnectedFormat", strong->m_launchpadName)
                    : res::GetString(L"LaunchpadStatusNotConnected"));
                panel.Children().Append(status);

                controls::TextBlock heading{};
                heading.Text(res::GetString(L"LaunchpadMapHeading"));
                heading.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                heading.Margin(xaml::Thickness{ 0, 6, 0, 0 });
                automation::AutomationProperties::SetHeadingLevel(heading, automation::Peers::AutomationHeadingLevel::Level2);

                // Note mode's key and layout.
                auto& settings = seq::AppSettings::Current();

                controls::TextBlock keyHeading{};
                keyHeading.Text(res::GetString(L"LaunchpadKeyHeading"));
                keyHeading.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                keyHeading.Margin(xaml::Thickness{ 0, 6, 0, 0 });
                automation::AutomationProperties::SetHeadingLevel(keyHeading, automation::Peers::AutomationHeadingLevel::Level2);
                panel.Children().Append(keyHeading);

                controls::ToggleSwitch follow{};
                follow.Header(winrt::box_value(res::GetString(L"LaunchpadFollowSongKey")));
                automation::AutomationProperties::SetHelpText(follow, res::GetString(L"LaunchpadFollowSongKeyHelp"));
                follow.IsOn(settings.LaunchpadFollowSongKey());
                panel.Children().Append(follow);
                panel.Children().Append(note(res::GetString(L"LaunchpadFollowSongKeyHelp")));

                controls::StackPanel keyRow{};
                keyRow.Orientation(controls::Orientation::Horizontal);
                keyRow.Spacing(12);

                auto const scaleKind = static_cast<midiapp::ScaleKind>(settings.LaunchpadScale());

                controls::ComboBox root{};
                root.Header(winrt::box_value(res::GetString(L"LaunchpadRoot")));
                root.MinWidth(96);

                for (uint8_t i = 0; i < 12; ++i)
                {
                    root.Items().Append(winrt::box_value(winrt::hstring{ midiapp::NoteName(i, midiapp::KeyUsesFlats(i, scaleKind)) }));
                }

                root.SelectedIndex(settings.LaunchpadRoot());
                keyRow.Children().Append(root);

                controls::ComboBox scale{};
                scale.Header(winrt::box_value(res::GetString(L"LaunchpadScale")));
                scale.MinWidth(200);

                for (auto const key : ScaleNameKeys)
                {
                    scale.Items().Append(winrt::box_value(res::GetString(key)));
                }

                scale.SelectedIndex(settings.LaunchpadScale());
                keyRow.Children().Append(scale);

                panel.Children().Append(keyRow);
                panel.Children().Append(note(res::GetString(L"LaunchpadKeyHelp")));

                controls::ToggleSwitch inScale{};
                inScale.Header(winrt::box_value(res::GetString(L"LaunchpadInScaleOnly")));
                automation::AutomationProperties::SetHelpText(inScale, res::GetString(L"LaunchpadInScaleOnlyHelp"));
                inScale.IsOn(settings.LaunchpadInScaleOnly());
                panel.Children().Append(inScale);
                panel.Children().Append(note(res::GetString(L"LaunchpadInScaleOnlyHelp")));

                // Each change is saved, and the pads relit, straight away.
                auto const relight = [weak = strong->get_weak()]()
                {
                    if (auto window = weak.get())
                    {
                        window->UpdateLaunchpadFeedback();
                    }
                };

                follow.Toggled([relight, weakSwitch = winrt::make_weak(follow)](auto&&, auto&&)
                {
                    if (auto toggle = weakSwitch.get())
                    {
                        seq::AppSettings::Current().LaunchpadFollowSongKey(toggle.IsOn());
                        relight();
                    }
                });

                root.SelectionChanged([relight, weakRoot = winrt::make_weak(root)](auto&&, auto&&)
                {
                    if (auto box = weakRoot.get(); box && box.SelectedIndex() >= 0)
                    {
                        seq::AppSettings::Current().LaunchpadRoot(static_cast<uint8_t>(box.SelectedIndex()));
                        relight();
                    }
                });

                scale.SelectionChanged([relight, weakScale = winrt::make_weak(scale)](auto&&, auto&&)
                {
                    if (auto box = weakScale.get(); box && box.SelectedIndex() >= 0)
                    {
                        seq::AppSettings::Current().LaunchpadScale(static_cast<uint8_t>(box.SelectedIndex()));
                        relight();
                    }
                });

                inScale.Toggled([relight, weakSwitch = winrt::make_weak(inScale)](auto&&, auto&&)
                {
                    if (auto toggle = weakSwitch.get())
                    {
                        seq::AppSettings::Current().LaunchpadInScaleOnly(toggle.IsOn());
                        relight();
                    }
                });

                panel.Children().Append(heading);
                panel.Children().Append(note(res::GetString(L"LaunchpadMapClip")));
                panel.Children().Append(note(res::GetString(L"LaunchpadMapNote")));
                panel.Children().Append(note(res::GetString(L"LaunchpadMapTransport")));

                enabled.Toggled([weak = strong->get_weak(), weakSwitch = winrt::make_weak(enabled)](auto&&, auto&&)
                {
                    auto window = weak.get();
                    auto toggle = weakSwitch.get();

                    if (!window || !toggle)
                    {
                        return;
                    }

                    seq::AppSettings::Current().LaunchpadEnabled(toggle.IsOn());

                    if (toggle.IsOn())
                    {
                        window->UpdateLaunchpadConnectionAsync();
                    }
                    else
                    {
                        window->StopLaunchpad(true);
                        window->ShowMessage(res::GetString(L"LaunchpadOff"));
                    }
                });

                controls::ContentDialog dialog{};
                dialog.XamlRoot(strong->Content().XamlRoot());
                dialog.Title(winrt::box_value(res::GetString(L"LaunchpadDialogTitle")));
                dialog.Content(panel);
                dialog.CloseButtonText(res::GetString(L"DialogDone"));
                dialog.DefaultButton(controls::ContentDialogButton::Close);
                dialog.RequestedTheme(strong->RootGrid().ActualTheme());

                co_await dialog.ShowAsync();
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the Launchpad X settings.")
        }(get_strong());
    }
}
