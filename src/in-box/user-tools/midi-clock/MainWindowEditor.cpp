// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "StringResources.h"

namespace native = ::midiclock;
namespace res = ::midiclock::resources;

namespace winrt::midiclock::implementation
{
    namespace
    {
        std::wstring Trimmed(_In_ winrt::hstring const& value) noexcept
        {
            try
            {
                std::wstring text{ value };

                auto const first = text.find_first_not_of(L" \t\r\n");

                if (first == std::wstring::npos)
                {
                    return {};
                }

                auto const last = text.find_last_not_of(L" \t\r\n");

                return text.substr(first, last - first + 1);
            }
            catch (...)
            {
                return {};
            }
        }

        // The rates a clock divider normally offers. Everything here is exact, because the app
        // is the clock source rather than a divider counting somebody else's pulses.
        struct ClockRatioPreset
        {
            int32_t Numerator;
            int32_t Denominator;
            wchar_t const* ResourceKey;
        };

        constexpr ClockRatioPreset ClockRatioPresets[]
        {
            { 1, 8, L"ClockRatioEighthSpeed" },
            { 1, 4, L"ClockRatioQuarterSpeed" },
            { 1, 3, L"ClockRatioThirdSpeed" },
            { 1, 2, L"ClockRatioHalfSpeed" },
            { 2, 3, L"ClockRatioDotted" },
            { 1, 1, L"ClockRatioNormal" },
            { 3, 2, L"ClockRatioTriplet" },
            { 2, 1, L"ClockRatioDoubleSpeed" },
            { 3, 1, L"ClockRatioTripleSpeed" },
            { 4, 1, L"ClockRatioQuadrupleSpeed" },
            { 8, 1, L"ClockRatioEightTimesSpeed" }
        };

