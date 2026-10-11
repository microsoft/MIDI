// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Who made the layout, on the About page of the layout settings, and which theme the layout
// uses with what that theme's file says about itself.
//
// The fields write as somebody types, like the name and description above them, so there is
// no Save button to forget. Reading them back has to give the block unchanged, or just opening
// the page would rewrite the file; glass::ApplyProvenanceFields is tested for exactly that.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "ProvenanceForm.h"
#include "SharingDialogs.h"
#include "StringResources.h"
#include "ThemeStore.h"

namespace resources = ::midiglass::resources;
namespace sharing = ::midiglass::sharing;

namespace winrt::midiglass::implementation
{
    namespace
    {
        namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;

        xaml::Style AppStyle(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(box_value(key)).as<xaml::Style>();
        }

        controls::TextBlock StyledText(_In_ winrt::hstring const& text, _In_ wchar_t const* styleKey)
        {
            controls::TextBlock block{};
            block.Style(AppStyle(styleKey));
            block.Text(text);

            return block;
        }

        controls::Button SmallButton(_In_ wchar_t const* glyph, _In_ winrt::hstring const& label)
        {
            controls::StackPanel content{};
            content.Orientation(controls::Orientation::Horizontal);
            content.Spacing(6.0);

            controls::FontIcon icon{};
            icon.Glyph(glyph);
            icon.FontSize(12.0);

            controls::TextBlock text{};
            text.Text(label);

            content.Children().Append(icon);
            content.Children().Append(text);

            controls::Button button{};
            button.Style(AppStyle(L"EditorSmallButtonStyle"));
            button.Content(content);

            automation::AutomationProperties::SetName(button, label);

            return button;
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OpenSettingsPaneWhenLoaded(std::wstring const& tag)
    {
        try
        {
            if (!m_loaded)
            {
                m_pendingSettingsPane = tag;
                return;
            }

            m_settingsPane = tag;
            ShowLayoutSettings(true);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to open the layout settings.")
    }

    // ------------------------------------------------------------ who made it

    void EditorWindow::RefreshAboutPanel()
    {
        try
        {
            AboutPanel().Children().Clear();

            auto const& document = m_editor.Document();

            AboutPanel().Children().Append(StyledText(resources::GetString(L"ShareAboutHeading"), L"SettingsHeadingStyle"));

            auto caption = StyledText(resources::GetString(L"SettingsAboutCaption"), L"SettingsCaptionStyle");
            caption.Margin({ 0, 0, 0, 10 });
            AboutPanel().Children().Append(caption);

            m_aboutForm = sharing::BuildProvenanceForm(glass::FieldsOf(document.Provenance), true);

            sharing::WatchProvenanceForm(m_aboutForm, [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->OnAboutFieldsChanged();
                    }
                });

            // Half an address is normal while it is being typed, so the problem is only pointed
            // out once somebody moves on.
            m_aboutForm->Website.LostFocus([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->ShowAboutWebsiteProblem();
                    }
                });

            AboutPanel().Children().Append(m_aboutForm->Panel);

            controls::StackPanel buttons{};
            buttons.Orientation(controls::Orientation::Horizontal);
            buttons.Spacing(7.0);
            buttons.Margin({ 0, 12, 0, 0 });

            m_aboutFillButton = sharing::MakeFillInButton(m_aboutForm, [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->OnAboutFieldsChanged();
                    }
                });

            m_aboutFillButton.Style(AppStyle(L"EditorSmallButtonStyle"));
            m_aboutFillButton.IsEnabled(sharing::HasAuthorProfile(midiapp::LoadAuthorProfile()));
            buttons.Children().Append(m_aboutFillButton);

            auto change = SmallButton(L"\uE70F", resources::GetString(L"SettingsAboutChangeInfo"));

            change.Click([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->ChangeAuthorInfoAsync();
                    }
                });

            buttons.Children().Append(change);
            AboutPanel().Children().Append(buttons);

            auto note = StyledText(resources::GetString(L"SettingsAboutInfoNote"), L"SettingsCaptionStyle");
            note.Margin({ 0, 6, 0, 0 });
            AboutPanel().Children().Append(note);

