// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// About this sequence (comp 8, design section 13): the same provenance block as MIDI Glass and
// MIDI Patchbay, the copyright notice exports carry, and where each clip came from.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "SequenceProvenance.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        namespace sources = ::midiapp::DigitalSourceTypes;
        namespace oversight = ::midiapp::HumanOversightLevels;

        constexpr double InfoDialogWidth = 620;
        constexpr double HowColumnWidth = 150;
        constexpr size_t MaximumClipRows = 200;
        constexpr int32_t MaximumLicenseLength = 128;

        struct Choice
        {
            wchar_t const* Value;
            wchar_t const* Key;
        };

        constexpr Choice SourceChoices[] = {
            { sources::Composite, L"SequenceSourceMix" },
            { sources::DigitalCapture, L"SequenceSourceRecorded" },
            { sources::DigitalCreation, L"SequenceSourceDrawn" },
            { sources::AlgorithmicMedia, L"SequenceSourceAlgorithm" },
            { sources::CompositeWithTrainedAlgorithmicMedia, L"SequenceSourceWithAi" },
            { sources::TrainedAlgorithmicMedia, L"SequenceSourceAi" },
        };

        constexpr Choice OversightChoices[] = {
            { oversight::HumanValidated, L"OversightChoiceChecked" },
            { oversight::PromptGuided, L"OversightChoicePrompted" },
            { oversight::FullyAutonomous, L"OversightChoiceNone" },
        };

        // Licenses people pick most for music they share, as SPDX names them. Any other can be typed.
        constexpr wchar_t const* Licenses[] = { L"CC-BY-4.0", L"CC-BY-SA-4.0", L"CC-BY-NC-4.0", L"CC0-1.0" };

        controls::TextBox Field(winrt::hstring const& header, std::wstring const& text, winrt::hstring const& placeholder, int32_t maximumLength)
        {
            controls::TextBox box{};
            box.Header(winrt::box_value(header));
            box.Text(winrt::hstring{ text });
            box.PlaceholderText(placeholder);
            box.MaxLength(maximumLength);
            box.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            automation::AutomationProperties::SetName(box, header);
            return box;
        }

        // Two controls side by side, sharing the width.
        controls::Grid Pair(xaml::FrameworkElement const& left, xaml::FrameworkElement const& right)
        {
            controls::Grid grid{};
            grid.ColumnSpacing(12);

            for (int i = 0; i < 2; ++i)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLength{ 1, xaml::GridUnitType::Star });
                grid.ColumnDefinitions().Append(column);
            }

            controls::Grid::SetColumn(right, 1);
            grid.Children().Append(left);
            grid.Children().Append(right);
            return grid;
        }

        controls::TextBlock Text(winrt::hstring const& text, media::Brush const& brush, double size = 14, bool wrap = true)
        {
            controls::TextBlock block{};
            block.Text(text);
            block.FontSize(size);
            block.TextWrapping(wrap ? xaml::TextWrapping::Wrap : xaml::TextWrapping::NoWrap);
            block.TextTrimming(xaml::TextTrimming::CharacterEllipsis);

            if (brush != nullptr)
            {
                block.Foreground(brush);
            }

            return block;
        }

        controls::TextBlock Heading(winrt::hstring const& text)
        {
            auto block = Text(text, nullptr, 14, false);
            block.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            block.Margin(xaml::Thickness{ 0, 8, 0, 0 });
            automation::AutomationProperties::SetHeadingLevel(block, automation::Peers::AutomationHeadingLevel::Level2);
            return block;
        }

        // "October 9, 2026, 9:14 PM" in the person's own date format, or the text as written when
        // it isn't a date.
        winrt::hstring CreatedText(std::wstring const& created)
        {
            auto const fileTime = ::midiapp::ParseProvenanceTime(created);

            if (fileTime == 0)
            {
                return winrt::hstring{ created };
            }

            FILETIME universal{};
            universal.dwLowDateTime = static_cast<DWORD>(static_cast<uint64_t>(fileTime) & 0xFFFFFFFF);
            universal.dwHighDateTime = static_cast<DWORD>(static_cast<uint64_t>(fileTime) >> 32);

            SYSTEMTIME utc{};
            SYSTEMTIME local{};

            if (!FileTimeToSystemTime(&universal, &utc) || !SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local))
            {
                return winrt::hstring{ created };
            }

            wchar_t date[128]{};
            wchar_t time[64]{};

            if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_LONGDATE, &local, nullptr, date, ARRAYSIZE(date), nullptr) == 0 ||
                GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &local, nullptr, time, ARRAYSIZE(time)) == 0)
            {
                return winrt::hstring{ created };
            }

            return res::FormatString(L"DateAndTimeFormat", std::wstring{ date }, std::wstring{ time });
        }

        // How a clip began, as the table shows it: an icon, a word, and a detail.
        struct ClipRow
        {
            wchar_t const* Glyph{ L"\uE9CE" };
            winrt::hstring How{};
            winrt::hstring Detail{};
        };

        ClipRow DescribeClip(::midisequencer::Clip const& clip)
        {
            using ::midisequencer::ClipOrigin;

            ClipRow row{};

            switch (clip.Origin)
            {
            case ClipOrigin::Recorded:
                row.Glyph = L"\uE7C8";
                row.How = res::GetString(L"ClipOriginRecorded");
                row.Detail = clip.OriginDetail.empty() ? winrt::hstring{} : res::FormatString(L"ClipFromFormat", clip.OriginDetail);
                break;

            case ClipOrigin::Drawn:
                if (clip.Pattern.has_value())
                {
                    row.Glyph = L"\uE80A";
                    row.How = res::GetString(L"ClipOriginPattern");
                    row.Detail = res::GetString(L"ClipPatternDetail");
                }
                else
                {
                    row.Glyph = L"\uE70F";
                    row.How = res::GetString(L"ClipOriginDrawn");
                    row.Detail = res::FormatString(clip.Notes.size() == 1 ? L"ClipNoteCountOneFormat" : L"ClipNoteCountFormat", clip.Notes.size());
                }
                break;

            case ClipOrigin::Generated:
            {
                row.Glyph = L"\uE9F5";
                row.How = res::GetString(L"ClipOriginGenerated");

                if (clip.Generator.has_value())
                {
                    auto const typeKey =
                        clip.Generator->Type == ::midisequencer::GeneratorType::Euclidean ? L"GeneratorTypeEuclidean" :
                        clip.Generator->Type == ::midisequencer::GeneratorType::ChanceMelody ? L"GeneratorTypeChanceMelody" :
                        L"GeneratorTypeArpeggiator";

                    row.Detail = res::FormatString(L"ClipGeneratorFormat", std::wstring{ res::GetString(typeKey) }, std::format(L"{:04X}", clip.Seed & 0xFFFF));
                }
                break;
            }

            case ClipOrigin::Imported:
                row.Glyph = L"\uE8B5";
                row.How = res::GetString(L"ClipOriginImported");
                row.Detail = winrt::hstring{ clip.OriginDetail };
                break;

            case ClipOrigin::Assistant:
                row.Glyph = L"\uE99A";
                row.How = res::GetString(L"ClipOriginAssistant");
                row.Detail = res::GetString(L"ClipAssistantDetail");
                break;

            default:
                row.How = res::GetString(L"ClipOriginUnknown");
                break;
            }

            return row;
        }
    }

    void MainWindow::StampProvenance() noexcept
    {
        try
        {
            if (m_doc.Provenance.has_value())
            {
                return;
            }

            auto provenance = ::midiapp::StartProvenance(::midiapp::LoadAuthorProfile(), std::wstring_view{ res::GetString(L"AppDisplayName") });
            provenance.DigitalSourceType = ::midisequencer::SuggestDigitalSourceType(m_doc);
            m_doc.Provenance = std::move(provenance);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to fill in who made the sequence.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSequenceInfoClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ShowSequenceInfo();
    }

    void MainWindow::ShowSequenceInfo()
    {
        [](winrt::com_ptr<MainWindow> strong) -> winrt::fire_and_forget
        {
            try
            {
                auto const& doc = strong->m_doc;
                auto const profile = ::midiapp::LoadAuthorProfile();
                auto const toolName = std::wstring{ res::GetString(L"AppDisplayName") };
                auto const start = doc.Provenance.value_or(::midiapp::StartProvenance(profile, toolName));
                auto const suggested = ::midisequencer::SuggestDigitalSourceType(doc);
                auto const text2 = strong->BrushFor(strong->m_palette.Text2);
                auto const text3 = strong->BrushFor(strong->m_palette.Text3);
                auto const optional = res::GetString(L"SequenceInfoOptional");

                controls::StackPanel panel{};
                panel.Spacing(10);
                panel.Width(InfoDialogWidth);

                panel.Children().Append(Text(res::GetString(L"SequenceInfoIntro"), text2));

                auto const name = Field(res::GetString(L"SequenceInfoName"), doc.Name, winrt::hstring{}, static_cast<int32_t>(::midiapp::MaximumProvenanceNameLength));
                panel.Children().Append(name);

                auto const author = Field(res::GetString(L"SequenceInfoAuthor"), start.Author, res::GetString(L"SequenceInfoAuthorPlaceholder"), static_cast<int32_t>(::midiapp::MaximumProvenanceNameLength));
                auto const organization = Field(res::GetString(L"SequenceInfoOrganization"), start.Organization, optional, static_cast<int32_t>(::midiapp::MaximumProvenanceNameLength));
                panel.Children().Append(Pair(author, organization));

                auto const website = Field(res::GetString(L"SequenceInfoWebsite"), start.Url, L"https://", static_cast<int32_t>(::midiapp::MaximumProvenanceUrlLength));

                controls::ComboBox license{};
                license.IsEditable(true);
                license.PlaceholderText(res::GetString(L"SequenceInfoLicensePlaceholder"));
                license.Header(winrt::box_value(res::GetString(L"SequenceInfoLicense")));
                license.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                automation::AutomationProperties::SetName(license, res::GetString(L"SequenceInfoLicense"));

                for (auto const item : Licenses)
                {
                    license.Items().Append(winrt::box_value(winrt::hstring{ item }));
                }

                license.Text(winrt::hstring{ start.License });
                panel.Children().Append(Pair(website, license));

                auto const badWebsite = Text(res::GetString(L"SequenceInfoBadWebsite"), strong->ThemeBrush(L"SeqWarnTextBrush"), 12);
                badWebsite.Visibility(xaml::Visibility::Collapsed);
                panel.Children().Append(badWebsite);

                panel.Children().Append(Text(res::GetString(L"SequenceInfoLicenseNote"), text3, 12));

                auto const copyright = Field(res::GetString(L"SequenceInfoCopyright"), doc.Copyright, res::GetString(L"SequenceInfoCopyrightPlaceholder"), static_cast<int32_t>(::midiapp::MaximumProvenanceNameLength));
                panel.Children().Append(copyright);

                // Your info for sharing: the same profile the other tools use.
                controls::StackPanel profileRow{};
                profileRow.Orientation(controls::Orientation::Horizontal);
                profileRow.Spacing(16);

                controls::Button fill{};
                fill.Content(winrt::box_value(res::GetString(L"FillInYourInfo")));
                controls::ToolTipService::SetToolTip(fill, winrt::box_value(res::GetString(L"FillInYourInfoTip")));
                fill.Click([author, organization, website, license, profile](auto&&, auto&&)
                {
                    author.Text(winrt::hstring{ profile.Name });
                    organization.Text(winrt::hstring{ profile.Organization });
                    website.Text(winrt::hstring{ profile.Url });
                    license.Text(winrt::hstring{ profile.License });
                });
                profileRow.Children().Append(fill);

                controls::CheckBox remember{};
                remember.Content(winrt::box_value(res::GetString(L"SequenceInfoRemember")));
                profileRow.Children().Append(remember);
                panel.Children().Append(profileRow);

                controls::TextBlock madeOn = Text(res::FormatString(L"SequenceInfoMadeOnFormat", std::wstring{ CreatedText(start.Created) }), text2, 13);
                controls::TextBlock madeWith = Text(res::FormatString(L"SequenceInfoMadeWithFormat", start.Tool.empty() ? toolName : start.Tool), text2, 13);
                panel.Children().Append(Pair(madeOn, madeWith));

                // ---- how it was made ----

                panel.Children().Append(Heading(res::GetString(L"SequenceInfoSourceHeading")));

                controls::ComboBox source{};
                source.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                automation::AutomationProperties::SetName(source, res::GetString(L"SequenceInfoSourceHeading"));

                std::vector<std::wstring> sourceValues{};
                int32_t selectedSource{ -1 };
                auto const current = start.DigitalSourceType.empty() ? suggested : start.DigitalSourceType;

                for (auto const& choice : SourceChoices)
                {
                    auto label = std::wstring{ res::GetString(choice.Key) };

                    if (suggested == choice.Value)
                    {
                        label += L" \u00B7 " + std::wstring{ res::GetString(L"SequenceInfoSuggested") };
                    }

                    if (current == choice.Value)
                    {
                        selectedSource = static_cast<int32_t>(sourceValues.size());
                    }

                    sourceValues.emplace_back(choice.Value);
                    source.Items().Append(winrt::box_value(winrt::hstring{ label }));
                }

                // A term this build doesn't know stays as the file wrote it.
                if (selectedSource < 0)
                {
                    selectedSource = static_cast<int32_t>(sourceValues.size());
                    sourceValues.push_back(current);
                    source.Items().Append(winrt::box_value(res::FormatString(L"SourceAsWrittenFormat", ::midiapp::SanitizeProvenanceText(current, 64))));
                }

                source.SelectedIndex(selectedSource);
                panel.Children().Append(source);

                // The AI disclosure, when generative AI was part of it.
                controls::ComboBox involvement{};
                involvement.Header(winrt::box_value(res::GetString(L"ProvenanceOversightLabel")));
                involvement.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                automation::AutomationProperties::SetName(involvement, res::GetString(L"ProvenanceOversightLabel"));
                int32_t selectedOversight{ 0 };

                for (size_t i = 0; i < std::size(OversightChoices); ++i)
                {
                    involvement.Items().Append(winrt::box_value(res::GetString(OversightChoices[i].Key)));

                    if (start.HumanOversightLevel == OversightChoices[i].Value)
                    {
                        selectedOversight = static_cast<int32_t>(i);
                    }
                }

                involvement.SelectedIndex(selectedOversight);

                auto const model = Field(res::GetString(L"ProvenanceModelLabel"), start.AiModelName, optional, static_cast<int32_t>(::midiapp::MaximumProvenanceNameLength));
                auto const aiRow = Pair(involvement, model);
                panel.Children().Append(aiRow);

                // The handler finds the list through its sender, so it doesn't keep the list alive.
                auto const showAi = [aiRow, sourceValues](controls::ComboBox const& list)
                {
                    auto const index = list.SelectedIndex();
                    auto const ai = index >= 0 && static_cast<size_t>(index) < sourceValues.size() && ::midiapp::InvolvesGenerativeAi(sourceValues[static_cast<size_t>(index)]);
                    aiRow.Visibility(ai ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                };

                showAi(source);
                source.SelectionChanged([showAi](foundation::IInspectable const& sender, auto&&)
                {
                    if (auto const list = sender.try_as<controls::ComboBox>())
                    {
                        showAi(list);
                    }
                });

                // ---- where the clips came from ----

                panel.Children().Append(Heading(res::GetString(L"SequenceInfoClipsHeading")));

                controls::Grid table{};
                table.ColumnSpacing(12);
                table.RowSpacing(6);

                for (auto const width : { xaml::GridLength{ 1, xaml::GridUnitType::Star }, xaml::GridLength{ HowColumnWidth, xaml::GridUnitType::Pixel }, xaml::GridLength{ 1.4, xaml::GridUnitType::Star } })
                {
                    controls::ColumnDefinition column{};
                    column.Width(width);
                    table.ColumnDefinitions().Append(column);
                }

                int32_t tableRow{ 0 };

                auto const addCell = [&table, &tableRow](xaml::FrameworkElement const& element, int32_t column)
                {
                    controls::Grid::SetRow(element, tableRow);
                    controls::Grid::SetColumn(element, column);
                    table.Children().Append(element);
                };

                auto const addRow = [&]()
                {
                    controls::RowDefinition row{};
                    row.Height(xaml::GridLength{ 0, xaml::GridUnitType::Auto });
                    table.RowDefinitions().Append(row);
                };

                addRow();

                int32_t headerColumn{ 0 };

                for (auto const key : { L"SequenceInfoClipColumn", L"SequenceInfoHowColumn", L"SequenceInfoDetailsColumn" })
                {
                    addCell(Text(res::GetString(key), text3, 12, false), headerColumn++);
                }

                auto const used = ::midisequencer::ClipsInUse(doc);
                size_t shown{ 0 };
                size_t hidden{ 0 };

                for (auto const& clip : doc.Clips)
                {
                    if (!used.contains(clip.Id))
                    {
                        continue;
                    }

                    if (shown >= MaximumClipRows)
                    {
                        ++hidden;
                        continue;
                    }

                    ++shown;
                    ++tableRow;
                    addRow();

                    auto const described = DescribeClip(clip);
                    auto const clipName = winrt::hstring{ clip.Name.empty() ? std::wstring{ res::GetString(L"UnnamedClip") } : clip.Name };

                    addCell(Text(clipName, nullptr, 13, false), 0);

                    controls::StackPanel how{};
                    how.Orientation(controls::Orientation::Horizontal);
                    how.Spacing(6);

                    controls::FontIcon icon{};
                    icon.Glyph(described.Glyph);
                    icon.FontSize(12);
                    icon.Foreground(text2);
                    automation::AutomationProperties::SetAccessibilityView(icon, automation::Peers::AccessibilityView::Raw);
                    how.Children().Append(icon);
                    how.Children().Append(Text(described.How, nullptr, 13, false));
                    addCell(how, 1);

                    auto detail = Text(described.Detail, text2, 13, false);
                    controls::ToolTipService::SetToolTip(detail, winrt::box_value(described.Detail));
                    addCell(detail, 2);
                }

                if (shown == 0)
                {
                    ++tableRow;
                    addRow();
                    auto none = Text(res::GetString(L"SequenceInfoNoClips"), text2, 13);
                    controls::Grid::SetColumnSpan(none, 3);
                    addCell(none, 0);
                }

                panel.Children().Append(table);

                if (hidden > 0)
                {
                    panel.Children().Append(Text(res::FormatString(L"SequenceInfoMoreClipsFormat", hidden), text2, 13));
                }

                panel.Children().Append(Text(res::GetString(L"SequenceInfoClipsNote"), text3, 12));

                controls::ScrollViewer scroller{};
                scroller.Content(panel);
                scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Auto);
                scroller.Padding(xaml::Thickness{ 0, 0, 12, 0 });

                controls::ContentDialog dialog{};
                dialog.XamlRoot(strong->Content().XamlRoot());
                dialog.Title(winrt::box_value(res::GetString(L"SequenceInfoTitle")));
                dialog.Content(scroller);
                dialog.PrimaryButtonText(res::GetString(L"SequenceInfoSave"));
                dialog.CloseButtonText(res::GetString(L"DialogCancel"));
                dialog.DefaultButton(controls::ContentDialogButton::Primary);
                dialog.RequestedTheme(strong->RootGrid().ActualTheme());
                dialog.Resources().Insert(winrt::box_value(L"ContentDialogMaxWidth"), winrt::box_value(InfoDialogWidth + 80.0));

                // A web page has to be one people can open safely, so anything else stops the save.
                dialog.PrimaryButtonClick([website, badWebsite](auto&&, controls::ContentDialogButtonClickEventArgs const& args)
                {
                    std::wstring const url{ website.Text() };
                    auto const bad = !url.empty() && !::midiapp::IsSafeWebLink(url);
                    badWebsite.Visibility(bad ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                    args.Cancel(bad);
                });

                if (co_await dialog.ShowAsync() != controls::ContentDialogResult::Primary)
                {
                    co_return;
                }

                // ---- save it, as one step to undo ----

                auto& now = strong->m_doc;
                auto const sanitize = [](winrt::hstring const& value, size_t maximum) { return ::midiapp::SanitizeProvenanceText(std::wstring_view{ value }, maximum); };

                auto provenance = now.Provenance.value_or(start);
                provenance.Author = sanitize(author.Text(), ::midiapp::MaximumProvenanceNameLength);
                provenance.Organization = sanitize(organization.Text(), ::midiapp::MaximumProvenanceNameLength);
                provenance.Url = sanitize(website.Text(), ::midiapp::MaximumProvenanceUrlLength);
                provenance.License = sanitize(license.Text(), static_cast<size_t>(MaximumLicenseLength));

                auto const sourceIndex = source.SelectedIndex();

                if (sourceIndex >= 0 && static_cast<size_t>(sourceIndex) < sourceValues.size())
                {
                    provenance.DigitalSourceType = sourceValues[static_cast<size_t>(sourceIndex)];
                }

                if (::midiapp::InvolvesGenerativeAi(provenance.DigitalSourceType))
                {
                    auto const oversightIndex = involvement.SelectedIndex();
                    provenance.HumanOversightLevel = oversightIndex >= 0 && static_cast<size_t>(oversightIndex) < std::size(OversightChoices)
                        ? OversightChoices[static_cast<size_t>(oversightIndex)].Value : std::wstring{};
                    provenance.AiModelName = sanitize(model.Text(), ::midiapp::MaximumProvenanceNameLength);
                }
                else
                {
                    provenance.HumanOversightLevel.clear();
                    provenance.AiModelName.clear();
                }

                auto newName = sanitize(name.Text(), ::midiapp::MaximumProvenanceNameLength);
                auto newCopyright = sanitize(copyright.Text(), ::midiapp::MaximumProvenanceNameLength);

                ::midisequencer::ChangeList changes{};
                changes.push_back(std::make_unique<::midisequencer::SnapshotChange<std::wstring>>(
                    [](::midisequencer::Sequence& sequence) { return &sequence.Name; }, now.Name, newName, now.Name.size() + newName.size()));
                changes.push_back(std::make_unique<::midisequencer::SnapshotChange<std::wstring>>(
                    [](::midisequencer::Sequence& sequence) { return &sequence.Copyright; }, now.Copyright, newCopyright, now.Copyright.size() + newCopyright.size()));
                changes.push_back(std::make_unique<::midisequencer::SnapshotChange<std::optional<::midiapp::ContentProvenance>>>(
                    [](::midisequencer::Sequence& sequence) { return &sequence.Provenance; }, now.Provenance, provenance, sizeof(::midiapp::ContentProvenance) * 2));

                now.Name = std::move(newName);
                now.Copyright = std::move(newCopyright);
                now.Provenance = std::move(provenance);

                if (remember.IsChecked() && remember.IsChecked().Value())
                {
                    ::midiapp::AuthorProfile saved{};
                    saved.Name = now.Provenance->Author;
                    saved.Organization = now.Provenance->Organization;
                    saved.Url = now.Provenance->Url;
                    saved.License = now.Provenance->License;
                    ::midiapp::SaveAuthorProfile(saved);
                }

                strong->Commit(std::wstring{ res::GetString(L"UndoSequenceInfo") }, std::move(changes));
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the sequence info.")
        }(get_strong());
    }
}
