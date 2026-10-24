// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// "Sync to device": sending a layout to another PC running Windows MIDI Glass, pairing with it
// the first time, the list of paired PCs, and the Receive screen. The network and pack logic is
// in sync\ (see sync\README.md); this is the part a person sees.

#include "AppFirewall.h"
#include "DeviceLinkIdentity.h"
#include "PairedDevices.h"

namespace midiglass::devicesync
{
    // The first argument of the elevated copy that adds the firewall rule for the Receive screen.
    constexpr wchar_t FirewallAllowSwitch[] = L"--allow-glass-sync-through-firewall";

    // TCP, private networks, and only from the local subnet.
    midiapp::firewall::FirewallTarget FirewallTarget();

    // In %LOCALAPPDATA%\Microsoft\MIDI Glass, never in Documents or ProgramData.
    std::wstring DataFile(_In_ std::wstring const& name);

    std::wstring PairedListPath();

    // This PC's device key, made the first time. Blocks while the key is opened, so never call
    // it on a UI thread. Null when Windows wouldn't make one.
    std::shared_ptr<midiapp::DeviceIdentity> Identity() noexcept;

    // Forgets a paired PC, with everything this session allowed it.
    void Unpair(_In_ midiapp::DeviceKeyHash const& key) noexcept;

    // The paired PC a layout was last sent to, for "Sync to <name>". Empty when there isn't one.
    std::wstring LastDeviceName() noexcept;

    // Chooses a PC, pairs with it when it's new, and sends the layout. With `direct`, the PC the
    // last layout went to is used without asking, when it's paired and can be found.
    winrt::Windows::Foundation::IAsyncAction SendLayoutAsync(
        _In_ winrt::Microsoft::UI::Xaml::XamlRoot root,
        _In_ std::wstring layoutFilePath,
        _In_ bool direct);

    winrt::Windows::Foundation::IAsyncAction ShowPairedDevicesAsync(
        _In_ winrt::Microsoft::UI::Xaml::XamlRoot root);

    // The Receive screen, a window of its own. This PC listens for paired PCs only while it's
    // open. `installed` runs on the UI thread after a layout from another PC is installed.
    void OpenReceiveWindow(_In_ std::function<void(std::wstring const&)> installed);
}