        constexpr int32_t NormalClockRatioIndex = 5;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::ShowEditorAsync(std::wstring id)
    {
        auto strong = get_strong();

        try
        {
            if (m_editorOpen || !m_initialized)
            {
                co_return;
            }

            auto& store = native::ClockStore::Current();

            auto const isNew = id.empty();

            native::ClockDefinition definition{};

            if (!isNew)
            {
                auto const existing = store.Find(id);

                if (existing == nullptr)
                {
                    co_return;
                }

                definition = *existing;
            }

            m_editingId = definition.Id;
            m_editorOpen = true;

            EditDialog().Title(winrt::box_value(
                res::GetString(isNew ? L"EditDialogTitleNew" : L"EditDialogTitleEdit")));

            // there is nothing to delete until the clock has been saved once
            EditDialog().SecondaryButtonText(isNew ? winrt::hstring{} : res::GetString(L"EditDialogDelete"));
            EditDialog().XamlRoot(RootGrid().XamlRoot());

            PopulateEditor(definition);

            auto const result = co_await EditDialog().ShowAsync();

            m_editorOpen = false;

            if (result == controls::ContentDialogResult::Primary)
            {
                auto updated = ReadEditor();

                updated.Id = definition.Id;
                updated.PulsesPerQuarterNote = definition.PulsesPerQuarterNote;
                updated.DisplayOrder = definition.DisplayOrder;

                auto const savedId = store.Upsert(updated);

                if (savedId.empty())
                {
                    ReportStoreError();
                }
                else
                {
                    if (!store.Save())
                    {
                        ReportStoreError();
                    }

                    RebuildTiles();
                    UpdateEmptyState();

                    // a clock that is already running follows the new tempo and shaping rather
                    // than having to be stopped and started again
                    m_engine.SetBeatsPerMinute(savedId, updated.BeatsPerMinute);
                    m_engine.SetClockRatio(savedId, updated.ClockRatioNumerator, updated.ClockRatioDenominator);
                    m_engine.SetSwingPercent(savedId, updated.SwingPercent);
                }
            }
            else if (result == controls::ContentDialogResult::Secondary && !definition.Id.empty())
            {
                auto const deletingId = definition.Id;

                if (m_engine.IsRunning(deletingId))
                {
                    UpdateStatus(res::GetString(L"StatusStopping"));

                    co_await native::RunOnBackgroundAsync([strong, deletingId]()
                        {
                            strong->m_engine.Stop(deletingId);
                        });
                }

                store.Remove(deletingId);

                if (!store.Save())
                {
                    ReportStoreError();
                }

                RebuildTiles();
                UpdateEmptyState();

                UpdateStatus(res::GetString(L"StatusClockRemoved"));
            }

            m_editingId.clear();

            // the list was frozen while the dialog was up
            RefreshEndpointList();
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to edit the clock.")
    }

    _Use_decl_annotations_
    void MainWindow::PopulateEditor(native::ClockDefinition const& definition) noexcept
    {
        try
        {
            EditNameTextBox().Text(winrt::hstring{ definition.Name });

            SetEditorTempo(definition.BeatsPerMinute);

            int32_t endpointIndex{ -1 };

            if (m_endpoints != nullptr)
            {
                for (uint32_t index = 0; index < m_endpoints.Size(); index++)
                {
                    if (midiapp::EndpointIdsMatch(
                        m_endpoints.GetAt(index).EndpointDeviceId(),
                        winrt::hstring{ definition.EndpointDeviceId }))
                    {
                        endpointIndex = static_cast<int32_t>(index);
                        break;
                    }
                }
            }

            EditEndpointComboBox().SelectedIndex(endpointIndex);

            RefreshEditorGroupList(definition.GroupIndex);

            EditSendStartStopToggle().IsOn(definition.SendStartStop);

            {
                auto const previous = m_suppressTimingHandlers;
                m_suppressTimingHandlers = true;

                if (m_clockKinds == nullptr)
                {
                    m_clockKinds = winrt::single_threaded_observable_vector<appshared::NamedChoice>();

                    m_clockKinds.Append(winrt::make<appshared::implementation::NamedChoice>(
                        res::GetString(L"ClockKindBeatClock"), static_cast<int32_t>(native::ClockKind::BeatClock)));
                    m_clockKinds.Append(winrt::make<appshared::implementation::NamedChoice>(
                        res::GetString(L"ClockKindTimeCode"), static_cast<int32_t>(native::ClockKind::TimeCode)));

                    EditKindComboBox().ItemsSource(m_clockKinds);
                }

                if (m_frameRates == nullptr)
                {
                    m_frameRates = winrt::single_threaded_observable_vector<appshared::NamedChoice>();

                    for (auto const key : { L"FrameRate24", L"FrameRate25", L"FrameRate2997Drop", L"FrameRate30" })
                    {
                        m_frameRates.Append(winrt::make<appshared::implementation::NamedChoice>(
                            res::GetString(key), static_cast<int32_t>(m_frameRates.Size())));
                    }

                    EditFrameRateComboBox().ItemsSource(m_frameRates);
                }

                if (m_clockRatios == nullptr)
                {
                    m_clockRatios = winrt::single_threaded_observable_vector<appshared::NamedChoice>();

                    for (int32_t index = 0; index < static_cast<int32_t>(std::size(ClockRatioPresets)); index++)
                    {
                        m_clockRatios.Append(winrt::make<appshared::implementation::NamedChoice>(
                            res::GetString(ClockRatioPresets[index].ResourceKey), index));
                    }

                    EditClockRatioComboBox().ItemsSource(m_clockRatios);
                }

                if (m_swingSubdivisions == nullptr)
                {
                    m_swingSubdivisions = winrt::single_threaded_observable_vector<appshared::NamedChoice>();

                    m_swingSubdivisions.Append(winrt::make<appshared::implementation::NamedChoice>(
                        res::GetString(L"SwingSubdivisionEighths"), 2));
                    m_swingSubdivisions.Append(winrt::make<appshared::implementation::NamedChoice>(
                        res::GetString(L"SwingSubdivisionSixteenths"), 4));

                    EditSwingSubdivisionComboBox().ItemsSource(m_swingSubdivisions);
                }

                int32_t ratioIndex{ NormalClockRatioIndex };

                for (int32_t index = 0; index < static_cast<int32_t>(std::size(ClockRatioPresets)); index++)
                {
                    if (ClockRatioPresets[index].Numerator == definition.ClockRatioNumerator &&
                        ClockRatioPresets[index].Denominator == definition.ClockRatioDenominator)
                    {
                        ratioIndex = index;
                        break;
                    }
                }

                EditClockRatioComboBox().SelectedIndex(ratioIndex);

                EditSwingSlider().Value(std::clamp(
                    definition.SwingPercent, native::MinimumSwingPercent, native::MaximumSwingPercent));

                EditSwingSubdivisionComboBox().SelectedIndex(definition.SwingSubdivision >= 4 ? 1 : 0);

                EditOffsetNumberBox().Value(std::clamp(
                    definition.OffsetMilliseconds,
                    -native::MaximumOffsetMilliseconds,
                    native::MaximumOffsetMilliseconds));

                EditKindComboBox().SelectedIndex(
                    definition.Kind == native::ClockKind::TimeCode ? 1 : 0);

                EditFrameRateComboBox().SelectedIndex(static_cast<int32_t>(definition.FrameRate));

                EditStartTimeCodeTextBox().Text(winrt::hstring{
                    midiapp::FormatPosition(definition.StartTimeCode, definition.FrameRate) });

                EditSendFullFrameToggle().IsOn(definition.SendFullFrameMessages);

                m_suppressTimingHandlers = previous;
            }

            RefreshEditorKindVisibility();

            // Open it when there is something in it worth seeing, so a plain clock keeps the
            // short dialog and a shaped one does not hide what it is doing.
            EditTimingExpander().IsExpanded(
                definition.ClockRatioNumerator != definition.ClockRatioDenominator ||
                definition.SwingPercent > native::MinimumSwingPercent ||
                definition.OffsetMilliseconds != 0.0);

            RefreshEditorTimingCaptions();

            m_tapTempo.Reset();
            TapTempoHintTextBlock().Text(res::GetString(L"TapTempoHint"));

            EditInfoBar().IsOpen(false);
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to fill in the clock editor.")
    }

    _Use_decl_annotations_
    void MainWindow::RefreshEditorGroupList(int32_t desiredGroupIndex) noexcept
    {
        try
        {
            if (m_groups == nullptr)
            {
                return;
            }

            m_groups.Clear();

            m_groups.Append(winrt::make<appshared::implementation::NamedChoice>(
                res::GetString(L"GroupAllDeclared"), native::AllDeclaredGroups));

            std::optional<midi2enum::MidiEndpointDeviceInformation> device{};

            auto const endpointIndex = EditEndpointComboBox().SelectedIndex();

            if (endpointIndex >= 0 && static_cast<size_t>(endpointIndex) < m_endpointDevices.size())
            {
                device = m_endpointDevices[static_cast<size_t>(endpointIndex)];
            }

            // A device that declares nothing comes back with all sixteen set, so the picker
            // stays usable rather than empty.
            auto const declared = device.has_value()
                ? midiapp::DeclaredGroups(device.value())
                : std::array<bool, 16>{ true, true, true, true, true, true, true, true,
                                        true, true, true, true, true, true, true, true };

            for (uint8_t index = 0; index < 16; index++)
            {
                if (!declared[index])
                {
                    continue;
                }

                auto const blockName = device.has_value()
                    ? midiapp::DescribeGroup(device.value(), index)
                    : winrt::hstring{};

                auto const groupNumber = static_cast<int32_t>(index) + 1;

                auto const label = blockName.empty()
                    ? res::FormatString(L"GroupChoiceFormat", groupNumber)
                    : res::FormatString(L"GroupChoiceWithNameFormat", groupNumber, blockName);

                m_groups.Append(winrt::make<appshared::implementation::NamedChoice>(
                    label, static_cast<int32_t>(index)));
            }

            int32_t selectedIndex{ -1 };

            for (uint32_t index = 0; index < m_groups.Size(); index++)
            {
                if (m_groups.GetAt(index).Value() == desiredGroupIndex)
                {
                    selectedIndex = static_cast<int32_t>(index);
                    break;
                }
            }

            // the group the clock used is gone, so fall back to the endpoint's first group
            if (selectedIndex < 0)
            {
                selectedIndex = m_groups.Size() > 1 ? 1 : 0;
            }

            EditGroupComboBox().SelectedIndex(selectedIndex);
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to fill in the group list.")
    }

    native::ClockDefinition MainWindow::ReadEditor() noexcept
    {
        native::ClockDefinition definition{};

        try
        {
            definition.Name = Trimmed(EditNameTextBox().Text());

            auto const tempo = EditTempoNumberBox().Value();

            definition.BeatsPerMinute = std::isfinite(tempo)
                ? std::clamp(tempo, native::MinimumBeatsPerMinute, native::MaximumBeatsPerMinute)
                : native::DefaultBeatsPerMinute;

            auto const endpointIndex = EditEndpointComboBox().SelectedIndex();

            if (endpointIndex >= 0 && static_cast<size_t>(endpointIndex) < m_endpointDevices.size())
            {
                auto const& device = m_endpointDevices[static_cast<size_t>(endpointIndex)];

                definition.EndpointDeviceId = std::wstring{ device.EndpointDeviceId() };
                definition.EndpointName = std::wstring{ device.Name() };
            }

            auto const groupIndex = EditGroupComboBox().SelectedIndex();

            if (m_groups != nullptr && groupIndex >= 0 &&
                static_cast<uint32_t>(groupIndex) < m_groups.Size())
            {
                definition.GroupIndex = m_groups.GetAt(static_cast<uint32_t>(groupIndex)).Value();
            }

            definition.SendStartStop = EditSendStartStopToggle().IsOn();

            auto const ratioIndex = EditClockRatioComboBox().SelectedIndex();

            if (ratioIndex >= 0 && ratioIndex < static_cast<int32_t>(std::size(ClockRatioPresets)))
            {
                definition.ClockRatioNumerator = ClockRatioPresets[ratioIndex].Numerator;
                definition.ClockRatioDenominator = ClockRatioPresets[ratioIndex].Denominator;
            }

            auto const swing = EditSwingSlider().Value();

            definition.SwingPercent = std::isfinite(swing)
                ? std::clamp(swing, native::MinimumSwingPercent, native::MaximumSwingPercent)
                : native::DefaultSwingPercent;

            definition.SwingSubdivision = EditSwingSubdivisionComboBox().SelectedIndex() == 1 ? 4 : 2;

            auto const offset = EditOffsetNumberBox().Value();

            definition.OffsetMilliseconds = std::isfinite(offset)
                ? std::clamp(offset, -native::MaximumOffsetMilliseconds, native::MaximumOffsetMilliseconds)
                : 0.0;

            definition.Kind = SelectedEditorKind();
            definition.FrameRate = SelectedEditorFrameRate();
            definition.SendFullFrameMessages = EditSendFullFrameToggle().IsOn();

            if (!midiapp::TryParsePosition(
                std::wstring{ EditStartTimeCodeTextBox().Text() }, definition.FrameRate, definition.StartTimeCode))
            {
                definition.StartTimeCode = midiapp::MidiTimeCodePosition{};
            }

            definition.StartTimeCode = midiapp::ClampPosition(definition.StartTimeCode, definition.FrameRate);
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to read the clock editor.")

        return definition;
    }

    native::ClockKind MainWindow::SelectedEditorKind() noexcept
    {
        try
        {
            return EditKindComboBox().SelectedIndex() == 1
                ? native::ClockKind::TimeCode
                : native::ClockKind::BeatClock;
        }
        catch (...)
        {
            return native::ClockKind::BeatClock;
        }
    }

    midiapp::MidiTimeCodeFrameRate MainWindow::SelectedEditorFrameRate() noexcept
    {
        try
        {
            return midiapp::FrameRateFromValue(EditFrameRateComboBox().SelectedIndex());
        }
        catch (...)
        {
            return midiapp::MidiTimeCodeFrameRate::Frames30;
        }
    }

    void MainWindow::RefreshEditorKindVisibility() noexcept
    {
        try
        {
            auto const isTimeCode = SelectedEditorKind() == native::ClockKind::TimeCode;

            auto const beatOnly = isTimeCode ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
            auto const timeCodeOnly = isTimeCode ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;

            EditBeatClockPanel().Visibility(beatOnly);
            EditSendStartStopToggle().Visibility(beatOnly);
            EditBeatShapingPanel().Visibility(beatOnly);
            EditTimeCodePanel().Visibility(timeCodeOnly);

            // The offset is the only thing in the expander that applies to both, so the heading
            // has to stop promising a divider and swing when there are none.
            EditTimingExpander().Header(winrt::box_value(
                res::GetString(isTimeCode ? L"EditOffsetOnlyExpanderHeader" : L"EditTimingExpanderHeader")));

            if (isTimeCode)
            {
                EditTimingExpander().IsExpanded(EditOffsetNumberBox().Value() != 0.0);
            }
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to update the clock editor for its kind.")
    }

    void MainWindow::RefreshEditorTimingCaptions() noexcept
    {
        try
        {
            auto const ratioIndex = EditClockRatioComboBox().SelectedIndex();

            if (ratioIndex >= 0 && ratioIndex < static_cast<int32_t>(std::size(ClockRatioPresets)))
            {
                auto const& preset = ClockRatioPresets[ratioIndex];

                auto const tempo = EditTempoNumberBox().Value();

                auto const effective = (std::isfinite(tempo) ? tempo : native::DefaultBeatsPerMinute) *
                    static_cast<double>(preset.Numerator) / static_cast<double>(preset.Denominator);

                EditClockRatioCaption().Text(
                    res::FormatString(L"ClockRatioEffectiveTempoFormat", std::format(L"{:.1f}", effective)));
            }

            auto const swing = EditSwingSlider().Value();

            EditSwingCaption().Text(swing <= native::MinimumSwingPercent
                ? res::GetString(L"SwingStraight")
                : res::FormatString(L"SwingValueFormat", std::format(L"{:.1f}", swing)));

            auto const frameRate = SelectedEditorFrameRate();

            midiapp::MidiTimeCodePosition position{};

            if (midiapp::TryParsePosition(std::wstring{ EditStartTimeCodeTextBox().Text() }, frameRate, position))
            {
                auto const clamped = midiapp::ClampPosition(position, frameRate);

                // A drop frame minute has no frame 0 or 1, so say what will actually be sent
                // rather than silently changing what was typed.
                EditStartTimeCodeCaption().Text(res::FormatString(
                    L"StartTimeCodeReadingFormat", midiapp::FormatPosition(clamped, frameRate)));
            }
            else
            {
                EditStartTimeCodeCaption().Text(res::GetString(L"StartTimeCodeNotUnderstood"));
            }
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to update the timing captions.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditKindSelectionChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        try
        {
            RefreshEditorKindVisibility();
            RefreshEditorTimingCaptions();
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to change the clock kind.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditFrameRateSelectionChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        try
        {
            RefreshEditorTimingCaptions();
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to change the frame rate.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditStartTimeCodeChanged(
        foundation::IInspectable const&,
        controls::TextChangedEventArgs const&)
    {
        try
        {
            RefreshEditorTimingCaptions();
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to read the starting timecode.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditClockRatioSelectionChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        try
        {
            RefreshEditorTimingCaptions();
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to change the clock ratio.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditSwingValueChanged(
        foundation::IInspectable const&,
        controls::Primitives::RangeBaseValueChangedEventArgs const&)
    {
        try
        {
            RefreshEditorTimingCaptions();
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to change the swing.")
    }

    _Use_decl_annotations_
    void MainWindow::SetEditorTempo(double beatsPerMinute) noexcept
    {
        try
        {
            auto const value = std::clamp(
                beatsPerMinute, native::MinimumBeatsPerMinute, native::MaximumBeatsPerMinute);

            auto const previous = m_suppressTempoHandlers;
            m_suppressTempoHandlers = true;

            EditTempoNumberBox().Value(value);
            EditTempoSlider().Value(value);

            m_suppressTempoHandlers = previous;
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to set the tempo controls.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditTempoValueChanged(
        controls::NumberBox const&,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        try
        {
            if (m_suppressTempoHandlers)
            {
                return;
            }

            auto const value = args.NewValue();

            if (!std::isfinite(value))
            {
                // the box was cleared; put the slider's value back rather than leaving it empty
                SetEditorTempo(EditTempoSlider().Value());
                return;
            }

            SetEditorTempo(value);
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to change the tempo.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditTempoSliderChanged(
        foundation::IInspectable const&,
        controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        try
        {
            if (m_suppressTempoHandlers)
            {
                return;
            }

            SetEditorTempo(args.NewValue());
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to change the tempo.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditEndpointSelectionChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (!m_editorOpen)
            {
                return;
            }

            // a different endpoint declares different groups, so fall back to its first one
            RefreshEditorGroupList(0);

            EditInfoBar().IsOpen(false);
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to update the group list.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTapTempoClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const tempo = m_tapTempo.Tap();

            if (tempo.has_value())
            {
                SetEditorTempo(tempo.value());

                auto count = m_tapTempo.TapCount();

                TapTempoHintTextBlock().Text(res::FormatString(L"TapTempoCountFormat", count));
            }
            else
            {
                TapTempoHintTextBlock().Text(res::GetString(L"TapTempoKeepTapping"));
            }
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to read the tapped tempo.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditDialogSaveClick(
        controls::ContentDialog const&,
        controls::ContentDialogButtonClickEventArgs const& args)
    {
        try
        {
            if (EditEndpointComboBox().SelectedIndex() < 0)
            {
                // keep the dialog up rather than saving a clock with nowhere to send
                args.Cancel(true);

                EditInfoBar().Title(res::GetString(L"EditNoEndpointTitle"));
                EditInfoBar().Message(res::GetString(L"EditNoEndpointMessage"));
                EditInfoBar().IsOpen(true);
            }
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to validate the clock editor.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditDialogDeleteClick(
        controls::ContentDialog const&,
        controls::ContentDialogButtonClickEventArgs const&)
    {
        // handled where the dialog result is read, so the removal and the save stay together
    }
}
