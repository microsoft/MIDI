// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Windows.Devices.Midi2.Utilities.Update;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services
{

    public class MidiUpdateService : IMidiUpdateService
    {
        // https://aka.ms/MidiServicesLatestSdkVersionJson

        public IReadOnlyList<MidiRuntimeRelease> GetAvailableRuntimeDownloads()
        {
            return MidiRuntimeUpdateUtility.GetAvailableReleases();
        }

        public bool DownloadUpdate(Uri uri)
        {
            // TODO




            return false;
        }
    }
}