            auto facts = sharing::ProvenanceFactsGrid(document.Provenance);

            if (facts.Children().Size() > 0)
            {
                auto heading = StyledText(resources::GetString(L"SettingsAboutDetailsHeading"), L"SettingsHeadingStyle");
                heading.Margin({ 0, 18, 0, 8 });

                AboutPanel().Children().Append(heading);
                AboutPanel().Children().Append(facts);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show who made the layout.")
    }

    void EditorWindow::OnAboutFieldsChanged()
    {
        try
        {
            if (m_aboutForm == nullptr)
            {
                return;
            }

            auto const applied = glass::ApplyProvenanceFields(
                m_editor.Document().Provenance, sharing::ReadProvenanceForm(*m_aboutForm));

            if (!applied.WebsiteRejected)
            {
                sharing::ShowWebsiteProblem(*m_aboutForm, false);
            }

            if (applied.Provenance.has_value() && m_editor.SetProvenance(*applied.Provenance))
            {
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change who made the layout.")
    }

    void EditorWindow::ShowAboutWebsiteProblem()
    {
        try
        {
            if (m_aboutForm == nullptr)
            {
                return;
            }

            auto const applied = glass::ApplyProvenanceFields(
                m_editor.Document().Provenance, sharing::ReadProvenanceForm(*m_aboutForm));

            sharing::ShowWebsiteProblem(*m_aboutForm, applied.WebsiteRejected);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to check the website.")
    }

    winrt::fire_and_forget EditorWindow::ChangeAuthorInfoAsync()
    {
        auto lifetime = get_strong();

        try
        {
            co_await sharing::EditAuthorProfileAsync(Content().XamlRoot());

            if (m_aboutFillButton != nullptr)
            {
                m_aboutFillButton.IsEnabled(sharing::HasAuthorProfile(midiapp::LoadAuthorProfile()));
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change your info.")
    }

    // ------------------------------------------------------------ the theme in use

    void EditorWindow::RefreshThemeInUse()
    {
        try
        {
            std::wstring byline{};

            if (m_editor.Document().HasOwnTheme)
            {
                byline = resources::GetString(L"ThemeOwnByline");
            }
            else if (m_theme.FilePath.empty())
            {
                byline = resources::GetString(L"ThemeBuiltInByline");
            }
            else
            {
                auto const maker = sharing::DescribeMaker(m_theme.Provenance, sharing::SignerOf(m_theme.FilePath));

                byline = maker.empty()
                    ? std::wstring{ resources::GetString(L"ThemeGroupNoAuthor") }
                    : std::wstring{ resources::FormatString(L"ThemeByFormat", maker) };
            }

            ThemeInUseName().Text(resources::FormatString(L"ThemeInUseFormat", m_theme.Name));
            ThemeInUseByline().Text(winrt::hstring{ byline });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the theme in use.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnThemeAboutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowThemeAboutAsync(m_theme, m_editor.Document().HasOwnTheme);
    }

    _Use_decl_annotations_
    winrt::fire_and_forget EditorWindow::ShowThemeAboutAsync(glass::Theme theme, bool keptInLayout)
    {
        auto lifetime = get_strong();

        try
        {
            if (keptInLayout)
            {
                ShowEditorNotice(
                    resources::FormatString(L"DetailsTitleFormat", theme.Name),
                    resources::FormatString(L"ThemeOwnDetailsFormat", theme.Name));

                co_return;
            }

            if (!co_await sharing::ShowThemeDetailsAsync(Content().XamlRoot(), theme))
            {
                co_return;
            }

            // The file changed, so what this window holds of it is read again.
            if (!m_editor.Document().HasOwnTheme && !m_theme.FilePath.empty() && m_theme.FilePath == theme.FilePath)
            {
                auto const read = glass::ReadThemeFile(m_theme.FilePath);

                if (read.Succeeded)
                {
                    m_theme.Provenance = read.Value.Provenance;
                }
            }

            RebuildThemeGallery();
            RefreshThemeInUse();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the theme's details.")
    }
}
