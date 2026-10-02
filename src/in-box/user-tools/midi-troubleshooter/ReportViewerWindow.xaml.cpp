// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ReportViewerWindow.xaml.h"
#include "ReportViewerWindow.g.cpp"

#include "..\mididiag\mididiag_field_defs.h"

#include <cmath>

#include "AppSettings.h"
#include "BackgroundWork.h"
#include "StringResources.h"
#include "resource.h"

namespace native = ::miditroubleshooter;
namespace res = ::miditroubleshooter::resources;
namespace rpt = ::mididiag::report;
namespace shapes = ::winrt::Microsoft::UI::Xaml::Shapes;
namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;

namespace miditroubleshooter
{
    namespace
    {
        // A column whose longest value is at least this long takes the rest of the table's width,
        // and its values wrap there. Measured on real reports: finding text, device ids and paths.
        constexpr size_t WideColumnLength{ 48 };

        // The least a wide column gets. A window narrower than that scrolls the table sideways.
        constexpr double MinimumWideColumnWidth{ 240 };

        // One item with more parts than this reads better down the page than across it.
        constexpr size_t MaximumPartsAcross{ 7 };

        // Wide enough for nearly every label on one line, so the values line up down a record.
        constexpr double FieldLabelColumnWidth{ 280 };

        // Labels that name what a record is about, best first.
        constexpr std::wstring_view RecordTitleLabels[]
        {
            L"name",
            L"session_name",
            L"transport_name",
            L"endpoint_name",
        };

        constexpr std::wstring_view RecordSubtitleLabels[]
        {
            L"transport_code",
            L"process_name",
            L"code",
        };

        std::wstring_view TrimRight(_In_ std::wstring_view text) noexcept
        {
            while (!text.empty() && (text.back() == L' ' || text.back() == L'\t'))
            {
                text.remove_suffix(1);
            }

            return text;
        }

        bool IsPlainName(_In_ std::wstring_view const name) noexcept
        {
            return !name.empty() && std::all_of(name.begin(), name.end(), [](wchar_t const character)
                {
                    return (character >= L'a' && character <= L'z') ||
                        (character >= L'A' && character <= L'Z') ||
                        (character >= L'0' && character <= L'9') ||
                        character == L'_';
                });
        }

        // A known section gets a title from the resources. One this version doesn't know, from
        // a newer mididiag, gets a title made from its name.
        winrt::hstring SectionTitle(_In_ std::wstring const& name)
        {
            // The name comes from a file, and a resource key with a slash in it reaches into
            // other resource maps, so only plain names are looked up.
            if (IsPlainName(name))
            {
                auto const key = L"ReportSection_" + name;
                auto title = res::GetString(key);

                // a missing resource comes back as its key
                if (std::wstring_view{ title } != key)
                {
                    return title;
                }
            }

            return winrt::hstring{ rpt::ReadableName(name) };
        }

        rpt::Field const* PlainField(_In_ rpt::Record const& record, _In_ std::wstring_view const label) noexcept
        {
            auto const* const field = record.FindField(label);

            return (field != nullptr && field->Parts.empty() && !TrimRight(field->Value).empty()) ? field : nullptr;
        }

        std::wstring RecordTitle(_In_ rpt::Record const& record)
        {
            for (auto const label : RecordTitleLabels)
            {
                if (auto const* const field = PlainField(record, label))
                {
                    return field->Value;
                }
            }

            return {};
        }

        std::wstring RecordSubtitle(_In_ rpt::Record const& record)
        {
            for (auto const label : RecordSubtitleLabels)
            {
                if (auto const* const field = PlainField(record, label))
                {
                    return field->Value;
                }
            }

            return {};
        }

        // An ERROR field is a message on its own. A section_timed_out field names the section.
        winrt::hstring ErrorText(_In_ rpt::Field const& field)
        {
            if (field.Label == MIDIDIAG_FIELD_LABEL_ERROR)
            {
                return winrt::hstring{ field.Value };
            }

            return res::FormatString(L"ReportLabelValueFormat", rpt::ReadableName(field.Label), field.Value);
        }

        winrt::hstring ErrorCountText(_In_ size_t const count)
        {
            return count == 1 ? res::GetString(L"ReportOneError") : res::FormatString(L"ReportErrorsFormat", count);
        }

        std::wstring_view FileNamePart(_In_ std::wstring_view const path) noexcept
        {
            auto const separator = path.find_last_of(L"\\/");

            return separator == std::wstring_view::npos ? path : path.substr(separator + 1);
        }

        xaml::GridLength AutoLength() noexcept
        {
            return xaml::GridLengthHelper::Auto();
        }

