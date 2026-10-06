// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "App.xaml.h"
#include "BackgroundWork.h"
#include "PatchLayout.h"
#include "PatchSerializer.h"
#include "RoundedShape.h"
#include "StringResources.h"
#include "resource.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;
namespace transfer = ::winrt::Windows::ApplicationModel::DataTransfer;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        constexpr int32_t DefaultEditorWidth = 1360;
        constexpr int32_t DefaultEditorHeight = 880;

        // Enough to take back a whole session of small changes, without holding a big patch a
        // hundred times over for ever.
        constexpr size_t MaximumUndoStates = 100;

        // How far a paste lands from what was copied, so the copy is seen to arrive.
        constexpr double PasteOffset = 32.0;

        // Where a new node goes relative to the point it was dropped on, so it lands under the
        // pointer rather than hanging off it.
        constexpr double BlockHalfWidth = patchbay::PatchCanvas::BlockNodeWidth / 2;
        constexpr double BlockHalfHeight = 38.0;
        constexpr double EndpointHalfWidth = patchbay::PatchCanvas::MinimumNodeWidth / 2;
        constexpr double EndpointHalfHeight = 40.0;

        // Ten points at a time up to 150%, then bigger steps so 400% is a few clicks away.
        constexpr float ZoomSteps[] = {
            0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f,
            1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f };

        float NextZoomStep(_In_ float current, _In_ bool zoomIn) noexcept
        {
            // So a zoom already on a step moves to the next one, whatever rounding it picked up.
            constexpr float tolerance = 0.01f;

            if (zoomIn)
            {
                for (auto const step : ZoomSteps)
                {
                    if (step > current + tolerance)
                    {
                        return step;
                    }
                }

                return ZoomSteps[std::size(ZoomSteps) - 1];
            }

            for (auto step = std::rbegin(ZoomSteps); step != std::rend(ZoomSteps); ++step)
            {
                if (*step < current - tolerance)
                {
                    return *step;
                }
            }

            return ZoomSteps[0];
        }

        bool SameText(_In_ std::wstring const& left, _In_ std::wstring const& right) noexcept
        {
            return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
        }

        // Case-insensitive, the way a person expects a search box to work.
        bool ContainsText(_In_ std::wstring_view text, _In_ std::wstring_view search) noexcept
        {
            if (search.empty())
            {
                return true;
            }

            if (text.empty())
            {
                return false;
            }

            return ::FindNLSStringEx(
                LOCALE_NAME_USER_DEFAULT,
                FIND_FROMSTART | LINGUISTIC_IGNORECASE,
                text.data(), static_cast<int>(text.size()),
                search.data(), static_cast<int>(search.size()),
                nullptr, nullptr, nullptr, 0) >= 0;
        }

        bool IsKeyDown(_In_ int virtualKey) noexcept
        {
            return (::GetKeyState(virtualKey) & 0x8000) != 0;
        }

        // A closed InfoBar keeps its place in a StackPanel, so the panel's spacing still leaves a
        // gap for each one. Collapsing the closed ones gives that room back to the canvas.
        void CollapseClosedBars(_In_ controls::Panel const& panel)
        {
            for (auto const& child : panel.Children())
            {
                auto const bar = child.try_as<controls::InfoBar>();

                if (bar == nullptr)
                {
                    continue;
                }

                bar.Visibility(bar.IsOpen() ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

                bar.RegisterPropertyChangedCallback(controls::InfoBar::IsOpenProperty(),
                    [](xaml::DependencyObject const& sender, xaml::DependencyProperty const&)
                    {
                        try
                        {
                            if (auto const changed = sender.try_as<controls::InfoBar>())
                            {
                                changed.Visibility(changed.IsOpen() ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                            }
                        }
                        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show or hide a message bar.")
                    });
            }
        }
    }

    MainWindow::MainWindow()
    {
        // XAML objects must not call InitializeComponent during construction; winrt::make does it
    }

    _Use_decl_annotations_
    bool MainWindow::OpenPatch(std::wstring const& patchKey) noexcept
    {
        if (patchbay::PatchLibrary::Current().Find(patchKey) == nullptr)
        {
            return false;
        }

        m_patchKey = patchKey;

        return true;
    }

    _Use_decl_annotations_
    void MainWindow::RestoreWindowPlacement(int32_t cascade) noexcept
    {
        try
        {
            auto saved = patchbay::AppSettings::Current().EditorPlacement();

            // A cascade means nothing on a maximized window, or with nowhere saved to move from.
            if (saved.Valid && cascade > 0 && !saved.Maximized)
            {
                saved.X += cascade;
                saved.Y += cascade;
            }

            midiapp::WindowChrome::RestorePlacement(*this, saved, DefaultEditorWidth, DefaultEditorHeight);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to restore the editor window placement.")
    }

    void MainWindow::SaveEditorPlacement() noexcept
    {
        try
        {
            // Not the chrome's own save: that is where the library window keeps its place.
            auto const placement = midiapp::WindowChrome::CapturePlacement(*this);

            if (placement.Valid)
            {
                patchbay::AppSettings::Current().EditorPlacement(placement);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to remember where the editor was.")
    }

    void MainWindow::ApplyAppearance() noexcept
    {
        try
        {
            if (!m_loaded || m_closing)
            {
                return;
            }

            m_chrome.ApplyTheme();
            m_chrome.ApplyAlwaysOnTop();

            RebuildCanvas();
            RefreshInspector();
            RebuildPalette();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to apply the appearance to an editor.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            if (m_loaded)
            {
                return;
            }

            m_loaded = true;

            InitializeWindowChrome();
            InitializeStaticText();
            InitializeSplitters();
            CollapseClosedBars(MessageBars());

            // The canvas only ever lives as long as this window, and is shut down before it goes.
            patchbay::PatchCanvas::Callbacks callbacks{};

            callbacks.SelectionChanged = [this]() { OnCanvasSelectionChanged(); };
            callbacks.ConnectionRequested = [this](patchbay::PatchConnection connection)
                { OnConnectionRequested(std::move(connection)); };
            callbacks.LayoutChanged = [this]() { OnCanvasLayoutChanged(); };
            callbacks.NodeContextMenuRequested = [this](std::wstring nodeId, foundation::Point position)
                { ShowNodeMenu(nodeId, position); };
            callbacks.ConnectionRetargetRequested = [this](std::wstring connectionId, patchbay::PatchConnection updated)
                { OnConnectionRetargetRequested(connectionId, std::move(updated)); };
            callbacks.ViewportChanged = [this]() { UpdateZoomText(CanvasScroller().ZoomFactor()); };
            callbacks.BlockDropped = [this](patchbay::BlockKind kind, foundation::Point point, std::wstring connectionId)
                { OnBlockDropped(kind, point, connectionId); };
            callbacks.EndpointDropped = [this](std::wstring endpointDeviceId, foundation::Point point)
                { OnEndpointDropped(endpointDeviceId, point); };
            callbacks.BlockActivated = [this](std::wstring blockId) { ShowBlockDialogAsync(blockId); };

            m_canvas.Initialize(CanvasScroller(), CanvasSurface(), MinimapSurface(), std::move(callbacks));

            // The hint on an empty patch sits in the middle of the canvas, where a first drop lands.
            m_canvas.AcceptDrops(EmptyCanvasPanel());

            auto weak = get_weak();

            // Visuals built in code cannot re-theme themselves, and changing the Windows theme
            // raises no settings change, so this is the one event that catches both.
            RootGrid().ActualThemeChanged([weak](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->RebuildCanvas();
                        strong->RefreshInspector();
                        strong->RebuildPalette();
                    }
                });

            // Here rather than on the canvas, so the keys work whichever part of the window has
            // focus. Text boxes handle their own copy and paste and never get this far.
            RootGrid().KeyDown([weak](auto&&, input::KeyRoutedEventArgs const& args)
                {
                    if (auto strong = weak.get())
                    {
                        strong->HandleKeyDown(args);
                    }
                });

            // An open step dialog follows the window, so making the window bigger shows more.
            RootGrid().SizeChanged([weak](auto&&, auto&&)
                {
                    if (auto strong = weak.get(); strong != nullptr && !strong->m_editingBlockId.empty())
                    {
                        strong->FitBlockDialogToWindow();
                    }
                });

            m_libraryToken = patchbay::PatchLibrary::Current().Subscribe(
                [weak](patchbay::LibraryChange change, std::wstring const& key)
                {
                    if (auto strong = weak.get())
                    {
                        strong->OnLibraryChanged(change, key);
                    }
                });

            m_closedToken = this->Closed([weak](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_closing = true;
                        strong->StopLearning();

                        patchbay::PatchLibrary::Current().Unsubscribe(strong->m_libraryToken);
                        strong->m_libraryToken = 0;

                        strong->m_canvas.Shutdown();
                        strong->SaveEditorPlacement();
                        strong->m_chrome.Shutdown();
                    }
                });

            m_committedState = CaptureState();
            m_activity = patchbay::PatchLibrary::Current().Activity(m_patchKey);

            RebuildPalette();
            RebuildCanvas();
            RefreshInspector();
            UpdatePatchHeader();
            UpdateMessages();
            UpdateConversionNotice();
            UpdateStatusStrip();
            UpdateCommandStates();

            // Once the panels have their sizes, so the fit uses the space there really is.
            RootGrid().UpdateLayout();
            m_canvas.FitToContent();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to finish loading the editor.")
    }

    _Use_decl_annotations_
    void MainWindow::HandleKeyDown(input::KeyRoutedEventArgs const& args) noexcept
    {
        try
        {
            using winrt::Windows::System::VirtualKey;

            // Anything the customer might be typing in keeps its keys.
            auto const focused = xaml::Input::FocusManager::GetFocusedElement(Content().XamlRoot());

            if (focused.try_as<controls::TextBox>() != nullptr ||
                focused.try_as<controls::NumberBox>() != nullptr ||
                focused.try_as<controls::AutoSuggestBox>() != nullptr ||
                focused.try_as<controls::RichEditBox>() != nullptr)
            {
                return;
            }

            auto const control = IsKeyDown(VK_CONTROL);
            auto const shift = IsKeyDown(VK_SHIFT);

            auto handled = true;

            switch (args.Key())
            {
            case VirtualKey::Delete:
                DeleteSelection();
                break;

            case VirtualKey::Z:
                if (!control)
                {
                    handled = false;
                }
                else if (shift)
                {
                    Redo();
                }
                else
                {
                    Undo();
                }
                break;

            case VirtualKey::Y:
                handled = control;
                if (control)
                {
                    Redo();
                }
                break;

            case VirtualKey::C:
                handled = control;
                if (control)
                {
                    CopySelection();
                }
                break;

            case VirtualKey::X:
                handled = control;
                if (control)
                {
                    CutSelection();
                }
                break;

            case VirtualKey::V:
                handled = control;
                if (control)
                {
                    PasteAsync();
                }
                break;

            case VirtualKey::D:
                handled = control;
                if (control)
                {
                    DuplicateSelection();
                }
                break;

            case VirtualKey::A:
                handled = control;
                if (control)
                {
                    if (auto const* patch = CurrentPatch())
                    {
                        std::vector<std::wstring> everything{};

                        for (auto const& endpoint : patch->Endpoints)
                        {
                            everything.push_back(endpoint.Id);
                        }

                        for (auto const& block : patch->Blocks)
                        {
                            everything.push_back(block.Id);
                        }

                        m_canvas.SelectNodes(everything);
                    }
                }
                break;

            default:
                handled = false;
                break;
            }

            if (handled)
            {
                args.Handled(true);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to act on a key.")
    }

    void MainWindow::InitializeWindowChrome() noexcept
    {
        try
        {
            midiapp::ApplyPreviewBadgeVisibility(PreviewChiclet());

            midiapp::WindowChromeElements elements{};

            elements.Window = *this;
            elements.Root = RootGrid();
            elements.Fill = WindowFill();
            elements.Tint = WindowTint();
            elements.TitleBar = AppTitleBar();
            elements.LeftInset = TitleBarLeftInsetColumn();
            elements.RightInset = TitleBarRightInsetColumn();

            m_chrome.Initialize(elements, patchbay::AppSettings::Current());
            m_chrome.SetWindowIconFromResource(IDI_APPICON);

            // 32px source for a 16px slot, so it stays crisp on a high DPI display
            if (auto const icon = midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32))
            {
                AppTitleBarIcon().Source(icon);
            }

            UpdateTitle();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to set up the window chrome.")
    }

    void MainWindow::UpdateTitle() noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            auto const title = patch == nullptr
                ? resources::GetString(L"AppDisplayName")
                : resources::FormatString(L"EditorTitleFormat", patch->Name);

            Title(title);
            AppTitleTextBlock().Text(title);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to set the window title.")
    }

    void MainWindow::InitializeStaticText() noexcept
    {
        try
        {
            ZoomText().Text(resources::FormatString(L"ZoomPercentFormat", 100));
            MinimapPanel().Visibility(xaml::Visibility::Collapsed);

            // Shown with the shortcut, so the keys are found by hovering.
            auto const setTip = [](xaml::DependencyObject const& target, wchar_t const* key)
                {
                    controls::ToolTipService::SetToolTip(target, winrt::box_value(resources::GetString(key)));
                };

            setTip(UndoButton(), L"UndoButtonTip");
            setTip(RedoButton(), L"RedoButtonTip");
            setTip(CutButton(), L"CutButtonTip");
            setTip(CopyButton(), L"CopyButtonTip");
            setTip(PasteButton(), L"PasteButtonTip");
            setTip(DeleteButton(), L"DeleteButtonTip");
            setTip(LibraryButton(), L"LibraryButtonTip");
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to set the static text.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLibraryButtonClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        App::ActivateLibraryWindow();
    }

    // ------------------------------------------------------------------ the patch

    patchbay::PatchDocument* MainWindow::CurrentPatch() noexcept
    {
        return patchbay::PatchLibrary::Current().Find(m_patchKey);
    }

    patchbay::PatchAnalysis const& MainWindow::Analysis() noexcept
    {
        return patchbay::PatchLibrary::Current().Analysis(m_patchKey);
    }

    _Use_decl_annotations_
    void MainWindow::OnLibraryChanged(patchbay::LibraryChange change, std::wstring const& key) noexcept
    {
        try
        {
            if (m_closing || m_patchKey.empty())
            {
                return;
            }

            switch (change)
            {
            case patchbay::LibraryChange::PatchList:
                // The library says so before it lets go of a patch. The canvas lets go of it
                // here, and the window closes once this has unwound.
                if (key == m_patchKey)
                {
                    m_patchKey.clear();

                    StopLearning();
                    m_canvas.Rebuild(nullptr, {}, {});

                    if (auto const queue = DispatcherQueue())
                    {
                        queue.TryEnqueue([weak = get_weak()]()
                            {
                                if (auto strong = weak.get())
                                {
                                    strong->Close();
                                }
                            });
                    }
                }
                break;

            case patchbay::LibraryChange::Patch:
                // Changed somewhere other than the canvas, such as by a dialog. The next change
                // made here is measured from what the patch is now.
                if (key == m_patchKey && !m_committing)
                {
                    m_committedState = CaptureState();

                    RebuildCanvas();
                    RefreshInspector();
                    UpdatePatchHeader();
                    UpdateMessages();
                    UpdateTitle();
                }
                break;

            case patchbay::LibraryChange::Routing:
                if (key.empty() || key == m_patchKey)
                {
                    UpdatePatchHeader();
                    UpdateMessages();
                    UpdateInspectorActivity();
                }
                break;

            case patchbay::LibraryChange::Endpoints:
                RebuildCanvas();
                RefreshInspector();
                UpdateMessages();
                RebuildPalette();
                break;

            case patchbay::LibraryChange::Activity:
                OnActivity();
                break;

            case patchbay::LibraryChange::Saved:
                if (key == m_patchKey)
                {
                    UpdatePatchHeader();
                    UpdateTitle();
                }
                break;

            default:
                break;
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to catch up with a change to the patches.")
    }

    _Use_decl_annotations_
    void MainWindow::CommitChange(bool routingAffected, bool redraw) noexcept
    {
        try
        {
            if (CurrentPatch() == nullptr)
            {
                return;
            }

            auto state = CaptureState();

            // Nothing that Undo would notice, such as a node dragged back where it was.
            if (!state.empty() && state != m_committedState)
            {
                if (!m_committedState.empty())
                {
                    m_undoStates.push_back(std::move(m_committedState));

                    if (m_undoStates.size() > MaximumUndoStates)
                    {
                        m_undoStates.erase(m_undoStates.begin());
                    }
                }

                m_redoStates.clear();
                m_committedState = std::move(state);
            }

            m_committing = true;
            auto const reset = wil::scope_exit([this]() { m_committing = false; });

            patchbay::PatchLibrary::Current().Changed(m_patchKey, routingAffected);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to record a change to the patch.")

        if (redraw)
        {
            RebuildCanvas();
            RefreshInspector();
            UpdateMessages();
        }

        UpdatePatchHeader();
        UpdateStatusStrip();
        UpdateCommandStates();

        if (redraw && PaletteTabs().SelectedItem() == PaletteEndpointsTab())
        {
            RebuildPalette();
        }
    }

    // --------------------------------------------------------------------- undo

    std::wstring MainWindow::CaptureState() noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return {};
            }

            // No name and no times, so saving or renaming never looks like an edit.
            patchbay::PatchDocument state{};
            state.WaitForSendComplete = patch->WaitForSendComplete;

            // Swapped in and out rather than copied: a big patch's steps are megabytes in
            // memory and a few kilobytes as text. Nothing between the swaps can throw.
            std::swap(state.Endpoints, patch->Endpoints);
            std::swap(state.Blocks, patch->Blocks);
            std::swap(state.Connections, patch->Connections);

            auto text = patchbay::WritePatchJson(state);

            std::swap(state.Endpoints, patch->Endpoints);
            std::swap(state.Blocks, patch->Blocks);
            std::swap(state.Connections, patch->Connections);

            return text;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to keep the patch for undo.")

        return {};
    }

    _Use_decl_annotations_
    void MainWindow::RestoreState(std::wstring const& state) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr || state.empty())
            {
                return;
            }

            auto restored = patchbay::ReadPatchJson(state, patch->Name);

            if (!restored.has_value())
            {
                return;
            }

            patch->Endpoints = std::move(restored->Endpoints);
            patch->Blocks = std::move(restored->Blocks);
            patch->Connections = std::move(restored->Connections);
            patch->WaitForSendComplete = restored->WaitForSendComplete;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to put the patch back.")
    }

    void MainWindow::Undo() noexcept
    {
        try
        {
            if (m_undoStates.empty() || CurrentPatch() == nullptr)
            {
                return;
            }

            auto previous = std::move(m_undoStates.back());
            m_undoStates.pop_back();

            m_redoStates.push_back(std::move(m_committedState));

            RestoreState(previous);

            // Read back rather than kept, so the next change compares like with like.
            m_committedState = CaptureState();

            m_committing = true;
            auto const reset = wil::scope_exit([this]() { m_committing = false; });

            patchbay::PatchLibrary::Current().Changed(m_patchKey, true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to undo.")

        RebuildCanvas();
        RefreshInspector();
        UpdateMessages();
        UpdatePatchHeader();
        UpdateStatusStrip();
        UpdateCommandStates();
    }

    void MainWindow::Redo() noexcept
    {
        try
        {
            if (m_redoStates.empty() || CurrentPatch() == nullptr)
            {
                return;
            }

            auto next = std::move(m_redoStates.back());
            m_redoStates.pop_back();

            m_undoStates.push_back(std::move(m_committedState));

            RestoreState(next);
            m_committedState = CaptureState();

            m_committing = true;
            auto const reset = wil::scope_exit([this]() { m_committing = false; });

            patchbay::PatchLibrary::Current().Changed(m_patchKey, true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to redo.")

        RebuildCanvas();
        RefreshInspector();
        UpdateMessages();
        UpdatePatchHeader();
        UpdateStatusStrip();
        UpdateCommandStates();
    }

    void MainWindow::UpdateCommandStates() noexcept
    {
        try
        {
            auto const hasNodes = !m_canvas.SelectedNodeIds().empty();
            auto const hasConnection = m_canvas.SelectionKind() == patchbay::CanvasSelectionKind::Connection;

            UndoButton().IsEnabled(!m_undoStates.empty());
            RedoButton().IsEnabled(!m_redoStates.empty());
            CutButton().IsEnabled(hasNodes);
            CopyButton().IsEnabled(hasNodes);
            DeleteButton().IsEnabled(hasNodes || hasConnection);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to update the commands.")
    }

    _Use_decl_annotations_
    void MainWindow::OnUndoClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        Undo();
    }

    _Use_decl_annotations_
    void MainWindow::OnRedoClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        Redo();
    }

    // ---------------------------------------------------------------- clipboard

    std::wstring MainWindow::SelectionAsText() noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();
            auto const& selected = m_canvas.SelectedNodeIds();

            if (patch == nullptr || selected.empty())
            {
                return {};
            }

            auto const isSelected = [&selected](std::wstring const& id)
                {
                    return std::find(selected.begin(), selected.end(), id) != selected.end();
                };

            patchbay::PatchDocument copy{};
            copy.Name = patch->Name;
            copy.WaitForSendComplete = patch->WaitForSendComplete;

            for (auto const& endpoint : patch->Endpoints)
            {
                if (isSelected(endpoint.Id))
                {
                    copy.Endpoints.push_back(endpoint);
                }
            }

            for (auto const& block : patch->Blocks)
            {
                if (isSelected(block.Id))
                {
                    copy.Blocks.push_back(block);
                }
            }

            // Only the links inside the selection. A link to something left behind would have
            // nothing to join up with in another patch.
            for (auto const& link : patch->Connections)
            {
                if (isSelected(link.SourceId) && isSelected(link.DestinationId))
                {
                    copy.Connections.push_back(link);
                }
            }

            return patchbay::WritePatchJson(copy);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to copy the selection.")

        return {};
    }

    void MainWindow::CopySelection() noexcept
    {
        auto const text = SelectionAsText();

        if (text.empty())
        {
            return;
        }

        try
        {
            // Patch file text, so a copy pastes into any patch, and reads as a patch anywhere else.
            transfer::DataPackage package{};

            package.RequestedOperation(transfer::DataPackageOperation::Copy);
            package.SetText(winrt::hstring{ text });

            transfer::Clipboard::SetContent(package);

            m_lastPastedText.clear();
            m_pasteRepeat = 0;
        }
        catch (...)
        {
            // Another app can hold the clipboard open for a moment.
            ShowStatus(resources::GetString(L"ClipboardUnavailable"), controls::InfoBarSeverity::Warning);
            return;
        }

        try
        {
            // Kept on the clipboard after this app closes. The copy has already worked, and
            // clipboard history often has the clipboard open right now, so a failure is ignored.
            transfer::Clipboard::Flush();
        }
        catch (...)
        {
        }
    }

    void MainWindow::CutSelection() noexcept
    {
        try
        {
            auto* patch = CurrentPatch();
            auto const selected = m_canvas.SelectedNodeIds();

            if (patch == nullptr || selected.empty())
            {
                return;
            }

            CopySelection();

            for (auto const& id : selected)
            {
                if (patch->IsBlock(id))
                {
                    patch->RemoveBlock(id);
                }
                else
                {
                    patch->RemoveEndpoint(id);
                }
            }

            m_canvas.ClearSelection();
            CommitChange(true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to cut the selection.")
    }

    void MainWindow::DuplicateSelection() noexcept
    {
        // Endpoints are already here, so they are reused, and only the steps and their links
        // are new.
        auto const text = SelectionAsText();

        if (!text.empty())
        {
            PasteText(text);
        }
    }

    winrt::fire_and_forget MainWindow::PasteAsync()
    {
        auto strong = get_strong();

        try
        {
            auto const view = transfer::Clipboard::GetContent();

            if (view == nullptr || !view.Contains(transfer::StandardDataFormats::Text()))
            {
                ShowStatus(resources::GetString(L"PasteNothing"), controls::InfoBarSeverity::Informational);
                co_return;
            }

            auto const text = co_await view.GetTextAsync();

            // The window can close, or lose its patch, while the clipboard is read.
            if (m_closing || CurrentPatch() == nullptr)
            {
                co_return;
            }

            if (!PasteText(std::wstring{ text }))
            {
                ShowStatus(resources::GetString(L"PasteNothing"), controls::InfoBarSeverity::Informational);
            }
        }
        catch (...)
        {
            ShowStatus(resources::GetString(L"ClipboardUnavailable"), controls::InfoBarSeverity::Warning);
        }
    }

    _Use_decl_annotations_
    bool MainWindow::PasteText(std::wstring const& text) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr || text.empty() || text.size() > patchbay::MaximumPatchFileBytes)
            {
                return false;
            }

            // Text from anywhere: the reader bounds everything, and anything that isn't a patch
            // comes back empty.
            auto pasted = patchbay::ReadPatchJson(text, {});

            if (!pasted.has_value() || (pasted->Endpoints.empty() && pasted->Blocks.empty()))
            {
                return false;
            }

            if (text == m_lastPastedText)
            {
                m_pasteRepeat++;
            }
            else
            {
                m_lastPastedText = text;
                m_pasteRepeat = 1;
            }

            // Where the copy sat, so it keeps its shape wherever it lands.
            auto left = std::numeric_limits<double>::max();
            auto top = std::numeric_limits<double>::max();
            auto right = std::numeric_limits<double>::lowest();
            auto bottom = std::numeric_limits<double>::lowest();

            auto const include = [&](patchbay::PatchDocument const& document, std::wstring const& id, double x, double y)
                {
                    auto const size = patchbay::EstimatedNodeSize(document, id);

                    left = (std::min)(left, x);
                    top = (std::min)(top, y);
                    right = (std::max)(right, x + size.Width);
                    bottom = (std::max)(bottom, y + size.Height);
                };

            for (auto const& endpoint : pasted->Endpoints)
            {
                include(*pasted, endpoint.Id, endpoint.CanvasX, endpoint.CanvasY);
            }

            for (auto const& block : pasted->Blocks)
            {
                include(*pasted, block.Id, block.CanvasX, block.CanvasY);
            }

            auto offsetX = PasteOffset * m_pasteRepeat;
            auto offsetY = PasteOffset * m_pasteRepeat;

            // Copied from somewhere out of view, or from another patch: it comes to the middle of
            // what is showing instead, so it is never pasted out of sight.
            auto const view = VisibleCanvasRect();

            auto const landsInView =
                left + offsetX >= view.X && top + offsetY >= view.Y &&
                left + offsetX < view.X + view.Width && top + offsetY < view.Y + view.Height;

            if (!landsInView)
            {
                offsetX = view.X + view.Width / 2 - (left + right) / 2 + PasteOffset * (m_pasteRepeat - 1);
                offsetY = view.Y + view.Height / 2 - (top + bottom) / 2 + PasteOffset * (m_pasteRepeat - 1);
            }

            std::unordered_map<std::wstring, std::wstring> newIds{};
            std::vector<std::wstring> added{};
            auto limited = false;

            for (auto const& endpoint : pasted->Endpoints)
            {
                // An endpoint that is here already is used as it is, so the paste joins up with it.
                auto const existing = std::find_if(patch->Endpoints.begin(), patch->Endpoints.end(),
                    [&endpoint](patchbay::PatchEndpoint const& e) { return patchbay::IsSameDevice(e, endpoint); });

                if (existing != patch->Endpoints.end())
                {
                    newIds[endpoint.Id] = existing->Id;
                    continue;
                }

                if (patch->Endpoints.size() >= patchbay::MaximumEndpointsPerPatch)
                {
                    limited = true;
                    continue;
                }

                auto copy = endpoint;

                copy.Id = patchbay::PatchDocument::NewId();
                copy.CanvasX = (std::max)(0.0, copy.CanvasX + offsetX);
                copy.CanvasY = (std::max)(0.0, copy.CanvasY + offsetY);

                newIds[endpoint.Id] = copy.Id;
                added.push_back(copy.Id);

                patch->Endpoints.push_back(std::move(copy));
            }

            for (auto& block : pasted->Blocks)
            {
                if (patch->Blocks.size() >= patchbay::MaximumBlocksPerPatch)
                {
                    limited = true;
                    break;
                }

                auto const oldId = block.Id;

                block.Id = patchbay::PatchDocument::NewId();
                block.CanvasX = (std::max)(0.0, block.CanvasX + offsetX);
                block.CanvasY = (std::max)(0.0, block.CanvasY + offsetY);

                newIds[oldId] = block.Id;
                added.push_back(block.Id);

                patch->Blocks.push_back(std::move(block));
            }

            size_t linked{ 0 };

            for (auto const& link : pasted->Connections)
            {
                auto const source = newIds.find(link.SourceId);
                auto const destination = newIds.find(link.DestinationId);

                if (source == newIds.end() || destination == newIds.end())
                {
                    continue;
                }

                if (patch->Connections.size() >= patchbay::MaximumConnectionsPerPatch)
                {
                    limited = true;
                    break;
                }

                if (patch->HasConnection(source->second, link.SourceGroupIndex, destination->second, link.DestinationGroupIndex))
                {
                    continue;
                }

                auto copy = link;

                copy.Id = patchbay::PatchDocument::NewId();
                copy.SourceId = source->second;
                copy.DestinationId = destination->second;

                patch->Connections.push_back(std::move(copy));
                linked++;
            }

            if (added.empty() && linked == 0)
            {
                ShowStatus(resources::GetString(L"PasteAlreadyHere"), controls::InfoBarSeverity::Informational);
                return true;
            }

            CommitChange(true);

            m_canvas.SelectNodes(added);

            if (limited)
            {
                ShowStatus(resources::GetString(L"PasteLimited"), controls::InfoBarSeverity::Warning);
            }

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to paste.")

        return false;
    }

    _Use_decl_annotations_
    void MainWindow::OnCutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        CutSelection();
    }

    _Use_decl_annotations_
    void MainWindow::OnCopyClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        CopySelection();
    }

    _Use_decl_annotations_
    void MainWindow::OnPasteClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        PasteAsync();
    }

    _Use_decl_annotations_
    void MainWindow::OnDeleteClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        DeleteSelection();
    }

    // ------------------------------------------------------------------- canvas

    void MainWindow::RebuildCanvas() noexcept
    {
        try
        {
            if (m_closing)
            {
                return;
            }

            auto* patch = CurrentPatch();

            m_canvas.Rebuild(patch, patchbay::PatchLibrary::Current().LiveEndpoints(), Analysis());
            m_canvas.RefreshStatus(m_activity);

            auto const isEmpty = patch == nullptr || (patch->Endpoints.empty() && patch->Blocks.empty());

            EmptyCanvasPanel().Visibility(isEmpty ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            MinimapPanel().Visibility(isEmpty ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to rebuild the canvas.")
    }

    void MainWindow::OnCanvasSelectionChanged() noexcept
    {
        // Typed into an annotation that is going out of the inspector: one step for Undo.
        if (m_annotationTextChanged)
        {
            m_annotationTextChanged = false;
            CommitChange(false, false);
        }

        RefreshInspector();
        UpdateCommandStates();
    }

    void MainWindow::OnCanvasLayoutChanged() noexcept
    {
        // The canvas has already moved the nodes, so there is nothing to draw again.
        CommitChange(false, false);
    }

    foundation::Rect MainWindow::VisibleCanvasRect() noexcept
    {
        try
        {
            auto const scroller = CanvasScroller();
            auto const zoom = static_cast<double>((std::max)(scroller.ZoomFactor(), 0.01f));

            return foundation::Rect{
                static_cast<float>(scroller.HorizontalOffset() / zoom),
                static_cast<float>(scroller.VerticalOffset() / zoom),
                static_cast<float>(scroller.ViewportWidth() / zoom),
                static_cast<float>(scroller.ViewportHeight() / zoom) };
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to find what is in view.")

        return foundation::Rect{ 0, 0, 800, 600 };
    }

    _Use_decl_annotations_
    bool MainWindow::CanConnect(patchbay::PatchConnection const& candidate, std::wstring const& ignoreConnectionId) noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr || !patch->HasNode(candidate.SourceId) || !patch->HasNode(candidate.DestinationId))
            {
                return false;
            }

            auto const sourceIsBlock = patch->IsBlock(candidate.SourceId);
            auto const destinationIsBlock = patch->IsBlock(candidate.DestinationId);

            // An annotation has no Out to start one from.
            if (auto const* source = patch->FindBlock(candidate.SourceId);
                source != nullptr && !patchbay::HasOutput(source->Kind))
            {
                return false;
            }

            if (auto const* destination = patch->FindBlock(candidate.DestinationId);
                destination != nullptr && !patchbay::HasInput(destination->Kind))
            {
                ShowStatus(resources::GetString(L"ConnectionIntoGeneratorRejected"), controls::InfoBarSeverity::Warning);
                return false;
            }

            // A step can't feed itself, and neither can one group of an endpoint.
            if (candidate.SourceId == candidate.DestinationId &&
                (sourceIsBlock || candidate.SourceGroupIndex == candidate.DestinationGroupIndex))
            {
                ShowStatus(resources::GetString(L"ConnectionSelfRejected"), controls::InfoBarSeverity::Warning);
                return false;
            }

            for (auto const& other : patch->Connections)
            {
                if (other.Id != ignoreConnectionId &&
                    other.SourceId == candidate.SourceId &&
                    other.SourceGroupIndex == candidate.SourceGroupIndex &&
                    other.DestinationId == candidate.DestinationId &&
                    other.DestinationGroupIndex == candidate.DestinationGroupIndex)
                {
                    ShowStatus(resources::GetString(L"ConnectionDuplicate"), controls::InfoBarSeverity::Informational);
                    return false;
                }
            }

            if (ignoreConnectionId.empty() && patch->Connections.size() >= patchbay::MaximumConnectionsPerPatch)
            {
                ShowStatus(resources::GetString(L"ConnectionLimitReached"), controls::InfoBarSeverity::Warning);
                return false;
            }

            // Steps that feed each other in a circle would pass a message round for ever, and the
            // routing turns the whole patch down when it sees one. Better to say so now. An LFO's
            // In goes no further, so a connection into one can't close a circle.
            if (sourceIsBlock && destinationIsBlock && !patchbay::IsGenerator(patch->FindBlock(candidate.DestinationId)->Kind))
            {
                std::vector<std::wstring> pending{ candidate.DestinationId };
                std::unordered_set<std::wstring> seen{};

                while (!pending.empty())
                {
                    auto const current = pending.back();
                    pending.pop_back();

                    if (current == candidate.SourceId)
                    {
                        ShowStatus(resources::GetString(L"ConnectionStepLoopRejected"), controls::InfoBarSeverity::Warning);
                        return false;
                    }

                    if (!seen.insert(current).second)
                    {
                        continue;
                    }

                    for (auto const& link : patch->Connections)
                    {
                        auto const* next = link.Id != ignoreConnectionId && link.SourceId == current
                            ? patch->FindBlock(link.DestinationId)
                            : nullptr;

                        if (next != nullptr && !patchbay::IsGenerator(next->Kind))
                        {
                            pending.push_back(link.DestinationId);
                        }
                    }
                }
            }

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to check a connection.")

        return false;
    }

    _Use_decl_annotations_
    void MainWindow::OnConnectionRequested(patchbay::PatchConnection connection) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr || !CanConnect(connection, {}))
            {
                return;
            }

            if (connection.Id.empty())
            {
                connection.Id = patchbay::PatchDocument::NewId();
            }

            auto const id = connection.Id;

            patch->Connections.push_back(std::move(connection));

            CommitChange(true);

            m_canvas.Select(patchbay::CanvasSelectionKind::Connection, id);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to add the connection.")
    }

    _Use_decl_annotations_
    void MainWindow::OnConnectionRetargetRequested(
        std::wstring const& connectionId,
        patchbay::PatchConnection updated) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr || patch->FindConnection(connectionId) == nullptr || !CanConnect(updated, connectionId))
            {
                return;
            }

            auto* existing = patch->FindConnection(connectionId);

            existing->SourceId = updated.SourceId;
            existing->SourceGroupIndex = updated.SourceGroupIndex;
            existing->DestinationId = updated.DestinationId;
            existing->DestinationGroupIndex = updated.DestinationGroupIndex;

            CommitChange(true);

            m_canvas.Select(patchbay::CanvasSelectionKind::Connection, connectionId);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to move the connection.")
    }

    // ------------------------------------------------------------------ palette

    _Use_decl_annotations_
    void MainWindow::OnPaletteTabChanged(
        controls::SelectorBar const& sender,
        controls::SelectorBarSelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        RebuildPalette();
    }

    _Use_decl_annotations_
    void MainWindow::OnPaletteSearchChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        RebuildPalette();
    }

    void MainWindow::RebuildPalette() noexcept
    {
        try
        {
            if (!m_loaded || m_closing)
            {
                return;
            }

            auto const content = PaletteContent();
            content.Children().Clear();
            m_paletteGrids.clear();

            auto const search = std::wstring{ PaletteSearchBox().Text() };
            auto const showEndpoints = PaletteTabs().SelectedItem() == PaletteEndpointsTab();
            auto const tertiary = patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush");

            auto weak = get_weak();

            // A step category's heading starts with a square in its color, the same color as the
            // edge of its steps on the canvas.
            auto const addHeading = [&content, &tertiary](winrt::hstring const& text, media::Brush const& marker)
                {
                    controls::StackPanel heading{};

                    heading.Orientation(controls::Orientation::Horizontal);
                    heading.Spacing(7);
                    heading.Margin(xaml::ThicknessHelper::FromLengths(2, 10, 0, 4));

                    if (marker != nullptr)
                    {
                        auto square = patchbay::MakeRoundedShape(2, marker);

                        square.Width(8);
                        square.Height(8);
                        square.VerticalAlignment(xaml::VerticalAlignment::Center);

                        heading.Children().Append(square);
                    }

                    controls::TextBlock label{};

                    label.Text(text);
                    label.FontSize(12);
                    label.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());

                    if (tertiary != nullptr)
                    {
                        label.Foreground(tertiary);
                    }

                    heading.Children().Append(label);

                    content.Children().Append(heading);
                };

            // Lists and grids rather than buttons, because an item can be dragged as well as
            // clicked, and the keyboard reaches it the same way.
            auto const wire = [weak](controls::ListViewBase const& view, bool isEndpointList)
                {
                    view.SelectionMode(controls::ListViewSelectionMode::None);
                    view.IsItemClickEnabled(true);
                    view.CanDragItems(true);
                    view.CanReorderItems(false);
                    view.AllowDrop(false);

                    // The default ones slide every tile into place again each time the palette is
                    // resized or rebuilt.
                    view.ItemContainerTransitions(media::Animation::TransitionCollection{});

                    view.DragItemsStarting([weak, isEndpointList](auto&&, controls::DragItemsStartingEventArgs const& args)
                        {
                            auto const items = args.Items();
                            auto const element = items.Size() == 0 ? nullptr : items.GetAt(0).try_as<xaml::FrameworkElement>();

                            auto const value = element == nullptr
                                ? winrt::hstring{}
                                : winrt::unbox_value_or<winrt::hstring>(element.Tag(), L"");

                            auto strong = weak.get();

                            // An endpoint already on the patch would only be turned away.
                            if (value.empty() || strong == nullptr ||
                                (isEndpointList && strong->IsOnCanvas(std::wstring{ value })))
                            {
                                args.Cancel(true);
                                return;
                            }

                            args.Data().Properties().Insert(
                                winrt::hstring{ isEndpointList ? patchbay::PaletteEndpointProperty : patchbay::PaletteBlockKindProperty },
                                winrt::box_value(value));

                            args.Data().RequestedOperation(transfer::DataPackageOperation::Copy);
                        });

                    view.ItemClick([weak, isEndpointList](auto&&, controls::ItemClickEventArgs const& args)
                        {
                            auto strong = weak.get();
                            auto const element = args.ClickedItem().try_as<xaml::FrameworkElement>();

                            if (strong == nullptr || element == nullptr)
                            {
                                return;
                            }

                            auto const value = std::wstring{ winrt::unbox_value_or<winrt::hstring>(element.Tag(), L"") };

                            if (value.empty())
                            {
                                return;
                            }

                            if (isEndpointList)
                            {
                                strong->OnEndpointDropped(value, strong->m_canvas.ViewCenter());
                                return;
                            }

                            auto const kind = patchbay::BlockKindFromKey(value);

                            if (!kind.has_value())
                            {
                                return;
                            }

                            // With a link selected, the step goes into it. Otherwise it goes in
                            // the middle of what is in view.
                            if (strong->m_canvas.SelectionKind() == patchbay::CanvasSelectionKind::Connection)
                            {
                                strong->InsertBlockIntoConnection(kind.value(), strong->m_canvas.SelectedConnectionId(), std::nullopt);
                            }
                            else
                            {
                                strong->AddBlock(kind.value(), strong->m_canvas.ViewCenter());
                            }
                        });
                };

            auto const addList = [&content, &wire]()
                {
                    controls::ListView list{};

                    wire(list, true);
                    content.Children().Append(list);

                    return list;
                };

            auto const addTileGrid = [this, &content, &wire, weak]()
                {
                    controls::GridView grid{};

                    auto const dictionary = RootGrid().Resources();

                    grid.ItemsPanel(dictionary.Lookup(winrt::box_value(L"PaletteTilesPanel")).as<controls::ItemsPanelTemplate>());
                    grid.ItemContainerStyle(dictionary.Lookup(winrt::box_value(L"PaletteTileContainerStyle")).as<xaml::Style>());

                    // The palette scrolls as a whole, so a grid inside it never does.
                    controls::ScrollViewer::SetVerticalScrollMode(grid, controls::ScrollMode::Disabled);
                    controls::ScrollViewer::SetVerticalScrollBarVisibility(grid, controls::ScrollBarVisibility::Disabled);

                    grid.Loaded([weak](auto&&, auto&&)
                        {
                            if (auto strong = weak.get())
                            {
                                strong->SizePaletteTiles();
                            }
                        });

                    wire(grid, false);
                    content.Children().Append(grid);
                    m_paletteGrids.push_back(grid);

                    return grid;
                };

            size_t shown{ 0 };

            if (!showEndpoints)
            {
                for (auto const category : { patchbay::BlockCategory::Filter, patchbay::BlockCategory::Transform,
                                             patchbay::BlockCategory::Sending, patchbay::BlockCategory::Distribution,
                                             patchbay::BlockCategory::CapabilityInquiry,
                                             patchbay::BlockCategory::Generator,
                                             patchbay::BlockCategory::Annotation })
                {
                    controls::GridView grid{ nullptr };

                    for (auto const kind : patchbay::AllBlockKinds)
                    {
                        if (patchbay::CategoryOf(kind) != category)
                        {
                            continue;
                        }

                        if (!ContainsText(patchbay::BlockKindName(kind), search) &&
                            !ContainsText(patchbay::BlockKindHint(kind), search))
                        {
                            continue;
                        }

                        if (grid == nullptr)
                        {
                            addHeading(patchbay::BlockCategoryName(category), patchbay::PatchCanvas::CategoryBrush(category));
                            grid = addTileGrid();
                        }

                        if (auto const tile = BuildBlockTile(kind))
                        {
                            grid.Items().Append(tile);
                            shown++;
                        }
                    }
                }
            }
            else
            {
                auto live = patchbay::PatchLibrary::Current().LiveEndpoints();

                std::sort(live.begin(), live.end(), [](patchbay::LiveEndpoint const& a, patchbay::LiveEndpoint const& b)
                    {
                        return ::CompareStringOrdinal(a.Name.c_str(), -1, b.Name.c_str(), -1, TRUE) == CSTR_LESS_THAN;
                    });

                controls::ListView liveList{ nullptr };

                for (auto const& endpoint : live)
                {
                    if (!ContainsText(endpoint.Name, search) && !ContainsText(endpoint.ManufacturerName, search))
                    {
                        continue;
                    }

                    if (liveList == nullptr)
                    {
                        addHeading(resources::GetString(L"PaletteConnectedHeading"), nullptr);
                        liveList = addList();
                    }

                    auto detail = endpoint.ManufacturerName;

                    if (!endpoint.TransportCode.empty())
                    {
                        detail = detail.empty() ? endpoint.TransportCode : detail + L" \u00B7 " + endpoint.TransportCode;
                    }

                    if (auto const tile = BuildEndpointTile(
                        endpoint.EndpointDeviceId, endpoint.Name, detail, endpoint.ImagePath, IsOnCanvas(endpoint.EndpointDeviceId)))
                    {
                        liveList.Items().Append(tile);
                        shown++;
                    }
                }

                controls::ListView rememberedList{ nullptr };

                for (auto const& endpoint : RememberedEndpoints())
                {
                    if (!ContainsText(endpoint.DisplayName, search))
                    {
                        continue;
                    }

                    if (rememberedList == nullptr)
                    {
                        addHeading(resources::GetString(L"PaletteSeenBefore"), nullptr);
                        rememberedList = addList();
                    }

                    if (auto const tile = BuildEndpointTile(
                        endpoint.Match.EndpointDeviceId,
                        endpoint.DisplayName,
                        std::wstring{ resources::GetString(L"NodeNotConnected") },
                        std::wstring{},
                        IsOnCanvas(endpoint)))
                    {
                        rememberedList.Items().Append(tile);
                        shown++;
                    }
                }
            }

            if (shown == 0)
            {
                controls::TextBlock empty{};

                empty.Text(search.empty()
                    ? resources::GetString(L"PaletteNoEndpoints")
                    : resources::GetString(L"PaletteNoMatches"));
                empty.FontSize(12);
                empty.TextWrapping(xaml::TextWrapping::Wrap);
                empty.Margin(xaml::ThicknessHelper::FromLengths(2, 10, 2, 0));

                if (tertiary != nullptr)
                {
                    empty.Foreground(tertiary);
                }

                content.Children().Append(empty);
            }

            if (showEndpoints)
            {
                controls::Button loopback{};

                loopback.Content(winrt::box_value(resources::GetString(L"PaletteCreateLoopback")));
                loopback.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                loopback.Margin(xaml::ThicknessHelper::FromLengths(0, 12, 0, 0));

                loopback.Click([weak](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ShowCreateLoopbackDialogAsync();
                        }
                    });

                content.Children().Append(loopback);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the palette.")
    }

    _Use_decl_annotations_
    xaml::UIElement MainWindow::BuildBlockTile(patchbay::BlockKind kind) noexcept
    {
        try
        {
            auto const category = patchbay::CategoryOf(kind);
            auto const name = patchbay::BlockKindName(kind);
            auto const hint = patchbay::BlockKindHint(kind);
            auto const badge = patchbay::BlockKindBadge(kind);

            auto const& brushes = patchbay::ThemeBrushes::Current();

            controls::Grid tile{};

            // A quiet square face, the way MIDI Glass draws its palette. A Rectangle rather than a
            // Border: a Border's corners are stepped at fractional scaling.
            shapes::Rectangle face{};

            face.RadiusX(6);
            face.RadiusY(6);
            face.StrokeThickness(1);
            face.UseLayoutRounding(false);
            face.Fill(brushes.Get(L"SubtleFillColorSecondaryBrush"));
            face.Stroke(brushes.Get(L"ControlStrokeColorDefaultBrush"));

            tile.Children().Append(face);

            // Two rows of fixed height rather than a stack, as in MIDI Glass: every name in a row
            // starts on the same line, whether it takes one line or two.
            controls::Grid stack{};

            stack.VerticalAlignment(xaml::VerticalAlignment::Center);

            controls::RowDefinition artRow{};
            artRow.Height(xaml::GridLengthHelper::FromPixels(24));
            stack.RowDefinitions().Append(artRow);

            controls::RowDefinition captionRow{};
            captionRow.Height(xaml::GridLengthHelper::FromPixels(28));
            stack.RowDefinitions().Append(captionRow);

            // The same badge in the same color as the step on the canvas, so the two are seen to match.
            controls::TextBlock badgeText{};

            badgeText.Text(badge);
            badgeText.FontSize(badge.size() >= 3 ? 12 : 14);
            badgeText.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            badgeText.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            badgeText.VerticalAlignment(xaml::VerticalAlignment::Center);
            badgeText.Foreground(patchbay::PatchCanvas::CategoryBrush(category));

            controls::Grid::SetRow(badgeText, 0);
            stack.Children().Append(badgeText);

            controls::TextBlock caption{};

            caption.Text(patchbay::BlockKindShortName(kind));
            caption.FontSize(11);
            caption.LineHeight(13);
            caption.LineStackingStrategy(xaml::LineStackingStrategy::BlockLineHeight);
            caption.TextWrapping(xaml::TextWrapping::WrapWholeWords);
            caption.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            caption.MaxLines(2);
            caption.TextAlignment(xaml::TextAlignment::Center);
            caption.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            caption.VerticalAlignment(xaml::VerticalAlignment::Top);
            caption.Margin(xaml::ThicknessHelper::FromLengths(3, 1, 3, 0));
            caption.Foreground(brushes.Get(L"TextFillColorSecondaryBrush"));

            controls::Grid::SetRow(caption, 1);
            stack.Children().Append(caption);

            tile.Children().Append(stack);

            tile.Tag(winrt::box_value(winrt::hstring{ patchbay::BlockKindKey(kind) }));

            controls::ToolTipService::SetToolTip(tile, winrt::box_value(name + winrt::hstring{ L"\n" } + hint));
            xaml::Automation::AutomationProperties::SetName(tile, name);
            xaml::Automation::AutomationProperties::SetHelpText(tile, hint);

            return tile;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build a palette tile.")

        return nullptr;
    }

    _Use_decl_annotations_
    xaml::UIElement MainWindow::BuildEndpointTile(
        std::wstring const& endpointDeviceId,
        std::wstring const& name,
        std::wstring const& detail,
        std::wstring const& imagePath,
        bool onCanvas) noexcept
    {
        try
        {
            controls::Grid tile{};

            tile.ColumnSpacing(10);
            tile.Padding(xaml::ThicknessHelper::FromLengths(0, 3, 0, 3));

            for (auto const width : { xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                      xaml::GridLength{ 1, xaml::GridUnitType::Star },
                                      xaml::GridLength{ 0, xaml::GridUnitType::Auto } })
            {
                controls::ColumnDefinition column{};
                column.Width(width);
                tile.ColumnDefinitions().Append(column);
            }

            // The picture the customer chose for the device, cut to a rounded square, or an empty
            // square where it would go, so the names line up either way.
            shapes::Rectangle art{ nullptr };

            if (auto const picture = patchbay::PatchCanvas::LoadEndpointImage(imagePath, 56))
            {
                media::ImageBrush image{};
                image.ImageSource(picture);
                image.Stretch(media::Stretch::Uniform);

                art = patchbay::MakeRoundedShape(5, image);
            }
            else
            {
                art = patchbay::MakeRoundedShape(5,
                    patchbay::ThemeBrushes::Current().Get(L"SubtleFillColorSecondaryBrush"),
                    patchbay::ThemeBrushes::Current().Get(L"ControlStrokeColorDefaultBrush"));
            }

            art.Width(28);
            art.Height(28);
            art.VerticalAlignment(xaml::VerticalAlignment::Center);

            tile.Children().Append(art);

            controls::StackPanel text{};
            text.VerticalAlignment(xaml::VerticalAlignment::Center);

            controls::TextBlock title{};
            title.Text(winrt::hstring{ name });
            title.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            text.Children().Append(title);

            auto const secondary = onCanvas ? std::wstring{ resources::GetString(L"PaletteOnThisPatch") } : detail;

            if (!secondary.empty())
            {
                controls::TextBlock line{};

                line.Text(winrt::hstring{ secondary });
                line.FontSize(11);
                line.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                line.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));

                text.Children().Append(line);
            }

            controls::Grid::SetColumn(text, 1);
            tile.Children().Append(text);

            if (onCanvas)
            {
                controls::FontIcon check{};

                check.Glyph(L"\uE73E");
                check.FontSize(12);
                check.VerticalAlignment(xaml::VerticalAlignment::Center);
                check.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));

                controls::Grid::SetColumn(check, 2);
                tile.Children().Append(check);
            }

            tile.Tag(winrt::box_value(winrt::hstring{ endpointDeviceId }));

            controls::ToolTipService::SetToolTip(tile, winrt::box_value(onCanvas
                ? resources::FormatString(L"PaletteEndpointOnPatchFormat", name)
                : winrt::hstring{ name }));

            xaml::Automation::AutomationProperties::SetName(tile, onCanvas
                ? resources::FormatString(L"PaletteEndpointOnPatchFormat", name)
                : winrt::hstring{ name });

            return tile;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build an endpoint tile.")

        return nullptr;
    }

    // -------------------------------------------------------------------- steps

    _Use_decl_annotations_
    std::wstring MainWindow::AddBlock(patchbay::BlockKind kind, foundation::Point const& center) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return {};
            }

            if (patch->Blocks.size() >= patchbay::MaximumBlocksPerPatch)
            {
                ShowStatus(resources::GetString(L"StatusStepLimit"), controls::InfoBarSeverity::Warning);
                return {};
            }

            patchbay::PatchBlock block{};

            block.Id = patchbay::PatchDocument::NewId();
            block.Kind = kind;
            block.CanvasX = (std::max)(0.0, center.X - BlockHalfWidth);
            block.CanvasY = (std::max)(0.0, center.Y - BlockHalfHeight);
            block.Settings = patchbay::DefaultBlockSettings(kind);

            auto const id = block.Id;

            patch->Blocks.push_back(std::move(block));

            // Built first, so its real size is known before it is moved clear of the others,
            // and the move is part of the same undo step.
            RebuildCanvas();
            m_canvas.MoveClearOfOtherNodes(id);

            CommitChange(true);

            m_canvas.Select(patchbay::CanvasSelectionKind::Block, id);

            // Moved clear of the others, it can end up out of view, which looks like nothing happened.
            m_canvas.BringIntoView(id);

            // Ready to type, which is the first thing anybody does with a new annotation.
            if (patchbay::IsAnnotation(kind))
            {
                FocusAnnotationText(id);
            }

            return id;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to add a step.")

        return {};
    }

    _Use_decl_annotations_
    void MainWindow::InsertBlockIntoConnection(
        patchbay::BlockKind kind,
        std::wstring const& connectionId,
        std::optional<foundation::Point> const& center) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr || patch->FindConnection(connectionId) == nullptr)
            {
                return;
            }

            // A generator passes on only what it makes, and an annotation passes on nothing, so
            // either is added on its own instead.
            if (!patchbay::CanGoIntoConnection(kind))
            {
                AddBlock(kind, center.value_or(m_canvas.ViewCenter()));
                return;
            }

            if (patch->Blocks.size() >= patchbay::MaximumBlocksPerPatch)
            {
                ShowStatus(resources::GetString(L"StatusStepLimit"), controls::InfoBarSeverity::Warning);
                return;
            }

            if (patch->Connections.size() >= patchbay::MaximumConnectionsPerPatch)
            {
                ShowStatus(resources::GetString(L"ConnectionLimitReached"), controls::InfoBarSeverity::Warning);
                return;
            }

            auto const original = *patch->FindConnection(connectionId);

            // Halfway between the two ends, unless the step was dropped somewhere in particular.
            auto middle = center.value_or(foundation::Point{});

            if (!center.has_value())
            {
                auto const* sourceX = patch->NodeX(original.SourceId);
                auto const* sourceY = patch->NodeY(original.SourceId);
                auto const* destinationX = patch->NodeX(original.DestinationId);
                auto const* destinationY = patch->NodeY(original.DestinationId);

                if (sourceX == nullptr || sourceY == nullptr || destinationX == nullptr || destinationY == nullptr)
                {
                    middle = m_canvas.ViewCenter();
                }
                else
                {
                    auto const sourceSize = patchbay::EstimatedNodeSize(*patch, original.SourceId);

                    middle = foundation::Point{
                        static_cast<float>((*sourceX + sourceSize.Width + *destinationX) / 2),
                        static_cast<float>((*sourceY + *destinationY) / 2 + BlockHalfHeight) };
                }
            }

            patchbay::PatchBlock block{};

            block.Id = patchbay::PatchDocument::NewId();
            block.Kind = kind;
            block.CanvasX = (std::max)(0.0, middle.X - BlockHalfWidth);
            block.CanvasY = (std::max)(0.0, middle.Y - BlockHalfHeight);
            block.Settings = patchbay::DefaultBlockSettings(kind);

            // The first half keeps the link's id and its mute, so its counts carry on. The
            // second half takes the group the link was sending to.
            auto* first = patch->FindConnection(connectionId);

            first->DestinationId = block.Id;
            first->DestinationGroupIndex = patchbay::AllGroups;

            patchbay::PatchConnection second{};

            second.Id = patchbay::PatchDocument::NewId();
            second.SourceId = block.Id;
            second.SourceGroupIndex = patchbay::AllGroups;
            second.DestinationId = original.DestinationId;
            second.DestinationGroupIndex = original.DestinationGroupIndex;

            auto const id = block.Id;

            patch->Blocks.push_back(std::move(block));
            patch->Connections.push_back(std::move(second));

            CommitChange(true);

            m_canvas.Select(patchbay::CanvasSelectionKind::Block, id);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to put a step into the connection.")
    }

    _Use_decl_annotations_
    void MainWindow::OnBlockDropped(
        patchbay::BlockKind kind,
        foundation::Point const& point,
        std::wstring const& connectionId) noexcept
    {
        if (!connectionId.empty())
        {
            InsertBlockIntoConnection(kind, connectionId, point);
            return;
        }

        AddBlock(kind, point);
    }

    _Use_decl_annotations_
    void MainWindow::OnEndpointDropped(std::wstring const& endpointDeviceId, foundation::Point const& point) noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            // Already here: show where it is instead.
            for (auto const& endpoint : patch->Endpoints)
            {
                if (patchbay::StandsFor(endpoint, endpointDeviceId))
                {
                    m_canvas.Select(patchbay::CanvasSelectionKind::Endpoint, endpoint.Id);
                    ShowStatus(resources::GetString(L"StatusEndpointAlreadyHere"), controls::InfoBarSeverity::Informational);
                    return;
                }
            }

            if (auto const live = patchbay::EndpointCatalog::Current().Find(endpointDeviceId))
            {
                AddEndpointToPatch(live.value(), point);
                return;
            }

            for (auto const& remembered : RememberedEndpoints())
            {
                if (SameText(remembered.Match.EndpointDeviceId, endpointDeviceId))
                {
                    AddRememberedEndpointToPatch(remembered, point);
                    return;
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to add the endpoint that was dropped.")
    }

    _Use_decl_annotations_
    void MainWindow::SetBlockBypassed(std::wstring const& blockId, bool bypassed) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();
            auto* block = patch == nullptr ? nullptr : patch->FindBlock(blockId);

            if (block == nullptr || block->Bypassed == bypassed)
            {
                return;
            }

            block->Bypassed = bypassed;

            CommitChange(true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to bypass the step.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAutoArrangeClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        m_canvas.AutoArrange();
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomFitClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            ZoomFlyout().Hide();

            m_canvas.FitToContent(patchbay::PatchCanvas::MaximumZoom);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to fit the canvas to the screen.")
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomInClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyZoom(NextZoomStep(CanvasScroller().ZoomFactor(), true));
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomOutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyZoom(NextZoomStep(CanvasScroller().ZoomFactor(), false));
    }

    _Use_decl_annotations_
    void MainWindow::ApplyZoom(float zoom) noexcept
    {
        try
        {
            // Snapped to 5%, the same grain as the zoom box's spin buttons.
            auto const snapped = std::clamp(std::round(zoom * 20.0f) / 20.0f,
                patchbay::PatchCanvas::MinimumZoom, patchbay::PatchCanvas::MaximumZoom);

            CanvasScroller().ChangeView(nullptr, nullptr,
                winrt::box_value(snapped).as<foundation::IReference<float>>());

            UpdateZoomText(snapped);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change the zoom.")
    }

    _Use_decl_annotations_
    void MainWindow::UpdateZoomText(float zoom) noexcept
    {
        try
        {
            ZoomText().Text(resources::FormatString(L"ZoomPercentFormat",
                static_cast<int>(std::lround(zoom * 100))));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the zoom level.")
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomFlyoutOpening(foundation::IInspectable const& sender, foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            m_settingZoomBox = true;
            ZoomInputBox().Value(std::lround(CanvasScroller().ZoomFactor() * 100));
            m_settingZoomBox = false;
        }
        catch (...)
        {
            m_settingZoomBox = false;
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to show the zoom box.");
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomValueChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (m_settingZoomBox)
        {
            return;
        }

        // NumberBox raises this on commit, not per keystroke, so Enter and moving focus away
        // both land here and a half typed number never takes effect.
        auto const value = args.NewValue();

        if (!std::isnan(value))
        {
            ApplyZoom(static_cast<float>(value / 100.0));
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnZoomApplyClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            // Taking focus is what makes the number box commit what was typed.
            ZoomApplyButton().Focus(xaml::FocusState::Programmatic);

            ZoomFlyout().Hide();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to apply the zoom.")
    }

    _Use_decl_annotations_
    void MainWindow::OnInspectorCloseClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        m_canvas.ClearSelection();
    }

    _Use_decl_annotations_
    void MainWindow::OnShowLoopClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const& analysis = Analysis();

            if (analysis.Loops.empty() || analysis.Loops.front().ConnectionIds.empty())
            {
                return;
            }

            m_canvas.Select(patchbay::CanvasSelectionKind::Connection, analysis.Loops.front().ConnectionIds.front());
            m_canvas.FitToContent();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the loop.")
    }
}
