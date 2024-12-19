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
using System.Diagnostics.Contracts;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.Marshalling;
using System.Text;
using System.Threading.Tasks;
using WinRT;
using WinRT.Interop;

namespace Microsoft.Windows.Devices.Midi2.Initialization
{
    public enum MidiAppSDKPlatform : UInt32
    {
        x64 = 1,
        //    Arm64 = 2,
        //    Arm64EC = 3,
        Arm64X = 4,
    };


    [GeneratedComInterface(Options = ComInterfaceOptions.ComObjectWrapper, StringMarshalling = StringMarshalling.Utf16)]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid("8087b303-d551-bce2-1ead-a2500d50c580")]
    internal partial interface IMidiClientInitializer
    {
        void Initialize();
        void Shutdown();

        void GetInstalledWindowsMidiServicesSdkVersion(
            ref MidiAppSDKPlatform buildPlatform,
            ref UInt32 versionMajor,
            ref UInt32 versionMinor,
            ref UInt32 versionRevision,
            ref UInt32 versionDateNumber,
            ref UInt32 versionTimeNumber,
            ref string buildSource,
            ref string versionName,
            ref string versionFullString
        );

        void EnsureServiceAvailable();
    }

    public class MidiDesktopAppSdkInitializer : IDisposable
    {
        public const string LatestMidiAppSdkDownloadUrl = @"https://aka.ms/MidiServicesLatestSdkRuntimeInstaller";

        private bool _disposedValue;

        private static Guid clsid = new Guid("c3263827-c3b0-bdbd-2500-ce63a3f3f2c3");
        private static Guid iid = typeof(IMidiClientInitializer).GUID;

        private IMidiClientInitializer _initializer;

        private MidiDesktopAppSdkInitializer(IMidiClientInitializer initializer)
        {
            _initializer = initializer;

        }

        public static MidiDesktopAppSdkInitializer? Create()
        {
            object obj;
            int hr = Ole32.CoCreateInstance(
                ref clsid,
                IntPtr.Zero,
                (uint)Ole32.CLSCTX.CLSCTX_INPROC_SERVER,
                ref iid,
                out obj);

            if (hr == 0 && obj != null)
            {
                var cominit = (IMidiClientInitializer)obj;

                try
                {
                    cominit.Initialize();

                    var initializer = new MidiDesktopAppSdkInitializer(cominit);

                    return initializer;
                }
                catch (Exception)
                {
                    return null;
                }
            }
            else
            {
                // failed to create the initializer
                return null;
            }
        }

        public bool EnsureServiceAvailable()
        {
            try
            {
                _initializer.EnsureServiceAvailable();

                return true;
            }
            catch (Exception)
            {
                // todo: Log
                return false;
            }
        }

        public bool InitializeSdkRuntime()
        {
            try
            {
                _initializer.Initialize();

                return true;
            }
            catch (Exception)
            {
                // todo: Log
                return false;
            }
        }

        public bool CheckForMinimumRequiredSdkVersion(
            UInt32 minRequiredVersionMajor,
            UInt32 minRequiredVersionMinor,
            UInt32 minRequiredVersionRevision)
        {
            // TODO

            MidiAppSDKPlatform platform = MidiAppSDKPlatform.x64;

            string buildSource = string.Empty;
            string versionName = string.Empty;
            string versionFullString = string.Empty;

            uint versionMajor = 0;
            uint versionMinor = 0;
            uint versionRevision = 0;
            uint versionDayNumber = 0;
            uint versionTimeNumber = 0;

            try
            {
                // for C#, all parameters must be supplied
                _initializer.GetInstalledWindowsMidiServicesSdkVersion(
                    ref platform,
                    ref versionMajor,
                    ref versionMinor,
                    ref versionRevision,
                    ref versionDayNumber,
                    ref versionTimeNumber,
                    ref buildSource,
                    ref versionName,
                    ref versionFullString
                    );

                if (minRequiredVersionMajor > versionMajor) return false;
                if (minRequiredVersionMinor > versionMinor) return false;
                if (minRequiredVersionRevision > versionRevision) return false;

                return true;
            }
            catch
            {
                return false;
            }
        }

        public string GetInstalledSdkDescription(bool includeSource, bool includeVersionName, bool includePlatform)
        {
            MidiAppSDKPlatform platform = MidiAppSDKPlatform.x64;

            string buildSource = string.Empty;
            string versionName = string.Empty;
            string versionFullString = string.Empty;

            uint versionMajor = 0;
            uint versionMinor = 0;
            uint versionRevision = 0;
            uint versionDayNumber = 0;
            uint versionTimeNumber = 0;

            try
            { 
                _initializer.GetInstalledWindowsMidiServicesSdkVersion(
                    ref platform,
                    ref versionMajor,
                    ref versionMinor,
                    ref versionRevision,
                    ref versionDayNumber,
                    ref versionTimeNumber,
                    ref buildSource,
                    ref versionName,
                    ref versionFullString
                    );

                string retval = string.Empty;

                if (includeSource)
                {
                    retval += $"{buildSource}";
                }

                if (includeVersionName)
                {
                    if (retval != string.Empty)
                    {
                        retval += " ";
                    }

                    retval += $"{versionName}";
                }

                if (retval != string.Empty)
                {
                    retval = retval.Trim() + " - ";
                }

                retval += $"{versionFullString}";

                if (includePlatform)
                {
                    retval += $" ({platform.ToString()})";
                }

                return retval;
            }
            catch
            {
                return "Not available";
            }
        }

        public void ShutdownSdkRuntime()
        {
            if (_initializer != null )
            {
                _initializer.Shutdown();
            }
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposedValue)
            {
                if (disposing)
                {
                }

                // TODO: free unmanaged resources (unmanaged objects) and override finalizer
                // TODO: dispose managed state (managed objects)
                ShutdownSdkRuntime();

                // TODO: set large fields to null
                _disposedValue = true;
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
