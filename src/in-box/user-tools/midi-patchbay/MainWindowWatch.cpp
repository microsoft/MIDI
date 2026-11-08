// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The connection monitor windows, from the editor's side: the connection's own menu, Watch
// messages, the dots on the canvas, and keeping the windows up to date with the patch. The
// windows themselves are in WatchWindow.cpp.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        controls::FontIcon MenuIcon(_In_ wchar_t const* glyph)
        {
            controls::FontIcon icon{};
            icon.Glyph(glyph);
            return icon;
        }
    }

    std::shared_ptr<patchbay::WatchWindows> MainWindow::WatchWindowList() noexcept
    {
        try
        {
            if (m_watchWindows == nullptr && !m_closing)
            {
                m_watchWindows = std::make_shared<patchbay::WatchWindows>(m_chrome.WindowHandle(), m_patchKey,
                    [weak = get_weak()]()
                    {
                        auto strong = weak.get();

                        if (strong != nullptr && !strong->m_closing && strong->m_watchWindows != nullptr)
                        {
                            strong->m_canvas.SetWatchMarks(strong->m_watchWindows->Marks());
                        }
                    });
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to start the monitor windows.")

        return m_watchWindows;
    }

    patchbay::WatchPatchState MainWindow::CurrentWatchState() noexcept
    {
        patchbay::WatchPatchState state{};

        state.Patch = CurrentPatch();
        state.Routing = !m_patchKey.empty() && patchbay::PatchLibrary::Current().IsRouting(m_patchKey);

        if (state.Patch != nullptr)
        {
            state.LoopMutedConnectionIds = &Analysis().LoopMutedConnectionIds;
        }

        return state;
    }

    _Use_decl_annotations_
    void MainWindow::WatchConnection(std::wstring const& connectionId) noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr || patch->FindConnection(connectionId) == nullptr)
            {
                return;
            }

            if (auto const windows = WatchWindowList())
            {
                windows->Watch(patchbay::WatchSubject{ connectionId, {} }, CurrentWatchState());
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to watch a connection.")
    }

    _Use_decl_annotations_
    void MainWindow::WatchNode(std::wstring const& nodeId) noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr || (patch->FindBlock(nodeId) == nullptr && patch->FindEndpoint(nodeId) == nullptr))
            {
                return;
            }

            if (auto const windows = WatchWindowList())
            {
                windows->Watch(patchbay::WatchSubject{ {}, nodeId }, CurrentWatchState());
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to watch what comes out of a step.")
    }

    void MainWindow::WatchSelection() noexcept
    {
        switch (m_canvas.SelectionKind())
        {
        case patchbay::CanvasSelectionKind::Connection:
            WatchConnection(m_canvas.SelectedConnectionId());
            break;

        case patchbay::CanvasSelectionKind::Block:
        case patchbay::CanvasSelectionKind::Endpoint:
            WatchNode(m_canvas.SelectedNodeId());
            break;

        default:
            break;
        }
    }

    void MainWindow::RefreshWatchWindows() noexcept
    {
        try
        {
            if (m_watchWindows == nullptr || m_closing)
            {
                return;
            }

            m_watchWindows->Refresh(CurrentWatchState());
            m_canvas.SetWatchMarks(m_watchWindows->Marks());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to refresh the monitor windows.")
    }

    void MainWindow::FeedTraceToWatchWindows() noexcept
    {
        if (m_watchWindows != nullptr && !m_closing)
        {
            m_watchWindows->FeedTrace(m_trace);
        }
    }

    void MainWindow::CloseWatchWindows() noexcept
    {
        if (auto const windows = std::exchange(m_watchWindows, nullptr))
        {
            windows->CloseAll();
        }
    }

    _Use_decl_annotations_
    void MainWindow::AddWatchNodeItem(controls::MenuFlyout const& menu, std::wstring const& nodeId) noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            // Nothing comes out of a note on the canvas.
            if (auto const* block = patch->FindBlock(nodeId); block != nullptr && patchbay::IsAnnotation(block->Kind))
            {
                return;
            }

            controls::MenuFlyoutItem watch{};
            watch.Text(resources::GetString(L"WatchEverythingOutMenuItem"));
            watch.Icon(MenuIcon(L"\xE7B3"));
            watch.KeyboardAcceleratorTextOverride(resources::GetString(L"WatchShortcutText"));
            watch.Click([weak = get_weak(), nodeId](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->WatchNode(nodeId);
                    }
                });

            menu.Items().InsertAt(0, watch);
            menu.Items().InsertAt(1, controls::MenuFlyoutSeparator{});
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to add Watch to the menu.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowConnectionMenu(std::wstring const& connectionId, foundation::Point const& position) noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();
            auto const* connection = patch == nullptr ? nullptr : patch->FindConnection(connectionId);

            if (connection == nullptr)
            {
                return;
            }

            auto weak = get_weak();
            controls::MenuFlyout menu{};

            // Names the connection, the way its window will.
            controls::MenuFlyoutItem heading{};
            heading.Text(resources::FormatString(L"WatchCaptionFormat",
                std::wstring{ DescribeLinkEnd(connection->SourceId, connection->SourceGroupIndex, true) },
                std::wstring{ DescribeLinkEnd(connection->DestinationId, connection->DestinationGroupIndex, false) }));
            heading.IsEnabled(false);
            menu.Items().Append(heading);
            menu.Items().Append(controls::MenuFlyoutSeparator{});

            controls::MenuFlyoutItem watch{};
            watch.Text(resources::GetString(L"WatchMessagesMenuItem"));
            watch.Icon(MenuIcon(L"\xE7B3"));
            watch.KeyboardAcceleratorTextOverride(resources::GetString(L"WatchShortcutText"));
            watch.Click([weak, connectionId](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->WatchConnection(connectionId);
                    }
                });
            menu.Items().Append(watch);

            controls::ToggleMenuFlyoutItem mute{};
            mute.Text(resources::GetString(L"ActionMute"));
            mute.IsChecked(connection->Muted);
            mute.Click([weak, connectionId](auto&&, auto&&)
                {
                    auto strong = weak.get();
                    auto* current = strong == nullptr ? nullptr : strong->CurrentPatch();

                    if (current == nullptr)
                    {
                        return;
                    }

                    if (auto* target = current->FindConnection(connectionId))
                    {
                        target->Muted = !target->Muted;
                        strong->CommitChange(true);
                    }
                });
            menu.Items().Append(mute);

            // The same list the inspector's Insert step button shows.
            controls::MenuFlyoutSubItem putStep{};
            putStep.Text(resources::GetString(L"WatchPutStepMenuItem"));
            putStep.Icon(MenuIcon(L"\xE710"));

            auto const steps = BuildAddStepMenu(connectionId);

            while (steps.Items().Size() > 0)
            {
                auto const item = steps.Items().GetAt(0);
                steps.Items().RemoveAt(0);
                putStep.Items().Append(item);
            }

            menu.Items().Append(putStep);
            menu.Items().Append(controls::MenuFlyoutSeparator{});

            // Asks first when the settings say to, like the Delete key.
            controls::MenuFlyoutItem remove{};
            remove.Text(resources::GetString(L"ActionRemoveConnection"));
            remove.Icon(MenuIcon(L"\xE74D"));
            remove.KeyboardAcceleratorTextOverride(resources::GetString(L"WatchRemoveShortcutText"));
            remove.Click([weak, connectionId](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_canvas.Select(patchbay::CanvasSelectionKind::Connection, connectionId);
                        strong->DeleteSelection();
                    }
                });
            menu.Items().Append(remove);

            primitives::FlyoutShowOptions options{};

            options.Position(position);
            options.Placement(primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);

            menu.ShowAt(CanvasScroller(), options);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the connection's menu.")
    }
}
