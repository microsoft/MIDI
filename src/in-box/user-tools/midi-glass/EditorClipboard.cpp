// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Cut, copy and paste, from the keyboard and from the menu a right click opens on the canvas.
// What goes on the clipboard is worked out by the editor controller; this file only moves it to
// and from the Windows clipboard and redraws afterwards.

#include "pch.h"
#include "EditorWindow.xaml.h"

namespace winrt::midiglass::implementation
{
    namespace
    {
        namespace datatransfer = ::winrt::Windows::ApplicationModel::DataTransfer;
        namespace streams = ::winrt::Windows::Storage::Streams;

        // Only this app writes it and only this app reads it. Anything else on the clipboard is
        // taken as text or not at all.
        constexpr wchar_t ControlsFormat[] = L"MidiGlass.Controls";

        // The clipboard hands a custom format back as a stream whatever was put in, and the stream
        // can come back longer than what was written. So the controls go in as UTF-8 behind their
        // own length, and a paste reads exactly that many bytes. Any app can put data under this
        // format name, so the length is checked against what arrived and against a cap.
        constexpr uint32_t MaxControlsBytes = 64u * 1024u * 1024u;
    }

    bool EditorWindow::IsTextEntryFocused()
    {
        auto const focused = xaml::Input::FocusManager::GetFocusedElement(RootGrid().XamlRoot());

        return focused != nullptr &&
            (focused.try_as<controls::TextBox>() != nullptr ||
             focused.try_as<controls::NumberBox>() != nullptr ||
             focused.try_as<controls::AutoSuggestBox>() != nullptr ||
             focused.try_as<controls::RichEditBox>() != nullptr ||
             focused.try_as<controls::PasswordBox>() != nullptr);
    }

    _Use_decl_annotations_
    winrt::fire_and_forget EditorWindow::CopySelectionToClipboardAsync(bool cut)
    {
        auto lifetime = get_strong();

        try
        {
            auto const text = m_editor.CopySelection();

            if (text.empty())
            {
                co_return;
            }

            auto const utf8 = winrt::to_string(text);

            if (utf8.size() > MaxControlsBytes)
            {
                co_return;
            }

            // A cut removes what was selected when it was asked for, not whatever is selected by
            // the time the clipboard has the copy.
            auto const copied = m_editor.Selection();

            std::vector<uint8_t> bytes(utf8.begin(), utf8.end());

            streams::InMemoryRandomAccessStream stream{};
            streams::DataWriter writer{ stream };

            writer.ByteOrder(streams::ByteOrder::LittleEndian);
            writer.WriteUInt32(static_cast<uint32_t>(bytes.size()));
            writer.WriteBytes(bytes);

            co_await writer.StoreAsync();

            writer.DetachStream();
            stream.Seek(0);

            datatransfer::DataPackage package{};

            package.RequestedOperation(cut
                ? datatransfer::DataPackageOperation::Move
                : datatransfer::DataPackageOperation::Copy);

            package.SetData(ControlsFormat, stream);

            // Throws when another app has the clipboard open. Nothing is removed until the copy
            // has landed, so a cut that fails loses nothing.
            datatransfer::Clipboard::SetContent(package);

            try
            {
                // Still there after this window closes.
                datatransfer::Clipboard::Flush();
            }
            catch (...)
            {
            }

            if (cut && m_editor.Selection() == copied && !m_editor.CutSelection().empty())
            {
                RebuildSurface();
                RebuildOutline();
                RefreshInspector();
                UpdateOffPageBar();
                UpdateStatusBar();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to put the selection on the clipboard.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget EditorWindow::PasteFromClipboardAsync(bool atMenuPoint)
    {
        auto lifetime = get_strong();

        // Read now: another right click could move them while the clipboard is being read.
        auto pageX = m_menuPageX;
        auto pageY = m_menuPageY;

        try
        {
            auto const content = datatransfer::Clipboard::GetContent();

            if (content == nullptr)
            {
                co_return;
            }

            auto pasted = false;

            if (content.Contains(ControlsFormat))
            {
                auto const data = co_await content.GetDataAsync(ControlsFormat);
                auto const stream = data.try_as<streams::IRandomAccessStream>();

                if (stream != nullptr && stream.Size() >= sizeof(uint32_t))
                {
                    auto const size = static_cast<uint32_t>(
                        (std::min)(stream.Size(), static_cast<uint64_t>(MaxControlsBytes) + sizeof(uint32_t)));

                    streams::DataReader reader{ stream.GetInputStreamAt(0) };
                    reader.ByteOrder(streams::ByteOrder::LittleEndian);

                    auto const loaded = co_await reader.LoadAsync(size);

                    if (loaded >= sizeof(uint32_t))
                    {
                        auto const length = reader.ReadUInt32();

                        if (length <= loaded - sizeof(uint32_t) && length <= MaxControlsBytes)
                        {
                            std::vector<uint8_t> bytes(length);
                            reader.ReadBytes(bytes);

                            auto const json = winrt::to_hstring(std::string_view{
                                reinterpret_cast<char const*>(bytes.data()), bytes.size() });

                            pasted = m_editor.PasteControls(std::wstring{ json });
                        }
                    }
                }
            }
            else if (content.Contains(datatransfer::StandardDataFormats::Text()))
            {
                auto const text = co_await content.GetTextAsync();

                // At the right click that asked for it, or in the middle of what is on screen.
                if (!atMenuPoint)
                {
                    auto const viewport = CanvasScroll().TransformToVisual(OverlayCanvas()).TransformPoint(
                        foundation::Point{
                            static_cast<float>(CanvasScroll().ViewportWidth() / 2.0),
                            static_cast<float>(CanvasScroll().ViewportHeight() / 2.0) });

                    pageX = PointToPageX(viewport.X);
                    pageY = PointToPageY(viewport.Y);
                }

                pasted = m_editor.PasteText(std::wstring{ text }, pageX, pageY);
            }

            if (pasted)
            {
                RebuildSurface();
                RebuildOutline();
                RefreshInspector();
                UpdateOffPageBar();
                UpdateStatusBar();
                SelectOutlineRowForSelection();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to paste.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnCutAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (IsTextEntryFocused())
        {
            return;
        }

        args.Handled(true);
        CopySelectionToClipboardAsync(true);
    }

    _Use_decl_annotations_
    void EditorWindow::OnCopyAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (IsTextEntryFocused())
        {
            return;
        }

        args.Handled(true);
        CopySelectionToClipboardAsync(false);
    }

    _Use_decl_annotations_
    void EditorWindow::OnPasteAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (IsTextEntryFocused())
        {
            return;
        }

        args.Handled(true);
        PasteFromClipboardAsync(false);
    }

    _Use_decl_annotations_
    void EditorWindow::OnCutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        CopySelectionToClipboardAsync(true);
    }

    _Use_decl_annotations_
    void EditorWindow::OnCopyClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        CopySelectionToClipboardAsync(false);
    }

    _Use_decl_annotations_
    void EditorWindow::OnPasteClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        PasteFromClipboardAsync(m_hasMenuPoint);
    }

