// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Starter patterns and hearing steps (design section 27). Templates in a pattern's header lists
// drum machine style starters for a drum pattern, and basslines and sequences in a chosen key for
// a melodic one. When the pattern already has steps, it asks whether to replace them or to add the
// template on top. Hear steps plays a step's sound as it's turned on, so a drum can be heard while
// it's being added.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "PatternTemplates.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        // How long a step heard while editing sounds.
        constexpr uint64_t StepPreviewMilliseconds = 250;

        // The same names the chance melody's key list uses.
        constexpr std::array<wchar_t const*, 12> KeyNames{ L"C", L"C#", L"D", L"D#", L"E", L"F", L"F#", L"G", L"G#", L"A", L"A#", L"B" };

        wchar_t const* GroupKey(seq::PatternTemplateGroup group) noexcept
        {
            switch (group)
            {
            case seq::PatternTemplateGroup::Electronic: return L"PatternTemplateGroupElectronic";
            case seq::PatternTemplateGroup::HipHop: return L"PatternTemplateGroupHipHop";
            case seq::PatternTemplateGroup::World: return L"PatternTemplateGroupWorld";
            case seq::PatternTemplateGroup::Basslines: return L"PatternTemplateGroupBasslines";
            case seq::PatternTemplateGroup::Sequences: return L"PatternTemplateGroupSequences";
            case seq::PatternTemplateGroup::DrumMachine:
            default: return L"PatternTemplateGroupDrumMachine";
            }
        }

        winrt::hstring TemplateName(std::wstring_view id)
        {
            return res::GetString(L"PatternTemplate" + std::wstring{ id });
        }
    }

    // ---------------------------------------------------------------- the Templates menu

    void MainWindow::OnPatternTemplatesOpening(foundation::IInspectable const&, foundation::IInspectable const&)
    {
        try
        {
            auto menu = PatternTemplatesMenu();
            menu.Items().Clear();

            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || !clip->Pattern.has_value())
            {
                return;
            }

            auto const mode = clip->Pattern->Mode;
            auto weak = get_weak();

            // A melodic template goes in a key, chosen here and kept while the window is open.
            if (mode == seq::PatternMode::Melodic)
            {
                controls::MenuFlyoutSubItem key{};
                key.Text(res::FormatString(L"PatternTemplateKeyFormat", KeyNames[m_templateKey % KeyNames.size()]));

                for (uint8_t i = 0; i < KeyNames.size(); ++i)
                {
                    controls::RadioMenuFlyoutItem item{};
                    item.Text(KeyNames[i]);
                    item.GroupName(L"PatternTemplateKey");
                    item.IsChecked(i == m_templateKey);

                    item.Click([weak, i](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->m_templateKey = i;
                        }
                    });

                    key.Items().Append(item);
                }

                menu.Items().Append(key);
                menu.Items().Append(controls::MenuFlyoutSeparator{});
            }

            // The templates for this kind of pattern, a submenu for each group, in the list's order.
            controls::MenuFlyoutSubItem group{ nullptr };
            std::optional<seq::PatternTemplateGroup> groupShown{};

            for (auto const& source : seq::PatternTemplates())
            {
                if (source.Mode != mode)
                {
                    continue;
                }

                if (!groupShown.has_value() || *groupShown != source.Group)
                {
                    group = controls::MenuFlyoutSubItem{};
                    group.Text(res::GetString(GroupKey(source.Group)));
                    menu.Items().Append(group);
                    groupShown = source.Group;
                }

                controls::MenuFlyoutItem item{};
                item.Text(TemplateName(source.Id));
                controls::ToolTipService::SetToolTip(item, winrt::box_value(res::FormatString(L"PatternTemplateTempoFormat", static_cast<uint32_t>(source.BeatsPerMinute))));

                item.Click([weak, id = std::wstring{ source.Id }](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->UsePatternTemplate(id);
                    }
                });

                group.Items().Append(item);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to list the templates.")
    }

    _Use_decl_annotations_
    void MainWindow::UsePatternTemplate(std::wstring const& id) noexcept
    {
        try
        {
            auto const source = seq::FindPatternTemplate(id);
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (source == nullptr || clip == nullptr || !clip->Pattern.has_value() || clip->Pattern->Mode != source->Mode)
            {
                return;
            }

            auto const name = std::wstring{ TemplateName(source->Id) };
            auto made = std::make_shared<seq::PatternSettings>(seq::MakePatternFromTemplate(*source, m_templateKey, clip->Pattern->Channel));

            // Applied as one undo step, to the clip that was open when the template was picked.
            auto const use = [weak = get_weak(), clipId = m_editorClipId, name, made](bool replace)
            {
                auto strong = weak.get();

                if (strong == nullptr || strong->m_editorClipId != clipId)
                {
                    return;
                }

                strong->m_pattern.ClearSelection();

                strong->EditPattern(std::wstring{ res::FormatString(replace ? L"UndoUseTemplateFormat" : L"UndoAddTemplateFormat", name) },
                    [&made, replace](seq::PatternSettings& pattern)
                    {
                        if (replace)
                        {
                            // Settings this version doesn't know stay with the pattern.
                            auto replaced = *made;
                            replaced.Unknown = pattern.Unknown;
                            pattern = std::move(replaced);
                        }
                        else
                        {
                            seq::OverlayPattern(pattern, *made);
                        }
                    });

                // A melodic step turned on next starts from the template's root.
                if (made->Mode == seq::PatternMode::Melodic && !made->Lanes.empty())
                {
                    strong->m_pattern.SetNextNote(made->Lanes.front().Note);
                }
            };

            if (!seq::PatternHasSteps(*clip->Pattern))
            {
                use(true);
                return;
            }

            // Steps are on already: replace them, or add the template on top of them.
            [](winrt::com_ptr<MainWindow> strong, winrt::hstring title, std::function<void(bool)> answer) -> winrt::fire_and_forget
            {
                try
                {
                    controls::TextBlock text{};
                    text.Text(res::GetString(L"PatternTemplateAskText"));
                    text.TextWrapping(xaml::TextWrapping::Wrap);
                    text.MaxWidth(440);

                    controls::ContentDialog dialog{};
                    dialog.XamlRoot(strong->Content().XamlRoot());
                    dialog.Title(winrt::box_value(title));
                    dialog.Content(text);
                    dialog.PrimaryButtonText(res::GetString(L"PatternTemplateReplace"));
                    dialog.SecondaryButtonText(res::GetString(L"PatternTemplateAddOnTop"));
                    dialog.CloseButtonText(res::GetString(L"DialogCancel"));
                    dialog.DefaultButton(controls::ContentDialogButton::Primary);
                    dialog.RequestedTheme(strong->RootGrid().ActualTheme());

                    auto const result = co_await dialog.ShowAsync();

                    if (result == controls::ContentDialogResult::Primary)
                    {
                        answer(true);
                    }
                    else if (result == controls::ContentDialogResult::Secondary)
                    {
                        answer(false);
                    }
                }
                MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to ask how to use the template.")
            }(get_strong(), res::FormatString(L"PatternTemplateAskTitleFormat", name), use);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to use the template.")
    }

    // ---------------------------------------------------------------- hearing steps

    void MainWindow::OnPatternHearClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            seq::AppSettings::Current().HearPatternSteps(PatternHearToggle().IsChecked().GetBoolean());
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change whether steps are heard.")
    }

    _Use_decl_annotations_
    void MainWindow::HearPatternStep(uint8_t note, uint16_t velocity) noexcept
    {
        try
        {
            if (!seq::AppSettings::Current().HearPatternSteps())
            {
                return;
            }

            auto const track = EditorTrack();
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (track == nullptr || track->Destination.Endpoint.IsEmpty() || clip == nullptr || !clip->Pattern.has_value())
            {
                return;
            }

            // On the pattern's channel unless the track's destination sets one, the same as when
            // the pattern plays. A drum pattern is usually on channel 10.
            seq::Note value{};
            value.Number = static_cast<uint8_t>(note & 0x7F);
            value.Channel = clip->Pattern->Channel;
            value.Velocity = std::max<uint16_t>(velocity, 1);
            value.ReleaseVelocity = 0x8000;

            uint32_t on[2]{};
            uint32_t off[2]{};
            seq::BuildNoteOn(value, track->Destination.Group, on);
            seq::BuildNoteOff(value, track->Destination.Group, off);

            SendNow(track->Destination, on, 2);

            // The note off goes now as well, for the service to send a moment later, so nothing
            // here has to remember the note or keep a timer.
            auto const later = midi2::MidiClock::Now() + midi2::MidiClock::TimestampFrequency() * StepPreviewMilliseconds / 1000;
            SendNow(track->Destination, off, 2, later);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to play a step.")
    }
}
