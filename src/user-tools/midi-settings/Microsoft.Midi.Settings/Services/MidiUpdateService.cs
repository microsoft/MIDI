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

        public IReadOnlyList<MidiRuntimeRelease> GetAllAvailableRuntimeDownloads()
        {
            return MidiRuntimeUpdateUtility.GetAllAvailableReleases();
        }

        public MidiRuntimeRelease? GetHighestAvailableRuntimeRelease()
        {
            // TODO: Check the registry to see if we're on preview or stable release channel

            return MidiRuntimeUpdateUtility.GetHighestAvailableRelease(MidiRuntimeUpdateReleaseTypes.Stable);
        }


        // this is the general installer page, not the global page. Normally, this is not used from
        // within the settings app, because it's there to install the runtime, including the settings app.
        public Uri GetMidiSdkInstallerPageUri()
        {
            return new Uri(MidiDesktopAppSdkInitializer.LatestMidiAppSdkDownloadUrl);
        }


        public bool DownloadAndInstallUpdate(Uri uri)
        {
            // TODO

            // validate internet access
            // download the update
            // launch the installer
            // close this app



            return false;
        }
    }
}
