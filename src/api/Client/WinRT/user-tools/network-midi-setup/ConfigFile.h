// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midinetworksetup
{
    // A remote client this PC has already decided about, read back from a host's allow or
    // deny list in the configuration file.
    struct KnownClientEntry
    {
        winrt::hstring UmpEndpointName{};
        winrt::hstring ProductInstanceId{};
        bool Allowed{ true };
    };

    // Reads and writes the Windows MIDI Services configuration file. The service applies a
    // change immediately when it is asked to; the file is what makes it survive a restart, so
    // everything here runs only after the matching service call has succeeded.
    //
    // Nothing here throws. A failure leaves the file untouched and is reported through
    // LastErrorMessage so the page can tell the customer rather than fail silently.
    class NetworkConfigFile
    {
    public:
        static NetworkConfigFile& Current() noexcept;

        // Used by the --configfile command line switch so a copy of the real file can be
        // worked on while developing. Empty means the machine's configured file.
        void OverridePath(_In_ std::wstring const& path) noexcept;

        std::wstring const& Path() const noexcept { return m_path; }
        bool IsOverridden() const noexcept { return m_isOverridden; }
        bool Exists() const noexcept;

        winrt::hstring LastErrorMessage() const noexcept { return m_lastError; }

        // creationConfig / connectConfig json comes straight from the SDK config object's
        // ConfigJson, which is already wrapped from the root of the file
        bool MergeSection(_In_ winrt::Windows::Data::Json::JsonObject const& wrappedSection) noexcept;

        bool RemoveHost(_In_ winrt::hstring const& hostIdKey) noexcept;
        bool RemoveClient(_In_ winrt::hstring const& clientIdKey) noexcept;

        // Moves the client between the host's allowedClients and deniedClients arrays. The
        // two lists are mutually exclusive, so the opposite list is always cleaned up.
        bool SetRemoteClientDecision(
            _In_ winrt::hstring const& hostIdKey,
            _In_ winrt::hstring const& umpEndpointName,
            _In_ winrt::hstring const& productInstanceId,
            _In_ bool const allowed) noexcept;

        bool ForgetRemoteClient(
            _In_ winrt::hstring const& hostIdKey,
            _In_ winrt::hstring const& umpEndpointName,
            _In_ winrt::hstring const& productInstanceId) noexcept;

        std::vector<KnownClientEntry> GetKnownClients(_In_ winrt::hstring const& hostIdKey) noexcept;

        // The names given to configured client entries, keyed by entry identifier. The service's
        // client enumeration does not carry a name, so a direct entry which is not answering
        // would otherwise have nothing to show but its address.
        std::unordered_map<std::wstring, winrt::hstring> GetClientDisplayNames() noexcept;

        // Lowercased entry identifiers of every configured client, so the page can tell an
        // entry it created from one the service is only still holding in memory.
        std::vector<std::wstring> GetClientEntryIds() noexcept;

        // true when the file has a host or client entry with this identifier
        bool HasHostEntry(_In_ winrt::hstring const& hostIdKey) noexcept;

        // The product instance id a saved client entry is matching on. When a device changes its
        // identity, usually in a firmware update, nothing on the network matches this any more
        // and the saved entry is the only place the old value survives.
        winrt::hstring GetClientMatchProductInstanceId(_In_ winrt::hstring const& clientIdKey) noexcept;

    private:
        NetworkConfigFile() noexcept;

        bool Load(_Out_ winrt::Windows::Data::Json::JsonObject& config) noexcept;

        // Same as Load, but hands back a shared parse which is only redone when the file's write
        // time or size has changed. The pages poll several times a minute and the file is 25 KB,
        // so re-parsing it per poll is pure waste. The object returned is shared: callers which
        // intend to modify and save must use Load instead.
        bool LoadCached(_Out_ winrt::Windows::Data::Json::JsonObject& config) noexcept;

        // Hands one transport section to the SDK, which re-reads, merges and writes the file
        // under its own lock. This tool no longer writes the file itself.
        bool SaveSection(_In_ winrt::Windows::Data::Json::JsonObject const& transportSection) noexcept;

        bool RemoveEntry(
            _In_ std::wstring_view const entriesKey,
            _In_ winrt::hstring const& entryIdKey) noexcept;

        // hosts or clients object inside create, created on demand when creating is true
        winrt::Windows::Data::Json::JsonObject GetEntriesObject(
            _In_ winrt::Windows::Data::Json::JsonObject const& config,
            _In_ std::wstring_view const entriesKey,
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