        xaml::GridLength StarLength() noexcept
        {
            return xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star);
        }
    }

    // Builds the view of one report. The summary, findings and errors are built straight away;
    // each section and each record is built the first time somebody opens it, because a large
    // report has thousands of values and most people open only the sections they need.
    class ReportPresenter : public std::enable_shared_from_this<ReportPresenter>
    {
    public:
        ReportPresenter(
            _In_ xaml::ResourceDictionary const& resources,
            _In_ std::shared_ptr<rpt::Report const> report) :
            m_resources(resources),
            m_report(std::move(report))
        {
        }

        void Build(_In_ controls::StackPanel const& panel);

        void SetAllExpanded(_In_ bool const expanded);

    private:
        xaml::Style StyleNamed(_In_ std::wstring_view const key) const;

        controls::TextBlock MakeText(_In_ winrt::hstring const& text, _In_ std::wstring_view const styleKey) const;
        controls::Grid MakePill(_In_ std::wstring_view const kind, _In_ winrt::hstring const& text) const;
        controls::Grid MakeFieldGrid() const;
        void AddFieldRow(
            _In_ controls::Grid const& grid,
            _In_ std::wstring const& label,
            _In_ std::wstring const& value) const;

        std::wstring FieldValue(_In_ std::wstring_view const section, _In_ std::wstring_view const label) const;

        xaml::UIElement BuildSummaryCard();
        xaml::UIElement BuildFindingsCard();
        xaml::UIElement BuildErrorsCard();

        controls::Expander BuildSectionExpander(_In_ size_t const sectionIndex);
        xaml::UIElement BuildSectionContent(_In_ rpt::Section const& section);
        controls::Expander BuildRecordExpander(_In_ rpt::Record const& record, _In_ std::wstring const& title);

        xaml::UIElement BuildFields(_In_ std::span<rpt::Field const> const fields);
        xaml::UIElement BuildPartsBlock(_In_ std::span<rpt::Field const> const rows);
        xaml::UIElement BuildTable(_In_ std::span<rpt::Field const> const rows);
        xaml::UIElement BuildErrorRow(_In_ rpt::Field const& field);

        void RevealSection(_In_ size_t const sectionIndex);

        xaml::ResourceDictionary m_resources{ nullptr };
        std::shared_ptr<rpt::Report const> m_report{};

        // One per section, in report order, and null for a section that isn't shown.
        std::vector<controls::Expander> m_sectionExpanders{};

        // Added to as sections are opened.
        std::vector<controls::Expander> m_recordExpanders{};
    };

    _Use_decl_annotations_
    xaml::Style ReportPresenter::StyleNamed(std::wstring_view const key) const
    {
        auto const found = m_resources.TryLookup(winrt::box_value(winrt::hstring{ key }));

        return found != nullptr ? found.try_as<xaml::Style>() : nullptr;
    }

    _Use_decl_annotations_
    controls::TextBlock ReportPresenter::MakeText(winrt::hstring const& text, std::wstring_view const styleKey) const
    {
        controls::TextBlock block{};

        block.Style(StyleNamed(styleKey));
        block.Text(text);

        return block;
    }

    // kind is Success, Caution or Critical, and picks the styles.
    _Use_decl_annotations_
    controls::Grid ReportPresenter::MakePill(std::wstring_view const kind, winrt::hstring const& text) const
    {
        controls::Grid pill{};
        pill.VerticalAlignment(xaml::VerticalAlignment::Center);
        pill.HorizontalAlignment(xaml::HorizontalAlignment::Left);

        shapes::Rectangle shape{};
        shape.Style(StyleNamed(std::format(L"Report{}PillShapeStyle", kind)));

        controls::StackPanel content{};
        content.Style(StyleNamed(L"ReportPillPanelStyle"));

        controls::FontIcon glyph{};
        glyph.Style(StyleNamed(std::format(L"Report{}PillGlyphStyle", kind)));

        controls::TextBlock label{};
        label.Style(StyleNamed(std::format(L"Report{}PillTextStyle", kind)));
        label.Text(text);

        content.Children().Append(glyph);
        content.Children().Append(label);

        pill.Children().Append(shape);
        pill.Children().Append(content);

        return pill;
    }

    controls::Grid ReportPresenter::MakeFieldGrid() const
    {
        controls::Grid grid{};
        grid.RowSpacing(4);

        controls::ColumnDefinition labels{};
        labels.Width(xaml::GridLengthHelper::FromPixels(FieldLabelColumnWidth));

        controls::ColumnDefinition values{};
        values.Width(StarLength());

        grid.ColumnDefinitions().Append(labels);
        grid.ColumnDefinitions().Append(values);

        return grid;
    }

    _Use_decl_annotations_
    void ReportPresenter::AddFieldRow(
        controls::Grid const& grid,
        std::wstring const& label,
        std::wstring const& value) const
    {
        auto const row = static_cast<int32_t>(grid.RowDefinitions().Size());

        controls::RowDefinition definition{};
        definition.Height(AutoLength());
        grid.RowDefinitions().Append(definition);

        // the readable label, with the one in the report text in its tooltip for searching
        auto labelText = MakeText(winrt::hstring{ rpt::ReadableName(label) }, L"ReportFieldLabelStyle");
        controls::ToolTipService::SetToolTip(labelText, winrt::box_value(winrt::hstring{ label }));
        controls::Grid::SetRow(labelText, row);

        auto valueText = TrimRight(value).empty() ?
            MakeText(res::GetString(L"ReportEmptyValue"), L"ReportEmptyValueStyle") :
            MakeText(winrt::hstring{ value }, L"ReportFieldValueStyle");

        controls::Grid::SetRow(valueText, row);
        controls::Grid::SetColumn(valueText, 1);

        grid.Children().Append(labelText);
        grid.Children().Append(valueText);
    }

    _Use_decl_annotations_
    std::wstring ReportPresenter::FieldValue(std::wstring_view const section, std::wstring_view const label) const
    {
        auto const* const field = m_report->FindField(section, label);

        return field != nullptr ? std::wstring{ TrimRight(field->Value) } : std::wstring{};
    }

    _Use_decl_annotations_
    void ReportPresenter::Build(controls::StackPanel const& panel)
    {
        panel.Children().Clear();

        m_sectionExpanders.clear();
        m_recordExpanders.clear();

        panel.Children().Append(BuildSummaryCard());

        if (auto const findings = BuildFindingsCard())
        {
            panel.Children().Append(findings);
        }

        if (auto const errors = BuildErrorsCard())
        {
            panel.Children().Append(errors);
        }

        auto heading = MakeText(res::GetString(L"ReportSectionsHeading"), L"ReportCardTitleStyle");
        heading.Margin(xaml::Thickness{ 0, 12, 0, 0 });
        panel.Children().Append(heading);

        auto const& sections = m_report->Sections;

        m_sectionExpanders.assign(sections.size(), controls::Expander{ nullptr });

        for (size_t index = 0; index < sections.size(); ++index)
        {
            // Sections with nothing in them, such as successful_run, are markers. The summary
            // already says how the report ended.
            if (sections[index].Records.empty())
            {
                continue;
            }

            auto expander = BuildSectionExpander(index);

            m_sectionExpanders[index] = expander;
            panel.Children().Append(expander);
        }
    }

    _Use_decl_annotations_
    void ReportPresenter::SetAllExpanded(bool const expanded)
    {
        for (auto const& expander : m_sectionExpanders)
        {
            if (expander != nullptr)
            {
                expander.IsExpanded(expanded);
            }
        }

        // Opening the sections above has just built their records. By index, because the
        // list is only ever added to while this runs.
        for (size_t index = 0; index < m_recordExpanders.size(); ++index)
        {
            m_recordExpanders[index].IsExpanded(expanded);
        }
    }

    _Use_decl_annotations_
    void ReportPresenter::RevealSection(size_t const sectionIndex)
    {
        if (sectionIndex >= m_sectionExpanders.size() || m_sectionExpanders[sectionIndex] == nullptr)
        {
            return;
        }

        auto const& expander = m_sectionExpanders[sectionIndex];

        expander.IsExpanded(true);
        expander.StartBringIntoView();
    }

    xaml::UIElement ReportPresenter::BuildSummaryCard()
    {
        auto const& report = *m_report;

        controls::Border card{};
        card.Style(StyleNamed(L"ReportCardStyle"));

        controls::StackPanel content{};
        content.Spacing(10);

        // the title, and how the report ended
        controls::StackPanel heading{};
        heading.Orientation(controls::Orientation::Horizontal);
        heading.Spacing(12);

        auto const created = FieldValue(MIDIDIAG_SECTION_LABEL_HEADER, MIDIDIAG_FIELD_LABEL_CURRENT_TIME);

        auto title = MakeText(
            created.empty() ? res::GetString(L"ReportSummaryTitle") : res::FormatString(L"ReportSummaryTitleFormat", created),
            L"ReportSectionTitleStyle");
        title.VerticalAlignment(xaml::VerticalAlignment::Center);

        heading.Children().Append(title);

        switch (report.Result)
        {
        case rpt::Outcome::Finished:
            heading.Children().Append(MakePill(L"Success", res::GetString(L"ReportOutcomeFinished")));
            break;

        case rpt::Outcome::StoppedEarly:
            heading.Children().Append(MakePill(L"Critical", res::GetString(L"ReportOutcomeStoppedEarly")));
            break;

        case rpt::Outcome::Incomplete:
        default:
            heading.Children().Append(MakePill(L"Caution", res::GetString(L"ReportOutcomeIncomplete")));
            break;
        }

        content.Children().Append(heading);

        // the facts a support engineer asks for first
        controls::Grid facts{};
        facts.ColumnSpacing(24);
        facts.RowSpacing(4);

        controls::ColumnDefinition labelColumn{};
        labelColumn.Width(AutoLength());

        controls::ColumnDefinition valueColumn{};
        valueColumn.Width(StarLength());

        facts.ColumnDefinitions().Append(labelColumn);
        facts.ColumnDefinitions().Append(valueColumn);

        auto const addFact = [this, &facts](std::wstring_view const labelKey, std::wstring const& value)
            {
                if (value.empty())
                {
                    return;
                }

                auto const row = static_cast<int32_t>(facts.RowDefinitions().Size());

                controls::RowDefinition definition{};
                definition.Height(AutoLength());
                facts.RowDefinitions().Append(definition);

                auto label = MakeText(res::GetString(labelKey), L"ReportFieldLabelStyle");
                controls::Grid::SetRow(label, row);

                auto text = MakeText(winrt::hstring{ value }, L"ReportFieldValueStyle");
                controls::Grid::SetRow(text, row);
                controls::Grid::SetColumn(text, 1);

                facts.Children().Append(label);
                facts.Children().Append(text);
            };

        auto const withDetail = [](std::wstring const& value, std::wstring const& detail) -> std::wstring
            {
                if (detail.empty() || value.empty())
                {
                    return value.empty() ? detail : value;
                }

                return std::wstring{ res::FormatString(L"ReportValueWithDetailFormat", value, detail) };
            };

        addFact(L"ReportSummaryWindowsLabel", withDetail(
            FieldValue(MIDIDIAG_SECTION_LABEL_OS, MIDIDIAG_FIELD_LABEL_OS_VERSION),
            FieldValue(MIDIDIAG_SECTION_LABEL_OS, MIDIDIAG_FIELD_LABEL_OS_DISPLAY_VERSION)));

        addFact(L"ReportSummaryMidiServicesLabel", withDetail(
            FieldValue(MIDIDIAG_SECTION_LABEL_HEADER, MIDIDIAG_HEADER_FIELD_LABEL_VERSION_FULL),
            FieldValue(MIDIDIAG_SECTION_LABEL_HEADER, MIDIDIAG_HEADER_FIELD_LABEL_VERSION_NAME)));

        addFact(L"ReportSummaryApiModeLabel",
            FieldValue(MIDIDIAG_SECTION_LABEL_API_MODE, MIDIDIAG_FIELD_LABEL_API_MODE));

        addFact(L"ReportSummaryServiceLabel",
            FieldValue(MIDIDIAG_SECTION_LABEL_SERVICE_STATUS, MIDIDIAG_FIELD_LABEL_SERVICE_STATE_BEFORE_REPORT));

        addFact(L"ReportSummaryElevatedLabel",
            FieldValue(MIDIDIAG_SECTION_LABEL_HEADER, MIDIDIAG_FIELD_LABEL_RUNNING_ELEVATED));

        addFact(L"ReportSummaryFormatLabel", std::to_wstring(report.FormatVersion));

        if (report.FormatVersion >= 2)
        {
            addFact(L"ReportSummaryFindingsLabel", std::to_wstring(report.Findings.size()));
        }

        addFact(L"ReportSummaryErrorsLabel", std::to_wstring(report.ErrorCount));

        content.Children().Append(facts);

        auto const addNote = [&content](controls::InfoBarSeverity const severity, std::wstring_view const messageKey)
            {
                controls::InfoBar note{};

                note.Severity(severity);
                note.Message(res::GetString(messageKey));
                note.IsClosable(false);
                note.IsOpen(true);

                content.Children().Append(note);
            };

        if (report.Result == rpt::Outcome::StoppedEarly)
        {
            addNote(controls::InfoBarSeverity::Error, L"ReportOutcomeStoppedEarlyMessage");
        }
        else if (report.Result == rpt::Outcome::Incomplete)
        {
            addNote(controls::InfoBarSeverity::Warning, L"ReportOutcomeIncompleteMessage");
        }

        if (report.FormatVersion > MIDIDIAG_REPORT_FORMAT_VERSION)
        {
            addNote(controls::InfoBarSeverity::Informational, L"ReportNewerFormatMessage");
        }
        else if (report.FormatVersion < 2)
        {
            addNote(controls::InfoBarSeverity::Informational, L"ReportOlderFormatMessage");
        }

        card.Child(content);

        return card;
    }

    xaml::UIElement ReportPresenter::BuildFindingsCard()
    {
        auto const& report = *m_report;

        // reports from before findings existed have nothing to show here
        if (report.FormatVersion < 2)
        {
            return nullptr;
        }

        controls::Border card{};
        card.Style(StyleNamed(L"ReportCardStyle"));

        controls::StackPanel content{};
        content.Spacing(10);

        content.Children().Append(MakeText(
            res::FormatString(L"ReportFindingsTitleFormat", report.Findings.size()), L"ReportCardTitleStyle"));

        auto const addRow = [this, &content](std::wstring_view const glyphStyle, winrt::hstring const& text, std::wstring const& detail)
            {
                controls::Grid row{};
                row.ColumnSpacing(10);

                controls::ColumnDefinition glyphColumn{};
                glyphColumn.Width(AutoLength());

                controls::ColumnDefinition textColumn{};
                textColumn.Width(StarLength());

                row.ColumnDefinitions().Append(glyphColumn);
                row.ColumnDefinitions().Append(textColumn);

                controls::FontIcon glyph{};
                glyph.Style(StyleNamed(glyphStyle));

                controls::StackPanel lines{};
                lines.Spacing(2);
                lines.Children().Append(MakeText(text, L"ReportBodyTextStyle"));

                if (!detail.empty())
                {
                    lines.Children().Append(MakeText(winrt::hstring{ detail }, L"ReportSecondaryTextStyle"));
                }

                controls::Grid::SetColumn(lines, 1);

                row.Children().Append(glyph);
                row.Children().Append(lines);

                content.Children().Append(row);
            };

        if (report.Findings.empty())
        {
            addRow(L"ReportSuccessGlyphStyle", res::GetString(L"ReportNoFindings"), {});
        }

        // The id is what a script checks, so it's shown under the sentence.
        for (auto const& finding : report.Findings)
        {
            addRow(L"ReportFindingGlyphStyle", winrt::hstring{ finding.Text }, finding.Id);
        }

        card.Child(content);

        return card;
    }

    xaml::UIElement ReportPresenter::BuildErrorsCard()
    {
        auto const& report = *m_report;

        if (report.ErrorCount == 0)
        {
            return nullptr;
        }

        controls::Border card{};
        card.Style(StyleNamed(L"ReportCardStyle"));

        controls::StackPanel content{};
        content.Spacing(8);

        content.Children().Append(MakeText(
            res::FormatString(L"ReportErrorsTitleFormat", report.ErrorCount), L"ReportCardTitleStyle"));

        for (size_t sectionIndex = 0; sectionIndex < report.Sections.size(); ++sectionIndex)
        {
            auto const& section = report.Sections[sectionIndex];

            if (section.ErrorCount == 0)
            {
                continue;
            }

            auto const sectionTitle = SectionTitle(section.Name);

            for (auto const& record : section.Records)
            {
                for (auto const& field : record.Fields)
                {
                    if (!field.IsError())
                    {
                        continue;
                    }

                    controls::Grid row{};
                    row.ColumnSpacing(10);

                    controls::ColumnDefinition glyphColumn{};
                    glyphColumn.Width(AutoLength());

                    controls::ColumnDefinition textColumn{};
                    textColumn.Width(StarLength());

                    controls::ColumnDefinition linkColumn{};
                    linkColumn.Width(AutoLength());

                    row.ColumnDefinitions().Append(glyphColumn);
                    row.ColumnDefinitions().Append(textColumn);
                    row.ColumnDefinitions().Append(linkColumn);

                    controls::FontIcon glyph{};
                    glyph.Style(StyleNamed(L"ReportErrorGlyphStyle"));

                    auto const message = res::FormatString(L"ReportLabelValueFormat", sectionTitle, ErrorText(field));

                    auto text = MakeText(message, L"ReportErrorTextStyle");
                    controls::Grid::SetColumn(text, 1);
                    automation::AutomationProperties::SetName(text, res::FormatString(L"ReportErrorAutomationFormat", message));

                    controls::HyperlinkButton show{};
                    show.Content(winrt::box_value(res::GetString(L"ReportShowSection")));
                    show.Padding(xaml::Thickness{ 4, 0, 4, 0 });
                    show.VerticalAlignment(xaml::VerticalAlignment::Top);
                    automation::AutomationProperties::SetName(show, res::FormatString(L"ReportShowSectionAutomationFormat", sectionTitle));
                    controls::Grid::SetColumn(show, 2);

                    show.Click([weak = weak_from_this(), sectionIndex](auto const&, auto const&)
                        {
                            try
                            {
                                if (auto const presenter = weak.lock())
                                {
                                    presenter->RevealSection(sectionIndex);
                                }
                            }
                            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show a report section.")
                        });

                    row.Children().Append(glyph);
                    row.Children().Append(text);
                    row.Children().Append(show);

                    content.Children().Append(row);
                }
            }
        }

        card.Child(content);

        return card;
    }

    _Use_decl_annotations_
    controls::Expander ReportPresenter::BuildSectionExpander(size_t const sectionIndex)
    {
        auto const& section = m_report->Sections[sectionIndex];
        auto const title = SectionTitle(section.Name);

        controls::Expander expander{};
        expander.Style(StyleNamed(L"ReportExpanderStyle"));

        controls::StackPanel header{};
        header.Orientation(controls::Orientation::Horizontal);
        header.Spacing(12);
        header.Padding(xaml::Thickness{ 0, 6, 0, 6 });

        controls::StackPanel names{};
        names.VerticalAlignment(xaml::VerticalAlignment::Center);
        names.Children().Append(MakeText(title, L"ReportSectionTitleStyle"));

        // the name in the report text too, so it can be found there
        auto const caption = section.Records.size() > 1 ?
            res::FormatString(L"ReportSectionEntriesFormat", section.Name, section.Records.size()) :
            winrt::hstring{ section.Name };

        names.Children().Append(MakeText(caption, L"ReportSecondaryTextStyle"));

        header.Children().Append(names);

        if (section.ErrorCount > 0)
        {
            header.Children().Append(MakePill(L"Critical", ErrorCountText(section.ErrorCount)));

            automation::AutomationProperties::SetName(expander,
                res::FormatString(L"ReportLabelValueFormat", title, ErrorCountText(section.ErrorCount)));
        }
        else
        {
            automation::AutomationProperties::SetName(expander, title);
        }

        expander.Header(header);

        expander.Expanding([weak = weak_from_this(), sectionIndex](controls::Expander const& sender, controls::ExpanderExpandingEventArgs const&)
            {
                try
                {
                    if (sender.Content() != nullptr)
                    {
                        return;
                    }

                    if (auto const presenter = weak.lock())
                    {
                        sender.Content(presenter->BuildSectionContent(presenter->m_report->Sections[sectionIndex]));
                    }
                }
                MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show a report section.")
            });

        return expander;
    }

    _Use_decl_annotations_
    xaml::UIElement ReportPresenter::BuildSectionContent(rpt::Section const& section)
    {
        if (section.Records.size() == 1)
        {
            return BuildFields(section.Records.front().Fields);
        }

        controls::StackPanel panel{};
        panel.Spacing(6);

        auto previousWasInline = false;

        for (auto const& record : section.Records)
        {
            auto const title = RecordTitle(record);

            // An item with a name, such as an endpoint or an app, gets a line of its own that
            // opens. A count, or a short list of settings, is shown as it is.
            if (!title.empty() && record.Fields.size() > 1)
            {
                panel.Children().Append(BuildRecordExpander(record, title));
                previousWasInline = false;
                continue;
            }

            if (previousWasInline)
            {
                shapes::Rectangle divider{};
                divider.Style(StyleNamed(L"ReportDividerStyle"));
                panel.Children().Append(divider);
            }

            panel.Children().Append(BuildFields(record.Fields));
            previousWasInline = true;
        }

        return panel;
    }

    _Use_decl_annotations_
    controls::Expander ReportPresenter::BuildRecordExpander(rpt::Record const& record, std::wstring const& title)
    {
        controls::Expander expander{};
        expander.Style(StyleNamed(L"ReportExpanderStyle"));

        size_t errors{ 0 };

        for (auto const& field : record.Fields)
        {
            errors += field.IsError() ? 1 : 0;
        }

        controls::StackPanel header{};
        header.Orientation(controls::Orientation::Horizontal);
        header.Spacing(12);
        header.Padding(xaml::Thickness{ 0, 4, 0, 4 });

        controls::StackPanel names{};
        names.VerticalAlignment(xaml::VerticalAlignment::Center);
        names.Children().Append(MakeText(winrt::hstring{ title }, L"ReportSectionTitleStyle"));

        if (auto const subtitle = RecordSubtitle(record); !subtitle.empty())
        {
            names.Children().Append(MakeText(winrt::hstring{ subtitle }, L"ReportSecondaryTextStyle"));
        }

        header.Children().Append(names);

        if (errors > 0)
        {
            header.Children().Append(MakePill(L"Critical", ErrorCountText(errors)));
        }

        expander.Header(header);

        automation::AutomationProperties::SetName(expander, errors > 0 ?
            res::FormatString(L"ReportLabelValueFormat", title, ErrorCountText(errors)) :
            winrt::hstring{ title });

        // The report never changes once it's loaded, so the record stays where it is for as
        // long as the presenter that owns the report is alive.
        auto const* const recordPointer = &record;

        expander.Expanding([weak = weak_from_this(), recordPointer](controls::Expander const& sender, controls::ExpanderExpandingEventArgs const&)
            {
                try
                {
                    if (sender.Content() != nullptr)
                    {
                        return;
                    }

                    if (auto const presenter = weak.lock())
                    {
                        sender.Content(presenter->BuildFields(recordPointer->Fields));
                    }
                }
                MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show a report entry.")
            });

        m_recordExpanders.push_back(expander);

        return expander;
    }

    _Use_decl_annotations_
    xaml::UIElement ReportPresenter::BuildFields(std::span<rpt::Field const> const fields)
    {
        controls::StackPanel panel{};
        panel.Spacing(4);

        // Plain fields that follow one another share a grid, so their values line up.
        controls::Grid grid{ nullptr };

        auto const flush = [&panel, &grid]()
            {
                if (grid != nullptr)
                {
                    panel.Children().Append(grid);
                    grid = nullptr;
                }
            };

        size_t index{ 0 };

        while (index < fields.size())
        {
            auto const& field = fields[index];

            // format 1: a blank line inside a record started a group, such as one function block
            if (field.StartsGroup)
            {
                flush();

                shapes::Rectangle divider{};
                divider.Style(StyleNamed(L"ReportDividerStyle"));
                panel.Children().Append(divider);
            }

            if (field.IsText())
            {
                flush();
                panel.Children().Append(MakeText(winrt::hstring{ field.Value }, L"ReportBodyTextStyle"));
                ++index;
                continue;
            }

            if (field.IsError())
            {
                flush();
                panel.Children().Append(BuildErrorRow(field));
                ++index;
                continue;
            }

            // A run of the same label written as pairs, such as one line per port, is a table.
            if (!field.Parts.empty())
            {
                auto end = index + 1;

                while (end < fields.size() &&
                    fields[end].Label == field.Label &&
                    !fields[end].Parts.empty() &&
                    !fields[end].StartsGroup)
                {
                    ++end;
                }

                flush();
                panel.Children().Append(BuildPartsBlock(fields.subspan(index, end - index)));

                index = end;
                continue;
            }

            if (grid == nullptr)
            {
                grid = MakeFieldGrid();
            }

            AddFieldRow(grid, field.Label, field.Value);
            ++index;
        }

        flush();

        return panel;
    }

    _Use_decl_annotations_
    xaml::UIElement ReportPresenter::BuildPartsBlock(std::span<rpt::Field const> const rows)
    {
        controls::StackPanel block{};

        auto const& label = rows.front().Label;
        auto const readable = rpt::ReadableName(label);

        auto caption = MakeText(
            rows.size() > 1 ? res::FormatString(L"ReportGroupCountFormat", readable, rows.size()) : winrt::hstring{ readable },
            L"ReportGroupCaptionStyle");

        controls::ToolTipService::SetToolTip(caption, winrt::box_value(winrt::hstring{ label }));

        block.Children().Append(caption);

        if (rows.size() == 1 && rows.front().Parts.size() > MaximumPartsAcross)
        {
            auto grid = MakeFieldGrid();

            for (auto const& part : rows.front().Parts)
            {
                AddFieldRow(grid, part.Key, part.Value);
            }

            block.Children().Append(grid);
        }
        else
        {
            block.Children().Append(BuildTable(rows));
        }

        return block;
    }

    _Use_decl_annotations_
    xaml::UIElement ReportPresenter::BuildTable(std::span<rpt::Field const> const rows)
    {
        // every key any row has, in the order they first appear, and the longest value of each
        std::vector<std::wstring_view> keys{};
        std::vector<size_t> longest{};

        for (auto const& row : rows)
        {
            for (auto const& part : row.Parts)
            {
                auto const found = std::find(keys.begin(), keys.end(), part.Key);
                auto const column = static_cast<size_t>(found - keys.begin());

                if (found == keys.end())
                {
                    keys.push_back(part.Key);
                    longest.push_back(0);
                }

                longest[column] = std::max(longest[column], part.Value.size());
            }
        }

        auto const widest = std::max_element(longest.begin(), longest.end());

        auto const wideColumn = (widest != longest.end() && *widest >= WideColumnLength) ?
            static_cast<uint32_t>(widest - longest.begin()) :
            static_cast<uint32_t>(keys.size());

        auto const hasWideColumn = wideColumn < keys.size();

        controls::Grid table{};
        table.ColumnSpacing(20);
        table.RowSpacing(3);

        // room for the scroll bar, which is drawn over the content when it shows
        table.Margin(xaml::Thickness{ 0, 0, 0, 10 });

        for (size_t column = 0; column < keys.size(); ++column)
        {
            controls::ColumnDefinition definition{};
            definition.Width(column == wideColumn ? StarLength() : AutoLength());
            table.ColumnDefinitions().Append(definition);
        }

        for (size_t row = 0; row <= rows.size(); ++row)
        {
            controls::RowDefinition definition{};
            definition.Height(AutoLength());
            table.RowDefinitions().Append(definition);
        }

        for (size_t column = 0; column < keys.size(); ++column)
        {
            auto header = MakeText(winrt::hstring{ rpt::ReadableName(keys[column]) }, L"ReportTableHeaderStyle");

            controls::ToolTipService::SetToolTip(header, winrt::box_value(winrt::hstring{ keys[column] }));
            controls::Grid::SetColumn(header, static_cast<int32_t>(column));

            table.Children().Append(header);
        }

        for (size_t row = 0; row < rows.size(); ++row)
        {
            for (size_t column = 0; column < keys.size(); ++column)
            {
                auto const* const value = rows[row].FindPart(keys[column]);

                if (value == nullptr || value->empty())
                {
                    continue;
                }

                auto cell = MakeText(winrt::hstring{ *value },
                    column == wideColumn ? L"ReportTableWideCellStyle" : L"ReportTableCellStyle");

                controls::Grid::SetRow(cell, static_cast<int32_t>(row + 1));
                controls::Grid::SetColumn(cell, static_cast<int32_t>(column));

                table.Children().Append(cell);
            }
        }

        controls::ScrollViewer scroller{};
        scroller.HorizontalScrollMode(controls::ScrollMode::Auto);
        scroller.HorizontalScrollBarVisibility(controls::ScrollBarVisibility::Auto);
        scroller.VerticalScrollMode(controls::ScrollMode::Disabled);
        scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
        scroller.Content(table);

        // A sideways scroller gives its content unlimited width, so nothing in it would wrap.
        // A table with a wide column is made exactly as wide as the window instead, and only
        // wider when its other columns leave the wide one less than its minimum.
        if (hasWideColumn)
        {
            scroller.SizeChanged([table, wideColumn](foundation::IInspectable const&, xaml::SizeChangedEventArgs const& args)
                {
                    try
                    {
                        auto const columns = table.ColumnDefinitions();

                        auto needed = MinimumWideColumnWidth +
                            table.ColumnSpacing() * static_cast<double>(columns.Size() - 1);

                        for (uint32_t column = 0; column < columns.Size(); ++column)
                        {
                            if (column != wideColumn)
                            {
                                needed += columns.GetAt(column).ActualWidth();
                            }
                        }

                        auto const width = std::max(static_cast<double>(args.NewSize().Width), needed);
                        auto const current = table.Width();

                        // set only on a real change, or each height change from wrapping would loop
                        if (std::isnan(current) || std::abs(current - width) > 0.5)
                        {
                            table.Width(width);
                        }
                    }
                    MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to fit a report table to the window.")
                });
        }

        return scroller;
    }

    _Use_decl_annotations_
    xaml::UIElement ReportPresenter::BuildErrorRow(rpt::Field const& field)
    {
        controls::Grid row{};
        row.ColumnSpacing(8);
        row.Margin(xaml::Thickness{ 0, 2, 0, 2 });

        controls::ColumnDefinition glyphColumn{};
        glyphColumn.Width(AutoLength());

        controls::ColumnDefinition textColumn{};
        textColumn.Width(StarLength());

        row.ColumnDefinitions().Append(glyphColumn);
        row.ColumnDefinitions().Append(textColumn);

        controls::FontIcon glyph{};
        glyph.Style(StyleNamed(L"ReportErrorGlyphStyle"));

        auto const message = ErrorText(field);

        auto text = MakeText(message, L"ReportErrorTextStyle");
        controls::Grid::SetColumn(text, 1);
        automation::AutomationProperties::SetName(text, res::FormatString(L"ReportErrorAutomationFormat", message));

        row.Children().Append(glyph);
        row.Children().Append(text);

        return row;
    }
}

