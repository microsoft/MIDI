// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "GlassLayoutWriter.h"
#include "RemoteControl.h"
#include "StringResources.h"

#include <fstream>

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr auto RemoteInterval = std::chrono::milliseconds{ 100 };

        // A surface whose SDK can't say it connected gets everything this often, in timer ticks.
        constexpr uint32_t RemoteResendTicks = 40;

        std::wstring NewGuidText()
        {
            GUID guid{};

            if (FAILED(::CoCreateGuid(&guid)))
            {
                return {};
            }

            return std::format(L"{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
                guid.Data1, guid.Data2, guid.Data3,
                guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        }

        std::wstring UtcNowText()
        {
            auto const now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
            return std::format(L"{:%Y-%m-%dT%H:%M:%SZ}", now);
        }

        // A file name can't hold some characters a sequence name can.
        std::wstring SafeFileName(std::wstring name)
        {
            for (auto& c : name)
            {
                if (c < 0x20 || std::wstring_view{ L"<>:\"/\\|?*" }.find(c) != std::wstring_view::npos)
                {
                    c = L'_';
                }
            }

            return name;
        }
    }

    // ---------------------------------------------------------------- the endpoint

    winrt::fire_and_forget MainWindow::StartRemoteControlAsync()
    {
        auto strong = get_strong();

        try
        {
            if (m_session == nullptr || m_remote != nullptr || m_remoteStarting || m_closing)
            {
                co_return;
            }

            m_remoteStarting = true;

            auto remote = std::make_shared<seq::RemoteEndpoint>(m_session);
            auto const name = std::wstring{ res::GetString(L"RemoteEndpointName") };
            auto const description = std::wstring{ res::GetString(L"RemoteEndpointDescription") };
            auto const blockName = std::wstring{ res::GetString(L"RemoteBlockName") };
            auto const weak = get_weak();
            auto const queue = DispatcherQueue();
            auto opened = false;

            co_await ::midisequencer::RunOnBackgroundAsync([&]()
            {
                opened = remote->Open(name, description, blockName,
                    [weak, queue](uint32_t const* words, uint8_t count)
                    {
                        std::array<uint32_t, 4> copy{};
                        std::copy_n(words, std::min<uint8_t>(count, 4), copy.begin());

                        queue.TryEnqueue([weak, copy, count]()
                        {
                            if (auto window = weak.get())
                            {
                                window->OnRemoteMessage(copy.data(), count);
                            }
                        });
                    },
                    [weak, queue]()
                    {
                        queue.TryEnqueue([weak]()
                        {
                            if (auto window = weak.get())
                            {
                                window->m_remoteShown.reset();
                            }
                        });
                    });
            });

            m_remoteStarting = false;

            // Turned off, or closing, while it was being made.
            if (m_closing || !seq::AppSettings::Current().RemoteControlEnabled())
            {
                co_await ::midisequencer::RunOnBackgroundAsync([remote]() { remote->Close(); });
                co_return;
            }

            if (!opened)
            {
                ShowMessage(res::GetString(L"RemoteUnavailable"));
                co_return;
            }

            m_remote = remote;
            m_remoteShown.reset();
            m_remoteTicks = 0;

            if (m_remoteTimer == nullptr)
            {
                m_remoteTimer = xaml::DispatcherTimer{};
                m_remoteTimer.Interval(RemoteInterval);
                m_remoteTimer.Tick([weak](auto&&, auto&&)
                {
                    if (auto window = weak.get())
                    {
                        window->UpdateRemoteFeedback();
                    }
                });
            }

            m_remoteTimer.Start();
            UpdateRemoteFeedback();
            ShowMessage(res::FormatString(L"RemoteOnFormat", name));
        }
        catch (...)
        {
            m_remoteStarting = false;
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void MainWindow::StopRemoteControl() noexcept
    {
        try
        {
            if (m_remoteTimer != nullptr)
            {
                m_remoteTimer.Stop();
            }

            if (auto remote = std::exchange(m_remote, nullptr); remote != nullptr)
            {
                remote->Close();
            }

            m_remoteShown.reset();
        }
        catch (...)
        {
        }
    }

    // ---------------------------------------------------------------- what comes in

    _Use_decl_annotations_
    void MainWindow::OnRemoteMessage(uint32_t const* words, uint8_t wordCount) noexcept
    {
        try
        {
            auto const command = seq::ParseRemoteMessage(words, wordCount);

            if (!command.has_value() || m_closing || m_remote == nullptr)
            {
                return;
            }

            auto const order = seq::RemoteTrackOrder(m_doc);
            auto const trackId = command->Track < order.size() ? order[command->Track] : std::wstring{};

            auto const forTrack =
                command->Action == seq::RemoteAction::LaunchSlot || command->Action == seq::RemoteAction::StopTrack ||
                command->Action == seq::RemoteAction::TrackToTimeline || command->Action == seq::RemoteAction::Mute ||
                command->Action == seq::RemoteAction::Solo || command->Action == seq::RemoteAction::Arm;

            if (forTrack && trackId.empty())
            {
                return;
            }

            switch (command->Action)
            {
            case seq::RemoteAction::LaunchSlot:
                LaunchSlot(trackId, command->Scene);
                break;

            case seq::RemoteAction::StopTrack:
                if (m_engine != nullptr)
                {
                    m_engine->StopTrack(trackId, LaunchQuantize());
                    UpdateTransport();
                }
                break;

            case seq::RemoteAction::TrackToTimeline:
                if (m_engine != nullptr)
                {
                    m_engine->ReturnToTimeline(trackId, LaunchQuantize());
                }
                break;

            case seq::RemoteAction::Mute:
            case seq::RemoteAction::Solo:
            {
                auto const track = seq::FindTrack(m_doc, trackId);
                auto const mute = command->Action == seq::RemoteAction::Mute;

                // A surface sends its switch's state; only a change is an edit to undo.
                if (track != nullptr && (mute ? track->Muted : track->Soloed) != command->On)
                {
                    ToggleTrackFlag(trackId, mute ? L'M' : L'S', command->On);
                }
                break;
            }

            case seq::RemoteAction::Arm:
                if (m_armed.contains(trackId) != command->On)
                {
                    ToggleTrackFlag(trackId, L'R', command->On);
                }
                break;

            case seq::RemoteAction::LaunchScene:
                LaunchScene(command->Scene);
                break;

            case seq::RemoteAction::StopAll:
                if (m_engine != nullptr)
                {
                    for (auto const& id : order)
                    {
                        m_engine->StopTrack(id, LaunchQuantize());
                    }

                    UpdateTransport();
                }
                break;

            case seq::RemoteAction::AllToTimeline:
                if (m_engine != nullptr)
                {
                    m_engine->ReturnToTimeline(std::wstring{}, LaunchQuantize());
                }
                break;

            case seq::RemoteAction::Play:
                if (m_engine != nullptr && !m_engine->IsPlaying())
                {
                    Play(m_position);
                }
                break;

            case seq::RemoteAction::Stop:
                Stop();
                break;

            case seq::RemoteAction::Record:
                if (m_recording)
                {
                    StopRecording();
                }
                else
                {
                    StartRecording();
                }
                break;

            case seq::RemoteAction::Loop:
                LoopToggle().IsChecked(!(LoopToggle().IsChecked() != nullptr && LoopToggle().IsChecked().GetBoolean()));
                ApplyEngineSettings();
                InvalidateArrange();
                break;

            case seq::RemoteAction::Metronome:
            {
                auto const on = !(MetronomeToggle().IsChecked() != nullptr && MetronomeToggle().IsChecked().GetBoolean());
                MetronomeToggle().IsChecked(on);
                seq::AppSettings::Current().MetronomeEnabled(on);
                ApplyEngineSettings();
                PrepareConnectionsAsync();
                UpdateTransport();
                break;
            }

            case seq::RemoteAction::GoToStart:
                SetPosition(0);
                ScrollToTick(0);
                break;

            default:
                break;
            }

            UpdateRemoteFeedback();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to act on a remote control message.")
    }

    // ---------------------------------------------------------------- what goes back

    seq::RemoteState MainWindow::CurrentRemoteState()
    {
        seq::RemoteState state{};

        auto const order = seq::RemoteTrackOrder(m_doc);
        auto const scenes = std::min(m_doc.Scenes.size(), seq::RemoteMaximumScenes);

        for (auto const& id : order)
        {
            auto const track = seq::FindTrack(m_doc, id);
            std::vector<bool> slots(scenes, false);

            if (track != nullptr)
            {
                auto const view = m_launchViews.find(id);

                for (size_t scene = 0; scene < scenes && scene < track->Slots.size(); ++scene)
                {
                    auto const& clipId = track->Slots[scene];
                    auto const playing = view != m_launchViews.end() && view->second.Plays(clipId, scene);
                    auto const recording = m_slotTake.has_value() && m_slotTake->TrackId == id && m_slotTake->Scene == scene;
                    slots[scene] = playing || recording;
                }
            }

            state.SlotsLit.push_back(std::move(slots));
            state.Muted.push_back(track != nullptr && track->Muted);
            state.Soloed.push_back(track != nullptr && track->Soloed);
            state.Armed.push_back(m_armed.contains(id));
        }

        state.Playing = m_engine != nullptr && m_engine->IsPlaying();
        state.Recording = m_recording || m_slotRecording;
        state.Loop = LoopToggle().IsChecked() != nullptr && LoopToggle().IsChecked().GetBoolean();
        state.Metronome = MetronomeToggle().IsChecked() != nullptr && MetronomeToggle().IsChecked().GetBoolean();
        return state;
    }

    void MainWindow::UpdateRemoteFeedback() noexcept
    {
        try
        {
            if (m_remote == nullptr)
            {
                return;
            }

            if (!m_remote->KnowsWhenAppsConnect() && ++m_remoteTicks % RemoteResendTicks == 0)
            {
                m_remoteShown.reset();
            }

            auto state = CurrentRemoteState();

            for (auto const word : seq::RemoteFeedback(m_remoteShown.has_value() ? &*m_remoteShown : nullptr, state))
            {
                m_remote->Send(&word, 1);
            }

            m_remoteShown = std::move(state);
        }
        catch (...)
        {
        }
    }

    // ---------------------------------------------------------------- the dialog

    _Use_decl_annotations_
    void MainWindow::OnRemoteControlClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            ShowRemoteControlDialog();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the remote control settings.")
    }

    void MainWindow::ShowRemoteControlDialog()
    {
        [](winrt::com_ptr<MainWindow> strong) -> winrt::fire_and_forget
        {
            try
            {
                auto const secondary = strong->BrushFor(strong->m_palette.Text2);
                auto const name = std::wstring{ res::GetString(L"RemoteEndpointName") };

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
                intro.Text(res::FormatString(L"RemoteDialogIntroFormat", name));
                intro.TextWrapping(xaml::TextWrapping::Wrap);
                panel.Children().Append(intro);

                controls::ToggleSwitch enabled{};
                enabled.Header(winrt::box_value(res::GetString(L"RemoteSwitchHeader")));
                enabled.IsOn(seq::AppSettings::Current().RemoteControlEnabled());
                panel.Children().Append(enabled);

                controls::TextBlock heading{};
                heading.Text(res::GetString(L"RemoteMapHeading"));
                heading.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                heading.Margin(xaml::Thickness{ 0, 6, 0, 0 });
                automation::AutomationProperties::SetHeadingLevel(heading, automation::Peers::AutomationHeadingLevel::Level2);
                panel.Children().Append(heading);

                panel.Children().Append(note(res::GetString(L"RemoteMapTracks")));
                panel.Children().Append(note(res::GetString(L"RemoteMapSequence")));
                panel.Children().Append(note(res::GetString(L"RemoteMapMore")));

                controls::Button makeLayout{};
                makeLayout.Content(winrt::box_value(res::GetString(L"RemoteMakeLayout")));
                makeLayout.Margin(xaml::Thickness{ 0, 6, 0, 0 });
                panel.Children().Append(makeLayout);
                panel.Children().Append(note(res::GetString(L"RemoteMakeLayoutHelp")));

                enabled.Toggled([weak = strong->get_weak(), weakSwitch = winrt::make_weak(enabled)](auto&&, auto&&)
                {
                    auto window = weak.get();
                    auto toggle = weakSwitch.get();

                    if (!window || !toggle)
                    {
                        return;
                    }

                    seq::AppSettings::Current().RemoteControlEnabled(toggle.IsOn());

                    if (toggle.IsOn())
                    {
                        window->StartRemoteControlAsync();
                    }
                    else
                    {
                        window->StopRemoteControl();
                        window->ShowMessage(res::GetString(L"RemoteOff"));
                    }
                });

                controls::ContentDialog dialog{};
                dialog.XamlRoot(strong->Content().XamlRoot());
                dialog.Title(winrt::box_value(res::GetString(L"RemoteDialogTitle")));
                dialog.Content(panel);
                dialog.CloseButtonText(res::GetString(L"DialogDone"));
                dialog.DefaultButton(controls::ContentDialogButton::Close);
                dialog.RequestedTheme(strong->RootGrid().ActualTheme());

                makeLayout.Click([weak = strong->get_weak(), weakDialog = winrt::make_weak(dialog)](auto&&, auto&&)
                {
                    if (auto open = weakDialog.get())
                    {
                        open.Hide();
                    }

                    if (auto window = weak.get())
                    {
                        window->MakeGlassLayoutAsync();
                    }
                });

                co_await dialog.ShowAsync();
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the remote control settings.")
        }(get_strong());
    }

    // ---------------------------------------------------------------- MIDI Glass

    winrt::fire_and_forget MainWindow::MakeGlassLayoutAsync()
    {
        auto strong = get_strong();

        try
        {
            seq::GlassLayoutText text{};
            auto const sequenceName = m_doc.Name.empty() ? std::wstring{ res::GetString(L"UntitledSequence") } : m_doc.Name;

            text.Name = res::FormatString(L"GlassLayoutNameFormat", sequenceName);
            text.Description = res::FormatString(L"GlassLayoutDescriptionFormat", sequenceName);
            text.EndpointName = res::GetString(L"RemoteEndpointName");
            text.Tool = res::GetString(L"AppDisplayName");
            text.Id = NewGuidText();
            text.Created = UtcNowText();

            if (m_doc.Provenance.has_value())
            {
                text.Author = m_doc.Provenance->Author;
                text.License = m_doc.Provenance->License;
            }

            text.Play = res::GetString(L"GlassPlay");
            text.Stop = res::GetString(L"GlassStop");
            text.Record = res::GetString(L"GlassRecord");
            text.Loop = res::GetString(L"GlassLoop");
            text.Metronome = res::GetString(L"GlassMetronome");
            text.GoToStart = res::GetString(L"GlassGoToStart");
            text.StopAll = res::GetString(L"GlassStopAll");
            text.BackToTimeline = res::GetString(L"GlassBackToTimeline");
            text.Mute = res::GetString(L"GlassMute");
            text.Solo = res::GetString(L"GlassSolo");
            text.Arm = res::GetString(L"GlassArm");
            text.StopTrack = res::GetString(L"GlassStopTrack");
            text.EmptySlot = res::GetString(L"GlassEmptySlot");
            text.SceneName = [](size_t scene) { return std::wstring{ res::FormatString(L"SceneNumberFormat", scene + 1) }; };
            text.TrackName = [](size_t track) { return std::wstring{ res::FormatString(L"GlassTrackFormat", track + 1) }; };
            text.PageName = [](size_t firstTrack, size_t lastTrack, size_t firstScene, size_t lastScene)
            {
                return std::wstring{ res::FormatString(L"GlassPageFormat", firstTrack + 1, lastTrack + 1, firstScene + 1, lastScene + 1) };
            };
            text.PageTabName = [](size_t page) { return std::wstring{ res::FormatString(L"GlassPageTabFormat", page + 1) }; };

            auto const layout = seq::MakeGlassLayout(m_doc, text);
            auto const [tracks, scenes] = seq::GlassLayoutCoverage(m_doc);

            // Where MIDI Glass looks for layouts, so it's in the library straight away.
            auto const folder = DocumentsFolder(L"MIDI Layouts");

            auto const path = ShowFileDialog(true, SafeFileName(text.Name) + L".midilayout",
                { { std::wstring{ res::GetString(L"GlassLayoutFilter") }, L"*.midilayout" } }, false, folder);

            if (path.empty())
            {
                co_return;
            }

            auto written = false;
            auto const bytes = seq::ToUtf8(layout);

            co_await ::midisequencer::RunOnBackgroundAsync([&]()
            {
                std::ofstream file{ std::filesystem::path{ path }, std::ios::binary | std::ios::trunc };
                file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                written = file.good();
            });

            if (!written)
            {
                ShowMessage(res::GetString(L"GlassLayoutNotSaved"));
                co_return;
            }

            auto const all = tracks == seq::RemoteTrackOrder(m_doc).size() && scenes == m_doc.Scenes.size();
            ShowMessage(all ? res::GetString(L"GlassLayoutSaved") : res::FormatString(L"GlassLayoutSavedPartFormat", tracks, scenes));

            // The layout only works with remote control on.
            if (!seq::AppSettings::Current().RemoteControlEnabled())
            {
                seq::AppSettings::Current().RemoteControlEnabled(true);
                StartRemoteControlAsync();
            }

            // Open it in MIDI Glass, which runs it: nothing is sent until a control is touched.
            auto const glass = SiblingToolPath(L"midiglass");
            auto const arguments = L"--run \"" + path + L"\"";

            SHELLEXECUTEINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOASYNC;
            info.lpVerb = L"open";
            info.nShow = SW_SHOWNORMAL;

            auto const glassPath = glass.wstring();

            if (!glassPath.empty())
            {
                info.lpFile = glassPath.c_str();
                info.lpParameters = arguments.c_str();
            }
            else
            {
                info.lpFile = path.c_str();
            }

            ::ShellExecuteExW(&info);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to make a MIDI Glass layout.")
    }
}
