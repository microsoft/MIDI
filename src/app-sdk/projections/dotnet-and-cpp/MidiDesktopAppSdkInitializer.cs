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
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.Marshalling;
using System.Text;
using System.Threading.Tasks;
using WinRT;
using WinRT.Interop;

namespace Microsoft.Windows.Devices.Midi2.Initialization
{
    internal enum MidiAppSDKPlatform : UInt32
    {
        Platform_x64 = 1,
        //    Platform_Arm64 = 2,
        //    Platform_Arm64EC = 3,
        Platform_Arm64X = 4,
    };


    [GeneratedComInterface(Options = ComInterfaceOptions.ComObjectWrapper, StringMarshalling = StringMarshalling.Utf16)]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid("8087b303-d551-bce2-1ead-a2500d50c580")]
    internal partial interface IMidiClientInitializer
    {
        void Initialize();
        //void GetInstalledWindowsMidiServicesSdkVersion(
        //    ref WindowsMidiServicesAppSdkVersion sdkVersion);

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
        void Shutdown();
    }


    //[GeneratedComClass]
    //[Guid("c3263827-c3b0-bdbd-2500-ce63a3f3f2c3")]
    //internal class MidiClientInitializer : IMidiClientInitializer
    //{
    //}



    public class MidiDesktopAppSdkInitializer : IDisposable
    {
        const string LatestMidiAppSdkDownloadUrl = "https://aka.ms/MidiServicesLatestSdkRuntimeInstaller";
        private bool _disposedValue;

        //Guid clsid = typeof(MidiClientInitializer).GUID; 
        Guid clsid = new Guid("c3263827-c3b0-bdbd-2500-ce63a3f3f2c3");
        Guid iid = typeof(IMidiClientInitializer).GUID;

        private IMidiClientInitializer? _initializer = null;
        public MidiDesktopAppSdkInitializer()
        {


        }


        public bool EnsureServiceAvailable()
        {
            if (_initializer == null)
            {
                return false;
            }

            try
            {
                _initializer!.EnsureServiceAvailable();

                return true;
            }
            catch (Exception ex)
            {
                // todo: Log
                return false;
            }

            return false;
        }

        public bool InitializeSdkRuntime()
        {
            if ( _initializer != null )
            {
                // already initialized
                return true;
            }

            object obj;
            int hr = Ole32.CoCreateInstance(
                ref clsid,
                IntPtr.Zero,
                (uint)Ole32.CLSCTX.CLSCTX_INPROC_SERVER,
                ref iid,
                out obj);

            //var cw = new StrategyBasedComWrappers();
            //var pointer = cw.GetOrCreateComInterfaceForObject(_initializer, CreateComInterfaceFlags.None);

            if (hr == 0 && obj != null)
            {
                _initializer = (IMidiClientInitializer)obj;
            }

            try
            {
                _initializer!.Initialize();

                return true;
            }
            catch (Exception ex)
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



            return false;
        }

        public void ShutdownSdkRuntime()
        {
            if (_initializer != null )
            {
                _initializer.Shutdown();
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