namespace winrt::miditroubleshooter::implementation
{
    namespace
    {
        constexpr int32_t DefaultWindowWidth = 1100;
        constexpr int32_t DefaultWindowHeight = 860;
    }

    void ReportViewerWindow::RestoreWindowPlacement() noexcept
    {
        // static, because this runs before the chrome is initialized
        midiapp::WindowChrome::RestorePlacement(
            *this,
            native::AppSettings::Current().ReportViewerPlacement(),
            DefaultWindowWidth,
            DefaultWindowHeight);
    }

    void ReportViewerWindow::SaveWindowPlacement() noexcept
    {
        try
        {
            // its own placement, so it doesn't move the main window the next time that opens
            auto const placement = midiapp::WindowChrome::CapturePlacement(*this);

            if (placement.Valid)
            {
                native::AppSettings::Current().ReportViewerPlacement(placement);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to remember where the report viewer was.")
    }

    _Use_decl_annotations_
    void ReportViewerWindow::OnRootSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const&)
    {
        try
        {
            m_chrome.UpdateTitleBarInsets();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to update the title bar insets.")
    }

    _Use_decl_annotations_
    void ReportViewerWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_loaded)
            {
                return;
            }

            Title(res::GetString(L"ReportViewerTitle"));
            AppTitleTextBlock().Text(res::GetString(L"ReportViewerTitle"));

            midiapp::ApplyPreviewBadgeVisibility(PreviewChiclet());
            midiapp::MakeLiveStatusRegion(ReportSourceText());

            midiapp::WindowChromeElements elements{};

            elements.Window = *this;
            elements.Root = RootGrid();
            elements.Fill = WindowFill();
            elements.Tint = WindowTint();
            elements.TitleBar = AppTitleBar();
            elements.LeftInset = TitleBarLeftInsetColumn();
            elements.RightInset = TitleBarRightInsetColumn();

            m_chrome.Initialize(elements, native::AppSettings::Current());
            m_chrome.SetWindowIconFromResource(IDI_APPICON);

            // 32px source for a 16px slot, so it stays crisp on a high DPI display
            AppTitleBarIcon().Source(midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32));

