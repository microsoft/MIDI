// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// How another Windows MIDI Services tool asks the MIDI Settings app to open one of its dialogs,
// shared by MIDI Settings, which answers, and the tools which ask.

// %ProgramFiles%\Windows MIDI Services\Tools\<folder>\<exe>, where earlier previews installed it.
// The installers now put every tool in Tools itself, which the sibling lookup finds.
#define MIDI_SETTINGS_EXECUTABLE_NAME       L"midisettings.exe"
#define MIDI_SETTINGS_INSTALLED_FOLDER      L"Settings"

// The single instance key MIDI Settings takes. Used to find the copy which is already running.
#define MIDI_SETTINGS_INSTANCE_KEY          L"Settings"

// Opens the notifications dialog once the window is up.
#define MIDI_SETTINGS_SWITCH_NOTIFICATIONS  L"notifications"

// Asks a copy which is already running to do the same thing, so the customer is not made to
// answer an elevation prompt only to be handed the window they already had. Registered rather
// than a fixed WM_APP value, because the message reaches a window this app does not own the
// class of and a fixed value could collide with something else's private message.
//
// Posting this carries no data and asks only to navigate. Everything the dialog can change is
// still behind a deliberate click, so a message from an unknown sender costs nothing.
#define MIDI_SETTINGS_SHOW_NOTIFICATIONS_MESSAGE_NAME \
    L"Microsoft.WindowsMidiServices.Settings.ShowNotifications"

// Opens the Configurations window. A page name may follow: configurations, backups, share or
// import. Reopen as administrator uses it, so the elevated copy lands where the customer was.
#define MIDI_SETTINGS_SWITCH_CONFIGURATIONS L"configurations"

// Opens a package on the Configurations window's Import page. The path follows. The installer
// registers this for .midiconfigpack, so double-clicking a package lands here.
#define MIDI_SETTINGS_SWITCH_IMPORT         L"import"

// Asks a copy which is already running to open the Configurations window, with the page in
// WPARAM. Like the notifications message it carries no data and only navigates. A package path
// is handed over with WM_COPYDATA instead, the way MIDI Patchbay and MIDI Glass do it.
#define MIDI_SETTINGS_SHOW_CONFIGURATIONS_MESSAGE_NAME \
    L"Microsoft.WindowsMidiServices.Settings.ShowConfigurations"

// The Configurations window's pages, by the names the switch takes, in the order the message's
// WPARAM numbers them.
inline constexpr wchar_t const* MidiSettingsConfigurationsPages[]{ L"configurations", L"backups", L"share", L"import" };
