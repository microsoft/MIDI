// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Windows.Input;

using Microsoft.UI.Xaml.Media;

namespace Microsoft.Midi.Settings.Models;

public class MidiToolAppInfo
{
    public string Name { get; init; } = string.Empty;

    public string Description { get; init; } = string.Empty;

    public ImageSource? Icon { get; init; }

    public ICommand? LaunchCommand { get; init; }
}
