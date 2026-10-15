// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// UNSUPPORTED - DO NOT COPY THIS APPROACH. This code reads and writes the Windows MIDI Services
// configuration file directly. Microsoft does not support direct manipulation of that file by
// anything other than the in-box MIDI tools. Its format, its name, its location and the registry
// value that selects it are implementation details and can change in any release without notice,
// so this code can corrupt a configuration or stop working at any time. It exists for development
// and diagnostics inside the Windows MIDI Services repository only. Applications and third-party
// tools must use the Windows MIDI Services API (Windows.Devices.Midi2.ServiceConfig) instead.
// ============================================================================

#pragma once

// What the Configurations window needs from this PC and from the app, kept apart from the pages:
// the configuration engine's view of the real system, whether the MIDI service still has to restart,
// and the words for dates, counts and failures.

#include "ConfigEngine.h"
#include "ConfigBackups.h"
#include "ConfigFiles.h"
#include "ConfigInventory.h"

namespace midisettings::configurations
{
    namespace engine = ::midisettings::configengine;

    // The configurations folder, the backups folder beside it, the registry values that pick the
    // configuration and the port names, and the shared pictures folder.
    engine::ConfigEnvironment MakeEnvironment() noexcept;

    // The MIDI service's process, or 0 when it isn't running. A different one than before means
    // the service has restarted since.
    uint32_t ServiceProcessId() noexcept;

    // RTP-MIDI is installed here, asked of the service. True when the service can't say, so a
    // package isn't described as waiting for something that may already be here.
    bool IsRtpMidiInstalled() noexcept;

    // An endpoint that's here right now, for saying whether a device in a package is plugged in.
    struct LiveEndpoint
    {
        std::wstring EndpointDeviceId{};
        std::wstring DeviceInstanceId{};
        std::wstring TransportSuppliedName{};
    };

    // Asked of the service, so not on the UI thread. Empty when it can't say.
    std::vector<LiveEndpoint> LiveEndpoints() noexcept;

    // What waits for the MIDI service to restart.
    namespace restartkinds
    {
        inline constexpr uint32_t Switch{ 0x1 };
        inline constexpr uint32_t Changes{ 0x2 };
        inline constexpr uint32_t PortNames{ 0x4 };
    }

    struct PendingRestart
    {
        uint32_t Kinds{ 0 };

        // for a switch: the configuration the service is still using
        std::wstring RunningFileName{};
    };

    // What's waiting, or nothing when the service has restarted since it was recorded. UI thread.
    PendingRestart ReadPendingRestart(_In_ uint32_t serviceProcessId) noexcept;

    // Adds to what's waiting. The first switch keeps the configuration the service still runs.
    void AddPendingRestart(
        _In_ uint32_t serviceProcessId,
        _In_ uint32_t kinds,
        _In_ std::wstring const& runningFileName) noexcept;

    void WritePendingRestart(_In_ uint32_t serviceProcessId, _In_ PendingRestart const& pending) noexcept;
    void ClearPendingRestart() noexcept;

    // Configurations imported as new ones, so the list can say where each came from.
    void RememberImported(_In_ std::wstring const& fileName, _In_ std::wstring const& computerName) noexcept;
    bool TryGetImported(
        _In_ std::wstring const& fileName,
        _Out_ std::wstring& computerName,
        _Out_ FILETIME& importedAt) noexcept;

    // ---------------------------------------------------------------------------------- words

    // Why something didn't happen, from the resources, with the system's reason when there is one.
    winrt::hstring ErrorText(_In_ engine::ConfigResult const& result) noexcept;

    // "1 device name" or "16 device names", from a key pair named <key>One and <key>Format.
    winrt::hstring CountText(_In_ std::wstring_view key, _In_ size_t count) noexcept;

    // Joined the way a sentence lists them: "a", "a and b", "a, b and c".
    winrt::hstring ListText(_In_ std::vector<winrt::hstring> const& parts) noexcept;

    // The local day of a time, relative to now: 0 today, 1 yesterday, and so on.
    int64_t DaysAgo(_In_ FILETIME const& time) noexcept;

    // "3:20 PM"
    winrt::hstring TimeText(_In_ FILETIME const& time) noexcept;

    // A date in the form of a picture in the resources, such as "MMM d", with the year added
    // from the WithYear picture when it isn't this year.
    winrt::hstring DateText(_In_ FILETIME const& time, _In_ std::wstring_view pictureKey) noexcept;

    // "Today at 3:20 PM", "Yesterday at 7:41 PM", "Oct 12 at 4:05 PM"; capitalized or not.
    winrt::hstring WhenText(_In_ FILETIME const& time, _In_ bool capitalized) noexcept;

    // The counts a configuration shows in a list row: "8 device names, 2 loopbacks, 1 network
    // host", at most three of them, or "Nothing set up yet".
    winrt::hstring ShortContentsText(_In_ engine::ConfigInventory const& inventory) noexcept;

    // "Compatible" or "Modern", or a dash when there's no synthesizer mode.
    winrt::hstring SynthModeText(_In_ std::wstring const& synthMode) noexcept;

    // The display name of the configuration a file holds.
    std::wstring ConfigDisplayName(_In_ engine::ConfigFileEntry const& entry);

    // A name that's usable as a file name, for the save dialog.
    std::wstring SafeFileName(_In_ std::wstring const& name);

    // The folder part of a path, without the trailing separator.
    std::wstring ParentFolder(_In_ std::wstring const& path);

    // The Win32 save and open dialogs. Empty when the customer canceled.
    std::wstring ShowSavePackageDialog(
        _In_ HWND owner,
        _In_ std::wstring const& suggestedFileName,
        _In_ std::wstring const& startFolder) noexcept;

    std::wstring ShowOpenPackageDialog(_In_ HWND owner, _In_ std::wstring const& startFolder) noexcept;

    // Opens the folder in File Explorer with the file selected.
    void ShowInFolder(_In_ std::wstring const& path) noexcept;
}
