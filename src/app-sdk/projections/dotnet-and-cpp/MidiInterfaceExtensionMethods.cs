using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using WinRT;

namespace Microsoft.Windows.Devices.Midi2.NetProjection
{
    public static class MidiInterfaceExtensionMethods
    {
        public static IMidiEndpointConnectionRaw GetMidiEndpointConnectionRawInterface(this MidiEndpointConnection connection)
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
