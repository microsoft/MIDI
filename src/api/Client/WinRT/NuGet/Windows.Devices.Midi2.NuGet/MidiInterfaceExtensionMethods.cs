using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using WinRT;

namespace Windows.Devices.Midi2
{
    public static class MidiInterfaceExtensionMethods
    {
        public static IMidiEndpointConnectionRaw GetMidiEndpointConnectionRawInterface(this Windows.Devices.Midi2.MidiEndpointConnection connection)
        {
            return connection.As<IMidiEndpointConnectionRaw>();
        }

        //public static IntPtr GetMidiEndpointConnectionRawInterfacePointer(this MidiEndpointConnection connection)
        //{
        //    return Marshal.QueryInterface(
        //        Marshal.GetIUnknownForObject(connection),
        //        typeof(IMidiEndpointConnectionRaw).GUID,
        //        out IntPtr rawInterfacePtr);
        //}

    }
}