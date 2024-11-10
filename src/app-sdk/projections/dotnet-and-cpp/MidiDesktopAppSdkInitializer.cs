// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK. This file may be compiled
// into your applications to bootstrap the service and SDK without the need
// for additional binaries shipped local to the app.
// Further information: https://aka.ms/midi
// ============================================================================


using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Devices.Midi2.Initialization
{
    public class MidiDesktopAppSdkInitializer : IDisposable
    {
        const string LatestMidiAppSdkDownloadUrl = "https://aka.ms/MidiServicesLatestSdkRuntimeInstaller";
        private bool disposedValue;

        public bool EnsureServiceAvailable()
        {
            return false;
        }

        public bool InitializeSdkRuntime()
        {
            return false;
        }

        public bool CheckForMinimumRequiredSdkVersion(
            UInt32 minRequiredVersionMajor,
            UInt32 minRequiredVersionMinor,
            UInt32 minRequiredVersionRevision)
        {
            return false;
        }

        public void ShutdownSdkRuntime()
        {

        }

        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                }

                // TODO: free unmanaged resources (unmanaged objects) and override finalizer
                // TODO: dispose managed state (managed objects)
                ShutdownSdkRuntime();

                // TODO: set large fields to null
                disposedValue = true;
            }
        }

        // TODO: override finalizer only if 'Dispose(bool disposing)' has code to free unmanaged resources
        ~MidiDesktopAppSdkInitializer()
        {
            // Do not change this code. Put cleanup code in 'Dispose(bool disposing)' method
            Dispose(disposing: false);
        }

        public void Dispose()
        {
            // Do not change this code. Put cleanup code in 'Dispose(bool disposing)' method
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }
    }
}
