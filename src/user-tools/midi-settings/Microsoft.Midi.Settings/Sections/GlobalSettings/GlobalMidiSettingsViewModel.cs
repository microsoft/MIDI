// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.UI.Dispatching;
using Microsoft.Midi.Settings;
using Microsoft.Midi.Settings.Models;
using Microsoft.Midi.Settings.Controls;
using System.Windows.Input;
using CommunityToolkit.Mvvm.Input;
using Windows.Storage.Pickers;
using Windows.Storage;
using Microsoft.Windows.Devices.Midi2.Utilities.SysExTransfer;
using Microsoft.Extensions.Logging.Abstractions;
using Windows.Foundation;
using Microsoft.Midi.Settings.Services;
using Microsoft.Midi.Settings.Contracts.Services;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class GlobalMidiSettingsViewModel : ObservableRecipient
    {
    }
}
