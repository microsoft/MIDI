// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "MidiClipFile.h"
#include "SampleSequence.h"
#include "SequenceSerializer.h"
#include "StandardMidiFileBridge.h"
#include "StringResources.h"

#include "midi_file_smf_reader.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        // A sequence file is JSON text; far above anything a person makes, and still finite.
        constexpr uint64_t MaximumSequenceFileBytes = 512ull * 1024 * 1024;
        constexpr uint64_t MaximumClipFileBytes = 256ull * 1024 * 1024;

        bool ReadAllBytes(std::wstring const& path, std::vector<uint8_t>& bytes, uint64_t maximum) noexcept
        {
            try
            {
                wil::unique_hfile file{ ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) };

                if (!file)
                {
                    return false;
                }

                LARGE_INTEGER size{};

                if (!::GetFileSizeEx(file.get(), &size) || size.QuadPart < 0 || static_cast<uint64_t>(size.QuadPart) > maximum)
                {
                    return false;
                }

                bytes.resize(static_cast<size_t>(size.QuadPart));
                size_t offset{ 0 };

                while (offset < bytes.size())
                {
                    DWORD read{ 0 };
                    auto const chunk = static_cast<DWORD>(std::min<size_t>(bytes.size() - offset, 16u * 1024 * 1024));

                    if (!::ReadFile(file.get(), bytes.data() + offset, chunk, &read, nullptr) || read == 0)
                    {
                        return false;
                    }

                    offset += read;
                }

                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        // Written beside the file, then swapped in, so a crash part way never leaves half a file.
        bool WriteAllBytes(std::wstring const& path, void const* data, size_t size) noexcept
        {
            try
            {
                auto const temporary = path + L".saving";

                {
                    wil::unique_hfile file{ ::CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) };

                    if (!file)
                    {
                        return false;
                    }

                    auto const* bytes = static_cast<uint8_t const*>(data);
                    size_t offset{ 0 };

                    while (offset < size)
                    {
                        DWORD written{ 0 };
                        auto const chunk = static_cast<DWORD>(std::min<size_t>(size - offset, 16u * 1024 * 1024));

                        if (!::WriteFile(file.get(), bytes + offset, chunk, &written, nullptr) || written == 0)
                        {
                            return false;
                        }

                        offset += written;
                    }

                    ::FlushFileBuffers(file.get());
                }

                if (!::MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                {
                    ::DeleteFileW(temporary.c_str());
                    return false;
                }

                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        std::wstring FileStem(std::wstring const& path)
        {
            try
            {
                return std::filesystem::path{ path }.stem().wstring();
            }
            catch (...)
            {
                return {};
            }
        }

        // A name that's safe as part of a file name.
        std::wstring SafeFileName(std::wstring name)
        {
            for (auto& c : name)
            {
                if (c < 32 || wcschr(L"<>:\"/\\|?*", c) != nullptr)
                {
                    c = L'_';
                }
            }

            while (!name.empty() && (name.back() == L' ' || name.back() == L'.'))
            {
                name.pop_back();
            }

            if (name.size() > 100)
            {
                name.resize(100);
            }

            return name.empty() ? std::wstring{ L"Sequence" } : name;
        }

        bool IsDefaultTempo(seq::Sequence const& sequence) noexcept
        {
            return sequence.Tempo.size() == 1 && std::abs(sequence.Tempo[0].BeatsPerMinute - 120.0) < 0.0001 &&
                sequence.Meter.size() == 1 && sequence.Meter[0].Numerator == 4 && sequence.Meter[0].Denominator == 4;
        }
    }

    // ---------------------------------------------------------------- new and open

    void MainWindow::NewDocument() noexcept
    {
        try
        {
            seq::Sequence doc{};

            for (int scene = 0; scene < 4; ++scene)
            {
                doc.Scenes.push_back(seq::Scene{ seq::NewId(L"s"), std::wstring{}, nullptr });
            }

            auto track = seq::MakeTrack(doc, std::wstring{ res::GetString(L"DefaultTrackName") } + L" 1", false);
            track.Color = seq::TrackColorSwatches()[5];
            track.Destination.Channel = 0;
            track.Slots.resize(doc.Scenes.size());

            // An empty clip, ready to draw in, so the first thing on screen is somewhere to start.
            auto clip = seq::MakeNotesClip(std::wstring{ res::GetString(L"DefaultClipName") } + L" 1", seq::TicksPerQuarterNote * 16);
            clip.Origin = seq::ClipOrigin::Drawn;
            track.Timeline.push_back(seq::Placement{ clip.Id, 0, 0 });

            doc.Tracks.push_back(std::move(track));
            doc.Clips.push_back(std::move(clip));

            seq::NormalizeSequence(doc);
            ShowNewDocument(std::move(doc));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to start a new sequence.")
    }

    void MainWindow::OpenSample() noexcept
    {
        try
        {
            seq::SampleText text{};
            text.SequenceName = res::GetString(L"SampleSequenceName");
            text.SceneIntro = res::GetString(L"SampleSceneIntro");
            text.SceneGroove = res::GetString(L"SampleSceneGroove");
            text.SceneBreak = res::GetString(L"SampleSceneBreak");
            text.SceneDrop = res::GetString(L"SampleSceneDrop");
            text.SceneOutro = res::GetString(L"SampleSceneOutro");
            text.Drums = res::GetString(L"SampleDrums");
            text.Synths = res::GetString(L"SampleSynths");
            text.Bass = res::GetString(L"SampleBass");
            text.Pad = res::GetString(L"SamplePad");
            text.Arp = res::GetString(L"SampleArp");
            text.Texture = res::GetString(L"SampleTexture");
            text.Keys = res::GetString(L"SampleKeys");
            text.Rhodes = res::GetString(L"SampleRhodes");
            text.Organ = res::GetString(L"SampleOrgan");
            text.Lead = res::GetString(L"SampleLead");
            text.Chords = res::GetString(L"SampleChords");
            text.BeatA = res::GetString(L"SampleBeatA");
            text.BeatB = res::GetString(L"SampleBeatB");
            text.BeatFill = res::GetString(L"SampleBeatFill");
            text.BassLineA = res::GetString(L"SampleBassLineA");
            text.BassLineB = res::GetString(L"SampleBassLineB");
            text.PadIntro = res::GetString(L"SamplePadIntro");
            text.PadChords = res::GetString(L"SamplePadChords");
            text.PadSwell = res::GetString(L"SamplePadSwell");
            text.EuclidFive = res::GetString(L"SampleEuclidFive");
            text.EuclidSeven = res::GetString(L"SampleEuclidSeven");
            text.Wander = res::GetString(L"SampleWander");
            text.Drift = res::GetString(L"SampleDrift");
            text.Comping = res::GetString(L"SampleComping");
            text.OrganPads = res::GetString(L"SampleOrganPads");
            text.LeadHook = res::GetString(L"SampleLeadHook");
            text.Stabs = res::GetString(L"SampleStabs");
            text.TagFilterOpens = res::GetString(L"SampleTagFilterOpens");
            text.TagRetake = res::GetString(L"SampleTagRetake");

            // The synth when it's been found; otherwise the tracks get it once it is.
            seq::EndpointRef synth{};

            if (auto const found = m_directory->Resolve(seq::EndpointRef{ L"", m_directory->SynthEndpointId() }); found.has_value())
            {
                synth = seq::EndpointDirectory::MakeRef(found->Live);
            }

            ShowNewDocument(seq::MakeSampleSequence(text, synth));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open the sample sequence.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowNewDocument(seq::Sequence doc) noexcept
    {
        try
        {
            m_doc = std::move(doc);
            m_path.clear();
            m_dirty = false;
            m_readOnly = false;
            m_savedAt.reset();
            m_autosaved = false;
            m_undo.Clear();
            m_selectedTrackId.clear();
            m_selectedPlacement = SIZE_MAX;
            m_wantsDefaultDestination = true;
            ++m_version;

            if (m_engine != nullptr)
            {
                m_engine->Stop();
            }

            m_position = 0;
            m_scrollX = 0;
            m_scrollY = 0;

            RebuildLayout();
            Publish();
            OpenFirstClipInEditor();
            RebuildHeaders();

            // Put the synth on the new tracks straight away when it's already been found.
            RefreshEndpoints();

            UpdateTitle();
            UpdateSavedState();
            UpdateUndoButtons();
            UpdateTransport();
            UpdateDisplays(0);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show a new sequence.")
    }

    bool MainWindow::CanReplaceDocument() const noexcept
    {
        return m_path.empty() && !m_dirty;
    }

    void MainWindow::OpenFirstClipInEditor() noexcept
    {
        try
        {
            // The first clip on the first track that has one, selected and open in the editor.
            seq::ForEachTrack(m_doc, [&](seq::Track const& track, size_t)
            {
                if (track.IsFolder || track.Timeline.empty())
                {
                    return true;
                }

                m_selectedTrackId = track.Id;
                m_selectedPlacement = 0;
                OpenClipInEditor(track.Timeline.front().ClipId, track.Id);
                return false;
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open the first clip.")
    }

    _Use_decl_annotations_
    void MainWindow::OpenInNewWindow(std::wstring const& path, bool sample) noexcept
    {
        try
        {
            std::wstring self(MAX_PATH, L'\0');
            auto const length = ::GetModuleFileNameW(nullptr, self.data(), static_cast<DWORD>(self.size()));

            if (length == 0 || length >= self.size())
            {
                return;
            }

            self.resize(length);

            auto const arguments = sample ? std::wstring{ L"--sample" } : path.empty() ? std::wstring{} : L"\"" + path + L"\"";

            SHELLEXECUTEINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOASYNC;
            info.lpVerb = L"open";
            info.lpFile = self.c_str();
            info.lpParameters = arguments.empty() ? nullptr : arguments.c_str();
            info.nShow = SW_SHOWNORMAL;

            ::ShellExecuteExW(&info);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open another window.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNewClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (CanReplaceDocument())
            {
                NewDocument();
            }
            else
            {
                OpenInNewWindow(std::wstring{});
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to start a new sequence.")
    }

    _Use_decl_annotations_
    void MainWindow::OnOpenSampleClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (CanReplaceDocument())
            {
                OpenSample();
            }
            else
            {
                OpenInNewWindow(std::wstring{}, true);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open the sample sequence.")
    }

    _Use_decl_annotations_
    void MainWindow::OnOpenClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const path = ShowFileDialog(false, std::wstring{}, {
                { std::wstring{ res::GetString(L"FileTypeSequence") }, L"*.midisequence" },
                { std::wstring{ res::GetString(L"FileTypeAllMidi") }, L"*.midisequence;*.mid;*.midi;*.kar;*.rmi;*.smf;*.midi2" } });

            if (path.empty())
            {
                return;
            }

            auto const extension = std::filesystem::path{ path }.extension().wstring();

            if (_wcsicmp(extension.c_str(), L".midisequence") != 0)
            {
                if (_wcsicmp(extension.c_str(), L".midi2") == 0)
                {
                    ImportClipFile(path);
                }
                else
                {
                    ImportStandardMidiFile(path);
                }

                return;
            }

            if (CanReplaceDocument())
            {
                OpenFileAsync(path);
            }
            else
            {
                OpenInNewWindow(path);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open a sequence.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OpenFileAsync(std::wstring path)
    {
        auto strong = get_strong();

        try
        {
            auto const extension = std::filesystem::path{ path }.extension().wstring();

            if (_wcsicmp(extension.c_str(), L".midisequence") != 0)
            {
                // A MIDI file from Explorer: a new sequence made from it.
                if (_wcsicmp(extension.c_str(), L".midi2") == 0)
                {
                    ImportClipFile(path);
                }
                else
                {
                    ImportStandardMidiFile(path);
                }

                co_return;
            }

            std::vector<uint8_t> bytes{};
            bool read{ false };

            co_await ::midisequencer::RunOnBackgroundAsync([&]() { read = ReadAllBytes(path, bytes, MaximumSequenceFileBytes); });

            if (!read)
            {
                ShowMessage(res::FormatString(L"OpenFailedFormat", FileStem(path)));
                co_return;
            }

            std::string_view utf8{ reinterpret_cast<char const*>(bytes.data()), bytes.size() };

            // A byte order mark is allowed, and ignored.
            if (utf8.size() >= 3 && static_cast<uint8_t>(utf8[0]) == 0xEF && static_cast<uint8_t>(utf8[1]) == 0xBB && static_cast<uint8_t>(utf8[2]) == 0xBF)
            {
                utf8.remove_prefix(3);
            }

            OpenSequenceText(path, seq::FromUtf8(utf8));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open a sequence file.")
    }

    _Use_decl_annotations_
    bool MainWindow::OpenSequenceText(std::wstring const& path, std::wstring const& text) noexcept
    {
        try
        {
            seq::Sequence doc{};
            auto const result = seq::ReadSequenceJson(text, doc);

            if (!result.Succeeded())
            {
                ShowMessage(res::FormatString(L"OpenNotASequenceFormat", FileStem(path)));
                return false;
            }

            if (m_engine != nullptr)
            {
                m_engine->Stop();
            }

            if (doc.Name.empty())
            {
                doc.Name = FileStem(path);
            }

            m_doc = std::move(doc);
            m_path = path;
            m_dirty = false;
            m_readOnly = result.FromNewerVersion;
            m_savedAt.reset();
            m_autosaved = false;
            m_undo.Clear();
            m_wantsDefaultDestination = false;
            m_selectedTrackId.clear();
            m_selectedPlacement = SIZE_MAX;
            m_position = 0;
            m_scrollX = 0;
            m_scrollY = 0;
            ++m_version;

            CloseEditor();
            RebuildLayout();
            Publish();
            OpenFirstClipInEditor();
            RebuildHeaders();
            PrepareConnectionsAsync();

            UpdateTitle();
            UpdateSavedState();
            UpdateUndoButtons();
            UpdateDisplays(0);

            if (m_readOnly)
            {
                ShowMessage(res::GetString(L"OpenedNewerVersion"));
            }
            else if (result.SkippedItems > 0)
            {
                ShowMessage(res::FormatString(L"OpenedWithSkippedFormat", result.SkippedItems));
            }

            try
            {
                seq::AppSettings::Current().LastFolder(std::filesystem::path{ path }.parent_path().wstring());
            }
            catch (...)
            {
            }

            return true;
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to read a sequence.")

        return false;
    }

    // ---------------------------------------------------------------- import

    _Use_decl_annotations_
    void MainWindow::ImportStandardMidiFile(std::wstring const& path) noexcept
    {
        try
        {
            midifile::MidiSequence file{};
            auto const result = midifile::ReadStandardMidiFile(path, file);

            if (!result.Succeeded())
            {
                ShowMessage(res::FormatString(L"ImportFailedFormat", FileStem(path)));
                return;
            }

            auto imported = seq::ImportStandardMidiFile(file, std::filesystem::path{ path }.filename().wstring());

            if (imported.Tracks.empty())
            {
                ShowMessage(res::FormatString(L"ImportNothingFormat", FileStem(path)));
                return;
            }

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

            // A brand new sequence whose only track is the untouched starting one gives way to the file.
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

            auto const& swatches = seq::TrackColorSwatches();
            size_t colorIndex{ seq::CountTracks(m_doc) };

            for (auto& track : imported.Tracks)
            {
                track.Slots.resize(m_doc.Scenes.size());
                track.Color = swatches[colorIndex++ % swatches.size()];

                // Each track plays to the synth until it's given a device; the file names none.
                if (track.Destination.Endpoint.IsEmpty())
                {
                    if (auto const synth = m_directory->Resolve(seq::EndpointRef{ L"", m_directory->SynthEndpointId() }); synth.has_value())
                    {
                        track.Destination.Endpoint = seq::EndpointDirectory::MakeRef(synth->Live);
                    }
                }

                m_doc.Tracks.push_back(std::move(track));
            }

            for (auto& clip : imported.Clips)
            {
                seq::SortClip(clip);
                changes.push_back(seq::MakeClipPresenceChange(clip, true));
                m_doc.Clips.push_back(std::move(clip));
            }

            changes.push_back(seq::MakeTracksChange(tracksBefore, m_doc.Tracks));

            Commit(std::wstring{ res::GetString(L"UndoImport") }, std::move(changes));
            ShowMessage(res::FormatString(L"ImportedFormat", FileStem(path), imported.Tracks.size()));
            PrepareConnectionsAsync();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to import a Standard MIDI File.")
    }

    _Use_decl_annotations_
    void MainWindow::ImportClipFile(std::wstring const& path) noexcept
    {
        try
        {
            std::vector<uint8_t> bytes{};

            if (!ReadAllBytes(path, bytes, MaximumClipFileBytes))
            {
                ShowMessage(res::FormatString(L"ImportFailedFormat", FileStem(path)));
                return;
            }

            seq::ClipFile file{};
            auto const result = seq::ReadClipFile(bytes, file);

            if (!result.Succeeded())
            {
                ShowMessage(res::FormatString(L"ImportFailedFormat", FileStem(path)));
                return;
            }

            auto imported = seq::ImportClipFile(file);

            seq::ChangeList changes{};
            auto const tracksBefore = m_doc.Tracks;

            if (IsDefaultTempo(m_doc))
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

            auto clip = std::move(imported.Content);
            clip.Id = seq::NewId(L"c");
            clip.Name = imported.Name.empty() ? FileStem(path) : imported.Name;
            clip.Origin = seq::ClipOrigin::Imported;
            clip.OriginDetail = std::filesystem::path{ path }.filename().wstring();
            seq::SortClip(clip);

            auto track = seq::MakeTrack(m_doc, clip.Name, false);
            track.Startup = imported.Startup;
            track.Slots.resize(m_doc.Scenes.size());
            track.Timeline.push_back(seq::Placement{ clip.Id, 0, 0 });

            if (auto const synth = m_directory->Resolve(seq::EndpointRef{ L"", m_directory->SynthEndpointId() }); synth.has_value())
            {
                track.Destination.Endpoint = seq::EndpointDirectory::MakeRef(synth->Live);
            }

            m_doc.Tracks.push_back(std::move(track));
            changes.push_back(seq::MakeClipPresenceChange(clip, true));
            m_doc.Clips.push_back(std::move(clip));
            changes.push_back(seq::MakeTracksChange(tracksBefore, m_doc.Tracks));

            Commit(std::wstring{ res::GetString(L"UndoImport") }, std::move(changes));
            ShowMessage(res::FormatString(L"ImportedFormat", FileStem(path), 1));
            PrepareConnectionsAsync();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to import a MIDI 2.0 clip file.")
    }

    _Use_decl_annotations_
    void MainWindow::OnImportStandardMidiFileClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const path = ShowFileDialog(false, std::wstring{}, {
                { std::wstring{ res::GetString(L"FileTypeStandardMidiFile") }, L"*.mid;*.midi;*.kar;*.rmi;*.smf" } });

            if (!path.empty())
            {
                ImportStandardMidiFile(path);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to import a Standard MIDI File.")
    }

    _Use_decl_annotations_
    void MainWindow::OnImportClipFileClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const path = ShowFileDialog(false, std::wstring{}, {
                { std::wstring{ res::GetString(L"FileTypeClipFile") }, L"*.midi2" } });

            if (!path.empty())
            {
                ImportClipFile(path);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to import a MIDI 2.0 clip file.")
    }

    // ---------------------------------------------------------------- export

    _Use_decl_annotations_
    void MainWindow::OnExportStandardMidiFileClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const name = SafeFileName(m_doc.Name.empty() ? std::wstring{ res::GetString(L"UntitledSequence") } : m_doc.Name);
            auto const path = ShowFileDialog(true, name + L".mid", {
                { std::wstring{ res::GetString(L"FileTypeStandardMidiFile") }, L"*.mid" } });

            if (path.empty())
            {
                return;
            }

            seq::ClipFileText text{};
            text.SequenceName = m_doc.Name;

            if (m_doc.Provenance.has_value())
            {
                text.Composer = m_doc.Provenance->Author;
            }

            auto const exported = seq::ExportStandardMidiFile(m_doc, {}, text);

            if (!exported.Succeeded || !WriteAllBytes(path, exported.Bytes.data(), exported.Bytes.size()))
            {
                ShowMessage(res::FormatString(L"ExportFailedFormat", FileStem(path)));
                return;
            }

            if (exported.SkippedMessages > 0)
            {
                ShowMessage(res::FormatString(L"ExportedSkippedFormat", FileStem(path), exported.SkippedMessages));
            }
            else
            {
                ShowMessage(res::FormatString(L"ExportedFormat", FileStem(path)));
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to export a Standard MIDI File.")
    }

    _Use_decl_annotations_
    void MainWindow::OnExportClipFilesClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            // One clip file per track until the MIDI Container File is published (design section 11).
            auto const folder = ShowFileDialog(false, std::wstring{}, {}, true);

            if (folder.empty())
            {
                return;
            }

            seq::ClipFileText text{};
            text.SequenceName = m_doc.Name;

            if (m_doc.Provenance.has_value())
            {
                text.Composer = m_doc.Provenance->Author;
            }

            auto const stem = SafeFileName(m_doc.Name.empty() ? std::wstring{ res::GetString(L"UntitledSequence") } : m_doc.Name);
            size_t written{ 0 };
            size_t failed{ 0 };

            seq::ForEachTrack(m_doc, [&](seq::Track const& track, size_t)
            {
                if (track.IsFolder || track.Timeline.empty())
                {
                    return true;
                }

                auto const clip = seq::ExportTrackToClipFile(m_doc, track, text);
                auto const bytes = seq::WriteClipFile(clip);
                auto const path = (std::filesystem::path{ folder } / (stem + L" - " + SafeFileName(track.Name) + L".midi2")).wstring();

                if (WriteAllBytes(path, bytes.data(), bytes.size()))
                {
                    ++written;
                }
                else
                {
                    ++failed;
                }

                return true;
            });

            if (failed > 0)
            {
                ShowMessage(res::FormatString(L"ExportClipFilesFailedFormat", failed));
            }
            else
            {
                ShowMessage(res::FormatString(L"ExportedClipFilesFormat", written));
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to export MIDI 2.0 clip files.")
    }

    // ---------------------------------------------------------------- save

    _Use_decl_annotations_
    void MainWindow::OnSaveClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        SaveAsync(false);
    }

    _Use_decl_annotations_
    void MainWindow::OnSaveAsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        SaveAsync(true);
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::SaveAsync(bool chooseName)
    {
        auto strong = get_strong();

        try
        {
            auto path = m_path;

            // A file from a newer version is never saved over: its settings this version doesn't
            // know about would be lost. A copy can be saved.
            if (m_readOnly)
            {
                chooseName = true;
            }

            if (chooseName || path.empty())
            {
                auto const name = SafeFileName(m_doc.Name.empty() ? std::wstring{ res::GetString(L"UntitledSequence") } : m_doc.Name);

                path = ShowFileDialog(true, name + L".midisequence", {
                    { std::wstring{ res::GetString(L"FileTypeSequence") }, L"*.midisequence" } });

                if (path.empty())
                {
                    m_closeConfirmed = false;
                    co_return;
                }
            }

            if (m_doc.Name.empty())
            {
                m_doc.Name = FileStem(path);
            }

            auto const text = seq::ToUtf8(seq::WriteSequenceJson(m_doc));
            bool saved{ false };

            co_await ::midisequencer::RunOnBackgroundAsync([&]() { saved = WriteAllBytes(path, text.data(), text.size()); });

            if (!saved)
            {
                m_closeConfirmed = false;
                ShowMessage(res::FormatString(L"SaveFailedFormat", FileStem(path)));
                co_return;
            }

            m_path = path;
            m_dirty = false;
            m_readOnly = false;
            m_savedAt = std::chrono::system_clock::now();
            m_autosaved = false;

            UpdateTitle();
            UpdateSavedState();

            try
            {
                seq::AppSettings::Current().LastFolder(std::filesystem::path{ path }.parent_path().wstring());
            }
            catch (...)
            {
            }

            if (m_closeConfirmed)
            {
                Close();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to save the sequence.")
    }

    winrt::fire_and_forget MainWindow::AutosaveAsync()
    {
        auto strong = get_strong();

        try
        {
            // Only a sequence that already has a file saves itself. A new one asks for a name first.
            if (m_path.empty() || !m_dirty || m_readOnly || m_closing)
            {
                co_return;
            }

            auto const path = m_path;
            auto const version = m_version;
            auto const text = seq::ToUtf8(seq::WriteSequenceJson(m_doc));
            bool saved{ false };

            co_await ::midisequencer::RunOnBackgroundAsync([&]() { saved = WriteAllBytes(path, text.data(), text.size()); });

            if (saved && path == m_path)
            {
                // Edits made while it was saving still need saving.
                m_dirty = version != m_version;
                m_savedAt = std::chrono::system_clock::now();
                m_autosaved = true;
                UpdateTitle();
                UpdateSavedState();

                if (m_dirty)
                {
                    m_autosaveTimer.Start();
                }
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to save the sequence automatically.")
    }

    void MainWindow::UpdateSavedState() noexcept
    {
        try
        {
            std::wstring_view key{};

            if (m_readOnly)
            {
                key = L"ChipReadOnly";
            }
            else if (m_dirty)
            {
                key = m_path.empty() ? L"ChipNotSaved" : L"ChipSaving";
            }
            else
            {
                key = m_path.empty() ? L"ChipNotSaved" : L"ChipSaved";
            }

            SavedChipText().Text(res::GetString(key));

            auto const saved = !m_dirty && !m_path.empty() && !m_readOnly;

            SavedChipShape().Fill(ThemeBrush(saved ? L"SeqChipOnBrush" : L"SeqChipBrush"));
            SavedChipShape().Stroke(ThemeBrush(saved ? L"SeqChipOnStrokeBrush" : L"SeqStrokeStrongBrush"));

            auto const textBrush = saved ? ThemeBrush(L"SeqPlayBrush") : media::Brush{ BrushFor(m_palette.Text2) };

            if (textBrush != nullptr)
            {
                SavedChipText().Foreground(textBrush);
                SavedChipIcon().Foreground(textBrush);
            }

            SavedChipIcon().Glyph(saved ? L"\uE73E" : m_readOnly ? L"\uE72E" : L"\uE7C3");

            if (m_savedAt.has_value())
            {
                auto const time = std::chrono::system_clock::to_time_t(*m_savedAt);
                SYSTEMTIME local{};
                FILETIME file{};
                ULARGE_INTEGER value{};
                value.QuadPart = (static_cast<uint64_t>(time) + 11644473600ull) * 10000000ull;
                file.dwLowDateTime = value.LowPart;
                file.dwHighDateTime = value.HighPart;
                FILETIME localFile{};
                ::FileTimeToLocalFileTime(&file, &localFile);
                ::FileTimeToSystemTime(&localFile, &local);

                wchar_t clock[64]{};
                ::GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &local, nullptr, clock, 64);

                SavedStatusText().Text(res::FormatString(m_autosaved ? L"AutosavedAtFormat" : L"SavedAtFormat", std::wstring{ clock }));
            }
            else
            {
                SavedStatusText().Text(L"");
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show whether the sequence is saved.")
    }

    // ---------------------------------------------------------------- dialogs

    _Use_decl_annotations_
    std::wstring MainWindow::ShowFileDialog(bool save, std::wstring const& defaultName, std::vector<std::pair<std::wstring, std::wstring>> const& filters, bool folders) noexcept
    {
        // The Win32 dialog, not the WinRT picker: the picker does nothing in an elevated process
        // or over an open ContentDialog, which is how it has failed silently in this family before.
        try
        {
            wil::com_ptr<IFileDialog> dialog{};

            if (save)
            {
                dialog = wil::CoCreateInstance<IFileSaveDialog>(CLSID_FileSaveDialog, CLSCTX_INPROC_SERVER).query<IFileDialog>();
            }
            else
            {
                dialog = wil::CoCreateInstance<IFileOpenDialog>(CLSID_FileOpenDialog, CLSCTX_INPROC_SERVER).query<IFileDialog>();
            }

            FILEOPENDIALOGOPTIONS options{};
            dialog->GetOptions(&options);
            options |= FOS_FORCEFILESYSTEM;

            if (folders)
            {
                options |= FOS_PICKFOLDERS;
            }

            dialog->SetOptions(options);

            std::vector<COMDLG_FILTERSPEC> specs{};

            for (auto const& [name, pattern] : filters)
            {
                specs.push_back(COMDLG_FILTERSPEC{ name.c_str(), pattern.c_str() });
            }

            if (!specs.empty())
            {
                dialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
                dialog->SetFileTypeIndex(1);
            }

            if (save && !defaultName.empty())
            {
                dialog->SetFileName(defaultName.c_str());

                auto const extension = std::filesystem::path{ defaultName }.extension().wstring();

                if (extension.size() > 1)
                {
                    dialog->SetDefaultExtension(extension.c_str() + 1);
                }
            }

            auto folder = seq::AppSettings::Current().LastFolder();

            if (folder.empty() || !std::filesystem::exists(folder))
            {
                folder = DocumentsFolder();
            }

            if (!folder.empty())
            {
                wil::com_ptr<IShellItem> start{};

                if (SUCCEEDED(::SHCreateItemFromParsingName(folder.c_str(), nullptr, IID_PPV_ARGS(&start))))
                {
                    dialog->SetFolder(start.get());
                }
            }

            if (FAILED(dialog->Show(WindowHandle())))
            {
                return {};
            }

            wil::com_ptr<IShellItem> item{};

            if (FAILED(dialog->GetResult(&item)) || item == nullptr)
            {
                return {};
            }

            wil::unique_cotaskmem_string path{};

            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) || path == nullptr)
            {
                return {};
            }

            return std::wstring{ path.get() };
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show a file dialog.")

        return {};
    }

    std::wstring MainWindow::DocumentsFolder() noexcept
    {
        try
        {
            wil::unique_cotaskmem_string documents{};

            if (FAILED(::SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents)))
            {
                return {};
            }

            auto folder = std::filesystem::path{ documents.get() } / L"MIDI Sequences";
            std::error_code error{};
            std::filesystem::create_directories(folder, error);

            return folder.wstring();
        }
        catch (...)
        {
            return {};
        }
    }

    // ---------------------------------------------------------------- edits and undo

    _Use_decl_annotations_
    void MainWindow::Commit(std::wstring const& name, seq::ChangeList changes)
    {
        m_undo.Commit(name, std::move(changes));
        DocumentChanged();
    }

    _Use_decl_annotations_
    void MainWindow::EditTracks(std::wstring const& name, std::function<void(seq::Sequence&)> const& edit)
    {
        try
        {
            auto const before = m_doc.Tracks;
            edit(m_doc);

            seq::ChangeList changes{};
            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));
            Commit(name, std::move(changes));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the tracks.")
    }

    void MainWindow::DocumentChanged() noexcept
    {
        try
        {
            m_dirty = true;
            m_wantsDefaultDestination = false;
            ++m_version;

            m_renderer.InvalidateCaches();
            RebuildLayout();
            Publish();
            RefreshEditor();

            // Later, not now: an edit made from one of the inspector's own lists would otherwise
            // replace that list's items while it's still raising its selection event.
            DispatcherQueue().TryEnqueue([weak = get_weak()]()
            {
                if (auto strong = weak.get())
                {
                    strong->RefreshInspector();
                }
            });

            UpdateTitle();
            UpdateSavedState();
            UpdateUndoButtons();
            UpdateStatusBar();

            if (m_autosaveTimer != nullptr && !m_path.empty() && !m_readOnly)
            {
                m_autosaveTimer.Stop();
                m_autosaveTimer.Start();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show a change to the sequence.")
    }

    void MainWindow::Publish() noexcept
    {
        try
        {
            if (m_engine != nullptr)
            {
                m_engine->SetSequence(std::make_shared<seq::Sequence const>(m_doc));
            }

            UpdateEchoRoutes();
            PrepareConnectionsAsync();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to hand the sequence to the engine.")
    }

    void MainWindow::Undo() noexcept
    {
        try
        {
            if (m_undo.Undo(m_doc))
            {
                m_roll.ClearSelection();
                m_selectedPlacement = SIZE_MAX;
                DocumentChanged();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to undo.")
    }

    void MainWindow::Redo() noexcept
    {
        try
        {
            if (m_undo.Redo(m_doc))
            {
                m_roll.ClearSelection();
                m_selectedPlacement = SIZE_MAX;
                DocumentChanged();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to redo.")
    }

    void MainWindow::UpdateUndoButtons() noexcept
    {
        try
        {
            UndoButton().IsEnabled(m_undo.CanUndo());
            RedoButton().IsEnabled(m_undo.CanRedo());

            auto const undoName = m_undo.UndoName();
            auto const redoName = m_undo.RedoName();

            controls::ToolTipService::SetToolTip(UndoButton(), winrt::box_value(undoName.empty()
                ? res::GetString(L"UndoNothing") : res::FormatString(L"UndoFormat", undoName)));
            controls::ToolTipService::SetToolTip(RedoButton(), winrt::box_value(redoName.empty()
                ? res::GetString(L"RedoNothing") : res::FormatString(L"RedoFormat", redoName)));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to update the undo buttons.")
    }

    _Use_decl_annotations_
    void MainWindow::OnUndoClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        Undo();
    }

    _Use_decl_annotations_
    void MainWindow::OnRedoClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        Redo();
    }
}
