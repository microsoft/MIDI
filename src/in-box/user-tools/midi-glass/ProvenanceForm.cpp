// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ProvenanceForm.h"

#include "SharingDialogs.h"
#include "StringResources.h"

namespace resources = ::midiglass::resources;
namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;

namespace midiglass::sharing
{
    namespace
    {
        using winrt::box_value;

        constexpr wchar_t const* SourceTerms[]
        {
            midiapp::DigitalSourceTypes::DigitalCreation,
            midiapp::DigitalSourceTypes::TrainedAlgorithmicMedia,
            midiapp::DigitalSourceTypes::CompositeWithTrainedAlgorithmicMedia,
            midiapp::DigitalSourceTypes::AlgorithmicMedia,
        };

        constexpr wchar_t const* SourceKeys[]
        {
            L"SourcePerson",
            L"SourceAi",
            L"SourcePersonWithAi",
            L"SourceProgram",
        };

        constexpr wchar_t const* OversightTerms[]
        {
            midiapp::HumanOversightLevels::FullyAutonomous,
            midiapp::HumanOversightLevels::PromptGuided,
            midiapp::HumanOversightLevels::HumanValidated,
        };

        constexpr wchar_t const* OversightKeys[]
        {
            L"OversightChoiceNone",
            L"OversightChoicePrompted",
            L"OversightChoiceChecked",
        };

        static_assert(std::size(SourceTerms) == std::size(SourceKeys));
        static_assert(std::size(OversightTerms) == std::size(OversightKeys));

        controls::Grid RowsGrid()
        {
            controls::Grid grid{};
            grid.RowSpacing(6.0);

            controls::ColumnDefinition labels{};
            labels.Width(xaml::GridLengthHelper::Auto());

            controls::ColumnDefinition values{};
            values.Width(xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star));

            grid.ColumnDefinitions().Append(labels);
            grid.ColumnDefinitions().Append(values);

            return grid;
        }

        void AddRow(
            _Inout_ controls::Grid& grid,
            _In_ winrt::hstring const& label,
            _In_ xaml::FrameworkElement const& value,
            _In_ std::wstring const& spokenValue)
        {
            auto const row = static_cast<int32_t>(grid.RowDefinitions().Size());

            controls::RowDefinition definition{};
            definition.Height(xaml::GridLengthHelper::Auto());
            grid.RowDefinitions().Append(definition);

            auto name = Paragraph(label, 12.0, L"TextFillColorSecondaryBrush");
            name.Margin({ 0, 2, 12, 0 });

            // Spoken with the value instead, so it isn't heard twice.
            automation::AutomationProperties::SetAccessibilityView(name, automation::Peers::AccessibilityView::Raw);

            controls::Grid::SetRow(name, row);
            controls::Grid::SetColumn(name, 0);
            grid.Children().Append(name);

            controls::Grid::SetRow(value, row);
            controls::Grid::SetColumn(value, 1);
            grid.Children().Append(value);

            std::wstring const labelText{ label };

            automation::AutomationProperties::SetName(
                value, resources::FormatString(L"ProvenanceRowSpokenFormat", labelText, spokenValue));
        }

        void AddTextRow(
            _Inout_ controls::Grid& grid,
            _In_ wchar_t const* labelKey,
            _In_ std::wstring const& text)
        {
            if (text.empty())
            {
                return;
            }

            auto value = Paragraph(winrt::hstring{ text }, 13.0);
            value.IsTextSelectionEnabled(true);

            AddRow(grid, resources::GetString(labelKey), value, text);
        }

        void AddBasedOnRow(
            _Inout_ controls::Grid& grid,
            _In_ midiapp::ContentProvenance const& block)
        {
            if (!block.BasedOn.has_value() || block.BasedOn->IsEmpty())
            {
                return;
            }

            auto const& source = *block.BasedOn;

            std::wstring text{ source.Name };

            if (source.BuiltIn)
            {
                text = resources::FormatString(L"ProvenanceBasedOnBuiltInFormat", source.Name);
            }
            else if (!source.Author.empty())
            {
                text = resources::FormatString(L"ProvenanceBasedOnFormat", source.Name, source.Author);
            }

            AddTextRow(grid, L"ProvenanceBasedOnLabel", text);
        }

        controls::ComboBox Choice(_In_ winrt::hstring const& header)
        {
            controls::ComboBox box{};
            box.Header(box_value(header));
            box.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            box.PlaceholderText(resources::GetString(L"ProvenanceNotSaid"));

            automation::AutomationProperties::SetName(box, header);

            return box;
        }

