// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Importing files that aren't sequences, pasting from MIDI Monitor, and DAWproject export.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "CaptureImport.h"
#include "DawProject.h"
#include "DocumentFiles.h"
#include "EndpointCatalog.h"
#include "StringResources.h"

#include <winver.h>

#pragma comment(lib, "version.lib")

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    using namespace documentfiles;

    namespace
    {
        constexpr uint64_t MaximumCaptureFileBytes = 512ull * 1024 * 1024;

        // A DAWproject import lists this many tracks to choose devices for. Real projects have fewer.
        constexpr size_t MaximumListedTracks = 100;
        constexpr double DawProjectDialogWidth = 600;
        constexpr double DawProjectDeviceWidth = 280;

        controls::TextBlock DialogHeading(winrt::hstring const& text)
        {
            controls::TextBlock heading{};
            heading.Text(text);
            heading.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            heading.Margin(xaml::Thickness{ 0, 12, 0, 2 });
            automation::AutomationProperties::SetHeadingLevel(heading, automation::Peers::AutomationHeadingLevel::Level2);
            return heading;
        }

        controls::TextBlock DialogNote(winrt::hstring const& text, media::Brush const& brush)
        {
            controls::TextBlock note{};
            note.Text(text);
            note.TextWrapping(xaml::TextWrapping::Wrap);
            note.FontSize(12);
            note.Foreground(brush);
            return note;
        }

        // The version this program's file gives as text, such as "1.0.0.1". The numbers in the same
        // resource are the Windows build it was made with, not the tool's version. Empty when it
        // can't be read.
        std::wstring ExecutableVersion()
        {
            try
            {
                wchar_t path[MAX_PATH]{};
                auto const length = ::GetModuleFileNameW(nullptr, path, MAX_PATH);

                if (length == 0 || length >= MAX_PATH)
                {
                    return {};
                }

                DWORD unused{ 0 };
                auto const size = ::GetFileVersionInfoSizeW(path, &unused);

                if (size == 0)
                {
                    return {};
                }

                struct Translation
                {
                    WORD Language;
                    WORD CodePage;
                };

                std::vector<uint8_t> data(size);
                Translation* translations{ nullptr };
                UINT translationsSize{ 0 };

                if (!::GetFileVersionInfoW(path, 0, size, data.data()) ||
                    !::VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translations), &translationsSize) ||
                    translations == nullptr || translationsSize < sizeof(Translation))
                {
                    return {};
                }

                auto const key = std::format(L"\\StringFileInfo\\{:04x}{:04x}\\FileVersion", translations[0].Language, translations[0].CodePage);
                wchar_t const* text{ nullptr };
                UINT textSize{ 0 };

                if (!::VerQueryValueW(data.data(), key.c_str(), reinterpret_cast<void**>(const_cast<wchar_t**>(&text)), &textSize) || text == nullptr || textSize == 0)
                {
                    return {};
                }

                return std::wstring{ text, ::wcsnlen(text, textSize) };
            }
            catch (...)
            {
                return {};
            }
        }
    }

    _Use_decl_annotations_
    void MainWindow::ImportFile(std::wstring const& path) noexcept
    {
        try
        {
            auto const extension = std::filesystem::path{ path }.extension().wstring();

            if (_wcsicmp(extension.c_str(), L".midi2") == 0)
            {
                ImportClipFile(path);
            }
            else if (_wcsicmp(extension.c_str(), L".dawproject") == 0)
            {
                ImportDawProjectAsync(path);
            }
            else if (_wcsicmp(extension.c_str(), L".csv") == 0)
            {
                ImportCaptureAsync(path);
            }
            else
            {
                ImportStandardMidiFile(path);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to import a file.")
    }

    // An imported track plays to the synth until it's given a device. Right after the window opens
    // the synth may not be known yet; then the track gets it as soon as it is.
    _Use_decl_annotations_
    void MainWindow::PlayOnSynth(seq::Track& track) noexcept
    {
        try
        {
            if (track.IsFolder || !track.Destination.Endpoint.IsEmpty())
            {
                return;
            }

            if (auto const synth = m_directory->Resolve(seq::EndpointRef{ L"", m_directory->SynthEndpointId() }); synth.has_value())
            {
                track.Destination.Endpoint = seq::EndpointDirectory::MakeRef(synth->Live);
            }
            else
            {
                m_awaitingSynth.push_back(track.Id);
            }
        }
        catch (...)
        {
        }
    }

    // A capture plays back the way it was played, at the sequence's tempo, with a track for each
    // group. Read and worked out away from the window, because a long jam is a big file.
    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::ImportCaptureAsync(std::wstring path)
    {
        auto strong = get_strong();

        try
        {
            // Named here: string resources are read on the window's thread.
            std::array<std::wstring, 16> names{};

            for (uint8_t group = 0; group < names.size(); ++group)
            {
                names[group] = std::wstring{ res::FormatString(L"CaptureTrackNameFormat", group + 1) };
            }

            seq::CaptureText text{};
            text.TrackName = [names](uint8_t group) { return names[group & 0x0F]; };
            text.OriginDetail = std::filesystem::path{ path }.filename().wstring();

            auto const tempo = m_doc.Tempo;
            auto const meter = m_doc.Meter;
            auto read{ false };
            seq::ReadCapture capture{};
            seq::ImportedCapture imported{};

            co_await ::midisequencer::RunOnBackgroundAsync([&]()
            {
                std::vector<uint8_t> bytes{};
                read = ReadAllBytes(path, bytes, MaximumCaptureFileBytes);

                if (read)
                {
                    capture = seq::ReadCaptureCsv(std::string_view{ reinterpret_cast<char const*>(bytes.data()), bytes.size() });
                    bytes = {};

                    if (capture.Status == seq::CaptureStatus::Read)
                    {
                        imported = seq::ImportCapture(capture.Messages, tempo, meter, text);
                        capture.Messages = {};
                    }
                }
            });

            if (m_closing)
            {
                co_return;
            }

            if (!read)
            {
                ShowMessage(res::FormatString(L"ImportFailedFormat", FileStem(path)));
                co_return;
            }

            if (capture.Status == seq::CaptureStatus::NotACapture)
            {
                ShowMessage(res::FormatString(L"CaptureNotACaptureFormat", FileStem(path)));
                co_return;
            }

            if (capture.Status == seq::CaptureStatus::TooLarge)
            {
                ShowMessage(res::FormatString(L"CaptureTooLargeFormat", FileStem(path)));
                co_return;
            }

            if (imported.Tracks.empty())
            {
                ShowMessage(res::FormatString(L"ImportNothingFormat", FileStem(path)));
                co_return;
            }

            seq::ChangeList changes{};
            auto const tracksBefore = m_doc.Tracks;

            // A brand new sequence whose only track is the untouched starting one gives way to the capture.
            if (CanReplaceDocument())
            {
                m_doc.Tracks.clear();
                auto removed = seq::RemoveUnusedClips(m_doc);

                for (auto& clip : removed)
                {
                    changes.push_back(seq::MakeClipPresenceChange(std::move(clip), false));
                }

                if (m_doc.Name.empty())
                {
                    m_doc.Name = FileStem(path);
                }
            }

            auto const& swatches = seq::TrackColorSwatches();
            size_t colorIndex{ seq::CountTracks(m_doc) };
            auto const added = imported.Tracks.size();

            for (auto& track : imported.Tracks)
            {
                track.Slots.resize(m_doc.Scenes.size());
                track.Color = swatches[colorIndex++ % swatches.size()];

                // A capture names no device.
                PlayOnSynth(track);

                m_doc.Tracks.push_back(std::move(track));
            }

            for (auto& clip : imported.Clips)
            {
                changes.push_back(seq::MakeClipPresenceChange(clip, true));
                m_doc.Clips.push_back(std::move(clip));
            }

            changes.push_back(seq::MakeTracksChange(tracksBefore, m_doc.Tracks));

            Commit(std::wstring{ res::GetString(L"UndoImport") }, std::move(changes));

            auto const skipped = capture.SkippedLines + imported.SkippedMessages;

            ShowMessage(skipped > 0 ?
                res::FormatString(L"CaptureImportedSkippedFormat", FileStem(path), added, skipped) :
                res::FormatString(L"ImportedFormat", FileStem(path), added));

            PrepareConnectionsAsync();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to import a capture.")
    }

    _Use_decl_annotations_
    void MainWindow::OnImportCaptureClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const path = ShowFileDialog(false, std::wstring{}, {
                { std::wstring{ res::GetString(L"FileTypeCapture") }, L"*.csv" } });

            if (!path.empty())
            {
                ImportCaptureAsync(path);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to import a capture.")
    }

    _Use_decl_annotations_
    void MainWindow::OnImportPasteClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        PasteMessagesAsync();
    }

    // Comp 7: the clipboard has the messages but not their times, so the person picks how they're
    // laid out.
    winrt::fire_and_forget MainWindow::PasteMessagesAsync()
    {
        auto strong = get_strong();

        try
        {
            namespace transfer = winrt::Windows::ApplicationModel::DataTransfer;

            auto const content = transfer::Clipboard::GetContent();
            winrt::hstring text{};

            if (content != nullptr && content.Contains(transfer::StandardDataFormats::Text()))
            {
                text = co_await content.GetTextAsync();
            }

            if (m_closing)
            {
                co_return;
            }

            size_t skipped{ 0 };
            auto const messages = seq::ReadUmpWordText(std::wstring_view{ text }, skipped);

            if (messages.empty())
            {
                ShowMessage(res::GetString(L"PasteNothing"));
                co_return;
            }

            auto const target = seq::FindTrack(m_doc, m_selectedTrackId);

            if (target == nullptr || target->IsFolder)
            {
                ShowMessage(res::GetString(L"PasteNoTrack"));
                co_return;
            }

            auto const trackId = target->Id;
            auto const trackName = target->Name;
            auto const secondary = BrushFor(m_palette.Text2);

            controls::StackPanel panel{};
            panel.Spacing(8);
            panel.MaxWidth(DawProjectDialogWidth);
            panel.Children().Append(DialogNote(res::FormatString(L"PasteDialogIntroFormat", messages.size(), trackName), secondary));

            controls::RadioButton oneAStep{};
            oneAStep.GroupName(L"PasteLayout");
            oneAStep.IsChecked(true);
            oneAStep.Content(winrt::box_value(res::GetString(L"PasteOneAStep")));
            panel.Children().Append(oneAStep);

            controls::StackPanel stepRow{};
            stepRow.Orientation(controls::Orientation::Horizontal);
            stepRow.Spacing(8);
            stepRow.Margin(xaml::Thickness{ 28, 0, 0, 0 });

            controls::TextBlock stepLabel{};
            stepLabel.Text(res::GetString(L"PasteStepSize"));
            stepLabel.VerticalAlignment(xaml::VerticalAlignment::Center);
            stepRow.Children().Append(stepLabel);

            constexpr std::array<std::pair<wchar_t const*, int64_t>, 4> steps{ {
                { L"GridThirtySecond", seq::TicksPerQuarterNote / 8 },
                { L"GridSixteenth", seq::TicksPerQuarterNote / 4 },
                { L"GridEighth", seq::TicksPerQuarterNote / 2 },
                { L"GridQuarter", seq::TicksPerQuarterNote } } };

            controls::ComboBox stepSize{};

            for (auto const& [key, ticks] : steps)
            {
                stepSize.Items().Append(winrt::box_value(res::GetString(key)));
            }

            stepSize.SelectedIndex(1);
            automation::AutomationProperties::SetName(stepSize, res::GetString(L"PasteStepSize"));
            stepRow.Children().Append(stepSize);
            panel.Children().Append(stepRow);

            controls::RadioButton allAtOnce{};
            allAtOnce.GroupName(L"PasteLayout");
            allAtOnce.Content(winrt::box_value(res::GetString(L"PasteAllAtOnce")));
            panel.Children().Append(allAtOnce);

            auto help = DialogNote(res::GetString(L"PasteAllAtOnceHelp"), secondary);
            help.Margin(xaml::Thickness{ 28, 0, 0, 0 });
            panel.Children().Append(help);

            // The step size only matters for one message a step.
            oneAStep.Checked([stepSize](auto&&, auto&&) { stepSize.IsEnabled(true); });
            allAtOnce.Checked([stepSize](auto&&, auto&&) { stepSize.IsEnabled(false); });

            controls::ContentDialog dialog{};
            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(winrt::box_value(res::GetString(L"PasteDialogTitle")));
            dialog.Content(panel);
            dialog.PrimaryButtonText(res::GetString(L"PasteButton"));
            dialog.CloseButtonText(res::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);
            dialog.RequestedTheme(RootGrid().ActualTheme());

            auto const result = co_await dialog.ShowAsync();

            if (result != controls::ContentDialogResult::Primary || m_closing)
            {
                co_return;
            }

            auto const together = allAtOnce.IsChecked() != nullptr && allAtOnce.IsChecked().GetBoolean();
            auto const index = std::clamp(stepSize.SelectedIndex(), 0, static_cast<int32_t>(steps.size()) - 1);
            auto const stepTicks = together ? int64_t{ 0 } : steps[static_cast<size_t>(index)].second;

            auto pasted = seq::ClipFromMessages(messages, m_position, stepTicks, m_doc.Meter,
                std::wstring{ res::GetString(L"PastedClipName") }, std::wstring{ res::GetString(L"PastedOriginDetail") });

            // The track may have gone while the dialog was open.
            auto track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr)
            {
                co_return;
            }

            auto const tracksBefore = m_doc.Tracks;
            track->Timeline.push_back(seq::Placement{ pasted.Clip.Id, pasted.PlacementTick, 0 });
            std::stable_sort(track->Timeline.begin(), track->Timeline.end(), [](seq::Placement const& a, seq::Placement const& b) { return a.Tick < b.Tick; });

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipPresenceChange(pasted.Clip, true));
            m_doc.Clips.push_back(std::move(pasted.Clip));
            changes.push_back(seq::MakeTracksChange(tracksBefore, m_doc.Tracks));

            Commit(std::wstring{ res::GetString(L"UndoPaste") }, std::move(changes));

            ShowMessage(skipped > 0 ?
                res::FormatString(L"PastedSkippedFormat", messages.size(), trackName, skipped) :
                res::FormatString(L"PastedFormat", messages.size(), trackName));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to paste messages.")
    }

    // Read away from the window, because a project can be large. Then the person picks a device
    // for each track: the file names the instruments the other app played, not MIDI devices.
    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::ImportDawProjectAsync(std::wstring path)
    {
        auto strong = get_strong();

        try
        {
            seq::ImportedDawProject imported{};

            co_await ::midisequencer::RunOnBackgroundAsync([&]() { imported = seq::ImportDawProjectFile(std::filesystem::path{ path }); });

            if (m_closing)
            {
                co_return;
            }

            switch (imported.Status)
            {
            case seq::DawProjectStatus::Read:
                break;

            case seq::DawProjectStatus::Damaged:
                ShowMessage(res::FormatString(L"DawProjectDamagedFormat", FileStem(path)));
                co_return;

            case seq::DawProjectStatus::TooLarge:
                ShowMessage(res::FormatString(L"DawProjectTooLargeFormat", FileStem(path)));
                co_return;

            default:
                ShowMessage(res::FormatString(L"DawProjectNotAProjectFormat", FileStem(path)));
                co_return;
            }

            // The tracks that play, in the order they're drawn. They stay put until the import.
            std::vector<seq::Track*> playing{};
            std::function<void(std::vector<seq::Track>&)> collect = [&](std::vector<seq::Track>& tracks)
            {
                for (auto& track : tracks)
                {
                    if (track.IsFolder)
                    {
                        collect(track.Children);
                    }
                    else
                    {
                        playing.push_back(&track);
                    }
                }
            };

            collect(imported.Tracks);

            if (playing.empty())
            {
                ShowMessage(res::FormatString(L"ImportNothingFormat", FileStem(path)));
                co_return;
            }

            // Right after the window opens, the list of devices can still be on its way. It's
            // worth a short wait, or every list below would be empty.
            auto const devicesReady = [this]()
            {
                auto const& catalog = midiapp::EndpointCatalog::Current();

                if (!catalog.HasEnumerated())
                {
                    return false;
                }

                auto const catalogHasDestinations = std::ranges::any_of(catalog.Snapshot(),
                    [](midiapp::LiveEndpoint const& endpoint) { return endpoint.DestinationGroupCount() > 0; });

                return !catalogHasDestinations || std::ranges::any_of(m_directory->Snapshot(),
                    [](seq::EndpointSummary const& summary) { return summary.Live.DestinationGroupCount() > 0; });
            };

            for (int attempt = 0; attempt < 40 && !devicesReady(); ++attempt)
            {
                co_await ::midisequencer::RunOnBackgroundAsync([]() { ::Sleep(100); });

                if (m_closing)
                {
                    co_return;
                }
            }

            // Every track starts on the synth, like a Standard MIDI File's.
            seq::EndpointRef synth{};

            if (auto const found = m_directory->Resolve(seq::EndpointRef{ L"", m_directory->SynthEndpointId() }); found.has_value())
            {
                synth = seq::EndpointDirectory::MakeRef(found->Live);
            }

            auto const secondary = BrushFor(m_palette.Text2);
            auto const endpointTemplate = RootGrid().Resources().Lookup(winrt::box_value(L"EndpointChoiceTemplate")).as<xaml::DataTemplate>();
            auto const& application = imported.Application;

            controls::StackPanel panel{};
            panel.Spacing(8);
            panel.Padding(xaml::Thickness{ 0, 0, 16, 0 });
            panel.MaxWidth(DawProjectDialogWidth);

            panel.Children().Append(DialogNote(application.empty() ?
                res::GetString(L"DawProjectDialogIntro") :
                res::FormatString(L"DawProjectDialogIntroFormat", FileStem(path), application), secondary));

            panel.Children().Append(DialogHeading(res::GetString(L"DawProjectTracksHeading")));

            std::vector<controls::ComboBox> devices{};
            auto const listed = std::min(playing.size(), MaximumListedTracks);

            for (size_t index = 0; index < listed; ++index)
            {
                auto const& track = *playing[index];
                auto const name = track.Name.empty() ? std::wstring{ res::GetString(L"DefaultTrackName") } : track.Name;

                controls::Grid row{};
                row.ColumnSpacing(12);

                controls::ColumnDefinition labelColumn{};
                labelColumn.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
                row.ColumnDefinitions().Append(labelColumn);

                controls::ColumnDefinition deviceColumn{};
                deviceColumn.Width(xaml::GridLength{ DawProjectDeviceWidth, xaml::GridUnitType::Pixel });
                row.ColumnDefinitions().Append(deviceColumn);

                controls::StackPanel label{};
                label.VerticalAlignment(xaml::VerticalAlignment::Center);

                controls::TextBlock title{};
                title.Text(name);
                title.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                label.Children().Append(title);

                // What the track played in the other app, to help pick a device that sounds like it.
                auto const instrument = std::find_if(imported.Instruments.begin(), imported.Instruments.end(),
                    [&](auto const& hint) { return hint.first == track.Id; });

                if (instrument != imported.Instruments.end())
                {
                    label.Children().Append(DialogNote(application.empty() ?
                        res::FormatString(L"DawProjectInstrumentNoAppFormat", instrument->second) :
                        res::FormatString(L"DawProjectInstrumentFormat", instrument->second, application), secondary));
                }

                controls::Grid::SetColumn(label, 0);
                row.Children().Append(label);

                controls::ComboBox device{};
                device.Width(DawProjectDeviceWidth);
                device.ItemTemplate(endpointTemplate);
                device.VerticalAlignment(xaml::VerticalAlignment::Center);
                FillEndpointCombo(device, false, synth);
                automation::AutomationProperties::SetName(device, res::FormatString(L"DawProjectDeviceNameFormat", name));
                controls::Grid::SetColumn(device, 1);
                row.Children().Append(device);

                panel.Children().Append(row);
                devices.push_back(device);
            }

            if (playing.size() > listed)
            {
                panel.Children().Append(DialogNote(res::FormatString(L"DawProjectMoreTracksFormat", listed), secondary));
            }

            // What a sequence has no place for, so nothing goes missing without a word.
            auto const& left = imported.LeftOut;
            std::vector<winrt::hstring> leftOut{};

            if (left.AudioTracks > 0) leftOut.push_back(res::FormatString(L"DawProjectLeftOutAudioTracksFormat", left.AudioTracks));
            if (left.AudioClips > 0) leftOut.push_back(res::FormatString(L"DawProjectLeftOutAudioClipsFormat", left.AudioClips));
            if (left.Plugins > 0) leftOut.push_back(res::FormatString(L"DawProjectLeftOutPluginsFormat", left.Plugins));
            if (left.OtherAutomation > 0) leftOut.push_back(res::FormatString(L"DawProjectLeftOutAutomationFormat", left.OtherAutomation));
            if (left.NoteExpressions > 0) leftOut.push_back(res::FormatString(L"DawProjectLeftOutExpressionsFormat", left.NoteExpressions));

            if (!leftOut.empty())
            {
                panel.Children().Append(DialogHeading(res::GetString(L"DawProjectLeftOutHeading")));
                panel.Children().Append(DialogNote(res::GetString(L"DawProjectLeftOutWhy"), secondary));

                std::wstring lines{};

                for (auto const& line : leftOut)
                {
                    lines += lines.empty() ? L"" : L"\n";
                    lines += line;
                }

                controls::TextBlock list{};
                list.Text(lines);
                list.TextWrapping(xaml::TextWrapping::Wrap);
                list.FontSize(12);
                panel.Children().Append(list);
            }

            controls::ScrollViewer scroller{};
            scroller.Content(panel);
            scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Auto);

            controls::ContentDialog dialog{};
            dialog.XamlRoot(Content().XamlRoot());
            dialog.Resources().Insert(winrt::box_value(L"ContentDialogMaxWidth"), winrt::box_value(DawProjectDialogWidth + 80));
            dialog.Title(winrt::box_value(res::GetString(L"DawProjectDialogTitle")));
            dialog.Content(scroller);
            dialog.PrimaryButtonText(res::GetString(L"DawProjectImport"));
            dialog.CloseButtonText(res::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);
            dialog.RequestedTheme(RootGrid().ActualTheme());

            auto const result = co_await dialog.ShowAsync();

            if (result != controls::ContentDialogResult::Primary || m_closing)
            {
                co_return;
            }

            for (size_t index = 0; index < playing.size(); ++index)
            {
                if (index >= devices.size())
                {
                    PlayOnSynth(*playing[index]);
                    continue;
                }

                auto const choice = devices[index].SelectedItem().try_as<winrt::midisequencer::DeviceChoice>();

                playing[index]->Destination.Endpoint = choice == nullptr || choice.EndpointDeviceId().empty() ?
                    seq::EndpointRef{} :
                    seq::EndpointRef{ std::wstring{ choice.DisplayName() }, std::wstring{ choice.EndpointDeviceId() } };
            }

            AddDawProject(std::move(imported), path);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to import a DAWproject file.")
    }

    // Added to the sequence like a Standard MIDI File, with one undo step. The file's scenes come
    // after the sequence's own.
    _Use_decl_annotations_
    void MainWindow::AddDawProject(seq::ImportedDawProject imported, std::wstring const& path) noexcept
    {
        try
        {
            seq::ChangeList changes{};

            auto const tracksBefore = m_doc.Tracks;

            if (IsDefaultTempo(m_doc) && m_doc.Tags.empty())
            {
                if (!imported.Tempo.empty())
                {
                    changes.push_back(seq::MakeTempoChange(m_doc.Tempo, imported.Tempo));
                    m_doc.Tempo = imported.Tempo;
                }

                if (!imported.Meter.empty())
                {
                    changes.push_back(seq::MakeMeterChange(m_doc.Meter, imported.Meter));
                    m_doc.Meter = imported.Meter;
                }
            }

            if (!imported.Tags.empty())
            {
                auto tags = m_doc.Tags;
                tags.insert(tags.end(), imported.Tags.begin(), imported.Tags.end());
                std::stable_sort(tags.begin(), tags.end(), [](seq::Tag const& a, seq::Tag const& b) { return a.Tick < b.Tick; });
                changes.push_back(seq::MakeSequenceTagsChange(m_doc.Tags, tags));
                m_doc.Tags = std::move(tags);
            }

            // A brand new sequence whose only track is the untouched starting one gives way to the
            // file, scenes and all.
            auto const replaceStarter = CanReplaceDocument();

            if (replaceStarter)
            {
                m_doc.Tracks.clear();
                auto removed = seq::RemoveUnusedClips(m_doc);

                for (auto& clip : removed)
                {
                    changes.push_back(seq::MakeClipPresenceChange(std::move(clip), false));
                }

                if (m_doc.Name.empty())
                {
                    m_doc.Name = imported.Title.empty() ? FileStem(path) : imported.Title;
                }
            }

            auto const maximumScenes = seq::SequenceLimits{}.MaximumScenes;
            auto scenesReplaced{ false };

            if (replaceStarter && !imported.Scenes.empty())
            {
                if (imported.Scenes.size() > maximumScenes)
                {
                    imported.Scenes.resize(maximumScenes);
                }

                changes.push_back(seq::MakeScenesChange(m_doc.Scenes, imported.Scenes));
                m_doc.Scenes = std::move(imported.Scenes);
                imported.Scenes.clear();
                scenesReplaced = true;
            }

            auto const firstScene = scenesReplaced ? size_t{ 0 } : m_doc.Scenes.size();
            auto const room = maximumScenes > firstScene ? maximumScenes - firstScene : 0;

            if (imported.Scenes.size() > room)
            {
                imported.Scenes.resize(room);
            }

            if (!imported.Scenes.empty())
            {
                auto scenes = m_doc.Scenes;
                scenes.insert(scenes.end(), imported.Scenes.begin(), imported.Scenes.end());
                changes.push_back(seq::MakeScenesChange(m_doc.Scenes, scenes));
                m_doc.Scenes = std::move(scenes);
            }

            // Every track gets a slot for each scene. The file's slots move along past the
            // sequence's own scenes.
            auto const sceneCount = m_doc.Scenes.size();
            size_t added{ 0 };

            std::function<void(std::vector<seq::Track>&, bool)> fitSlots = [&](std::vector<seq::Track>& tracks, bool fromFile)
            {
                for (auto& track : tracks)
                {
                    if (fromFile)
                    {
                        track.Slots.insert(track.Slots.begin(), firstScene, std::wstring{});
                        added += track.IsFolder ? 0 : 1;
                    }

                    track.Slots.resize(sceneCount);
                    fitSlots(track.Children, fromFile);
                }
            };

            fitSlots(m_doc.Tracks, false);
            fitSlots(imported.Tracks, true);

            for (auto& track : imported.Tracks)
            {
                m_doc.Tracks.push_back(std::move(track));
            }

            for (auto& clip : imported.Clips)
            {
                changes.push_back(seq::MakeClipPresenceChange(clip, true));
                m_doc.Clips.push_back(std::move(clip));
            }

            changes.push_back(seq::MakeTracksChange(tracksBefore, m_doc.Tracks));

            Commit(std::wstring{ res::GetString(L"UndoImport") }, std::move(changes));
            ShowMessage(res::FormatString(L"ImportedFormat", FileStem(path), added));
            PrepareConnectionsAsync();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to add a DAWproject file.")
    }

    _Use_decl_annotations_
    void MainWindow::OnImportDawProjectClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const path = ShowFileDialog(false, std::wstring{}, {
                { std::wstring{ res::GetString(L"FileTypeDawProject") }, L"*.dawproject" } });

            if (!path.empty())
            {
                ImportDawProjectAsync(path);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to import a DAWproject file.")
    }

    _Use_decl_annotations_
    void MainWindow::OnExportDawProjectClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const name = SafeFileName(m_doc.Name.empty() ? std::wstring{ res::GetString(L"UntitledSequence") } : m_doc.Name);
            auto const path = ShowFileDialog(true, name + L".dawproject", {
                { std::wstring{ res::GetString(L"FileTypeDawProject") }, L"*.dawproject" } });

            if (path.empty())
            {
                return;
            }

            seq::DawProjectText text{};
            text.Application = std::wstring{ res::GetString(L"AppDisplayName") };
            text.Version = ExecutableVersion();
            text.Title = m_doc.Name;
            text.Copyright = m_doc.Copyright;

            if (m_doc.Provenance.has_value())
            {
                text.Artist = m_doc.Provenance->Author;
            }

            // DAWproject has no place for MIDI devices, so each track's comment names its own.
            text.PlayedOn = [](std::wstring const& device) { return std::wstring{ res::FormatString(L"DawProjectPlayedOnFormat", device) }; };

            auto const exported = seq::ExportDawProject(m_doc, text);

            if (!exported.Succeeded || !WriteAllBytes(path, exported.Bytes.data(), exported.Bytes.size()))
            {
                ShowMessage(res::FormatString(L"ExportFailedFormat", FileStem(path)));
                return;
            }

            winrt::hstring message{};

            if (exported.SkippedLanes > 0)
            {
                message = res::FormatString(L"ExportedDawProjectSkippedLanesFormat", FileStem(path), exported.SkippedMessages, exported.SkippedLanes);
            }
            else if (exported.SkippedMessages > 0)
            {
                message = res::FormatString(L"ExportedDawProjectSkippedFormat", FileStem(path), exported.SkippedMessages);
            }
            else
            {
                message = res::FormatString(L"ExportedFormat", FileStem(path));
            }

            // DAWproject has nowhere to put words.
            if (m_doc.Lyrics && !m_doc.Lyrics->Syllables.empty())
            {
                message = message + L" " + res::GetString(L"DawProjectNoLyrics");
            }

            ShowMessage(message);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to export a DAWproject file.")
    }
}
