// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Windows.Devices.Midi2.Utilities.RuntimeInformation;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services
{
    public class MidiSdkService : IMidiSdkService
    {
        private MidiDesktopAppSdkInitializer? _initializer;
        private bool _serviceInitialized = false;
        private bool _runtimeInitialized = false;

        public bool IsServiceInitialized
        {
            get
            {
                return _serviceInitialized;
            }
        }

        public bool IsRuntimeInitialized
        {
            get
            {
                return _runtimeInitialized;
            }
        }

        public MidiRuntimeVersion? InstalledVersion
        {
            get
            {
                if (IsRuntimeInitialized)
                {
                    return MidiRuntimeInformation.GetInstalledVersion();
                }
                else
                {
                    return null;
                }
            }
        }

        public MidiRuntimeReleaseTypes InstalledReleaseType
        {
            get
            {
                if (IsRuntimeInitialized)
                {
                    return MidiRuntimeInformation.GetInstalledReleaseType();
                }
                else
                {
                    // TODO: Return the new "Unknown" value
                    return MidiRuntimeReleaseTypes.Stable;
                }
            }
        }

        public MidiRuntimeArchitecture InstalledArchitecture
        {
            get
            {
                if (IsRuntimeInitialized)
                {
                    return MidiRuntimeInformation.GetInstalledArchitecture();
                }
                else
                {
                    return MidiRuntimeArchitecture.Unknown;
                }
            }
        }

        public string InstalledRuntimeDetailedVersionString
        {
            get
            {
                if (IsServiceInitialized)
                {
                    var version = MidiRuntimeInformation.GetInstalledVersion();

                    return version.ToString() + " (" + MidiRuntimeInformation.GetInstalledArchitecture().ToString() + ")";

                }
                else
                {
                    // TODO: Localize
                    return "SDK not initialized";
                }
            }
        }


        public bool InitializeSdk()
        {
            try
            {
                _runtimeInitialized = false;

                _initializer = Microsoft.Windows.Devices.Midi2.Initialization.MidiDesktopAppSdkInitializer.Create();

                if (_initializer == null)
                {
                    return false;
                }

                if (!_initializer!.InitializeSdkRuntime())
                {
                    return false;
                }

                _runtimeInitialized = true;
                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        public bool InitializeService()
        {
            try
            {
                if (_initializer == null)
                {
                    // TODO: Failed
                    return false;

                }

                if (!_initializer!.EnsureServiceAvailable())
                {
                    return false;
                }

                _serviceInitialized = true;

    //            StartDeviceWatcher(true);

                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }




    }
}