        // Lists the offered choices and picks the term. A term they don't cover goes last, in
        // the file's own words, and is what this returns.
        template <size_t Count>
        std::wstring FillChoices(
            _In_ controls::ComboBox const& box,
            _In_ wchar_t const* const (&terms)[Count],
            _In_ wchar_t const* const (&keys)[Count],
            _In_ std::wstring const& term,
            _In_ winrt::hstring const& otherLabel)
        {
            box.Items().Clear();

            int32_t selected{ -1 };

            for (size_t i = 0; i < Count; ++i)
            {
                box.Items().Append(box_value(resources::GetString(keys[i])));

                if (term == terms[i])
                {
                    selected = static_cast<int32_t>(i);
                }
            }

            std::wstring other{};

            if (selected < 0 && !term.empty())
            {
                other = term;
                box.Items().Append(box_value(otherLabel));
                selected = static_cast<int32_t>(Count);
            }

            box.SelectedIndex(selected);

            return other;
        }

        template <size_t Count>
        std::wstring SelectedTerm(
            _In_ controls::ComboBox const& box,
            _In_ wchar_t const* const (&terms)[Count],
            _In_ std::wstring const& other)
        {
            auto const index = box.SelectedIndex();

            if (index >= 0 && static_cast<size_t>(index) < Count)
            {
                return terms[index];
            }

            if (index == static_cast<int32_t>(Count))
            {
                return other;
            }

            return {};
        }

        winrt::hstring OtherLabel(_In_ std::wstring const& term, _In_ bool known, _In_ winrt::hstring const& words)
        {
            if (term.empty() || known)
            {
                return words;
            }

            return resources::FormatString(L"SourceAsWrittenFormat", term);
        }

