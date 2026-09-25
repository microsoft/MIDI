// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Setting the keyboard order by clicking through the page.
//
// The order the Tab key walks, and the order a screen reader reads, is one of the few things in
// a layout that is invisible until somebody tries to use it. Sorting by position guesses; this
// lets the customer say it, one control at a time, with every number on screen while they do it.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    _Use_decl_annotations_
    void EditorWindow::OnSortKeyboardOrderClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        SetKeyboardOrderMode(true);
    }

    _Use_decl_annotations_
    void EditorWindow::SetKeyboardOrderMode(bool active)
    {
        try
        {
            if (m_keyboardOrderMode == active)
            {
                return;
            }

            m_keyboardOrderMode = active;
            m_keyboardOrderPicked.clear();

            if (active)
            {
                // Handles and a selection outline over a page somebody is counting through are
                // noise, and a drag started by accident would move a control they only meant to
                // number.
                m_editor.ClearSelection();
                m_dragMode = DragMode::None;
                m_hasArmedKind = false;
                m_hasBand = false;

                SyncPaletteSelection();
            }

            KeyboardOrderBar().IsOpen(active);

            UpdateKeyboardOrderBar();
            UpdateOverlay();
            RefreshInspector();
            UpdateStatusBar();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the keyboard order mode.")
    }

    void EditorWindow::UpdateKeyboardOrderBar()
    {
        if (!m_keyboardOrderMode)
        {
            return;
        }

        auto const* const page = m_editor.CurrentPage();

        auto const total = page == nullptr ? size_t{ 0 } : page->Controls.size();
        auto const done = m_keyboardOrderPicked.size();

        KeyboardOrderBar().Title(resources::GetString(L"KeyboardOrderBarTitle"));

        KeyboardOrderBar().Message(done == 0
            ? resources::GetString(L"KeyboardOrderBarStart")
            : resources::FormatString(
                L"KeyboardOrderBarProgressFormat",
                std::to_wstring(done),
                std::to_wstring(total)));

        KeyboardOrderRestartButton().IsEnabled(done > 0);
    }

    // The number this control will end up with, and whether it has been clicked yet. Anything
    // not yet clicked keeps its old number, shown dimmed, so the page never looks half erased.
    _Use_decl_annotations_
    bool EditorWindow::KeyboardOrderNumberFor(std::wstring const& id, int32_t& number) const
    {
        for (size_t index = 0; index < m_keyboardOrderPicked.size(); ++index)
        {
            if (m_keyboardOrderPicked[index] == id)
            {
                number = static_cast<int32_t>(index) + 1;
                return true;
            }
        }

        number = 0;

        return false;
    }

    // A press on the canvas while the mode is up. Clicking a control gives it the next number;
    // clicking one that already has a new number takes it and everything after it back, which is
    // how somebody fixes a mis-click without starting again.
    _Use_decl_annotations_
    bool EditorWindow::HandleKeyboardOrderPress(double pageX, double pageY)
    {
        if (!m_keyboardOrderMode)
        {
            return false;
        }

        auto const hit = HitTest(pageX, pageY);

        if (hit.empty())
        {
            return true;
        }

        auto const found = std::find(m_keyboardOrderPicked.begin(), m_keyboardOrderPicked.end(), hit);

        if (found != m_keyboardOrderPicked.end())
        {
            m_keyboardOrderPicked.erase(found, m_keyboardOrderPicked.end());
        }
        else
        {
            m_keyboardOrderPicked.push_back(hit);
        }

        UpdateKeyboardOrderBar();
        UpdateOverlay();

        return true;
    }

    _Use_decl_annotations_
    void EditorWindow::OnKeyboardOrderDoneClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            // Anything never clicked keeps its relative order and follows the ones that were,
            // so leaving early is a partial answer rather than a wrecked one.
            if (m_editor.SetKeyboardOrderFromList(m_keyboardOrderPicked))
            {
                RebuildOutline();
                RefreshInspector();
                MarkChanged();
            }

            SetKeyboardOrderMode(false);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to apply the keyboard order.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnKeyboardOrderRestartClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        m_keyboardOrderPicked.clear();

        UpdateKeyboardOrderBar();
        UpdateOverlay();
    }

    _Use_decl_annotations_
    void EditorWindow::OnKeyboardOrderCancelClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        SetKeyboardOrderMode(false);
    }
}
