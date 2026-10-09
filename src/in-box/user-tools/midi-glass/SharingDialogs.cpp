// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SharingDialogs.h"

#include "AppProvenance.h"
#include "AppSettings.h"
#include "StringResources.h"
#include "ContentPack.h"
#include "LayoutPack.h"
#include "LayoutPackage.h"
#include "LayoutStore.h"
#include "ThemeStore.h"
#include "SignedItems.h"

#include <wil/cppwinrt_helpers.h>

#include <filesystem>
#include <fstream>

namespace resources = ::midiglass::resources;
namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;

namespace midiglass::sharing
{
    namespace
    {
        using winrt::box_value;

        constexpr double DialogWidth = 460.0;

        media::Brush ThemeBrush(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(box_value(key)).as<media::Brush>();
        }

        controls::TextBlock Paragraph(
            _In_ winrt::hstring const& text,
            _In_ double size = 14.0,
            _In_opt_ wchar_t const* brushKey = nullptr)
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

        controls::TextBox Field(
            _In_ winrt::hstring const& header,
            _In_ std::wstring const& text,
            _In_ int32_t maximumLength)
        {
            controls::TextBox box{};
            box.Header(box_value(header));
            box.Text(winrt::hstring{ text });
            box.MaxLength(maximumLength);

            automation::AutomationProperties::SetName(box, header);

            return box;
        }

        std::wstring Clean(_In_ winrt::hstring const& text, _In_ size_t maximumLength)
        {
            return midiapp::SanitizeProvenanceText(std::wstring_view{ text }, maximumLength);
        }

        std::wstring FormatDate(_In_ int64_t fileTime)
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

        std::wstring HexOf(_In_ std::vector<uint8_t> const& bytes)
        {
            static constexpr wchar_t digits[] = L"0123456789abcdef";

            std::wstring text{};

            for (auto const byte : bytes)
            {
                text += digits[byte >> 4];
                text += digits[byte & 0x0F];
            }

            return text;
        }

        bool IsTimestampServer(_In_ std::wstring const& url)
        {
            if (url.empty())
            {
                return true;
            }

            try
            {
                foundation::Uri const uri{ winrt::hstring{ url } };

                auto const scheme = std::wstring{ uri.SchemeName() };

                return (scheme == L"http" || scheme == L"https") && !uri.Host().empty() &&
                    uri.UserName().empty() && uri.Password().empty();
            }
            catch (...)
            {
                return false;
            }
        }

        foundation::IAsyncAction ShowNoticeAsync(
            xaml::XamlRoot root,
            winrt::hstring title,
            winrt::hstring body)
        {
            try
            {
                auto text = Paragraph(body);
                text.MaxWidth(DialogWidth);

                controls::ContentDialog dialog{};
                dialog.XamlRoot(root);
                dialog.Title(box_value(title));
                dialog.Content(text);
                dialog.CloseButtonText(resources::GetString(L"DialogClose"));

                co_await dialog.ShowAsync();
            }
            MIDI_GLASS_CATCH_AND_LOG(L"Unable to show a notice.")
        }

        std::wstring PickPath(
            _In_ HWND owner,
            _In_ bool saving,
            _In_ wchar_t const* titleKey,
            _In_ std::vector<std::pair<std::wstring, std::wstring>> const& filters,
            _In_ std::wstring const& defaultExtension,
            _In_ std::wstring const& suggestedName)
        {
            try
            {
                auto dialog = saving
                    ? winrt::create_instance<IFileDialog>(CLSID_FileSaveDialog)
                    : winrt::create_instance<IFileDialog>(CLSID_FileOpenDialog);

                if (dialog == nullptr)
                {
                    return {};
                }

                std::vector<COMDLG_FILTERSPEC> specs{};

                for (auto const& [name, pattern] : filters)
                {
                    specs.push_back(COMDLG_FILTERSPEC{ name.c_str(), pattern.c_str() });
                }

                dialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
                dialog->SetDefaultExtension(defaultExtension.c_str());
                dialog->SetTitle(resources::GetString(titleKey).c_str());

                if (!suggestedName.empty())
                {
                    dialog->SetFileName(suggestedName.c_str());
                }

                if (FAILED(dialog->Show(owner)))
                {
                    return {};
                }

                winrt::com_ptr<IShellItem> item{};

                if (FAILED(dialog->GetResult(item.put())) || item == nullptr)
                {
                    return {};
                }

                wil::unique_cotaskmem_string path{};

                if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, path.put())))
                {
                    return {};
                }

