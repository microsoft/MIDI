// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "PatchGraph.h"
#include "PatchModel.h"
#include "RouteEngine.h"

namespace midipatchbay
{
    enum class LibraryChange : int32_t
    {
        // A patch was added or removed, or renamed.
        PatchList = 0,

        // Something in one patch changed. The key says which.
        Patch = 1,

        // Which patches are routing changed.
        Routing = 2,

        // Endpoints came or went.
        Endpoints = 3,

        // New counts from the engine.
        Activity = 4,

        // A patch was written to disk, or could not be. The key says which.
        Saved = 5,
    };

    // The app's patches while it runs, shared by the library window and every editor. Each patch
    // is known by its SessionKey, which stays the same when the patch is renamed or saved.
    //
    // UI thread only. Routing is worked out here and handed to the engine on a background
    // thread, because the engine blocks on the service.
    class PatchLibrary
    {
    public:
        using Listener = std::function<void(LibraryChange, std::wstring const&)>;

        static PatchLibrary& Current() noexcept;

        // Reads the patch folder, and turns routing on for the patches that start by themselves.
        void Load() noexcept;

        // In the order they were loaded or made. A pointer stays good until its patch is removed.
        std::vector<PatchDocument*> Patches() noexcept;

        PatchDocument* Find(_In_ std::wstring const& key) noexcept;

        // A new patch is temporary until it is saved, and routes as soon as it exists:
        // drawing a connection that does nothing would be a puzzle rather than a feature.
        PatchDocument* Create(_In_ std::wstring const& preferredName = {}) noexcept;

        // Takes in a patch read from somewhere else, or a copy of one already here.
        PatchDocument* Add(_In_ PatchDocument patch, _In_ bool routing) noexcept;

        // Deletes the file as well.
        bool Remove(_In_ std::wstring const& key) noexcept;

        bool IsRouting(_In_ std::wstring const& key) const noexcept;
        void SetRouting(_In_ std::wstring const& key, _In_ bool routing) noexcept;
        void StopAllRouting() noexcept;
        size_t RoutingCount() const noexcept;

        // Something in the patch changed. It is marked unsaved and written a moment later if it
        // has a file. When routing is affected its loops are checked again and the routes rebuilt.
        void Changed(_In_ std::wstring const& key, _In_ bool routingAffected = true) noexcept;

        bool IsUnsaved(_In_ std::wstring const& key) const noexcept;

        // Writes the changed patches that have files, once nothing has changed for a moment.
        void SaveDueChanges() noexcept;

        // Writes the patch now. A temporary patch becomes a saved one.
        bool Save(_In_ std::wstring const& key) noexcept;

        winrt::hstring LastErrorMessage() const noexcept { return m_lastError; }

        PatchAnalysis const& Analysis(_In_ std::wstring const& key) noexcept;

        std::vector<LiveEndpoint> const& LiveEndpoints() const noexcept { return m_liveEndpoints; }

        // From the endpoint catalog: every patch is checked again and the routes rebuilt.
        void EndpointsChanged() noexcept;

        // Fetches the engine's counts and tells the windows.
        void RefreshActivity() noexcept;

        // Counts for one patch, keyed by link or block id.
        std::unordered_map<std::wstring, RouteStats> Activity(_In_ std::wstring const& key) const noexcept;

        // Messages that reached a destination, in every patch.
        uint64_t TotalDelivered() const noexcept;

        // Why a patch that is routing does not route, when it doesn't.
        std::optional<RouteProblemKind> Problem(_In_ std::wstring const& key) const noexcept;

        bool HasMissingEndpoint(_In_ std::wstring const& key) noexcept;

        // The names a patch can't have, so a new one doesn't collide.
        std::wstring UniqueName(_In_ std::wstring const& preferredName) const noexcept;

        uint32_t Subscribe(_In_ Listener listener) noexcept;
        void Unsubscribe(_In_ uint32_t token) noexcept;

        // Rebuilds the routes from every patch that is routing.
        void ApplyRouting() noexcept;

    private:
        PatchLibrary() noexcept = default;

        void Notify(_In_ LibraryChange change, _In_ std::wstring const& key) noexcept;
        void Analyze(_In_ PatchDocument const& patch) noexcept;

        std::vector<std::unique_ptr<PatchDocument>> m_patches{};

        std::unordered_set<std::wstring> m_routing{};
        std::unordered_set<std::wstring> m_unsaved{};
        std::chrono::steady_clock::time_point m_lastChange{};

        std::unordered_map<std::wstring, PatchAnalysis> m_analysis{};
        std::vector<LiveEndpoint> m_liveEndpoints{};

        // As the engine keys them: "session key|element id".
        std::unordered_map<std::wstring, RouteStats> m_stats{};

        std::unordered_map<std::wstring, RouteProblemKind> m_problems{};

        std::vector<std::pair<uint32_t, Listener>> m_listeners{};
        uint32_t m_nextToken{ 1 };

        winrt::hstring m_lastError{};
    };
}
