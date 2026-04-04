// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.UI.Xaml.Controls;

namespace Microsoft.Midi.Settings.Contracts.Services;

public interface IMessageBoxService
{
    void ShowInfo(string message, string title);
    void ShowInfo(string message);

    void ShowError(string message, string title);
    void ShowError(string message);

    // returns true if the user clicks "OK"
    bool ShowMessageWithOkCancel(string message, string title);

    // returns true if the user clicks "OK"
    bool ShowMessageWithOkCancel(string message);
}