        void ShowAiFields(_In_ ProvenanceForm const& form)
        {
            auto const term = SelectedTerm(form.Source, SourceTerms, form.OtherSource);

            form.AiPanel.Visibility(midiapp::InvolvesGenerativeAi(term)
                ? xaml::Visibility::Visible
                : xaml::Visibility::Collapsed);
        }
    }

    // ================================================================= pieces

    _Use_decl_annotations_
    media::Brush ThemeBrush(wchar_t const* key)
    {
        return xaml::Application::Current().Resources().Lookup(box_value(key)).as<media::Brush>();
    }

    _Use_decl_annotations_
    controls::TextBlock Paragraph(winrt::hstring const& text, double size, wchar_t const* brushKey)
    {
        controls::TextBlock block{};
        block.Text(text);
        block.TextWrapping(xaml::TextWrapping::Wrap);
        block.FontSize(size);

        if (brushKey != nullptr)
        {
            block.Foreground(ThemeBrush(brushKey));
        }

        return block;
    }

    _Use_decl_annotations_
    controls::TextBlock Heading(winrt::hstring const& text)
    {
        auto heading = Paragraph(text, 14.0);
        heading.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
        heading.Margin({ 0, 6, 0, 0 });

        return heading;
    }

    _Use_decl_annotations_
    controls::TextBox Field(winrt::hstring const& header, std::wstring const& text, int32_t maximumLength)
    {
        controls::TextBox box{};
        box.Header(box_value(header));
        box.Text(winrt::hstring{ text });
        box.MaxLength(maximumLength);

        automation::AutomationProperties::SetName(box, header);

        return box;
    }

    _Use_decl_annotations_
    std::wstring FormatDate(int64_t fileTime)
    {
        if (fileTime <= 0)
        {
            return {};
        }

        FILETIME utc{};
        utc.dwLowDateTime = static_cast<DWORD>(static_cast<uint64_t>(fileTime) & 0xFFFFFFFFu);
        utc.dwHighDateTime = static_cast<DWORD>(static_cast<uint64_t>(fileTime) >> 32);

        SYSTEMTIME universal{};

        if (!::FileTimeToSystemTime(&utc, &universal))
        {
            return {};
        }

        SYSTEMTIME local{};

        if (!::SystemTimeToTzSpecificLocalTime(nullptr, &universal, &local))
        {
            local = universal;
        }

        wchar_t buffer[128]{};

        if (::GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_LONGDATE, &local, nullptr,
            buffer, static_cast<int>(std::size(buffer)), nullptr) <= 0)
        {
            return {};
        }

        return buffer;
    }

    _Use_decl_annotations_
    winrt::hstring DescribeSource(std::wstring const& type)
    {
        namespace types = midiapp::DigitalSourceTypes;

        if (type == types::DigitalCreation)
        {
            return resources::GetString(L"SourcePerson");
        }

        if (type == types::TrainedAlgorithmicMedia)
        {
            return resources::GetString(L"SourceAi");
        }

        if (type == types::CompositeWithTrainedAlgorithmicMedia || type == types::CompositeSynthetic)
        {
            return resources::GetString(L"SourcePersonWithAi");
        }

        if (type == types::AlgorithmicMedia)
        {
            return resources::GetString(L"SourceProgram");
        }

        if (type == types::DigitalCapture)
        {
            return resources::GetString(L"SourceRecorded");
        }

        if (type == types::Composite)
        {
            return resources::GetString(L"SourceCombined");
        }

        return winrt::hstring{ type };
    }

    _Use_decl_annotations_
    winrt::hstring DescribeOversight(std::wstring const& level)
    {
        namespace levels = midiapp::HumanOversightLevels;

        if (level == levels::FullyAutonomous)
        {
            return resources::GetString(L"OversightNone");
        }

        if (level == levels::PromptGuided)
        {
            return resources::GetString(L"OversightPrompted");
        }

        if (level == levels::HumanValidated)
        {
            return resources::GetString(L"OversightChecked");
        }

        return winrt::hstring{ level };
    }

    // A website is a link only when it is plainly one, and shown in full so nobody follows a
    // name they didn't read.
    _Use_decl_annotations_
    controls::Grid ProvenanceGrid(
        std::optional<midiapp::ContentProvenance> const& provenance,
        std::wstring const& signerName)
    {
        auto grid = RowsGrid();

        if (!provenance.has_value() || provenance->IsEmpty())
        {
            auto nobody = Paragraph(resources::GetString(L"ProvenanceNobody"), 13.0, L"TextFillColorSecondaryBrush");
            controls::Grid::SetColumnSpan(nobody, 2);
            grid.Children().Append(nobody);

            return grid;
        }

        auto const& block = *provenance;

        AddTextRow(grid, L"ProvenanceAuthorLabel", DescribeMaker(provenance, signerName));
        AddTextRow(grid, L"ProvenanceOrganizationLabel", block.Organization);

        if (!block.Url.empty())
        {
            if (midiapp::IsSafeWebLink(block.Url))
            {
                controls::HyperlinkButton link{};
                link.Content(box_value(winrt::hstring{ block.Url }));
                link.NavigateUri(foundation::Uri{ winrt::hstring{ block.Url } });
                link.Padding({ 0, 0, 0, 0 });

                AddRow(grid, resources::GetString(L"ProvenanceWebsiteLabel"), link, block.Url);
            }
            else
            {
                AddTextRow(grid, L"ProvenanceWebsiteLabel", block.Url);
            }
        }

        AddTextRow(grid, L"ProvenanceLicenseLabel", block.License);
        AddTextRow(grid, L"ProvenanceVersionLabel", block.Version);
        AddTextRow(grid, L"ProvenanceCreatedLabel", FormatDate(midiapp::ParseProvenanceTime(block.Created)));
        AddTextRow(grid, L"ProvenanceToolLabel", block.Tool);

        if (!block.DigitalSourceType.empty())
        {
            AddTextRow(grid, L"ProvenanceSourceLabel", std::wstring{ DescribeSource(block.DigitalSourceType) });
        }

        if (!block.HumanOversightLevel.empty() || !block.AiModelName.empty())
        {
            std::wstring ai{};

            if (!block.HumanOversightLevel.empty())
            {
                ai = std::wstring{ DescribeOversight(block.HumanOversightLevel) };
            }

            if (!block.AiModelName.empty())
            {
                ai += ai.empty() ? L"" : L" ";
                ai += resources::FormatString(L"ProvenanceModelFormat", block.AiModelName);
            }

            AddTextRow(grid, L"ProvenanceAiLabel", ai);
        }

        AddBasedOnRow(grid, block);
        AddTextRow(grid, L"ProvenanceIdLabel", block.Id);

        return grid;
    }

    _Use_decl_annotations_
    controls::Grid ProvenanceFactsGrid(std::optional<midiapp::ContentProvenance> const& provenance)
    {
        auto grid = RowsGrid();

        if (!provenance.has_value())
        {
            return grid;
        }

        auto const& block = *provenance;

        AddTextRow(grid, L"ProvenanceCreatedLabel", FormatDate(midiapp::ParseProvenanceTime(block.Created)));
        AddTextRow(grid, L"ProvenanceToolLabel", block.Tool);
        AddBasedOnRow(grid, block);
        AddTextRow(grid, L"ProvenanceIdLabel", block.Id);

        return grid;
    }

    // ================================================================= the fields

    _Use_decl_annotations_
    std::shared_ptr<ProvenanceForm> BuildProvenanceForm(glass::ProvenanceFields const& fields, bool canEdit)
    {
        auto form = std::make_shared<ProvenanceForm>();

        auto& panel = form->Panel;
        panel.Spacing(10.0);

        auto const nameLength = static_cast<int32_t>(midiapp::MaximumProvenanceNameLength);

        form->Author = Field(resources::GetString(L"ProvenanceAuthorLabel"), {}, nameLength);
        form->Organization = Field(resources::GetString(L"ProvenanceOrganizationLabel"), {}, nameLength);
        form->Website = Field(resources::GetString(L"ProvenanceWebsiteLabel"), {},
            static_cast<int32_t>(midiapp::MaximumProvenanceUrlLength));
        form->Website.PlaceholderText(L"https://");
        form->License = Field(resources::GetString(L"ProvenanceLicenseLabel"), {}, nameLength);
        form->License.PlaceholderText(resources::GetString(L"AuthorLicensePlaceholder"));
        form->Version = Field(resources::GetString(L"ProvenanceVersionLabel"), {},
            static_cast<int32_t>(midiapp::MaximumProvenanceVersionLength));

        form->Source = Choice(resources::GetString(L"ProvenanceSourceLabel"));
        form->Oversight = Choice(resources::GetString(L"ProvenanceOversightLabel"));
        form->Model = Field(resources::GetString(L"ProvenanceModelLabel"), {}, nameLength);
        form->Model.PlaceholderText(resources::GetString(L"ProvenanceModelPlaceholder"));

        form->AiPanel = controls::StackPanel{};
        form->AiPanel.Spacing(10.0);
        form->AiPanel.Children().Append(form->Oversight);
        form->AiPanel.Children().Append(form->Model);

        for (auto const& box : { form->Author, form->Organization, form->Website, form->License, form->Version, form->Model })
        {
            box.IsEnabled(canEdit);
        }

        form->Source.IsEnabled(canEdit);
        form->Oversight.IsEnabled(canEdit);

        form->WebsiteProblem = Paragraph({}, 12.0, L"SystemFillColorCriticalBrush");
        form->WebsiteProblem.Margin({ 0, -4, 0, 0 });
        form->WebsiteProblem.Visibility(xaml::Visibility::Collapsed);
        midiapp::MakeLiveStatusRegion(form->WebsiteProblem);

        // Two short fields on one row, so the dialogs these go in stay short enough to see whole.
        controls::Grid licenseRow{};
        licenseRow.ColumnSpacing(10.0);

        controls::ColumnDefinition licenseColumn{};
        licenseColumn.Width(xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star));

        controls::ColumnDefinition versionColumn{};
        versionColumn.Width(xaml::GridLengthHelper::FromValueAndType(130.0, xaml::GridUnitType::Pixel));

        licenseRow.ColumnDefinitions().Append(licenseColumn);
        licenseRow.ColumnDefinitions().Append(versionColumn);

        controls::Grid::SetColumn(form->License, 0);
        controls::Grid::SetColumn(form->Version, 1);
        licenseRow.Children().Append(form->License);
        licenseRow.Children().Append(form->Version);

        panel.Children().Append(form->Author);
        panel.Children().Append(form->Organization);
        panel.Children().Append(form->Website);
        panel.Children().Append(form->WebsiteProblem);
        panel.Children().Append(licenseRow);
        panel.Children().Append(form->Source);
        panel.Children().Append(form->AiPanel);

        std::weak_ptr<ProvenanceForm> weak = form;

        form->Source.SelectionChanged([weak](auto&&, auto&&)
            {
                try
                {
                    if (auto const strong = weak.lock())
                    {
                        ShowAiFields(*strong);
                    }
                }
                MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the AI fields.")
            });

        FillProvenanceForm(*form, fields);

        return form;
    }

    _Use_decl_annotations_
    void FillProvenanceForm(ProvenanceForm& form, glass::ProvenanceFields const& fields)
    {
        auto const wasFilling = std::exchange(form.Filling, true);

        form.Author.Text(winrt::hstring{ fields.Author });
        form.Organization.Text(winrt::hstring{ fields.Organization });
        form.Website.Text(winrt::hstring{ fields.Url });
        form.License.Text(winrt::hstring{ fields.License });
        form.Version.Text(winrt::hstring{ fields.Version });
        form.Model.Text(winrt::hstring{ fields.AiModelName });

        auto const& source = fields.DigitalSourceType;
        auto const& oversight = fields.HumanOversightLevel;

        form.OtherSource = FillChoices(form.Source, SourceTerms, SourceKeys, source,
            OtherLabel(source, midiapp::IsKnownDigitalSourceType(source), DescribeSource(source)));

        form.OtherOversight = FillChoices(form.Oversight, OversightTerms, OversightKeys, oversight,
            OtherLabel(oversight, midiapp::IsKnownHumanOversightLevel(oversight), DescribeOversight(oversight)));

        ShowAiFields(form);

        form.Filling = wasFilling;
    }

    _Use_decl_annotations_
    glass::ProvenanceFields ReadProvenanceForm(ProvenanceForm const& form)
    {
        glass::ProvenanceFields fields{};

        fields.Author = form.Author.Text();
        fields.Organization = form.Organization.Text();
        fields.Url = form.Website.Text();
        fields.License = form.License.Text();
        fields.Version = form.Version.Text();
        fields.DigitalSourceType = SelectedTerm(form.Source, SourceTerms, form.OtherSource);
        fields.HumanOversightLevel = SelectedTerm(form.Oversight, OversightTerms, form.OtherOversight);
        fields.AiModelName = form.Model.Text();

        return fields;
    }

    _Use_decl_annotations_
    void ShowWebsiteProblem(ProvenanceForm const& form, bool show)
    {
        auto const text = show ? resources::GetString(L"AuthorWebsiteProblem") : winrt::hstring{};

        // A live region speaks on a change of text, so the same text again stays quiet.
        if (form.WebsiteProblem.Text() != text)
        {
            form.WebsiteProblem.Text(text);
        }

        form.WebsiteProblem.Visibility(show ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
    }

    _Use_decl_annotations_
    void FillProvenanceFormFromProfile(ProvenanceForm& form, midiapp::AuthorProfile const& profile)
    {
        auto const wasFilling = std::exchange(form.Filling, true);

        auto const fill = [](controls::TextBox const& box, std::wstring const& value)
            {
                if (!value.empty())
                {
                    box.Text(winrt::hstring{ value });
                }
            };

        fill(form.Author, profile.Name);
        fill(form.Organization, profile.Organization);
        fill(form.Website, profile.Url);
        fill(form.License, profile.License);

        form.Filling = wasFilling;
    }

    _Use_decl_annotations_
    bool HasAuthorProfile(midiapp::AuthorProfile const& profile) noexcept
    {
        return !profile.Name.empty() || !profile.Organization.empty() ||
            !profile.Url.empty() || !profile.License.empty();
    }

    _Use_decl_annotations_
    controls::Button MakeFillInButton(std::shared_ptr<ProvenanceForm> const& form, std::function<void()> filled)
    {
        controls::StackPanel content{};
        content.Orientation(controls::Orientation::Horizontal);
        content.Spacing(6.0);

        controls::FontIcon glyph{};
        glyph.Glyph(L"\uE77B");
        glyph.FontSize(12.0);

        controls::TextBlock text{};
        text.Text(resources::GetString(L"FillInYourInfo"));

        content.Children().Append(glyph);
        content.Children().Append(text);

        controls::Button button{};
        button.Content(content);

        automation::AutomationProperties::SetName(button, resources::GetString(L"FillInYourInfo"));
        controls::ToolTipService::SetToolTip(button, box_value(resources::GetString(L"FillInYourInfoTip")));

        std::weak_ptr<ProvenanceForm> weak = form;

        button.Click([weak, filled](auto&&, auto&&)
            {
                try
                {
                    auto const strong = weak.lock();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    FillProvenanceFormFromProfile(*strong, midiapp::LoadAuthorProfile());

                    if (filled)
                    {
                        filled();
                    }
                }
                MIDI_GLASS_CATCH_AND_LOG(L"Unable to fill in your info.")
            });

        return button;
    }

    _Use_decl_annotations_
    void WatchProvenanceForm(std::shared_ptr<ProvenanceForm> const& form, std::function<void()> changed)
    {
        if (form == nullptr || !changed)
        {
            return;
        }

        // Weak, because the boxes the handlers live on belong to the form.
        std::weak_ptr<ProvenanceForm> weak = form;

        auto const notify = [weak, changed]()
            {
                try
                {
                    auto const strong = weak.lock();

                    if (strong != nullptr && !strong->Filling)
                    {
                        changed();
                    }
                }
                MIDI_GLASS_CATCH_AND_LOG(L"Unable to take a change to who made it.")
            };

        for (auto const& box : { form->Author, form->Organization, form->Website, form->License, form->Version, form->Model })
        {
            box.TextChanged([notify](auto&&, auto&&) { notify(); });
        }

        form->Source.SelectionChanged([notify](auto&&, auto&&) { notify(); });
        form->Oversight.SelectionChanged([notify](auto&&, auto&&) { notify(); });
    }
}