            Closed([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_closing = true;
                        strong->SaveWindowPlacement();
                        strong->m_chrome.Shutdown();
                        strong->m_presenter.reset();
                    }
                });

            m_loaded = true;

            if (m_report.Report != nullptr)
            {
                RenderReport();
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set up the report viewer.")
    }

    _Use_decl_annotations_
    void ReportViewerWindow::ShowReport(native::LoadedReport report) noexcept
    {
        try
        {
            if (report.Report == nullptr)
            {
                return;
            }

            m_report = std::move(report);

            if (m_loaded && !m_closing)
            {
                RenderReport();
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show a report.")
    }

    void ReportViewerWindow::RenderReport() noexcept
    {
        try
        {
            auto presenter = std::make_shared<native::ReportPresenter>(RootGrid().Resources(), m_report.Report);

            presenter->Build(ReportPanel());

            m_presenter = std::move(presenter);

            EmptyReportText().Visibility(xaml::Visibility::Collapsed);
            ReportScrollViewer().Visibility(xaml::Visibility::Visible);
            ReportScrollViewer().ChangeView(nullptr, 0.0, nullptr, true);

            ExpandAllButton().IsEnabled(true);
            CollapseAllButton().IsEnabled(true);
            LoadErrorInfoBar().IsOpen(false);

            winrt::hstring source{};

            if (m_report.FilePath.empty())
            {
                source = res::GetString(L"ReportViewerSourceThisPc");
            }
            else if (!m_report.EntryName.empty())
            {
                source = res::FormatString(L"ReportViewerSourceZipFormat", m_report.EntryName, m_report.FilePath);
            }
            else
            {
                source = winrt::hstring{ m_report.FilePath };
            }

            ReportSourceText().Text(source);
            controls::ToolTipService::SetToolTip(ReportSourceText(), winrt::box_value(source));

            auto const title = m_report.FilePath.empty() ?
                res::GetString(L"ReportViewerTitle") :
                res::FormatString(L"ReportViewerTitleFormat", std::wstring{ native::FileNamePart(m_report.FilePath) });

            Title(title);
            AppTitleTextBlock().Text(title);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show a report.")
    }

    _Use_decl_annotations_
    void ReportViewerWindow::ShowLoadError(winrt::hstring const& message) noexcept
    {
        try
        {
            // whatever was showing stays, so a wrong pick doesn't lose the report being read
            LoadErrorInfoBar().Title(res::GetString(L"ReportLoadFailedTitle"));
            LoadErrorInfoBar().Message(message);
            LoadErrorInfoBar().IsOpen(true);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to report a file that couldn't be opened.")
    }

    void ReportViewerWindow::BringToFront() noexcept
    {
        try
        {
            if (auto const appWindow = AppWindow())
            {
                if (auto const presenter = appWindow.Presenter().try_as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>())
                {
                    if (presenter.State() == winrt::Microsoft::UI::Windowing::OverlappedPresenterState::Minimized)
                    {
                        presenter.Restore();
                    }
                }
            }

            Activate();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to bring the report viewer forward.")
    }

    void ReportViewerWindow::ApplyAppearance() noexcept
    {
        if (m_loaded && !m_closing)
        {
            m_chrome.ApplyTheme();
        }
    }

    void ReportViewerWindow::ApplyAlwaysOnTop() noexcept
    {
        if (m_loaded && !m_closing)
        {
            m_chrome.ApplyAlwaysOnTop();
        }
    }

    HWND ReportViewerWindow::WindowHandle() noexcept
    {
        try
        {
            HWND handle{ nullptr };

            if (auto const windowNative = this->try_as<::IWindowNative>())
            {
                windowNative->get_WindowHandle(&handle);
            }

            return handle;
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to get the window handle.")

        return nullptr;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ReportViewerWindow::OnOpenReportClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (m_opening)
            {
                co_return;
            }

            auto const path = native::ShowOpenReportDialog(WindowHandle());

            if (path.empty() || m_closing)
            {
                co_return;
            }

            m_opening = true;
            OpenReportButton().IsEnabled(false);

            auto const previousSource = ReportSourceText().Text();
            ReportSourceText().Text(res::GetString(L"ReportViewerOpening"));

            auto const finishOpening = wil::scope_exit([this, previousSource]() noexcept
                {
                    try
                    {
                        m_opening = false;

                        if (!m_closing)
                        {
                            OpenReportButton().IsEnabled(true);

                            // a report that loaded has already put its own source here
                            if (ReportSourceText().Text() == res::GetString(L"ReportViewerOpening"))
                            {
                                ReportSourceText().Text(previousSource);
                            }
                        }
                    }
                    catch (...)
                    {
                    }
                });

            native::LoadedReport loaded{};

            co_await native::RunOnBackgroundAsync([&loaded, &path]()
                {
                    loaded = native::LoadReportFile(path);
                });

            if (m_closing)
            {
                co_return;
            }

            if (loaded.Error != native::ReportLoadError::None)
            {
                ShowLoadError(native::ReportLoadErrorMessage(loaded.Error));
                co_return;
            }

            ShowReport(std::move(loaded));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to open a report.")
    }

    _Use_decl_annotations_
    void ReportViewerWindow::OnExpandAllClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_presenter != nullptr)
            {
                m_presenter->SetAllExpanded(true);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to open every report section.")
    }

    _Use_decl_annotations_
    void ReportViewerWindow::OnCollapseAllClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_presenter != nullptr)
            {
                m_presenter->SetAllExpanded(false);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to close every report section.")
    }
}