                return std::wstring{ path.get() };
            }
            catch (...)
            {
                return {};
            }
        }

        bool WriteFileBytes(_In_ std::wstring const& path, _In_ std::vector<uint8_t> const& bytes)
        {
            auto temporary = std::filesystem::path{ path };
            temporary += L".writing";

            {
                std::ofstream file{ temporary, std::ios::binary | std::ios::trunc };

                if (!file)
                {
                    return false;
                }

                file.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

                if (!file.good())
                {
                    file.close();
                    std::error_code ignored{};
                    std::filesystem::remove(temporary, ignored);
                    return false;
                }
            }

            std::error_code ec{};
            std::filesystem::rename(temporary, std::filesystem::path{ path }, ec);

            if (ec)
            {
                std::filesystem::remove(temporary, ec);
                return false;
            }

            return true;
        }

        std::vector<uint8_t> ReadPackFile(_In_ std::wstring const& path, _Out_ bool& tooBig)
        {
            tooBig = false;

            std::error_code ec{};
            auto const size = std::filesystem::file_size(std::filesystem::path{ path }, ec);

            if (ec)
            {
                return {};
            }

            if (size > midiapp::MaximumContentPackBytes)
            {
                tooBig = true;
                return {};
            }

            std::ifstream file{ std::filesystem::path{ path }, std::ios::binary };

            if (!file)
            {
                return {};
            }

            return std::vector<uint8_t>{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
        }

        winrt::hstring ProblemText(_In_ midiapp::ContentPackProblem problem)
        {
            using midiapp::ContentPackProblem;

            switch (problem)
            {
            case ContentPackProblem::Compressed:
                return resources::GetString(L"ImportFailedCompressed");
            case ContentPackProblem::TooBig:
            case ContentPackProblem::TooManyEntries:
                return resources::GetString(L"PackProblemTooBig");
            case ContentPackProblem::NewerFormat:
                return resources::GetString(L"PackProblemNewer");
            case ContentPackProblem::WrongKind:
                return resources::GetString(L"PackProblemWrongKind");
            case ContentPackProblem::BrokenSignature:
                return resources::GetString(L"PackProblemBrokenSignature");
            case ContentPackProblem::RevokedSignature:
                return resources::GetString(L"PackProblemRevoked");
            case ContentPackProblem::NotAPack:
            case ContentPackProblem::NoManifest:
                return resources::GetString(L"PackProblemNotAPack");
            default:
                return resources::GetString(L"PackProblemDamaged");
            }
        }

        winrt::hstring DescribeSource(_In_ std::wstring const& type)
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

            return winrt::hstring{ type };
        }

        winrt::hstring DescribeOversight(_In_ std::wstring const& level)
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

        void AddRow(
            _Inout_ controls::Grid& grid,
            _In_ winrt::hstring const& label,
            _In_ xaml::FrameworkElement const& value)
        {
            auto const row = static_cast<int32_t>(grid.RowDefinitions().Size());

            controls::RowDefinition definition{};
            definition.Height(xaml::GridLengthHelper::Auto());
            grid.RowDefinitions().Append(definition);

            auto name = Paragraph(label, 12.0, L"TextFillColorSecondaryBrush");
            name.Margin({ 0, 2, 12, 0 });

            controls::Grid::SetRow(name, row);
            controls::Grid::SetColumn(name, 0);
            grid.Children().Append(name);

            controls::Grid::SetRow(value, row);
            controls::Grid::SetColumn(value, 1);
            grid.Children().Append(value);

            automation::AutomationProperties::SetName(value, label);
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

            AddRow(grid, resources::GetString(labelKey), value);
        }

        // Everything the block says, one row each. A website is a link only when it is plainly
        // one, and shown in full so nobody follows a name they didn't read.
        controls::Grid ProvenanceGrid(
            _In_ std::optional<midiapp::ContentProvenance> const& provenance,
            _In_ std::wstring const& signerName)
        {
            controls::Grid grid{};
            grid.RowSpacing(6.0);

            controls::ColumnDefinition labels{};
            labels.Width(xaml::GridLengthHelper::Auto());

            controls::ColumnDefinition values{};
            values.Width(xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star));

            grid.ColumnDefinitions().Append(labels);
            grid.ColumnDefinitions().Append(values);

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

                    AddRow(grid, resources::GetString(L"ProvenanceWebsiteLabel"), link);
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

            if (block.BasedOn.has_value() && !block.BasedOn->IsEmpty())
            {
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

            AddTextRow(grid, L"ProvenanceIdLabel", block.Id);

            return grid;
        }

        // The line at the top of every dialog that says whether a signature backs any of it.
        controls::Grid SignatureLine(
            _In_ winrt::hstring const& headline,
            _In_ winrt::hstring const& note,
            _In_ bool trusted)
        {
            controls::Grid line{};
            line.ColumnSpacing(10.0);

            controls::ColumnDefinition iconColumn{};
            iconColumn.Width(xaml::GridLengthHelper::Auto());

            controls::ColumnDefinition textColumn{};
            textColumn.Width(xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star));

            line.ColumnDefinitions().Append(iconColumn);
            line.ColumnDefinitions().Append(textColumn);

            controls::FontIcon icon{};
            icon.Glyph(trusted ? L"\uE73E" : L"\uE946");
            icon.FontSize(16.0);
            icon.VerticalAlignment(xaml::VerticalAlignment::Top);
            icon.Margin({ 0, 2, 0, 0 });
            icon.Foreground(ThemeBrush(trusted ? L"AccentTextFillColorPrimaryBrush" : L"TextFillColorSecondaryBrush"));

            controls::Grid::SetColumn(icon, 0);
            line.Children().Append(icon);

            controls::StackPanel text{};
            text.Spacing(2.0);

            auto title = Paragraph(headline, 14.0);
            title.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());

            text.Children().Append(title);
            text.Children().Append(Paragraph(note, 12.0, L"TextFillColorSecondaryBrush"));

            controls::Grid::SetColumn(text, 1);
            line.Children().Append(text);

            automation::AutomationProperties::SetName(line, headline + L". " + note);

            return line;
        }

        // ---- the share form ----

        struct ShareForm
        {
            controls::StackPanel Panel{};

            controls::TextBox Author{ nullptr };
            controls::TextBox Organization{ nullptr };
            controls::TextBox Website{ nullptr };
            controls::TextBox License{ nullptr };
            controls::TextBox Version{ nullptr };
            controls::ComboBox Source{ nullptr };
            controls::ComboBox Certificate{ nullptr };
            controls::TextBox Timestamp{ nullptr };
            controls::TextBlock Problem{ nullptr };

            std::vector<midiapp::SigningCertificate> Certificates{};

            // A term this build has no words for, kept as it was.
            std::wstring UnlistedSource{};
        };

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

        std::unique_ptr<ShareForm> BuildShareForm(
            _In_ midiapp::ContentProvenance const& provenance,
            _In_ bool canEdit,
            _In_ winrt::hstring const& intro)
        {
            auto form = std::make_unique<ShareForm>();

            auto& panel = form->Panel;
            panel.Spacing(10.0);
            panel.Width(DialogWidth);

            panel.Children().Append(Paragraph(intro, 13.0));

            auto heading = Paragraph(resources::GetString(L"ShareAboutHeading"), 14.0);
            heading.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
            heading.Margin({ 0, 6, 0, 0 });
            panel.Children().Append(heading);

            panel.Children().Append(Paragraph(resources::GetString(L"ShareAboutNote"), 12.0, L"TextFillColorSecondaryBrush"));

            form->Author = Field(resources::GetString(L"AuthorNameLabel"), provenance.Author,
                static_cast<int32_t>(midiapp::MaximumProvenanceNameLength));
            form->Organization = Field(resources::GetString(L"AuthorOrganizationLabel"), provenance.Organization,
                static_cast<int32_t>(midiapp::MaximumProvenanceNameLength));
            form->Website = Field(resources::GetString(L"AuthorWebsiteLabel"), provenance.Url,
                static_cast<int32_t>(midiapp::MaximumProvenanceUrlLength));
            form->Website.PlaceholderText(L"https://");
            form->License = Field(resources::GetString(L"ProvenanceLicenseLabel"), provenance.License,
                static_cast<int32_t>(midiapp::MaximumProvenanceNameLength));
            form->License.PlaceholderText(resources::GetString(L"AuthorLicensePlaceholder"));
            form->Version = Field(resources::GetString(L"ProvenanceVersionLabel"), provenance.Version,
                static_cast<int32_t>(midiapp::MaximumProvenanceVersionLength));

            form->Source = controls::ComboBox{};
            form->Source.Header(box_value(resources::GetString(L"ProvenanceSourceLabel")));
            form->Source.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            automation::AutomationProperties::SetName(form->Source, resources::GetString(L"ProvenanceSourceLabel"));

            int32_t selected{ -1 };

            for (size_t i = 0; i < std::size(SourceTerms); ++i)
            {
                form->Source.Items().Append(box_value(resources::GetString(SourceKeys[i])));

                if (provenance.DigitalSourceType == SourceTerms[i])
                {
                    selected = static_cast<int32_t>(i);
                }
            }

            if (selected < 0 && !provenance.DigitalSourceType.empty())
            {
                form->UnlistedSource = provenance.DigitalSourceType;
                form->Source.Items().Append(box_value(resources::FormatString(
                    L"SourceAsWrittenFormat", provenance.DigitalSourceType)));
                selected = static_cast<int32_t>(std::size(SourceTerms));
            }

            form->Source.SelectedIndex(selected < 0 ? 0 : selected);

            for (auto const& box : { form->Author, form->Organization, form->Website, form->License, form->Version })
            {
                box.IsEnabled(canEdit);
                panel.Children().Append(box);
            }

            form->Source.IsEnabled(canEdit);
            panel.Children().Append(form->Source);

            auto signHeading = Paragraph(resources::GetString(L"ShareSignHeading"), 14.0);
            signHeading.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
            signHeading.Margin({ 0, 6, 0, 0 });
            panel.Children().Append(signHeading);

            panel.Children().Append(Paragraph(resources::GetString(L"ShareSignNote"), 12.0, L"TextFillColorSecondaryBrush"));

            form->Certificates = midiapp::ListSigningCertificates();

            form->Certificate = controls::ComboBox{};
            form->Certificate.Header(box_value(resources::GetString(L"ShareCertificateLabel")));
            form->Certificate.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            form->Certificate.Items().Append(box_value(resources::GetString(L"ShareSignNone")));
            automation::AutomationProperties::SetName(form->Certificate, resources::GetString(L"ShareCertificateLabel"));

            auto const remembered = ::midiglass::AppSettings::Current().SigningThumbprint();
            int32_t chosen{ 0 };

            for (size_t i = 0; i < form->Certificates.size(); ++i)
            {
                auto const& certificate = form->Certificates[i];

                form->Certificate.Items().Append(box_value(resources::FormatString(
                    L"ShareCertificateFormat", certificate.Name, certificate.Issuer, FormatDate(certificate.ValidTo))));

                if (!remembered.empty() && HexOf(certificate.Sha1Thumbprint) == remembered)
                {
                    chosen = static_cast<int32_t>(i + 1);
                }
            }

            form->Certificate.SelectedIndex(chosen);
            panel.Children().Append(form->Certificate);

            if (form->Certificates.empty())
            {
                form->Certificate.IsEnabled(false);
                panel.Children().Append(Paragraph(resources::GetString(L"ShareNoCertificates"), 12.0, L"TextFillColorSecondaryBrush"));
            }

            form->Timestamp = Field(resources::GetString(L"ShareTimestampLabel"),
                ::midiglass::AppSettings::Current().TimestampServer(), 1024);
            form->Timestamp.PlaceholderText(L"https://");
            form->Timestamp.IsEnabled(chosen > 0);
            panel.Children().Append(form->Timestamp);

            panel.Children().Append(Paragraph(resources::GetString(L"ShareTimestampNote"), 12.0, L"TextFillColorSecondaryBrush"));

            form->Certificate.SelectionChanged([timestamp = form->Timestamp](foundation::IInspectable const& sender, auto&&)
                {
                    if (auto const box = sender.try_as<controls::ComboBox>())
                    {
                        timestamp.IsEnabled(box.SelectedIndex() > 0);
                    }
                });

            form->Problem = Paragraph({}, 12.0, L"SystemFillColorCriticalBrush");
            form->Problem.Visibility(xaml::Visibility::Collapsed);
            panel.Children().Append(form->Problem);

            return form;
        }

        // What the form says, or a sentence about what is wrong with it.
        winrt::hstring ReadShareForm(
            _In_ ShareForm const& form,
            _Inout_ midiapp::ContentProvenance& provenance)
        {
            auto const website = Clean(form.Website.Text(), midiapp::MaximumProvenanceUrlLength);

            if (!website.empty() && !midiapp::IsSafeWebLink(website))
            {
                return resources::GetString(L"AuthorWebsiteProblem");
            }

            auto const timestamp = Clean(form.Timestamp.Text(), 1024);

            if (form.Certificate.SelectedIndex() > 0 && !IsTimestampServer(timestamp))
            {
                return resources::GetString(L"ShareTimestampProblem");
            }

            provenance.Author = Clean(form.Author.Text(), midiapp::MaximumProvenanceNameLength);
            provenance.Organization = Clean(form.Organization.Text(), midiapp::MaximumProvenanceNameLength);
            provenance.Url = website;
            provenance.License = Clean(form.License.Text(), midiapp::MaximumProvenanceNameLength);
            provenance.Version = Clean(form.Version.Text(), midiapp::MaximumProvenanceVersionLength);

            auto const index = form.Source.SelectedIndex();

            if (index >= 0 && static_cast<size_t>(index) < std::size(SourceTerms))
            {
                provenance.DigitalSourceType = SourceTerms[index];
            }
            else if (!form.UnlistedSource.empty())
            {
                provenance.DigitalSourceType = form.UnlistedSource;
            }

            return {};
        }

        bool SameProvenance(
            _In_ midiapp::ContentProvenance const& left,
            _In_ midiapp::ContentProvenance const& right)
        {
            return midiapp::ProvenanceToJsonText(left, 0) == midiapp::ProvenanceToJsonText(right, 0);
        }

        struct PackOutcome
        {
            bool Succeeded{ false };
            winrt::hstring Failure{};
            uint32_t FileCount{ 0 };

            std::wstring SignerName{};
            bool SignatureTrusted{ false };
        };

        // Runs off the UI thread: signing can ask a token for a PIN, and a timestamp goes out to the network.
        PackOutcome FinishPack(
            _In_ glass::PackBuildResult built,
            _In_ midiapp::ContentPackKind kind,
            _In_ std::vector<uint8_t> const& thumbprint,
            _In_ std::wstring const& timestampServer,
            _In_ std::wstring const& target)
        {
            PackOutcome outcome{};

            if (!built.Succeeded)
            {
                outcome.Failure = resources::GetString(built.FailureKey.empty() ? L"PackageFailedWrite" : built.FailureKey.c_str());
                return outcome;
            }

            auto bytes = std::move(built.Bytes);

            if (!thumbprint.empty())
            {
                HRESULT error{ S_OK };

                auto signedBytes = midiapp::SignContentPackWithStoreCertificate(bytes, thumbprint, timestampServer, error);

                if (signedBytes.empty())
                {
                    outcome.Failure = resources::FormatString(L"PackSignFailedFormat",
                        std::format(L"0x{:08X}", static_cast<uint32_t>(error)));
                    return outcome;
                }

                bytes = std::move(signedBytes);

                // Checked the way the PC that imports it will check it, so the publisher hears
                // now if Windows doesn't trust their certificate.
                auto const check = midiapp::OpenContentPack(bytes, kind, midiapp::ContentPackVerifyOptions{});

                for (auto const& signature : check.Signatures)
                {
                    outcome.SignerName = signature.SignerName;
                    outcome.SignatureTrusted = outcome.SignatureTrusted || signature.IsTrusted();
                }
            }

            if (!WriteFileBytes(target, bytes))
            {
                outcome.Failure = resources::GetString(L"PackageFailedWrite");
                return outcome;
            }

            outcome.FileCount = built.FileCount;
            outcome.Succeeded = true;

            return outcome;
        }

        winrt::hstring DescribeOutcome(_In_ PackOutcome const& outcome, _In_ std::wstring const& target)
        {
            std::wstring text{ resources::FormatString(L"PackDoneFormat",
                std::filesystem::path{ target }.filename().wstring(), std::to_wstring(outcome.FileCount)) };

            text += L" ";

            if (outcome.SignerName.empty())
            {
                text += resources::GetString(L"PackDoneNotSigned");
            }
            else
            {
                text += resources::FormatString(
                    outcome.SignatureTrusted ? L"PackDoneSignedFormat" : L"PackDoneSignedUntrustedFormat",
                    outcome.SignerName);
            }

            return winrt::hstring{ text };
        }

        // Asks, then remembers the choices. False when the customer canceled.
        foundation::IAsyncOperation<bool> AskShareAsync(
            xaml::XamlRoot root,
            winrt::hstring title,
            ShareForm* form,
            midiapp::ContentProvenance* provenance)
        {
            controls::ScrollViewer scroller{};
            scroller.Content(form->Panel);
            scroller.MaxHeight(560.0);
            scroller.Padding({ 0, 0, 16, 0 });

            controls::ContentDialog dialog{};
            dialog.XamlRoot(root);
            dialog.Title(box_value(title));
            dialog.Content(scroller);
            dialog.PrimaryButtonText(resources::GetString(L"ShareSaveAction"));
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);

            dialog.PrimaryButtonClick([form, provenance](auto const&, controls::ContentDialogButtonClickEventArgs const& args)
                {
                    try
                    {
                        auto edited = *provenance;
                        auto const problem = ReadShareForm(*form, edited);

                        if (!problem.empty())
                        {
                            form->Problem.Text(problem);
                            form->Problem.Visibility(xaml::Visibility::Visible);
                            args.Cancel(true);
                            return;
                        }

                        *provenance = edited;
                    }
                    MIDI_GLASS_CATCH_AND_LOG(L"Unable to read the share form.")
                });

            if (co_await dialog.ShowAsync() != controls::ContentDialogResult::Primary)
            {
                co_return false;
            }

            auto& settings = ::midiglass::AppSettings::Current();
            auto const index = form->Certificate.SelectedIndex();

            settings.SigningThumbprint(index > 0 && static_cast<size_t>(index) <= form->Certificates.size()
                ? HexOf(form->Certificates[static_cast<size_t>(index - 1)].Sha1Thumbprint)
                : std::wstring{});

            if (index > 0)
            {
                settings.TimestampServer(Clean(form->Timestamp.Text(), 1024));
            }

            co_return true;
        }

        std::vector<uint8_t> ChosenThumbprint(_In_ ShareForm const& form)
        {
            auto const index = form.Certificate.SelectedIndex();

            if (index <= 0 || static_cast<size_t>(index) > form.Certificates.size())
            {
                return {};
            }

            return form.Certificates[static_cast<size_t>(index - 1)].Sha1Thumbprint;
        }
    }

    // ================================================================= public

    _Use_decl_annotations_
    std::wstring DescribeMaker(
        std::optional<midiapp::ContentProvenance> const& provenance,
        std::wstring const& signerName)
    {
        if (!provenance.has_value() || provenance->Author.empty())
        {
            return {};
        }

        if (!signerName.empty())
        {
            return provenance->Author;
        }

        return std::wstring{ resources::FormatString(L"ProvenanceUnverifiedFormat", provenance->Author) };
    }

    _Use_decl_annotations_
    std::wstring SignerOf(std::wstring const& filePath)
    {
        auto const item = glass::SignedItemFor(filePath);

        return item.has_value() ? item->SignerName : std::wstring{};
    }

    _Use_decl_annotations_
    std::wstring PickImportPath(HWND owner)
    {
        return PickPath(
            owner,
            false,
            L"ImportOpenTitle",
            {
                { std::wstring{ resources::GetString(L"PackFilterImport") }, L"*.midilayoutpack;*.midithemepack;*.zip" },
                { std::wstring{ resources::GetString(L"PackFilterLayout") }, L"*.midilayoutpack" },
                { std::wstring{ resources::GetString(L"PackFilterTheme") }, L"*.midithemepack" },
            },
            L"midilayoutpack",
            {});
    }

    _Use_decl_annotations_
    foundation::IAsyncAction ShareLayoutAsync(
        xaml::XamlRoot root,
        HWND owner,
        std::wstring layoutFilePath,
        std::function<bool(midiapp::ContentProvenance const&)> apply)
    {
        auto const dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        try
        {
            auto read = glass::ReadLayoutFile(layoutFilePath);

            if (!read.Succeeded)
            {
                co_await ShowNoticeAsync(root, resources::GetString(L"PackFailedTitle"), resources::GetString(L"PackageFailedUnreadable"));
                co_return;
            }

            if (glass::SurveyLayoutPackage(layoutFilePath).TotalBytes > midiapp::MaximumContentPackBytes)
            {
                co_await ShowNoticeAsync(root, resources::GetString(L"PackFailedTitle"), resources::GetString(L"PackFailedTooBig"));
                co_return;
            }

            auto const fileName = std::filesystem::path{ layoutFilePath }.filename().wstring();
            auto const stem = glass::LayoutNameFromFileName(fileName);
            auto const name = read.Document.Name.empty() ? stem : read.Document.Name;

            auto const original = read.Document.Provenance;
            auto provenance = original.has_value() ? *original : ::midiglass::NewProvenance();

            // A newer version's layout is never written, so what it says is packed as it is.
            auto const canEdit = !read.Document.IsFromNewerVersion;

            auto form = BuildShareForm(provenance, canEdit, resources::GetString(L"ShareIntroLayout"));

            if (!co_await AskShareAsync(root, resources::FormatString(L"ShareTitleFormat", name), form.get(), &provenance))
            {
                co_return;
            }

            if (canEdit && (!original.has_value() || !SameProvenance(*original, provenance)))
            {
                read.Document.Provenance = provenance;

                auto const written = apply
                    ? apply(provenance)
                    : glass::WriteLayoutFile(read.Document, layoutFilePath);

                if (!written)
                {
                    co_await ShowNoticeAsync(root, resources::GetString(L"PackFailedTitle"), resources::GetString(L"PackageFailedWrite"));
                    co_return;
                }
            }

            auto const target = PickPath(
                owner,
                true,
                L"PackSaveTitle",
                { { std::wstring{ resources::GetString(L"PackFilterLayout") }, L"*.midilayoutpack" } },
                L"midilayoutpack",
                stem + glass::LayoutPackExtension);

            if (target.empty())
            {
                co_return;
            }

            auto const thumbprint = ChosenThumbprint(*form);
            auto const timestamp = Clean(form->Timestamp.Text(), 1024);

            co_await winrt::resume_background();

            auto const outcome = FinishPack(
                glass::BuildLayoutPack(layoutFilePath), midiapp::ContentPackKind::GlassLayout, thumbprint, timestamp, target);

            co_await wil::resume_foreground(dispatcher);

            co_await ShowNoticeAsync(
                root,
                resources::GetString(outcome.Succeeded ? L"PackDoneTitle" : L"PackFailedTitle"),
                outcome.Succeeded ? DescribeOutcome(outcome, target) : outcome.Failure);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to pack the layout.")
    }

    _Use_decl_annotations_
    foundation::IAsyncAction ShareThemeAsync(xaml::XamlRoot root, HWND owner, std::wstring themeFilePath)
    {
        auto const dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        try
        {
            auto read = glass::ReadThemeFile(themeFilePath);

            if (!read.Succeeded || read.Value.Name.empty())
            {
                co_await ShowNoticeAsync(root, resources::GetString(L"PackFailedTitle"), resources::GetString(L"ImportThemeFailedDetail"));
                co_return;
            }

            auto const original = read.Value.Provenance;
            auto provenance = original.has_value() ? *original : ::midiglass::NewProvenance();
            auto const canEdit = !read.IsFromNewerVersion;

            auto form = BuildShareForm(provenance, canEdit, resources::GetString(L"ShareIntroTheme"));

            if (!co_await AskShareAsync(root, resources::FormatString(L"ShareTitleFormat", read.Value.Name), form.get(), &provenance))
            {
                co_return;
            }

            if (canEdit && (!original.has_value() || !SameProvenance(*original, provenance)))
            {
                auto theme = read.Value;
                theme.Provenance = provenance;

                if (!glass::WriteThemeFile(theme, themeFilePath))
                {
                    co_await ShowNoticeAsync(root, resources::GetString(L"PackFailedTitle"), resources::GetString(L"PackageFailedWrite"));
                    co_return;
                }
            }

            auto suggested = std::filesystem::path{ themeFilePath }.filename().wstring();

            if (auto const dot = suggested.find(L'.'); dot != std::wstring::npos)
            {
                suggested = suggested.substr(0, dot);
            }

            auto const target = PickPath(
                owner,
                true,
                L"PackSaveTitle",
                { { std::wstring{ resources::GetString(L"PackFilterTheme") }, L"*.midithemepack" } },
                L"midithemepack",
                suggested + glass::ThemePackExtension);

            if (target.empty())
            {
                co_return;
            }

            auto const thumbprint = ChosenThumbprint(*form);
            auto const timestamp = Clean(form->Timestamp.Text(), 1024);

            co_await winrt::resume_background();

            auto const outcome = FinishPack(
                glass::BuildThemePack(themeFilePath), midiapp::ContentPackKind::GlassTheme, thumbprint, timestamp, target);

            co_await wil::resume_foreground(dispatcher);

            co_await ShowNoticeAsync(
                root,
                resources::GetString(outcome.Succeeded ? L"PackDoneTitle" : L"PackFailedTitle"),
                outcome.Succeeded ? DescribeOutcome(outcome, target) : outcome.Failure);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to pack the theme.")
    }

    _Use_decl_annotations_
    foundation::IAsyncAction ImportPackAsync(
        xaml::XamlRoot root,
        std::wstring packPath,
        std::function<void(ImportOutcome, std::wstring const&)> installed)
    {
        auto const dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        try
        {
            // Checking a signature can mean asking the network whether a certificate was withdrawn.
            co_await winrt::resume_background();

            auto tooBig = false;
            auto const bytes = ReadPackFile(packPath, tooBig);

            auto opened = bytes.empty()
                ? midiapp::OpenedContentPack{}
                : midiapp::OpenContentPack(bytes, midiapp::ContentPackKind::Unknown, midiapp::ContentPackVerifyOptions{});

            if (tooBig)
            {
                opened.Problem = midiapp::ContentPackProblem::TooBig;
            }

            glass::PackSummary summary{};
            auto const described = opened.Succeeded() && glass::SummarizePack(opened, summary);

            auto const existingSigner = summary.ExistingPath.empty() ? std::wstring{} : SignerOf(summary.ExistingPath);

            co_await wil::resume_foreground(dispatcher);

            if (!opened.Succeeded())
            {
                co_await ShowNoticeAsync(root, resources::GetString(L"ImportFailedTitle"), ProblemText(opened.Problem));
                co_return;
            }

            if (!described)
            {
                co_await ShowNoticeAsync(root, resources::GetString(L"ImportFailedTitle"), resources::GetString(L"PackProblemNotAPack"));
                co_return;
            }

            auto const trusted = opened.TrustedSignature();
            auto const signer = trusted != nullptr ? trusted->SignerName : std::wstring{};

            controls::StackPanel panel{};
            panel.Spacing(12.0);
            panel.Width(DialogWidth);

            if (trusted != nullptr)
            {
                panel.Children().Append(SignatureLine(
                    resources::FormatString(L"ImportSignedFormat", signer),
                    resources::GetString(trusted->Status == midiapp::PackSignatureStatus::Trusted
                        ? L"ImportSignedNote"
                        : L"ImportSignedOfflineNote"),
                    true));
            }
            else if (!opened.Signatures.empty())
            {
                panel.Children().Append(SignatureLine(
                    resources::FormatString(L"ImportUntrustedFormat", opened.Signatures.front().SignerName),
                    resources::GetString(L"ImportUntrustedNote"),
                    false));
            }
            else
            {
                panel.Children().Append(SignatureLine(
                    resources::GetString(L"ImportNotSigned"),
                    resources::GetString(L"ImportNotSignedNote"),
                    false));
            }

            if (!summary.Description.empty())
            {
                panel.Children().Append(Paragraph(winrt::hstring{ summary.Description }, 13.0));
            }

            panel.Children().Append(ProvenanceGrid(summary.Provenance, signer));

            panel.Children().Append(Paragraph(
                resources::FormatString(L"ImportContainsFormat",
                    std::to_wstring(summary.FileCount), resources::DescribeFileSize(summary.TotalBytes)),
                12.0,
                L"TextFillColorSecondaryBrush"));

            auto const isTheme = summary.Kind == midiapp::ContentPackKind::GlassTheme;
            auto const sameItem = !summary.ExistingPath.empty();
            auto const nameTaken = isTheme && (!summary.NameTakenByPath.empty() || summary.NameIsBuiltIn);

            if (sameItem)
            {
                auto const had = summary.ExistingProvenance.has_value() ? summary.ExistingProvenance->Version : std::wstring{};
                auto const has = summary.Provenance.has_value() ? summary.Provenance->Version : std::wstring{};

                panel.Children().Append(Paragraph(
                    (had.empty() || has.empty())
                        ? resources::GetString(L"ImportUpdateNoVersion")
                        : resources::FormatString(L"ImportUpdateFormat", had, has),
                    13.0));

                if (!existingSigner.empty() && existingSigner != signer)
                {
                    panel.Children().Append(Paragraph(
                        resources::FormatString(L"ImportWasSignedFormat", existingSigner),
                        13.0,
                        L"SystemFillColorCautionBrush"));
                }
            }
            else if (nameTaken)
            {
                panel.Children().Append(Paragraph(
                    resources::FormatString(summary.NameIsBuiltIn ? L"ImportThemeNameBuiltInFormat" : L"ImportThemeNameTakenFormat", summary.Name),
                    13.0));
            }

            controls::ScrollViewer scroller{};
            scroller.Content(panel);
            scroller.MaxHeight(560.0);
            scroller.Padding({ 0, 0, 16, 0 });

            controls::ContentDialog dialog{};
            dialog.XamlRoot(root);
            dialog.Title(box_value(resources::FormatString(L"ImportPackTitleFormat", summary.Name)));
            dialog.Content(scroller);
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Close);

            // Replacing is offered only for the same item, or for a theme whose name is taken by
            // one the customer made. A built-in theme is never replaced.
            auto const canReplace = sameItem || (nameTaken && !summary.NameIsBuiltIn);

            if (canReplace)
            {
                dialog.PrimaryButtonText(resources::GetString(L"ImportReplaceAction"));
                dialog.SecondaryButtonText(resources::GetString(L"ImportKeepBothAction"));
            }
            else
            {
                dialog.PrimaryButtonText(resources::GetString(nameTaken ? L"ImportKeepBothAction" : L"ImportAction"));
            }

            auto const answer = co_await dialog.ShowAsync();

            if (answer == controls::ContentDialogResult::None)
            {
                co_return;
            }

            auto const replace = canReplace && answer == controls::ContentDialogResult::Primary;

            std::wstring replacePath{};
            std::wstring newName{};

            if (replace)
            {
                replacePath = sameItem ? summary.ExistingPath : summary.NameTakenByPath;
            }
            else if (isTheme && (nameTaken || (sameItem && !replace)))
            {
                newName = glass::UnusedThemeName(summary.Name);
            }

            co_await winrt::resume_background();

            auto const result = isTheme
                ? glass::InstallThemePack(opened, replacePath, newName)
                : glass::InstallLayoutPack(opened, glass::LayoutsFolder(), replacePath);

            if (result.Succeeded)
            {
                if (trusted != nullptr)
                {
                    glass::SignedItem item{};
                    item.FilePath = result.Path;
                    item.SignerName = trusted->SignerName;
                    item.IssuerName = trusted->IssuerName;
                    item.Thumbprint = trusted->Thumbprint;
                    item.SignedAt = trusted->SignedAt;

                    glass::RememberSignedItem(item, result.Files);
                }
                else
                {
                    glass::ForgetSignedItem(result.Path);
                }
            }

            co_await wil::resume_foreground(dispatcher);

            if (!result.Succeeded)
            {
                co_await ShowNoticeAsync(root, resources::GetString(L"ImportFailedTitle"),
                    resources::GetString(result.FailureKey.empty() ? L"ImportFailedWrite" : result.FailureKey.c_str()));
                co_return;
            }

            if (installed)
            {
                installed(isTheme ? ImportOutcome::Theme : ImportOutcome::Layout, result.Path);
            }

            co_await ShowNoticeAsync(
                root,
                resources::GetString(L"ImportDoneTitle"),
                isTheme
                    ? resources::FormatString(L"ImportThemeDoneFormat", newName.empty() ? summary.Name : newName)
                    : resources::FormatString(L"ImportDoneFormat",
                        std::filesystem::path{ result.Path }.filename().wstring(), std::to_wstring(result.Files.size())));
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to import the pack.")
    }

    _Use_decl_annotations_
    foundation::IAsyncAction ShowLayoutDetailsAsync(xaml::XamlRoot root, std::wstring layoutFilePath)
    {
        auto const dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        try
        {
            co_await winrt::resume_background();

            auto const read = glass::ReadLayoutFile(layoutFilePath);
            auto const signedItem = glass::SignedItemFor(layoutFilePath);

            co_await wil::resume_foreground(dispatcher);

            if (!read.Succeeded)
            {
                co_await ShowNoticeAsync(root, resources::GetString(L"DetailsFailedTitle"), resources::GetString(L"PackageFailedUnreadable"));
                co_return;
            }

            auto const name = read.Document.Name.empty()
                ? glass::LayoutNameFromFileName(std::filesystem::path{ layoutFilePath }.filename().wstring())
                : read.Document.Name;

            controls::StackPanel panel{};
            panel.Spacing(12.0);
            panel.Width(DialogWidth);

            panel.Children().Append(signedItem.has_value()
                ? SignatureLine(
                    resources::FormatString(L"DetailsSignedFormat", signedItem->SignerName),
                    resources::FormatString(L"DetailsSignedNoteFormat", signedItem->IssuerName),
                    true)
                : SignatureLine(
                    resources::GetString(L"DetailsNotSigned"),
                    resources::GetString(L"DetailsNotSignedNote"),
                    false));

            panel.Children().Append(ProvenanceGrid(
                read.Document.Provenance, signedItem.has_value() ? signedItem->SignerName : std::wstring{}));

            controls::ScrollViewer scroller{};
            scroller.Content(panel);
            scroller.MaxHeight(560.0);
            scroller.Padding({ 0, 0, 16, 0 });

            controls::ContentDialog dialog{};
            dialog.XamlRoot(root);
            dialog.Title(box_value(resources::FormatString(L"DetailsTitleFormat", name)));
            dialog.Content(scroller);
            dialog.CloseButtonText(resources::GetString(L"DialogClose"));

            co_await dialog.ShowAsync();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the layout's details.")
    }

    _Use_decl_annotations_
    foundation::IAsyncAction ShowThemeDetailsAsync(xaml::XamlRoot root, glass::Theme theme)
    {
        auto const dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        try
        {
            co_await winrt::resume_background();

            auto const signedItem = theme.FilePath.empty()
                ? std::optional<glass::SignedItem>{}
                : glass::SignedItemFor(theme.FilePath);

            co_await wil::resume_foreground(dispatcher);

            controls::StackPanel panel{};
            panel.Spacing(12.0);
            panel.Width(DialogWidth);

            if (glass::FindBuiltInTheme(theme.Name) != nullptr && theme.FilePath.empty())
            {
                panel.Children().Append(SignatureLine(
                    resources::GetString(L"DetailsBuiltIn"),
                    resources::GetString(L"DetailsBuiltInNote"),
                    true));
            }
            else
            {
                panel.Children().Append(signedItem.has_value()
                    ? SignatureLine(
                        resources::FormatString(L"DetailsSignedFormat", signedItem->SignerName),
                        resources::FormatString(L"DetailsSignedNoteFormat", signedItem->IssuerName),
                        true)
                    : SignatureLine(
                        resources::GetString(L"DetailsNotSigned"),
                        resources::GetString(L"DetailsNotSignedNote"),
                        false));

                panel.Children().Append(ProvenanceGrid(
                    theme.Provenance, signedItem.has_value() ? signedItem->SignerName : std::wstring{}));
            }

            controls::ScrollViewer scroller{};
            scroller.Content(panel);
            scroller.MaxHeight(560.0);
            scroller.Padding({ 0, 0, 16, 0 });

            controls::ContentDialog dialog{};
            dialog.XamlRoot(root);
            dialog.Title(box_value(resources::FormatString(L"DetailsTitleFormat", theme.Name)));
            dialog.Content(scroller);
            dialog.CloseButtonText(resources::GetString(L"DialogClose"));

            co_await dialog.ShowAsync();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the theme's details.")
    }

    _Use_decl_annotations_
    foundation::IAsyncAction EditAuthorProfileAsync(xaml::XamlRoot root)
    {
        try
        {
            auto const profile = midiapp::LoadAuthorProfile();

            controls::StackPanel panel{};
            panel.Spacing(10.0);
            panel.Width(DialogWidth);

            panel.Children().Append(Paragraph(resources::GetString(L"AuthorIntro"), 13.0));

            auto name = Field(resources::GetString(L"AuthorNameLabel"), profile.Name,
                static_cast<int32_t>(midiapp::MaximumProvenanceNameLength));
            auto organization = Field(resources::GetString(L"AuthorOrganizationLabel"), profile.Organization,
                static_cast<int32_t>(midiapp::MaximumProvenanceNameLength));
            auto website = Field(resources::GetString(L"AuthorWebsiteLabel"), profile.Url,
                static_cast<int32_t>(midiapp::MaximumProvenanceUrlLength));
            website.PlaceholderText(L"https://");
            auto license = Field(resources::GetString(L"AuthorLicenseLabel"), profile.License,
                static_cast<int32_t>(midiapp::MaximumProvenanceNameLength));
            license.PlaceholderText(resources::GetString(L"AuthorLicensePlaceholder"));

            panel.Children().Append(name);
            panel.Children().Append(organization);
            panel.Children().Append(website);
            panel.Children().Append(license);
            panel.Children().Append(Paragraph(resources::GetString(L"AuthorLicenseNote"), 12.0, L"TextFillColorSecondaryBrush"));

            auto problem = Paragraph({}, 12.0, L"SystemFillColorCriticalBrush");
            problem.Visibility(xaml::Visibility::Collapsed);
            panel.Children().Append(problem);

            controls::ContentDialog dialog{};
            dialog.XamlRoot(root);
            dialog.Title(box_value(resources::GetString(L"AuthorTitle")));
            dialog.Content(panel);
            dialog.PrimaryButtonText(resources::GetString(L"CommonSave"));
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);

            dialog.PrimaryButtonClick([website, problem](auto const&, controls::ContentDialogButtonClickEventArgs const& args)
                {
                    auto const url = Clean(website.Text(), midiapp::MaximumProvenanceUrlLength);

                    if (!url.empty() && !midiapp::IsSafeWebLink(url))
                    {
                        problem.Text(resources::GetString(L"AuthorWebsiteProblem"));
                        problem.Visibility(xaml::Visibility::Visible);
                        args.Cancel(true);
                    }
                });

            if (co_await dialog.ShowAsync() != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            midiapp::AuthorProfile updated{};
            updated.Name = Clean(name.Text(), midiapp::MaximumProvenanceNameLength);
            updated.Organization = Clean(organization.Text(), midiapp::MaximumProvenanceNameLength);
            updated.Url = Clean(website.Text(), midiapp::MaximumProvenanceUrlLength);
            updated.License = Clean(license.Text(), midiapp::MaximumProvenanceNameLength);

            midiapp::SaveAuthorProfile(updated);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change your info.")
    }
}
