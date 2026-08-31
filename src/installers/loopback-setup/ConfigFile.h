// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiloopbacksetup
{
    // The two transports this tool configures. They use the same file layout, differing only
    // in the transport section they live under and in where the muted flag sits.
    enum class LoopbackKind
    {
        Loopback,       // LOOP - a pair of endpoints
        BasicLoopback   // BLOOP - a single endpoint
    };

    // Identifiers of the well known default loopbacks. Apps look for these, and the shipped
    // configuration file creates them, so the tool offers to put one back only when it is
    // missing. Both sides of the pair deliberately share one identifier; the service gives the
    // A and B endpoints different instance id prefixes.
    constexpr wchar_t DefaultLoopbackUniqueId[] = L"DEFAULT";
    constexpr wchar_t DefaultBasicLoopbackUniqueId[] = L"BASIC_DEF";

    // Reads and writes the Windows MIDI Services configuration file. The service applies a
    // change immediately when it is asked to; the file is what makes it survive a restart, so
    // everything here runs only after the matching service call has succeeded.
    //
    // Nothing here throws. A failure leaves the file untouched and is reported through
    // LastErrorMessage so the page can tell the customer rather than fail silently.
    class LoopbackConfigFile
    {
    public:
        static LoopbackConfigFile& Current() noexcept;

        // Used by the --configfile command line switch so a copy of the real file can be
        // worked on while developing. Empty means the machine's configured file.
        void OverridePath(_In_ std::wstring const& path) noexcept;

        std::wstring const& Path() const noexcept { return m_path; }
        bool IsOverridden() const noexcept { return m_isOverridden; }
        bool Exists() const noexcept;

        winrt::hstring LastErrorMessage() const noexcept { return m_lastError; }

        // The json comes straight from the SDK creation config's ConfigJson, which is already
        // wrapped from the root of the file.
        bool MergeSection(_In_ winrt::Windows::Data::Json::JsonObject const& wrappedSection) noexcept;

        bool RemoveEntry(_In_ LoopbackKind const kind, _In_ winrt::hstring const& associationKey) noexcept;

        bool SetMuted(
            _In_ LoopbackKind const kind,
            _In_ winrt::hstring const& associationKey,
            _In_ bool const isMuted) noexcept;

        // A loopback is entirely user-owned, so an edit overwrites the entry it was created
        // from rather than layering a separate customization on top of it. For a pair, an empty
        // nameB leaves the B side alone.
        bool UpdateEntryDetails(
            _In_ LoopbackKind const kind,
            _In_ winrt::hstring const& associationKey,
            _In_ winrt::hstring const& nameA,
            _In_ winrt::hstring const& descriptionA,
            _In_ winrt::hstring const& nameB,
            _In_ winrt::hstring const& descriptionB,
            _In_ winrt::hstring const& imageFileName) noexcept;

        // Writes the position of every entry the file knows about. The service and the SDK
        // both ignore this key; it exists only so the customer's arrangement of the list
        // survives a restart of the tool.
        bool SetDisplayOrder(
            _In_ LoopbackKind const kind,
            _In_ std::vector<winrt::hstring> const& orderedAssociationKeys) noexcept;

        // Association identifiers the file has an entry for, lowercased and unbraced so they
        // can be compared with what the service reports.
        std::vector<std::wstring> GetEntryIds(_In_ LoopbackKind const kind) noexcept;

        // Stored positions, keyed the same way as GetEntryIds. An entry with no stored
        // position is absent from the map rather than defaulting to zero.
        std::unordered_map<std::wstring, int32_t> GetDisplayOrders(_In_ LoopbackKind const kind) noexcept;

    private:
        LoopbackConfigFile() noexcept;

        bool Load(_Out_ winrt::Windows::Data::Json::JsonObject& config) noexcept;

        // Same as Load, but hands back a shared parse which is only redone when the file's write
        // time or size has changed. The pages poll several times a minute, so re-parsing per
        // poll is pure waste. The object returned is shared: callers which intend to modify and
        // save must use Load instead.
        bool LoadCached(_Out_ winrt::Windows::Data::Json::JsonObject& config) noexcept;

        // Hands one transport section to the SDK, which re-reads, merges and writes the file
        // under its own lock. This tool no longer writes the file itself.
        bool SaveSection(
            _In_ LoopbackKind const kind,
            _In_ winrt::Windows::Data::Json::JsonObject const& transportSection) noexcept;

        // the create object for the transport, created on demand when creating is true
        winrt::Windows::Data::Json::JsonObject GetCreateObject(
            _In_ winrt::Windows::Data::Json::JsonObject const& config,
            _In_ LoopbackKind const kind,
            _In_ bool const create) noexcept;

        void ResolveDefaultPath() noexcept;

        std::wstring m_path{};
        bool m_isOverridden{ false };
        winrt::hstring m_lastError{};

        winrt::Windows::Data::Json::JsonObject m_cachedConfig{ nullptr };
        FILETIME m_cachedWriteTime{};
        uint64_t m_cachedSize{ 0 };
    };
}
