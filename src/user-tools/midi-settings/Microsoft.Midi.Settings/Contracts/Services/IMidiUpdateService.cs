// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Windows.Devices.Midi2.Utilities.Update;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Contracts.Services
{
    public interface IMidiUpdateService
    {
        MidiRuntimeUpdateReleaseTypes GetCurrentInstalledChannel();

        MidiRuntimeUpdateReleaseTypes GetCurrentPreferredChannel();

        IReadOnlyList<MidiRuntimeRelease> GetAllAvailableRuntimeDownloads();

        MidiRuntimeRelease? GetHighestAvailableRuntimeRelease(MidiRuntimeUpdateReleaseTypes releaseChannelType);

        Uri GetMidiSdkInstallerPageUri();

        Task<bool> DownloadAndInstallUpdate(Uri uri);
    }

}
