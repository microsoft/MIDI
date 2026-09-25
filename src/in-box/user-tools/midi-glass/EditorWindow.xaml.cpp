// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The editor window itself: chrome, toolbar, undo, saving. The canvas is in EditorCanvas.cpp,
// the inspector and the two left panes in EditorInspector.cpp and EditorOutline.cpp, and the
// three dialogs in EditorDialogs.cpp.

#include "pch.h"
#include "EditorWindow.xaml.h"
#include "EditorWindow.g.cpp"
#include "App.xaml.h"

#include "AppSettings.h"
#include "StringResources.h"
#include "LayoutStore.h"
#include "ThemeStore.h"
#include "PageTemplates.h"
#include "resource.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        constexpr int32_t DefaultWindowWidth = 1480;
        constexpr int32_t DefaultWindowHeight = 960;

        // Long enough that typing does not write a file per keystroke, short enough that nobody
        // watches the chip. The same number MIDI Patchbay uses.
        constexpr int64_t AutoSaveDelayMilliseconds = 1500;
    }

    _Use_decl_annotations_
    bool EditorWindow::LoadLayout(std::wstring const& filePath)
    {
        try
        {
            auto const read = glass::ReadLayoutFile(filePath);

            if (!read.Succeeded)
            {
                return false;
            }

            m_filePath = filePath;
            m_editor.Load(read.Document);

            auto const themes = glass::AllThemes();

            m_theme = themes.empty() ? glass::Theme{} : themes[0];

            for (auto const& theme : themes)
            {
                if (theme.Name == read.Document.ThemeName)
                {
                    m_theme = theme;
                    break;
                }
            }

            try
            {
                if (auto const appWindow = AppWindow())
                {
                    appWindow.Resize({ DefaultWindowWidth, DefaultWindowHeight });
                }
            }
            catch (...)
            {
            }

            return true;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to open the layout for editing.")

        return false;
    }

    _Use_decl_annotations_
    void EditorWindow::OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            // Set first, not last. Checked handlers fire while the XAML is still being built,
            // when the x:Name fields are null, and calling a method on a null projected type is
            // an access violation rather than an exception a catch could hold. Everything those
            // handlers touch is gated on this, and by the time Loaded runs it is all real.
            m_loaded = true;

            m_dispatcher = DispatcherQueue();

            midiapp::WindowChromeElements elements{};

            elements.Window = *this;
            elements.Root = RootGrid();
            elements.Fill = WindowFill();
            elements.Tint = WindowTint();
            elements.TitleBar = AppTitleBar();
            elements.LeftInset = TitleBarLeftInsetColumn();
            elements.RightInset = TitleBarRightInsetColumn();

            m_chrome.Initialize(elements, ::midiglass::AppSettings::Current());
            m_chrome.SetWindowIconFromResource(IDI_APPICON);

            auto const title = resources::FormatString(
                L"EditorWindowTitleFormat", m_editor.Document().Name);

            Title(title);
            AppTitleTextBlock().Text(title);

            if (auto const icon = midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32))
            {
                AppTitleBarIcon().Source(icon);
            }

            m_updatingInspector = true;

            for (auto const size : { 4, 8, 16, 32 })
            {
                GridSizeCombo().Items().Append(box_value(
                    resources::FormatString(L"GridSizeFormat", std::to_wstring(size))));
            }

            GridSizeCombo().SelectedIndex(1);

            BuildInspectorChoices();
            BuildPalette();
            InitializeSplitters();
            RestoreEditorPanes();
            AddZOrderAccelerators();

            m_updatingInspector = false;

            // Auto save, the way MIDI Patchbay does it: a short wait after the last edit rather
            // than a Save button somebody has to remember.
            m_saveTimer = xaml::DispatcherTimer{};
            m_saveTimer.Interval(std::chrono::milliseconds{ AutoSaveDelayMilliseconds });

            m_saveTimer.Tick([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_saveTimer.Stop();
                        strong->SaveNow();
                    }
                });

            Closed({ this, &EditorWindow::OnWindowClosed });

            BuildPage();
            RebuildPageRail();
            RefreshInspector();
            UpdateSavedChip();
            UpdateStatusBar();
            UpdateMonitorEmptyText();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to set up the editor window.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnWindowClosed(foundation::IInspectable const& sender, xaml::WindowEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            // Where the window and its dividers were, before anything is torn down. A designer
            // that reopens somewhere else every time is a designer somebody has to rearrange
            // before they can start.
            SaveEditorPlacement();

            // Whatever was still pending goes to disk now. Closing a window is not a reason to
            // lose the last edit somebody made.
            if (m_saveTimer != nullptr)
            {
                m_saveTimer.Stop();
            }

            if (m_editor.IsDirty())
            {
                SaveNow();
            }

            // Nothing is left held, and the connections Try mode opened are closed. A finger
            // lifted by a window closing still has to end its note.
            m_input.ReleaseAll();
            m_input.Detach();

            if (m_player != nullptr)
            {
                m_player->Stop();
            }

            m_renderer.Teardown();
            m_chrome.Shutdown();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to shut the editor window down cleanly.")
    }

    // ---------------------------------------------------------------- saving

    void EditorWindow::MarkChanged()
    {
        try
        {
            if (m_saveTimer != nullptr)
            {
                m_saveTimer.Stop();
                m_saveTimer.Start();
            }

            // An edit made while Try mode is on has to reach the engine, or the fader under
            // somebody's finger keeps sending what it used to send.
            if (m_tryMode && m_player != nullptr)
            {
                m_player->UpdateDocument(m_editor.Document());
            }

            UpdateSavedChip();
            UpdateStatusBar();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to note the change.")
    }

    void EditorWindow::SaveNow()
    {
        try
        {
            if (m_filePath.empty())
            {
                return;
            }

            auto document = m_editor.Document();

            document.FilePath = m_filePath;
            document.ModifiedTimestamp = winrt::clock::now().time_since_epoch().count();

            m_saveFailed = !glass::WriteLayoutFile(document, m_filePath);

            if (!m_saveFailed)
            {
                m_editor.MarkSaved();
            }

            UpdateSavedChip();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to save the layout.")
    }

    void EditorWindow::UpdateSavedChip()
    {
        try
        {
            auto const& application = xaml::Application::Current().Resources();

            auto const brushNamed = [&application](wchar_t const* key)
                {
                    return application.Lookup(box_value(key)).as<media::Brush>();
                };

            if (m_saveFailed)
            {
                SavedChipShape().Fill(brushNamed(L"SystemFillColorCriticalBackgroundBrush"));
                SavedChipShape().Stroke(brushNamed(L"SystemFillColorCriticalBrush"));
                SavedChipGlyph().Glyph(L"\uE7BA");
                SavedChipGlyph().Foreground(brushNamed(L"SystemFillColorCriticalBrush"));
                SavedChipText().Text(resources::GetString(L"SavedChipFailed"));
                SavedChipText().Foreground(brushNamed(L"SystemFillColorCriticalBrush"));
            }
            else if (m_editor.IsDirty())
            {
                SavedChipShape().Fill(brushNamed(L"SystemFillColorCautionBackgroundBrush"));
                SavedChipShape().Stroke(brushNamed(L"SystemFillColorCautionBrush"));
                SavedChipGlyph().Glyph(L"\uE895");
                SavedChipGlyph().Foreground(brushNamed(L"SystemFillColorCautionBrush"));
                SavedChipText().Text(resources::GetString(L"SavedChipNotSaved"));
                SavedChipText().Foreground(brushNamed(L"SystemFillColorCautionBrush"));
            }
            else
            {
                SavedChipShape().Fill(brushNamed(L"SystemFillColorSuccessBackgroundBrush"));
                SavedChipShape().Stroke(brushNamed(L"SystemFillColorSuccessBrush"));
                SavedChipGlyph().Glyph(L"\uE73E");
                SavedChipGlyph().Foreground(brushNamed(L"SystemFillColorSuccessBrush"));
                SavedChipText().Text(resources::GetString(L"SavedChipSaved"));
                SavedChipText().Foreground(brushNamed(L"SystemFillColorSuccessBrush"));
            }

            xaml::Automation::AutomationProperties::SetName(SavedChip(), SavedChipText().Text());

            UndoButton().IsEnabled(m_editor.CanUndo());
            RedoButton().IsEnabled(m_editor.CanRedo());
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to update the saved chip.")
    }

    void EditorWindow::UpdateStatusBar()
    {
        try
        {
            auto const& document = m_editor.Document();
            auto const* const page = m_editor.CurrentPage();

            auto const controlCount = page == nullptr ? size_t{ 0 } : page->Controls.size();

            // An armed palette tool takes the status bar over, because what happens next on the
            // page depends on it and nothing else on screen says so.
            StatusText().Text(m_hasArmedKind
                ? resources::FormatString(L"PaletteArmedFormat", NameForArmedKind())
                : resources::FormatString(
                    L"EditorStatusFormat",
                    std::to_wstring(controlCount),
                    std::to_wstring(document.PageWidth),
                    std::to_wstring(document.PageHeight),
                    document.ThemeName));

            auto const selected = m_editor.Selection().size();

            SelectionText().Text(selected == 0
                ? resources::GetString(L"EditorNothingSelected")
                : resources::FormatString(L"EditorSelectedFormat", std::to_wstring(selected)));

            // What the next arrange or repeat will act on, said where the eye already is for
            // the Saved chip rather than at the far corner of the window.
            auto const& application = xaml::Application::Current().Resources();

            SelectionChip().Visibility(selected == 0
                ? xaml::Visibility::Collapsed
                : xaml::Visibility::Visible);

            if (selected != 0)
            {
                SelectionChipText().Text(resources::FormatString(
                    L"EditorSelectedFormat", std::to_wstring(selected)));

                SelectionChipShape().Fill(application
                    .Lookup(box_value(L"AccentFillColorSelectedTextBackgroundBrush")).as<media::Brush>());
                SelectionChipShape().Stroke(application
                    .Lookup(box_value(L"AccentControlElevationBorderBrush")).as<media::Brush>());
            }

            // In Try mode the question stops being "what have I built" and becomes "is this
            // reaching the instrument". A monitor with nothing in it cannot tell the difference
            // between nothing sent and nowhere to send it, so the devices are named here.
            if (m_tryMode && m_player != nullptr)            {
                std::wstring missing{};
                size_t available{ 0 };

                for (auto const& device : m_player->Devices())
                {
                    if (device.IsAvailable)
                    {
                        available++;
                    }
                    else
                    {
                        if (!missing.empty())
                        {
                            missing += L", ";
                        }

                        missing += device.Name;
                    }
                }

                SelectionText().Text(missing.empty()
                    ? (m_player->IsConnected()
                        ? resources::FormatString(L"TryDevicesReadyFormat", static_cast<int32_t>(available))
                        : resources::GetString(L"TryDevicesNotOpen"))
                    : resources::FormatString(L"TryDevicesMissingFormat", missing));
            }

            // Editing is not blocked in Try mode, but a rubber band over a live surface is, so
            // the tools that only make sense against a selection say so.
            RepeatButton().IsEnabled(selected > 0 && !m_tryMode);
            AlignLeftButton().IsEnabled(selected > 1 && !m_tryMode);
            AlignCenterButton().IsEnabled(selected > 1 && !m_tryMode);

            // The Arrange flyout carries the drawing order as well as the alignment, and
            // sending one panel to the back is the whole reason somebody opens it. It needs a
            // selection, not two; the items that need two say so themselves.
            ArrangeButton().IsEnabled(selected > 0 && !m_tryMode);

            for (auto const& item : { AlignLeftItem(), AlignCenterXItem(), AlignRightItem(),
                AlignTopItem(), AlignCenterYItem(), AlignBottomItem() })
            {
                item.IsEnabled(selected > 1);
            }

            SpreadAcrossItem().IsEnabled(selected > 2);
            SpreadDownItem().IsEnabled(selected > 2);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to update the status bar.")
    }

    // ---------------------------------------------------------------- toolbar

    _Use_decl_annotations_
    void EditorWindow::OnAppearanceButtonClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            UNREFERENCED_PARAMETER(sender);

            midiapp::AppearanceStrings strings{};

            strings.Title = resources::GetString(L"AppearanceTitle");
            strings.ThemeLabel = resources::GetString(L"AppearanceTheme");
            strings.ThemeSystem = resources::GetString(L"AppearanceThemeSystem");
            strings.ThemeLight = resources::GetString(L"AppearanceThemeLight");
            strings.ThemeDark = resources::GetString(L"AppearanceThemeDark");
            strings.BackdropLabel = resources::GetString(L"AppearanceBackdrop");
            strings.BackdropSolid = resources::GetString(L"AppearanceBackdropSolid");
            strings.BackdropMica = resources::GetString(L"AppearanceBackdropMica");
            strings.BackdropAcrylic = resources::GetString(L"AppearanceBackdropAcrylic");
            strings.CustomColorCheckBox = resources::GetString(L"AppearanceCustomColor");
            strings.ColorPickerName = resources::GetString(L"AppearanceColorPicker");

            auto weak = get_weak();

            midiapp::ShowAppearanceFlyout(
                AppearanceButton(),
                ::midiglass::AppSettings::Current(),
                strings,
                [weak]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_chrome.ApplyTheme();
                    }
                });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the appearance flyout.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnLibraryClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            if (m_editor.IsDirty())
            {
                SaveNow();
            }

            App::ActivateLibraryWindow();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to go back to the library.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnUndoClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_editor.Undo())
        {
            BuildPage();
            RebuildPageRail();
            RefreshInspector();
            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnRedoClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_editor.Redo())
        {
            BuildPage();
            RebuildPageRail();
            RefreshInspector();
            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnSnapToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        // Checked fires while the XAML is still being built, when the fields are still null.
        if (!m_loaded)
        {
            return;
        }

        auto const checked = SnapToggle().IsChecked();
        auto const enabled = checked != nullptr && checked.Value();

        m_editor.SetGridEnabled(enabled);
        GridSizeCombo().IsEnabled(enabled);

        RebuildGrid();
        UpdateOverlay();
    }

    _Use_decl_annotations_
    void EditorWindow::OnGridSizeChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        constexpr double sizes[]{ 4.0, 8.0, 16.0, 32.0 };

        auto const index = GridSizeCombo().SelectedIndex();

        if (index >= 0 && index < static_cast<int32_t>(std::size(sizes)))
        {
            m_editor.SetGridSize(sizes[index]);
            RebuildGrid();
            UpdateOverlay();
        }
    }

    // The square bracket keys have no name in the virtual key enum, so an accelerator for them
    // cannot be written in markup at all: a number where the enum is expected fails to parse at
    // run time, and the whole window fails to open with it.
    void EditorWindow::AddZOrderAccelerators()
    {
        try
        {
            constexpr auto OpenBracket = static_cast<winrt::Windows::System::VirtualKey>(0xDB);
            constexpr auto CloseBracket = static_cast<winrt::Windows::System::VirtualKey>(0xDD);

            struct Shortcut
            {
                winrt::Windows::System::VirtualKey Key{};
                winrt::Windows::System::VirtualKeyModifiers Modifiers{};
            };

            Shortcut const shortcuts[]
            {
                { CloseBracket, winrt::Windows::System::VirtualKeyModifiers::Control },
                { OpenBracket, winrt::Windows::System::VirtualKeyModifiers::Control },
                { CloseBracket,
                  winrt::Windows::System::VirtualKeyModifiers::Control | winrt::Windows::System::VirtualKeyModifiers::Shift },
                { OpenBracket,
                  winrt::Windows::System::VirtualKeyModifiers::Control | winrt::Windows::System::VirtualKeyModifiers::Shift },
            };

            for (auto const& shortcut : shortcuts)
            {
                xaml::Input::KeyboardAccelerator accelerator{};

                accelerator.Key(shortcut.Key);
                accelerator.Modifiers(shortcut.Modifiers);

                accelerator.Invoked({ this, &EditorWindow::OnZOrderAccelerator });

                RootGrid().KeyboardAccelerators().Append(accelerator);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to add the z-order shortcuts.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnZOrderAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        args.Handled(true);

        try
        {
            if (sender == nullptr)
            {
                return;
            }

            auto const shifted =
                (sender.Modifiers() & winrt::Windows::System::VirtualKeyModifiers::Shift) ==
                winrt::Windows::System::VirtualKeyModifiers::Shift;

            auto const forward = sender.Key() == static_cast<winrt::Windows::System::VirtualKey>(0xDD);

            auto const move =
                shifted ? (forward ? glass::ZOrderMove::ToFront : glass::ZOrderMove::ToBack)
                        : (forward ? glass::ZOrderMove::Forward : glass::ZOrderMove::Backward);

            if (m_editor.ChangeZOrder(move))
            {
                RebuildSurface();
                RebuildOutline();
                RefreshInspector();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the drawing order.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnArrangeClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            // Any element with the tag, not only a menu item: the same operations sit on the
            // toolbar as buttons, and those did nothing at all while this only took menu items.
            auto const element = sender.try_as<xaml::FrameworkElement>();

            if (element == nullptr)
            {
                return;
            }

            auto const tag = winrt::unbox_value_or<hstring>(element.Tag(), L"");

            auto changed = false;

            if (tag == L"alignleft") { changed = m_editor.AlignSelection(glass::AlignEdge::Left); }
            else if (tag == L"aligncenterx") { changed = m_editor.AlignSelection(glass::AlignEdge::CenterX); }
            else if (tag == L"alignright") { changed = m_editor.AlignSelection(glass::AlignEdge::Right); }
            else if (tag == L"aligntop") { changed = m_editor.AlignSelection(glass::AlignEdge::Top); }
            else if (tag == L"aligncentery") { changed = m_editor.AlignSelection(glass::AlignEdge::CenterY); }
            else if (tag == L"alignbottom") { changed = m_editor.AlignSelection(glass::AlignEdge::Bottom); }
            else if (tag == L"spreadx") { changed = m_editor.DistributeSelection(glass::ArrangeAxis::Horizontal); }
            else if (tag == L"spready") { changed = m_editor.DistributeSelection(glass::ArrangeAxis::Vertical); }
            else if (tag == L"tofront") { changed = m_editor.ChangeZOrder(glass::ZOrderMove::ToFront); }
            else if (tag == L"forward") { changed = m_editor.ChangeZOrder(glass::ZOrderMove::Forward); }
            else if (tag == L"backward") { changed = m_editor.ChangeZOrder(glass::ZOrderMove::Backward); }
            else if (tag == L"toback") { changed = m_editor.ChangeZOrder(glass::ZOrderMove::ToBack); }

            if (changed)
            {
                RebuildSurface();
                RebuildOutline();
                RefreshInspector();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to arrange the selection.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnRepeatClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowRepeatDialog();
    }

    _Use_decl_annotations_
    void EditorWindow::OnPageSizeClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowPageSizeDialog();
    }

    _Use_decl_annotations_
    void EditorWindow::OnRenameLayoutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowRenameDialog();
    }

    _Use_decl_annotations_
    void EditorWindow::OnRunClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            // What is running has to be what is on disk, or somebody would be feeling a surface
            // that is two edits behind the one in front of them.
            SaveNow();

            App::OpenRuntimeWindow(m_filePath);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to run the layout.")
    }

    // ---------------------------------------------------------------- accelerators

    _Use_decl_annotations_
    void EditorWindow::OnUndoAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        args.Handled(true);
        OnUndoClick(nullptr, nullptr);
    }

    _Use_decl_annotations_
    void EditorWindow::OnRedoAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        args.Handled(true);
        OnRedoClick(nullptr, nullptr);
    }

    _Use_decl_annotations_
    void EditorWindow::OnSaveAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        args.Handled(true);

        if (m_saveTimer != nullptr)
        {
            m_saveTimer.Stop();
        }

        SaveNow();
    }

    _Use_decl_annotations_
    void EditorWindow::OnDuplicateAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        args.Handled(true);

        if (m_editor.DuplicateSelection())
        {
            RebuildSurface();
            RebuildOutline();
            RefreshInspector();
            MarkChanged();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnSelectAllAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        args.Handled(true);

        m_editor.SelectAll();

        UpdateOverlay();
        RefreshInspector();
        UpdateStatusBar();
    }

    _Use_decl_annotations_
    void EditorWindow::OnEscapeAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (!m_hasArmedKind)
        {
            return;
        }

        args.Handled(true);

        m_hasArmedKind = false;

        SyncPaletteSelection();
        UpdateStatusBar();
    }

    _Use_decl_annotations_
    void EditorWindow::OnDeleteAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        // A text box in the inspector needs Delete for itself, so the canvas only takes it when
        // nothing in the inspector has focus.
        if (auto const focused = xaml::Input::FocusManager::GetFocusedElement(RootGrid().XamlRoot()))
        {
            if (focused.try_as<controls::TextBox>() != nullptr ||
                focused.try_as<controls::NumberBox>() != nullptr ||
                focused.try_as<controls::AutoSuggestBox>() != nullptr)
            {
                return;
            }
        }

        args.Handled(true);

        if (m_editor.DeleteSelection())
        {
            RebuildSurface();
            RebuildOutline();
            RefreshInspector();
            MarkChanged();
        }
    }
}
