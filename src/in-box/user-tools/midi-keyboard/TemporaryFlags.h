// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// ============================================================================================
// TEMPORARY - REVERT BEFORE THE INTERNAL WINDOWS REPO COMMIT
//
// The shipping service crashes when a virtual device is closed or disconnected, so the virtual
// device connection type is kept out of customers' hands until that fix ships. Everything that
// implements it is still here and still compiled; only the choice is withheld.
//
// To re-enable: set this to false, then grep for TemporarilyDisableVirtualDevice and delete
// each use along with this file. The uses are:
//   AppSettings.cpp        - coerces a saved VirtualDevice preference to ExistingEndpoint
//   MainWindow.xaml.cpp    - disables the radio button and shows the explanation
//   MainWindow.xaml        - the VirtualDeviceUnavailableNote TextBlock
//   Resources.resw         - VirtualDeviceUnavailableNote.Text
//
// The coercion is deliberately in memory only. A customer who had chosen the virtual device
// keeps that choice in the registry and gets it back the moment this is turned off.
// ============================================================================================

namespace midikeyboard
{
    inline constexpr bool TemporarilyDisableVirtualDevice = true;
}
