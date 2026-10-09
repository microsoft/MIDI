// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "PatchLibrary.h"

#include "AppSettings.h"
#include "BackgroundWork.h"
#include "CiFileStore.h"
#include "PatchProvenance.h"
#include "PatchStore.h"
#include "RouteGraph.h"
#include "StringResources.h"

namespace midipatchbay
{
    namespace
    {
        // Debounced: dragging a node raises a change per drop, and a patch with a dozen
        // endpoints would otherwise rewrite its file a dozen times in a few seconds.
        constexpr auto AutoSaveQuietPeriod = std::chrono::milliseconds{ 1500 };

        // Graphs are applied on the thread pool, so two in a row could arrive out of order. Each
        // carries a number, and one older than what is running is dropped.
        std::mutex g_applyLock{};
        uint64_t g_lastApplied{ 0 };
        std::atomic<uint64_t> g_lastRequested{ 0 };

        PatchAnalysis const& EmptyAnalysis() noexcept
        {
            static PatchAnalysis const empty{};
            return empty;
        }
    }

    PatchLibrary& PatchLibrary::Current() noexcept
    {
        static PatchLibrary instance{};
        return instance;
    }

    void PatchLibrary::Load() noexcept
    {
        try
        {
            std::vector<PatchDocument> loaded{};

            auto& store = PatchStore::Current();

            if (!store.LoadAll(loaded))
            {
                m_lastError = store.LastErrorMessage();
            }

            auto const startRouting = AppSettings::Current().ActivateSavedPatchesAtStartup();

            for (auto& patch : loaded)
            {
                patch.SessionKey = PatchDocument::NewId();

                // A newer version's patch only routes when the customer starts it, knowing some of
                // it can't run here.
                if (startRouting && patch.ActivateAtStartup && !patch.IsFromNewerVersion)
                {
                    m_routing.insert(patch.SessionKey);
                }

                m_patches.push_back(std::make_unique<PatchDocument>(std::move(patch)));
            }

            m_liveEndpoints = EndpointCatalog::Current().Snapshot();

            for (auto const& patch : m_patches)
            {
                Analyze(*patch);
            }

            ApplyRouting();
            Notify(LibraryChange::PatchList, {});
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to load the patches.")
    }

    std::vector<PatchDocument*> PatchLibrary::Patches() noexcept
    {
        std::vector<PatchDocument*> result{};

        try
        {
            for (auto const& patch : m_patches)
            {
                result.push_back(patch.get());
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to list the patches.")

        return result;
    }

    _Use_decl_annotations_
    PatchDocument* PatchLibrary::Find(std::wstring const& key) noexcept
    {
        if (key.empty())
        {
            return nullptr;
        }

        auto const found = std::find_if(m_patches.begin(), m_patches.end(),
            [&key](std::unique_ptr<PatchDocument> const& p) { return p->SessionKey == key; });

        return found == m_patches.end() ? nullptr : found->get();
    }

    _Use_decl_annotations_
    std::wstring PatchLibrary::UniqueName(std::wstring const& preferredName) const noexcept
    {
        try
        {
            auto const base = preferredName.empty()
                ? std::wstring{ resources::GetString(L"UntitledPatchName") }
                : SanitizeStoredString(preferredName);

            auto const taken = [this](std::wstring const& name)
                {
                    return std::any_of(m_patches.begin(), m_patches.end(),
                        [&name](std::unique_ptr<PatchDocument> const& p)
                        {
                            return ::CompareStringOrdinal(p->Name.c_str(), -1, name.c_str(), -1, TRUE) == CSTR_EQUAL;
                        });
                };

            auto name = base;

            for (int suffix = 2; taken(name) && suffix < 1000; suffix++)
            {
                name = preferredName.empty()
                    ? std::wstring{ resources::FormatString(L"UntitledPatchNameFormat", suffix) }
                    : std::wstring{ resources::FormatString(L"PatchNameSuffixFormat", base, suffix) };
            }

            return name;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to name a patch.")

        return preferredName;
    }

    _Use_decl_annotations_
    PatchDocument* PatchLibrary::Create(std::wstring const& preferredName) noexcept
    {
        try
        {
            PatchDocument patch{};

            patch.Name = UniqueName(preferredName);
            patch.IsTemporary = true;
            patch.ActivateAtStartup = true;
            patch.Provenance = NewProvenance();

            return Add(std::move(patch), true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to create a patch.")

        return nullptr;
    }

    _Use_decl_annotations_
    PatchDocument* PatchLibrary::Add(PatchDocument patch, bool routing) noexcept
    {
        try
        {
            if (m_patches.size() >= MaximumPatchCount)
            {
                m_lastError = resources::GetString(L"ErrorTooManyPatches");
                return nullptr;
            }

            patch.SessionKey = PatchDocument::NewId();

            auto const key = patch.SessionKey;

            m_patches.push_back(std::make_unique<PatchDocument>(std::move(patch)));

            if (routing)
            {
                m_routing.insert(key);
            }

            Analyze(*m_patches.back());

            if (routing)
            {
                ApplyRouting();
            }

            Notify(LibraryChange::PatchList, key);

            return m_patches.back().get();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to add a patch.")

        return nullptr;
    }

    _Use_decl_annotations_
    bool PatchLibrary::Remove(std::wstring key) noexcept
    {
        try
        {
            auto const found = std::find_if(m_patches.begin(), m_patches.end(),
                [&key](std::unique_ptr<PatchDocument> const& p) { return p->SessionKey == key; });

            if (found == m_patches.end())
            {
                return false;
            }

            if (!PatchStore::Current().Delete(**found))
            {
                m_lastError = PatchStore::Current().LastErrorMessage();
                return false;
            }

            auto const wasRouting = m_routing.erase(key) != 0;

            m_unsaved.erase(key);
            m_analysis.erase(key);
            m_problems.erase(key);

            // Listeners are told first, so an editor showing it can close while the patch is
            // still there to read.
            Notify(LibraryChange::PatchList, key);

            m_patches.erase(std::find_if(m_patches.begin(), m_patches.end(),
                [&key](std::unique_ptr<PatchDocument> const& p) { return p->SessionKey == key; }));

            if (wasRouting)
            {
                ApplyRouting();
                Notify(LibraryChange::Routing, key);
            }

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to remove a patch.")

        return false;
    }

    _Use_decl_annotations_
    bool PatchLibrary::IsRouting(std::wstring const& key) const noexcept
    {
        return m_routing.count(key) != 0;
    }

    _Use_decl_annotations_
    void PatchLibrary::SetRouting(std::wstring const& key, bool routing) noexcept
    {
        try
        {
            if (Find(key) == nullptr || IsRouting(key) == routing)
            {
                return;
            }

            if (routing)
            {
                m_routing.insert(key);
            }
            else
            {
                m_routing.erase(key);
            }

            ApplyRouting();
            Notify(LibraryChange::Routing, key);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change whether a patch is routing.")
    }

    void PatchLibrary::StopAllRouting() noexcept
    {
        try
        {
            m_routing.clear();

            ApplyRouting();
            Notify(LibraryChange::Routing, {});
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to stop routing.")
    }

    size_t PatchLibrary::RoutingCount() const noexcept
    {
        return m_routing.size();
    }

    _Use_decl_annotations_
    void PatchLibrary::Changed(std::wstring const& key, bool routingAffected) noexcept
    {
        try
        {
            auto const* patch = Find(key);

            if (patch == nullptr)
            {
                return;
            }

            m_unsaved.insert(key);
            m_lastChange = std::chrono::steady_clock::now();

            if (routingAffected)
            {
                Analyze(*patch);

                if (IsRouting(key))
                {
                    ApplyRouting();
                }
            }

            Notify(LibraryChange::Patch, key);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to record a change to a patch.")
    }

    _Use_decl_annotations_
    bool PatchLibrary::IsUnsaved(std::wstring const& key) const noexcept
    {
        return m_unsaved.count(key) != 0;
    }

    void PatchLibrary::SaveDueChanges() noexcept
    {
        try
        {
            if (m_unsaved.empty() || m_lastChange.time_since_epoch().count() == 0)
            {
                return;
            }

            if (std::chrono::steady_clock::now() - m_lastChange < AutoSaveQuietPeriod)
            {
                return;
            }

            // Temporary patches stay in memory until the customer names them, and a newer
            // version's patch is never written by this one.
            std::vector<std::wstring> due{};

            for (auto const& patch : m_patches)
            {
                if (!patch->FilePath.empty() && !patch->IsFromNewerVersion && m_unsaved.count(patch->SessionKey) != 0)
                {
                    due.push_back(patch->SessionKey);
                }
            }

            for (auto const& key : due)
            {
                Save(key);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to save the patches automatically.")
    }

    _Use_decl_annotations_
    bool PatchLibrary::Save(std::wstring const& key) noexcept
    {
        try
        {
            auto* patch = Find(key);

            if (patch == nullptr)
            {
                return false;
            }

            auto& store = PatchStore::Current();
            auto const saved = store.Save(*patch);

            // Cleared either way, so a failure is reported once rather than on every tick.
            m_unsaved.erase(key);

            if (!saved)
            {
                m_lastError = store.LastErrorMessage();
            }

            Notify(LibraryChange::Saved, key);

            return saved;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to save a patch.")

        return false;
    }

    _Use_decl_annotations_
    PatchAnalysis const& PatchLibrary::Analysis(std::wstring const& key) noexcept
    {
        try
        {
            auto found = m_analysis.find(key);

            if (found == m_analysis.end())
            {
                if (auto const* patch = Find(key))
                {
                    Analyze(*patch);
                    found = m_analysis.find(key);
                }
            }

            if (found != m_analysis.end())
            {
                return found->second;
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to look up a patch's loops.")

        return EmptyAnalysis();
    }

    _Use_decl_annotations_
    void PatchLibrary::Analyze(PatchDocument const& patch) noexcept
    {
        try
        {
            m_analysis[patch.SessionKey] = AnalyzePatch(patch, m_liveEndpoints);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to check a patch for loops.")
    }

    void PatchLibrary::EndpointsChanged() noexcept
    {
        try
        {
            m_liveEndpoints = EndpointCatalog::Current().Snapshot();

            for (auto const& patch : m_patches)
            {
                Analyze(*patch);
            }

            ApplyRouting();
            Notify(LibraryChange::Endpoints, {});
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to catch up with the endpoints.")
    }

    void PatchLibrary::ApplyRouting() noexcept
    {
        try
        {
            // Copies, with the links that close a certain loop muted, so the engine never sees
            // a circle the analysis already broke. Reserved, because the inputs point into it.
            std::vector<PatchDocument> working{};
            working.reserve(m_patches.size());

            std::vector<RoutePatch> inputs{};

            for (auto const& patch : m_patches)
            {
                if (!IsRouting(patch->SessionKey))
                {
                    continue;
                }

                auto& copy = working.emplace_back(*patch);
                auto const& analysis = Analysis(patch->SessionKey);

                for (auto& link : copy.Connections)
                {
                    if (analysis.LoopMutedConnectionIds.count(link.Id) != 0)
                    {
                        link.Muted = true;
                    }
                }

                // Each MIDI-CI responder answers from its file as it is now.
                for (auto& block : copy.Blocks)
                {
                    auto& responder = block.Settings.CiResponder;

                    if (block.Kind == BlockKind::CiResponder && !responder.FileName.empty())
                    {
                        responder.Description = CiFileStore::Current().Read(responder.FileName).Description;
                    }
                }

                RoutePatch input{};
                input.Key = copy.SessionKey;
                input.Patch = &copy;

                // An endpoint that is not here is left out, and so is everything that only leads
                // to it. The next pass wires it up when the device comes back.
                for (auto const& endpoint : copy.Endpoints)
                {
                    if (auto const live = ResolveEndpoint(endpoint))
                    {
                        input.DeviceIds[endpoint.Id] = live->EndpointDeviceId;
                    }
                }

                inputs.push_back(std::move(input));
            }

            auto graph = CompileRoutes(inputs);

            m_problems.clear();

            for (auto const& problem : graph.Problems)
            {
                m_problems[problem.PatchKey] = problem.Kind;
            }

            auto const generation = ++g_lastRequested;

            // Everything below blocks on the service, so it never runs on the XAML thread.
            RunOnBackgroundAsync([graph = std::move(graph), generation]() mutable
                {
                    std::scoped_lock guard{ g_applyLock };

                    if (generation < g_lastApplied)
                    {
                        return;
                    }

                    g_lastApplied = generation;
                    RouteEngine::Current().Apply(std::move(graph));
                });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to apply the routing.")
    }

    void PatchLibrary::RefreshActivity() noexcept
    {
        try
        {
            // A MIDI-CI file edited beside the patches is answered from straight away.
            if (CiFileStore::Current().CheckForChanges())
            {
                ApplyRouting();
                Notify(LibraryChange::Routing, {});
            }

            m_stats = RouteEngine::Current().Stats();

            Notify(LibraryChange::Activity, {});
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to fetch the routing counts.")
    }

    _Use_decl_annotations_
    std::unordered_map<std::wstring, RouteStats> PatchLibrary::Activity(std::wstring const& key) const noexcept
    {
        std::unordered_map<std::wstring, RouteStats> result{};

        try
        {
            auto const prefix = key + L'|';

            for (auto const& [cell, stats] : m_stats)
            {
                if (cell.size() > prefix.size() && cell.compare(0, prefix.size(), prefix) == 0)
                {
                    result.emplace(cell.substr(prefix.size()), stats);
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to read a patch's counts.")

        return result;
    }

    uint64_t PatchLibrary::TotalDelivered() const noexcept
    {
        uint64_t total{ 0 };

        try
        {
            for (auto const& patch : m_patches)
            {
                total += Delivered(patch->SessionKey);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to add up the routing counts.")

        return total;
    }

    _Use_decl_annotations_
    uint64_t PatchLibrary::Delivered(std::wstring const& key) const noexcept
    {
        uint64_t total{ 0 };

        try
        {
            auto const found = std::find_if(m_patches.begin(), m_patches.end(),
                [&key](std::unique_ptr<PatchDocument> const& p) { return p->SessionKey == key; });

            if (found == m_patches.end() || !IsRouting(key))
            {
                return 0;
            }

            auto const& patch = **found;

            for (auto const& link : patch.Connections)
            {
                if (patch.FindEndpoint(link.DestinationId) == nullptr)
                {
                    continue;
                }

                auto const stats = m_stats.find(key + L'|' + link.Id);

                if (stats != m_stats.end())
                {
                    total += stats->second.MessagesForwarded;
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to add up a patch's routing counts.")

        return total;
    }

    _Use_decl_annotations_
    std::optional<RouteProblemKind> PatchLibrary::Problem(std::wstring const& key) const noexcept
    {
        auto const found = m_problems.find(key);

        if (found == m_problems.end())
        {
            return std::nullopt;
        }

        return found->second;
    }

    _Use_decl_annotations_
    bool PatchLibrary::HasMissingEndpoint(std::wstring const& key) noexcept
    {
        try
        {
            auto const* patch = Find(key);

            if (patch == nullptr)
            {
                return false;
            }

            return std::any_of(patch->Endpoints.begin(), patch->Endpoints.end(),
                [](PatchEndpoint const& e) { return !ResolveEndpoint(e).has_value(); });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to check a patch's endpoints.")

        return false;
    }

    _Use_decl_annotations_
    uint32_t PatchLibrary::Subscribe(Listener listener) noexcept
    {
        try
        {
            auto const token = m_nextToken++;
            m_listeners.emplace_back(token, std::move(listener));
            return token;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to listen for patch changes.")

        return 0;
    }

    _Use_decl_annotations_
    void PatchLibrary::Unsubscribe(uint32_t token) noexcept
    {
        std::erase_if(m_listeners, [token](auto const& entry) { return entry.first == token; });
    }

    _Use_decl_annotations_
    void PatchLibrary::Notify(LibraryChange change, std::wstring const& key) noexcept
    {
        try
        {
            // A copy, because a listener can close a window and unsubscribe while this runs.
            auto const listeners = m_listeners;

            // A copy too: a listener can free or change the string the caller's key refers to,
            // such as the notification area menu item a patch was turned on from.
            std::wstring const changedKey{ key };

            for (auto const& [token, listener] : listeners)
            {
                auto const stillSubscribed = std::any_of(m_listeners.begin(), m_listeners.end(),
                    [token](auto const& entry) { return entry.first == token; });

                if (stillSubscribed && listener)
                {
                    listener(change, changedKey);
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to tell the windows about a change.")
    }
}
