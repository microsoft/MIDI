// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The rest of the layout settings rail: behavior, the name, the accessibility report, saving a
// theme out as a file and reading one back in, and export.
//
// All of it is about the LAYOUT rather than about this PC. The rule of thumb the design settled
// on is the one used here: if it travels with the file it belongs to the layout, and if it is
// about this machine it belongs to app settings.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "ThemeStore.h"
#include "LayoutStore.h"
#include "LayoutPackage.h"

#include <algorithm>
#include <filesystem>

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;

        // Settings the file can hold but the runtime does not act on yet. Each stays out of this
        // pane until its switch is turned on, so nobody sets something that does nothing.
        constexpr bool VirtualDeviceIsBuilt = false;
        constexpr bool IncomingClockIsBuilt = false;

        media::SolidColorBrush PaneBrush(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources()
                .Lookup(box_value(key)).as<media::SolidColorBrush>();
        }

        // Somewhere to put a theme or a package, or one to read. The Win32 common item dialog,
        // never Windows.Storage.Pickers: this is an unpackaged desktop app.
        std::wstring PickFilePath(
            _In_ HWND owner,
            _In_ bool saving,
            _In_ wchar_t const* titleKey,
            _In_ wchar_t const* filterText,
            _In_ wchar_t const* filterPattern,
            _In_ wchar_t const* defaultExtension,
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

                COMDLG_FILTERSPEC const filters[]
                {
                    { filterText, filterPattern },
                };

                dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
                dialog->SetDefaultExtension(defaultExtension);
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

        // Anything that is not safe in a file name, taken out. A theme name is typed by a person
        // and then becomes a file, which is exactly where a stray backslash does damage.
        std::wstring SafeThemeFileName(_In_ std::wstring const& name)
        {
            std::wstring safe{};

            for (auto const ch : name)
            {
                safe += (ch == L'\\' || ch == L'/' || ch == L':' || ch == L'*' || ch == L'?' ||
                    ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|' || ch < L' ')
                    ? L'_'
                    : ch;
            }

            while (!safe.empty() && (safe.back() == L' ' || safe.back() == L'.'))
            {
                safe.pop_back();
            }

            return safe;
        }
    }

    // ---------------------------------------------------------------- behavior

    void EditorWindow::RefreshBehaviorPane()
    {
        try
        {
            m_updatingSettings = true;

            SettingsBehaviorPanel().Children().Clear();

            auto const& document = m_editor.Document();

            auto const heading = [this](wchar_t const* key, bool first)
                {
                    controls::TextBlock text{};

                    text.Text(resources::GetString(key));
                    text.FontSize(13.5);
                    text.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
                    text.Margin({ 0, first ? 0.0 : 18.0, 0, 2 });

                    SettingsBehaviorPanel().Children().Append(text);
                };

            auto const caption = [this](wchar_t const* key)
                {
                    controls::TextBlock text{};

                    text.Text(resources::GetString(key));
                    text.FontSize(11.0);
                    text.LineHeight(16.0);
                    text.TextWrapping(xaml::TextWrapping::Wrap);
                    text.Margin({ 0, 0, 0, 8 });
                    text.Foreground(PaneBrush(L"TextFillColorTertiaryBrush"));

                    SettingsBehaviorPanel().Children().Append(text);
                };

            auto const check = [this](wchar_t const* key, bool on, std::function<void(bool)> apply)
                {
                    controls::CheckBox box{};

                    box.Content(box_value(resources::GetString(key)));
                    box.IsChecked(on);
                    box.MinWidth(0.0);
                    box.Margin({ 0, 0, 0, 4 });

                    automation::AutomationProperties::SetName(box, resources::GetString(key));

                    box.Checked([weak = get_weak(), apply](auto&&, auto&&)
                        {
                            if (auto strong = weak.get(); strong != nullptr && !strong->m_updatingSettings)
                            {
                                apply(true);
                            }
                        });

                    box.Unchecked([weak = get_weak(), apply](auto&&, auto&&)
                        {
                            if (auto strong = weak.get(); strong != nullptr && !strong->m_updatingSettings)
                            {
                                apply(false);
                            }
                        });

                    SettingsBehaviorPanel().Children().Append(box);
                };

            // ---- how it opens ----

            heading(L"BehaviorHeadingOpening", true);
            caption(L"BehaviorCaptionOpening");

            {
                controls::ComboBox scale{};

                scale.Header(box_value(resources::GetString(L"BehaviorScaleLabel")));
                scale.MinWidth(220.0);
                scale.FontSize(12.0);
                scale.Margin({ 0, 0, 0, 10 });

                for (auto const* key : { L"ScaleActualSize", L"ScaleFitToScreen", L"ScaleCustom" })
                {
                    scale.Items().Append(box_value(resources::GetString(key)));
                }

                scale.SelectedIndex(static_cast<int32_t>(document.Scale));

                automation::AutomationProperties::SetName(
                    scale, resources::GetString(L"BehaviorScaleLabel"));

                scale.SelectionChanged([weak = get_weak()](foundation::IInspectable const& sender, auto&&)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr || strong->m_updatingSettings)
                        {
                            return;
                        }

                        auto const index = sender.as<controls::ComboBox>().SelectedIndex();

                        if (index < 0)
                        {
                            return;
                        }

                        if (strong->m_editor.SetScaleMode(
                            static_cast<glass::ScaleMode>(index),
                            strong->m_editor.Document().CustomScalePercent))
                        {
                            strong->MarkChanged();
                        }
                    });

                SettingsBehaviorPanel().Children().Append(scale);
            }

            {
                controls::ComboBox corner{};

                corner.Header(box_value(resources::GetString(L"BehaviorCornerLabel")));
                corner.MinWidth(220.0);
                corner.FontSize(12.0);
                corner.Margin({ 0, 0, 0, 4 });

                for (auto const* key : { L"SettingsCornerTopLeft", L"SettingsCornerTopRight", L"SettingsCornerBottomLeft", L"SettingsCornerBottomRight" })
                {
                    corner.Items().Append(box_value(resources::GetString(key)));
                }

                corner.SelectedIndex(static_cast<int32_t>(document.FullScreenButtonCorner));

                automation::AutomationProperties::SetName(
                    corner, resources::GetString(L"BehaviorCornerLabel"));

                corner.SelectionChanged([weak = get_weak()](foundation::IInspectable const& sender, auto&&)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr || strong->m_updatingSettings)
                        {
                            return;
                        }

                        auto const index = sender.as<controls::ComboBox>().SelectedIndex();

                        if (index >= 0 && strong->m_editor.SetFullScreenButtonCorner(
                            static_cast<glass::ScreenCorner>(index)))
                        {
                            strong->MarkChanged();
                        }
                    });

                SettingsBehaviorPanel().Children().Append(corner);
            }

            // ---- the window it runs in ----

            heading(L"BehaviorHeadingWindow", false);
            caption(L"BehaviorCaptionWindow");

            check(L"BehaviorToolbarWindow", document.ToolbarWindow,
                [weak = get_weak()](bool on)
                {
                    if (auto strong = weak.get())
                    {
                        if (strong->m_editor.SetToolbarWindow(on))
                        {
                            strong->MarkChanged();
                        }
                    }
                });

            check(L"BehaviorAlwaysOnTop", document.AlwaysOnTop,
                [weak = get_weak()](bool on)
                {
                    if (auto strong = weak.get())
                    {
                        if (strong->m_editor.SetAlwaysOnTop(on))
                        {
                            strong->MarkChanged();
                        }
                    }
                });

            check(L"BehaviorSeeThrough", document.SeeThrough,
                [weak = get_weak()](bool on)
                {
                    if (auto strong = weak.get())
                    {
                        if (strong->m_editor.SetSeeThrough(on))
                        {
                            strong->MarkChanged();
                        }
                    }
                });

            // ---- what it sends when it opens ----

            heading(L"BehaviorHeadingStartup", false);
            caption(L"BehaviorCaptionStartup");

            check(L"BehaviorSuppressStartup", document.SuppressAllStartupValues,
                [weak = get_weak()](bool on)
                {
                    if (auto strong = weak.get())
                    {
                        if (strong->m_editor.SetSuppressAllStartupValues(on))
                        {
                            strong->MarkChanged();
                        }
                    }
                });

            // ---- what other apps see ----

            if constexpr (VirtualDeviceIsBuilt)
            {
                heading(L"BehaviorHeadingVirtual", false);
                caption(L"BehaviorCaptionVirtual");

                check(L"BehaviorVirtualDevice", document.PublishesVirtualDevice,
                    [weak = get_weak()](bool on)
                    {
                        if (auto strong = weak.get())
                        {
                            if (strong->m_editor.SetPublishesVirtualDevice(on))
                            {
                                strong->MarkChanged();
                            }
                        }
                    });
            }

            // ---- what keeps time ----

            heading(L"BehaviorHeadingTempo", false);
            caption(IncomingClockIsBuilt ? L"BehaviorCaptionTempo" : L"BehaviorCaptionTempoInternal");

            {
                if constexpr (IncomingClockIsBuilt)
                {
                    controls::ComboBox source{};

                    source.Header(box_value(resources::GetString(L"BehaviorTempoSourceLabel")));
                    source.MinWidth(220.0);
                    source.FontSize(12.0);
                    source.Margin({ 0, 0, 0, 8 });

                    for (auto const* key : { L"TempoInternal", L"TempoFollowIncoming" })
                    {
                        source.Items().Append(box_value(resources::GetString(key)));
                    }

                    source.SelectedIndex(static_cast<int32_t>(document.Tempo.Kind));

                    automation::AutomationProperties::SetName(
                        source, resources::GetString(L"BehaviorTempoSourceLabel"));

                    source.SelectionChanged([weak = get_weak()](foundation::IInspectable const& sender, auto&&)
                        {
                            auto strong = weak.get();

                            if (strong == nullptr || strong->m_updatingSettings)
                            {
                                return;
                            }

                            auto const index = sender.as<controls::ComboBox>().SelectedIndex();

                            if (index < 0)
                            {
                                return;
                            }

                            auto tempo = strong->m_editor.Document().Tempo;
                            tempo.Kind = static_cast<glass::TempoSourceKind>(index);

                            if (strong->m_editor.SetTempoSource(tempo))
                            {
                                strong->MarkChanged();
                            }
                        });

                    SettingsBehaviorPanel().Children().Append(source);
                }

                controls::NumberBox beats{};

                beats.Header(box_value(resources::GetString(L"BehaviorTempoLabel")));
                beats.Minimum(1.0);
                beats.Maximum(999.0);
                beats.Value(document.Tempo.BeatsPerMinute);
                beats.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
                beats.Width(160.0);
                beats.HorizontalAlignment(xaml::HorizontalAlignment::Left);

                automation::AutomationProperties::SetName(
                    beats, resources::GetString(L"BehaviorTempoLabel"));

                beats.ValueChanged([weak = get_weak()](foundation::IInspectable const& sender, auto&&)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr || strong->m_updatingSettings)
                        {
                            return;
                        }

                        auto const value = sender.as<controls::NumberBox>().Value();

                        if (std::isnan(value))
                        {
                            return;
                        }

                        auto tempo = strong->m_editor.Document().Tempo;
                        tempo.BeatsPerMinute = value;

                        if (strong->m_editor.SetTempoSource(tempo))
                        {
                            strong->MarkChanged();
                        }
                    });

                SettingsBehaviorPanel().Children().Append(beats);
            }

            // ---- the picture behind everything ----

            heading(L"BehaviorHeadingBackground", false);
            caption(L"BehaviorCaptionBackground");

            {
                controls::Button pick{};

                pick.Style(xaml::Application::Current().Resources()
                    .Lookup(box_value(L"EditorSmallButtonStyle")).as<xaml::Style>());

                controls::StackPanel content{};
                content.Orientation(controls::Orientation::Horizontal);
                content.Spacing(6.0);

                controls::FontIcon glyph{};
                glyph.Glyph(L"\uEB9F");
                glyph.FontSize(12.0);

                controls::TextBlock text{};
                text.Text(resources::GetString(L"BehaviorBackgroundButton"));

                content.Children().Append(glyph);
                content.Children().Append(text);

                pick.Content(content);

                automation::AutomationProperties::SetName(
                    pick, resources::GetString(L"BehaviorBackgroundButton"));

                pick.Click([weak = get_weak()](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ShowBackgroundDialog();
                        }
                    });

                SettingsBehaviorPanel().Children().Append(pick);
            }

            m_updatingSettings = false;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the layout behavior.")
    }

    // ------------------------------------------------------ name and description

    void EditorWindow::RefreshNamePane()
    {
        try
        {
            m_updatingSettings = true;

            auto const& document = m_editor.Document();

            SettingsNameBox().Text(winrt::hstring{ document.Name });
            SettingsDescriptionBox().Text(winrt::hstring{ document.Description });

            SettingsFileText().Text(m_filePath.empty()
                ? resources::GetString(L"SettingsFileNotSaved")
                : resources::FormatString(L"SettingsFileFormat", m_filePath));

            m_updatingSettings = false;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the layout name.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnSettingsNameChanged(
        foundation::IInspectable const& sender,
        controls::TextChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingSettings)
        {
            return;
        }

        try
        {
            if (m_editor.SetLayoutName(std::wstring{ SettingsNameBox().Text() }))
            {
                MarkChanged();
                UpdateStatusBar();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to rename the layout.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnSettingsDescriptionChanged(
        foundation::IInspectable const& sender,
        controls::TextChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingSettings)
        {
            return;
        }

        try
        {
            if (m_editor.SetLayoutDescription(std::wstring{ SettingsDescriptionBox().Text() }))
            {
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the description.")
    }

    // ------------------------------------------------------- accessibility check

    // Everything measured rather than asserted, and everything this build can honestly check.
    // What it does not do is claim a pass: a layout that clears every number here can still be
    // unreadable from six feet away, and only the person who built it can say.
    void EditorWindow::RefreshAccessibilityPane()
    {
        try
        {
            AccessibilityList().Children().Clear();

            auto const& document = m_editor.Document();

            auto const addRow = [this](bool good, winrt::hstring const& title, winrt::hstring const& detail)
                {
                    controls::Border card{};

                    card.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(6.0));
                    card.Padding({ 12, 10, 12, 11 });
                    card.BorderThickness({ 1, 1, 1, 1 });
                    card.Background(PaneBrush(good
                        ? L"SystemFillColorSuccessBackgroundBrush"
                        : L"SystemFillColorCautionBackgroundBrush"));
                    card.BorderBrush(PaneBrush(good
                        ? L"SystemFillColorSuccessBrush"
                        : L"SystemFillColorCautionBrush"));

                    controls::Grid row{};
                    row.ColumnSpacing(10.0);

                    controls::ColumnDefinition glyphColumn{};
                    glyphColumn.Width(xaml::GridLengthHelper::Auto());

                    controls::ColumnDefinition textColumn{};
                    textColumn.Width(xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star));

                    row.ColumnDefinitions().Append(glyphColumn);
                    row.ColumnDefinitions().Append(textColumn);

                    controls::FontIcon glyph{};
                    glyph.Glyph(good ? L"\uE73E" : L"\uE7BA");
                    glyph.FontSize(14.0);
                    glyph.VerticalAlignment(xaml::VerticalAlignment::Top);
                    glyph.Foreground(PaneBrush(good
                        ? L"SystemFillColorSuccessBrush"
                        : L"SystemFillColorCautionBrush"));

                    controls::Grid::SetColumn(glyph, 0);
                    row.Children().Append(glyph);

                    controls::StackPanel lines{};
                    lines.Spacing(3.0);

                    controls::TextBlock titleText{};
                    titleText.Text(title);
                    titleText.FontSize(12.0);
                    titleText.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
                    titleText.TextWrapping(xaml::TextWrapping::Wrap);

                    controls::TextBlock detailText{};
                    detailText.Text(detail);
                    detailText.FontSize(11.0);
                    detailText.LineHeight(16.0);
                    detailText.TextWrapping(xaml::TextWrapping::Wrap);
                    detailText.Foreground(PaneBrush(L"TextFillColorSecondaryBrush"));

                    lines.Children().Append(titleText);
                    lines.Children().Append(detailText);

                    controls::Grid::SetColumn(lines, 1);
                    row.Children().Append(lines);

                    card.Child(row);

                    AccessibilityList().Children().Append(card);
                };

            // Slot contrast, the measurement the design sheet is built on.
            {
                auto const measured = glass::MeasureContrast(m_theme);

                std::wstring failing{};
                auto lowest = 100.0;

                for (auto const& slot : measured)
                {
                    lowest = (std::min)(lowest, slot.Ratio);

                    if (!slot.MeetsMinimum)
                    {
                        if (!failing.empty()) { failing += L", "; }

                        failing += std::to_wstring(slot.SlotIndex + 1);
                    }
                }

                addRow(failing.empty(),
                    resources::GetString(L"CheckSlotContrastTitle"),
                    failing.empty()
                        ? resources::FormatString(L"CheckSlotContrastPass", std::format(L"{:.1f}", lowest))
                        : resources::FormatString(L"CheckSlotContrastFail", failing));
            }

            // Whether a control can be told from the space behind it at all.
            {
                auto const structural = PlateNeedsItsShadow();
                auto const carried = m_theme.PlateElevation > 0 || m_theme.RestingGlowPercent > 0;

                addRow(!structural || carried,
                    resources::GetString(L"CheckPlateTitle"),
                    (!structural || carried)
                        ? resources::GetString(L"CheckPlatePass")
                        : resources::GetString(L"CheckPlateFail"));
            }

            // A rim below about a quarter on a light plate is simply not there, and the control's
            // identity goes with it. On a dark plate a quarter is exactly right, so this is a
            // check about the plate rather than a number to raise everywhere.
            {
                auto const plate = m_theme.PlateColor.A != 0 ? m_theme.PlateColor : m_theme.Deck.Color;
                auto const light = glass::RelativeLuminance(plate) > 0.45;
                auto const enough = !light || m_theme.RimStrengthPercent >= 50;

                addRow(enough,
                    resources::GetString(L"CheckRimTitle"),
                    !light
                        ? resources::FormatString(L"CheckRimDarkPlate", m_theme.RimStrengthPercent)
                        : enough
                            ? resources::FormatString(L"CheckRimPass", m_theme.RimStrengthPercent)
                            : resources::FormatString(L"CheckRimFail", m_theme.RimStrengthPercent));
            }

            // What the theme itself costs, where it costs something.
            if (!m_theme.CautionResourceKey.empty())
            {
                addRow(false,
                    resources::GetString(L"CheckThemeCostTitle"),
                    resources::GetString(m_theme.CautionResourceKey));
            }

            // A control with no label and no value showing is a rectangle a screen reader can
            // find and nobody can name.
            {
                int32_t unnamed{ 0 };

                for (auto const& page : document.Pages)
                {
                    for (auto const& control : page.Controls)
                    {
                        if (control.Label.empty() &&
                            control.Kind != glass::ControlKind::Panel &&
                            control.Kind != glass::ControlKind::Image &&
                            control.Kind != glass::ControlKind::Label &&
                            control.Kind != glass::ControlKind::Line)
                        {
                            unnamed++;
                        }
                    }
                }

                addRow(unnamed == 0,
                    resources::GetString(L"CheckLabelsTitle"),
                    unnamed == 0
                        ? resources::GetString(L"CheckLabelsPass")
                        : resources::FormatString(L"CheckLabelsFail", unnamed));
            }

            // High contrast is offered, never forced, and it is the safety net that lets every
            // other theme be what it is.
            addRow(true,
                resources::GetString(L"CheckHighContrastTitle"),
                resources::GetString(L"CheckHighContrastDetail"));
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to run the accessibility check.")
    }

    // ------------------------------------------------------- saving a theme out

    _Use_decl_annotations_
    void EditorWindow::OnSaveThemeClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowSaveThemeDialog();
    }

    winrt::fire_and_forget EditorWindow::ShowSaveThemeDialog()
    {
        auto lifetime = get_strong();

        try
        {
            controls::TextBox nameBox{};

            nameBox.Header(box_value(resources::GetString(L"SaveThemeNameLabel")));
            nameBox.MaxLength(60);
            nameBox.Text(winrt::hstring{ m_theme.Name });

            automation::AutomationProperties::SetName(
                nameBox, resources::GetString(L"SaveThemeNameLabel"));

            controls::TextBlock note{};

            note.Text(resources::GetString(L"SaveThemeNote"));
            note.FontSize(11.0);
            note.LineHeight(16.0);
            note.TextWrapping(xaml::TextWrapping::Wrap);
            note.Margin({ 0, 10, 0, 0 });
            note.Foreground(PaneBrush(L"TextFillColorTertiaryBrush"));

            controls::StackPanel panel{};
            panel.Width(360.0);
            panel.Children().Append(nameBox);
            panel.Children().Append(note);

            controls::ContentDialog dialog{};

            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(box_value(resources::GetString(L"SaveThemeTitle")));
            dialog.Content(panel);
            dialog.PrimaryButtonText(resources::GetString(L"SaveThemeConfirm"));
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);

            if (co_await dialog.ShowAsync() != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            auto name = std::wstring{ nameBox.Text() };

            while (!name.empty() && name.front() == L' ') { name.erase(name.begin()); }
            while (!name.empty() && name.back() == L' ') { name.pop_back(); }

            if (name.empty())
            {
                co_return;
            }

            // A theme file that names a shipped theme would shadow it on somebody else's PC and
            // could never be overwritten, so the writer refuses and this says why.
            if (glass::FindBuiltInTheme(name) != nullptr)
            {
                ShowEditorNotice(
                    resources::GetString(L"SaveThemeFailedTitle"),
                    resources::FormatString(L"SaveThemeNameTaken", name));

                co_return;
            }

            auto saved = m_theme;
            saved.Name = name;
            saved.IsBuiltIn = false;

            // Not a theme's to carry: it is a sentence about one of ours, and a customer theme
            // has no business claiming it.
            saved.CautionResourceKey.clear();

            auto const folder = glass::ThemesFolder();

            if (folder.empty())
            {
                ShowEditorNotice(
                    resources::GetString(L"SaveThemeFailedTitle"),
                    resources::GetString(L"SaveThemeNoFolder"));

                co_return;
            }

            auto const path = (std::filesystem::path{ folder } /
                (SafeThemeFileName(name) + glass::ThemeFileExtension)).wstring();

            if (!glass::WriteThemeFile(saved, path))
            {
                ShowEditorNotice(
                    resources::GetString(L"SaveThemeFailedTitle"),
                    resources::GetString(L"SaveThemeFailedDetail"));

                co_return;
            }

            // The layout now points at a theme that exists on this PC, so it stops carrying its
            // own copy and picks up any later edit to the file.
            if (m_editor.ChooseTheme(saved))
            {
                m_theme = saved;
                MarkChanged();
                ApplyThemeEverywhere();
            }

            RefreshAppearancePane();

            ShowEditorNotice(
                resources::GetString(L"SaveThemeDoneTitle"),
                resources::FormatString(L"SaveThemeDoneFormat", name));
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to save the theme.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnImportThemeClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const source = PickFilePath(
                m_chrome.WindowHandle(),
                false,
                L"ImportThemeTitle",
                resources::GetString(L"ImportThemeFilter").c_str(),
                L"*.miditheme;*.miditheme.json",
                L"miditheme",
                {});

            if (source.empty())
            {
                return;
            }

            auto const read = glass::ReadThemeFile(source);

            if (!read.Succeeded || read.Value.Name.empty())
            {
                ShowEditorNotice(
                    resources::GetString(L"ImportThemeFailedTitle"),
                    read.Detail.empty()
                        ? resources::GetString(L"ImportThemeFailedDetail")
                        : winrt::hstring{ read.Detail });

                return;
            }

            auto theme = read.Value;

            // A theme from a stranger never claims to be one of ours, and never carries a line
            // about what one of ours costs.
            theme.IsBuiltIn = false;
            theme.CautionResourceKey.clear();

            auto const folder = glass::ThemesFolder();

            if (folder.empty() || glass::FindBuiltInTheme(theme.Name) != nullptr)
            {
                // It can still be used on this layout even where it cannot be filed, which is
                // the part somebody actually wanted.
                EditTheme([&theme](glass::Theme& target) { target = theme; });
                RefreshAppearancePane();

                return;
            }

            auto const path = (std::filesystem::path{ folder } /
                (SafeThemeFileName(theme.Name) + glass::ThemeFileExtension)).wstring();

            glass::WriteThemeFile(theme, path);

            if (m_editor.ChooseTheme(theme))
            {
                m_theme = theme;
                MarkChanged();
                ApplyThemeEverywhere();
            }

            RefreshAppearancePane();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to import a theme.")
    }

    // ------------------------------------------------------------------ export

    void EditorWindow::ExportLayoutPackage()
    {
        try
        {
            if (m_filePath.empty())
            {
                return;
            }

            auto suggested = std::filesystem::path{ m_filePath }.filename().wstring();

            if (auto const dot = suggested.find(L'.'); dot != std::wstring::npos)
            {
                suggested = suggested.substr(0, dot);
            }

            auto const target = PickFilePath(
                m_chrome.WindowHandle(),
                true,
                L"PackageSaveTitle",
                L"MIDI Glass layout package (*.zip)",
                L"*.zip",
                L"zip",
                suggested + glass::LayoutPackageExtension);

            if (target.empty())
            {
                return;
            }

            auto const result = glass::WriteLayoutPackage(m_filePath, target, true);

            if (!result.Succeeded)
            {
                ShowEditorNotice(
                    resources::GetString(L"PackageFailedTitle"),
                    resources::GetString(result.FailureKey.empty()
                        ? std::wstring{ L"PackageFailedWrite" }
                        : result.FailureKey));

                return;
            }

            ShowEditorNotice(
                resources::GetString(L"PackageDoneTitle"),
                resources::FormatString(
                    L"PackageDoneFormat",
                    std::filesystem::path{ result.Path }.filename().wstring(),
                    std::to_wstring(result.FileCount)));
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to export the layout.")
    }

    // A plain "here is what happened". Not a status bar line: something written to disk that
    // nobody was told about is something nobody trusts.
    _Use_decl_annotations_
    winrt::fire_and_forget EditorWindow::ShowEditorNotice(
        winrt::hstring title,
        winrt::hstring message)
    {
        auto lifetime = get_strong();

        try
        {
            controls::TextBlock text{};

            text.Text(message);
            text.TextWrapping(xaml::TextWrapping::Wrap);
            text.MaxWidth(400.0);

            controls::ContentDialog dialog{};

            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(box_value(title));
            dialog.Content(text);
            dialog.CloseButtonText(resources::GetString(L"DialogClose"));

            co_await dialog.ShowAsync();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show a notice.")
    }
}
