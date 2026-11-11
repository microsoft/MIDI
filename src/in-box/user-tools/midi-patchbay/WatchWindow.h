// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The connection monitor windows: small windows of their own that show everything going along
// one connection, or everything out of one step or endpoint, live while the patch routes and
// whenever Test runs. Each belongs to the editor window of its patch, and closes with it. They
// aren't saved with the patch.

#include "MessageWatch.h"
#include "PatchCanvas.h"
#include "PatchDocument.h"
#include "PatchTrace.h"

namespace midipatchbay
{
    // What a window watches: one connection, or everything out of one node.
    struct WatchSubject
    {
        std::wstring ConnectionId{};
        std::wstring NodeId{};

        bool operator==(WatchSubject const&) const = default;
    };

    // How the patch is now, as a window needs it.
    struct WatchPatchState
    {
        PatchDocument const* Patch{ nullptr };
        bool Routing{ false };
        std::unordered_set<std::wstring> const* LoopMutedConnectionIds{ nullptr };
    };

    class WatchWindow : public std::enable_shared_from_this<WatchWindow>
    {
    public:
        WatchWindow(
            _In_ HWND owner,
            _In_ std::wstring patchKey,
            _In_ uint32_t number,
            _In_ winrt::Windows::UI::Color color,
            _In_ WatchSubject subject,
            _In_ std::function<void(uint32_t)> closed) noexcept;

        ~WatchWindow() noexcept;

        WatchWindow(WatchWindow const&) = delete;
        WatchWindow& operator=(WatchWindow const&) = delete;

        bool Open(_In_ WatchPatchState const& state) noexcept;

        // Names, state and links, after any change to the patch or its routing.
        void Refresh(_In_ WatchPatchState const& state) noexcept;

        // A message from Test that went along a link this window may watch. ShowNewRows puts
        // what a whole test added into the list at once.
        void AddTest(_In_ std::wstring const& linkId, _In_ TraceMessage const& message) noexcept;
        void ShowNewRows() noexcept;

        void Activate() noexcept;
        void Close() noexcept;

        uint32_t Number() const noexcept { return m_number; }
        winrt::Windows::UI::Color Color() const noexcept { return m_color; }
        WatchSubject const& Subject() const noexcept { return m_subject; }

        // The connections it shows a dot on.
        std::vector<std::wstring> MarkedConnections() const;

    private:
        enum class State : int32_t
        {
            Routing = 0,
            NotRouting = 1,
            Muted = 2,
            Removed = 3,
        };

        void Build() noexcept;
        void PlaceNearOwner() noexcept;
        void OnClosed() noexcept;
        void OnTick() noexcept;
        void TakeIn(_In_ WatchedMessage const& message) noexcept;
        void UpdateCount() noexcept;
        void UpdateEmptyState() noexcept;
        void ShowState() noexcept;
        void Clear() noexcept;
        void SetPaused(_In_ bool paused) noexcept;
        void ApplyAlwaysOnTop() noexcept;
        void ShowRow(_In_ controls::ContainerContentChangingEventArgs const& args) noexcept;
        void ShowRowMenu(_In_ input::RightTappedRoutedEventArgs const& args) noexcept;
        void OnKeyDown(_In_ input::KeyRoutedEventArgs const& args) noexcept;

        std::wstring RowText(_In_ WatchRow const& row) const;
        winrt::hstring TimeText(_In_ WatchedMessage const& message) const;
        uint16_t LinkIndex(_In_ std::wstring const& linkId);
        std::vector<std::wstring> Cells() const;

        HWND m_owner{ nullptr };
        std::wstring m_patchKey{};
        uint32_t m_number{ 0 };
        winrt::Windows::UI::Color m_color{};
        WatchSubject m_subject{};
        std::function<void(uint32_t)> m_closed{};

        xaml::Window m_window{ nullptr };
        midiapp::WindowChrome m_chrome{};
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{ nullptr };

        WatchLease m_lease{};
        uint64_t m_origin{ 0 };
        uint64_t m_ticksPerSecond{ 0 };

        // The links this window has watched, in the order it first saw them, so a message's link
        // number always means the same link. Only grows.
        std::vector<std::wstring> m_links{};
        std::vector<std::wstring> m_linkNames{};

        std::wstring m_fromName{};
        std::wstring m_fromGroup{};
        std::wstring m_toName{};
        std::wstring m_toGroup{};
        std::wstring m_patchName{};
        State m_state{ State::NotRouting };

        WatchHistory m_history{};
        collections::IObservableVector<foundation::IInspectable> m_items{ nullptr };
        std::vector<WatchedMessage> m_drained{};

        uint64_t m_total{ 0 };

        // Rows added to the history, and pushed out of it, since the list last caught up.
        size_t m_pendingAdded{ 0 };
        size_t m_pendingRemoved{ 0 };

        controls::ScrollViewer ListScroller() noexcept;
        controls::ScrollViewer m_scroller{ nullptr };
        uint64_t m_droppedBefore{ 0 };
        uint64_t m_shownTotal{ UINT64_MAX };
        uint64_t m_shownDropped{ UINT64_MAX };

        bool m_paused{ false };
        bool m_showClock{ false };
        bool m_showActiveSensing{ false };
        bool m_alwaysOnTop{ true };
        bool m_closing{ false };

        // Named parts of the window, found once it is built.
        controls::Grid m_root{ nullptr };
        controls::TextBlock m_caption{ nullptr };
        controls::TextBlock m_fromLabel{ nullptr };
        controls::TextBlock m_fromText{ nullptr };
        controls::TextBlock m_toLabel{ nullptr };
        controls::TextBlock m_toText{ nullptr };
        xaml::UIElement m_arrow{ nullptr };
        shapes::Ellipse m_stateDot{ nullptr };
        controls::TextBlock m_stateText{ nullptr };
        controls::InfoBar m_removedBar{ nullptr };
        controls::Primitives::ToggleButton m_pauseButton{ nullptr };
        controls::FontIcon m_pauseIcon{ nullptr };
        controls::TextBlock m_countText{ nullptr };
        controls::ListView m_list{ nullptr };
        xaml::UIElement m_table{ nullptr };
        xaml::UIElement m_toolbar{ nullptr };
        xaml::UIElement m_empty{ nullptr };
        controls::TextBlock m_emptyTitle{ nullptr };
        controls::Border m_valueBrushSource{ nullptr };
        controls::Border m_successBrushSource{ nullptr };
    };

    // Every monitor window one editor has open. Numbers them, gives each a color, and closes
    // them all with the editor.
    class WatchWindows : public std::enable_shared_from_this<WatchWindows>
    {
    public:
        WatchWindows(
            _In_ HWND owner,
            _In_ std::wstring patchKey,
            _In_ std::function<void()> marksChanged) noexcept;

        // Opens a window, or brings forward the one already watching the same thing.
        void Watch(_In_ WatchSubject const& subject, _In_ WatchPatchState const& state) noexcept;

        void Refresh(_In_ WatchPatchState const& state) noexcept;

        // Hands what a test sent along each link to the windows watching it.
        void FeedTrace(_In_ TraceResult const& trace) noexcept;

        std::vector<PatchCanvas::WatchMark> Marks() const noexcept;

        void BringForward(_In_ uint32_t number) noexcept;

        void CloseAll() noexcept;

    private:
        void OnWindowClosed(_In_ uint32_t number) noexcept;

        HWND m_owner{ nullptr };
        std::wstring m_patchKey{};
        std::function<void()> m_marksChanged{};
        std::vector<std::shared_ptr<WatchWindow>> m_windows{};
    };
}