    _Use_decl_annotations_
    void EditorWindow::OnCanvasMenuOpening(foundation::IInspectable const& sender, foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const selected = !m_editor.Selection().empty();

            CanvasCutItem().IsEnabled(selected);
            CanvasCopyItem().IsEnabled(selected);
            CanvasDuplicateItem().IsEnabled(selected);
            CanvasDeleteItem().IsEnabled(selected);
            CanvasToFrontItem().IsEnabled(selected);
            CanvasToBackItem().IsEnabled(selected);

            CanvasGroupItem().IsEnabled(m_editor.Selection().size() > 1 && !m_editor.SelectionIsOneGroup());
            CanvasUngroupItem().IsEnabled(m_editor.SelectionHasGroup());

            auto canPaste = false;

            try
            {
                auto const content = datatransfer::Clipboard::GetContent();

                canPaste = content != nullptr &&
                    (content.Contains(ControlsFormat) ||
                     content.Contains(datatransfer::StandardDataFormats::Text()));
            }
            catch (...)
            {
                // Another app holding the clipboard open. Paste stays on; it will say nothing
                // if there turns out to be nothing to paste.
                canPaste = true;
            }

            CanvasPasteItem().IsEnabled(canPaste);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to fill in the canvas menu.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnCanvasMenuClosed(foundation::IInspectable const& sender, foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        // A menu opened from the keyboard has no point of its own; Paste then uses the middle.
        m_hasMenuPoint = false;
    }

    // ---------------------------------------------------------------- groups

    _Use_decl_annotations_
    void EditorWindow::ApplyGrouping(bool group)
    {
        try
        {
            if (group ? m_editor.GroupSelection() : m_editor.UngroupSelection())
            {
                UpdateOverlay();
                RebuildOutline();
                RefreshInspector();
                UpdateStatusBar();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to group or ungroup the selection.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnGroupClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        // One button that does whichever makes sense for what is picked.
        ApplyGrouping(!m_editor.SelectionIsOneGroup());
    }

    _Use_decl_annotations_
    void EditorWindow::OnGroupMenuClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyGrouping(true);
    }

    _Use_decl_annotations_
    void EditorWindow::OnUngroupMenuClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyGrouping(false);
    }

    _Use_decl_annotations_
    void EditorWindow::OnGroupAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (IsTextEntryFocused())
        {
            return;
        }

        args.Handled(true);
        ApplyGrouping(true);
    }

    _Use_decl_annotations_
    void EditorWindow::OnUngroupAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (IsTextEntryFocused())
        {
            return;
        }

        args.Handled(true);
        ApplyGrouping(false);
    }
}
