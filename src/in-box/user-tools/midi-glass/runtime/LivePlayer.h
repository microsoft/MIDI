// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <sal.h>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "LayoutModel.h"
#include "BindingEngine.h"
#include "ValueThrottle.h"
#include "DeviceCatalog.h"
#include "OutputRouter.h"

namespace glass
{
    // One message that reached a connection. What the editor's monitor rail draws, and the only
    // place the app can honestly say a thing was sent rather than merely built.
    struct SentMessage
    {
        uint64_t TimestampMilliseconds{ 0 };

        // The engine's index, counting every page in order. Not the item index of a page.
        uint32_t ControlIndex{ 0 };

        int32_t DestinationIndex{ -1 };

        uint32_t WordCount{ 0 };
        uint32_t Words[4]{};
    };

    // Everything between a value on the surface and words on the wire: the layout's device
    // table, the connections, the binding engine and the per-control throttles.
    //
    // The runtime window and the editor's Try mode both drive one of these. That is the whole
    // reason it exists: a fader has to send exactly the same thing while it is being built as it
    // does once the layout is running, and two copies of this code would drift apart.
    //
    // Held by shared_ptr, because it starts detached threads that block on the service and they
    // must not call into a freed object when a window closes under them.
    //
    // Not pure, and not in the unit tests: it opens connections and marshals through a dispatcher
    // queue. The arithmetic lives in BindingEngine and ValueThrottle, which are pure and are.
    class LivePlayer : public std::enable_shared_from_this<LivePlayer>
    {
    public:
        static std::shared_ptr<LivePlayer> Create();

        ~LivePlayer() noexcept;

        LivePlayer(LivePlayer const&) = delete;
        LivePlayer& operator=(LivePlayer const&) = delete;

        // ---- what the owner hears about. All raised on the dispatcher's thread. ----

        // The device table resolved, or something came or went. The owner redraws its status.
        std::function<void()> DevicesChanged{};

        // A device sent something that moves a control. The owner owns what that does on screen,
        // because the editor and the runtime window draw the same control differently.
        std::function<void(uint32_t controlIndex, double value)> FeedbackMoved{};

        // Every message that actually went out. Left empty by the runtime window, which pays
        // nothing for it; the editor's monitor rail sets it.
        std::function<void(SentMessage const& message)> Sent{};

        // ---- lifetime ----

        // ownerId separates one player's connections from another's inside the process-wide
        // router. A layout being edited and the same layout running are two owners on purpose,
        // so closing one does not take the other's connections down.
        void Start(
            _In_ LayoutDocument const& document,
            _In_ winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
            _In_ std::wstring const& ownerId);

        // The document changed under an already-started player, which is what every edit does in
        // Try mode. Re-resolves and re-prepares without closing anything that is still wanted.
        void UpdateDocument(_In_ LayoutDocument const& document);

        void Stop();

        // The editor leaves Try mode far more often than it closes, and tearing the player down
        // each time would race a close against the next open on the same owner id. Muting is the
        // gate instead: the watcher can still reopen connections underneath, but nothing reaches
        // the wire. Without this, a MIDI device arriving while the editor sat in Edit mode sent a
        // whole layout's worth of startup values.
        void SetOutputEnabled(_In_ bool enabled) noexcept { m_outputEnabled = enabled; }

        // ---- sending. Called on the UI thread, straight out of a pointer handler. ----

        void ValueChanged(_In_ uint32_t controlIndex, _In_ double value, _In_ bool isFinal);

        // Assistive technology setting a value is one discrete change, not a drag, so it is a
        // whole gesture. Going straight to the release would find nothing held back.
        void SetDirectly(_In_ uint32_t controlIndex, _In_ double value);

        void Switched(_In_ uint32_t controlIndex, _In_ bool isOn);

        // Where the nearest stop is, or the position unchanged.
        double SnapToDetent(_In_ uint32_t controlIndex, _In_ double position) const;

        // Sent once per run, in keyboard order. Does nothing until something is connected, so
        // the layout is not marked initialized before it actually was.
        void SendStartupValues();

        // Everything this process is driving, not just this player. Blocking work is detached.
        static void Panic();

        // ---- what the owner shows ----

        std::vector<ResolvedDevice> Devices() const { return m_devices.Devices(); }

        bool IsConnected() const noexcept { return !m_sendTable.empty(); }

        BindingEngine const& Engine() const noexcept { return m_engine; }

    private:
        LivePlayer() = default;

        void RebuildThrottles();
        void ReopenConnections();
        void SendPrepared(_In_ uint32_t controlIndex, _In_ uint32_t count) noexcept;

        // On a service callback thread. Resolves what moved and marshals only the answer.
        void OnFeedbackWords(
            _In_ uint32_t wordCount,
            _In_reads_(wordCount) uint32_t const* words);

        LayoutDocument m_document{};

        BindingEngine m_engine{};
        DeviceCatalog m_devices{};

        // One per control, so a fader on a DIN cable can be limited without touching a note on.
        std::vector<ValueThrottle> m_throttles{};

        // Rebuilt when the device table changes, read only on the UI thread, so the path a finger
        // takes never waits on a lock. Swapped whole rather than edited.
        std::vector<winrt::com_ptr<IMidiEndpointConnectionRaw>> m_sendTable{};

        // Caller-owned, reused, never resized on the hot path.
        std::array<PreparedSend, MaximumSendsPerEvent> m_sends{};

        std::wstring m_ownerId{};

        // What the device table resolved to last time. An unrelated device arriving must not
        // rebuild connections that did not change, and must not replay the startup values.
        std::wstring m_destinationSignature{};

        bool m_startupValuesSent{ false };
        bool m_started{ false };
        bool m_stopping{ false };
        bool m_outputEnabled{ true };

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
    };
}
