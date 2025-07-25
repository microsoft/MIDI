﻿// Copyright (c) Microsoft Corporation.
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
using Windows.Foundation.Metadata;
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
    [System.Runtime.InteropServices.Guid("8087b303-d551-bce2-1ead-a2500d50c580")]
    internal partial interface IMidiClientInitializer
    {
        void GetInstalledWindowsMidiServicesSdkVersion(
            ref MidiAppSDKPlatform buildPlatform,
            ref UInt16 versionMajor,
            ref UInt16 versionMinor,
            ref UInt16 versionPatch,

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

        private IMidiClientInitializer? _initializer;

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
                  //  cominit.Initialize();

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
            if (_initializer == null) return false;

            try
            {
                _initializer!.EnsureServiceAvailable();

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
            if (_initializer == null) return false;

            try
            {
                // try to make an SDK call. If we can't resolve the types
                // correctly, it'll throw.

                // build platform is an output param. We just default it here
                MidiAppSDKPlatform platform = MidiAppSDKPlatform.x64;

                UInt16 versionMajor = 0;
                UInt16 versionMinor = 0;
                UInt16 versionPatch = 0;
                string buildSource = string.Empty;
                string versionName = string.Empty;
                string versionFullString = string.Empty;

                _initializer.GetInstalledWindowsMidiServicesSdkVersion(
                    ref platform,
                    ref versionMajor,
                    ref versionMinor,
                    ref versionPatch,
                    ref buildSource,
                    ref versionName,
                    ref versionFullString
                    );

                if (versionFullString != string.Empty)
                {
                    return true;
                }
            }
            catch (Exception)
            {
            }

            return false;
        }

        public bool CheckForMinimumRequiredSdkVersion(
            UInt16 minRequiredVersionMajor,
            UInt16 minRequiredVersionMinor,
            UInt16 minRequiredVersionPatch)
        {
            // build platform is an output param. We just default it here
            MidiAppSDKPlatform platform = MidiAppSDKPlatform.x64;

            string buildSource = string.Empty;
            string versionName = string.Empty;
            string versionFullString = string.Empty;

            ushort versionMajor = 0;
            ushort versionMinor = 0;
            ushort versionPatch = 0;

            try
            {
                // for C#, all parameters must be supplied
                _initializer!.GetInstalledWindowsMidiServicesSdkVersion(
                    ref platform,
                    ref versionMajor,
                    ref versionMinor,
                    ref versionPatch,
                    ref buildSource,
                    ref versionName,
                    ref versionFullString
                    );

                if (minRequiredVersionMajor > versionMajor) return false;
                if (minRequiredVersionMinor > versionMinor) return false;
                if (minRequiredVersionPatch > versionPatch) return false;

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

            ushort versionMajor = 0;
            ushort versionMinor = 0;
            ushort versionPatch = 0;

            try
            { 
                _initializer!.GetInstalledWindowsMidiServicesSdkVersion(
                    ref platform,
                    ref versionMajor,
                    ref versionMinor,
                    ref versionPatch,
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
                //  _initializer.Shutdown();
                _initializer = null;
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
