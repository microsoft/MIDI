using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Windows.Devices.Midi2;
using Windows.Devices.Enumeration;

namespace Microsoft.Devices.Midi2.Tools.Shared.Endpoints
{
    public class EndpointUtility
    {
        const string STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_INPUT = "{ae174174-6396-4dee-ac9e-1e9c6f403230}";
        const string STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_OUTPUT = "{3705dc2b-17a7-4452-98ce-bf12c6f48a0b}";
        const string STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI = "{e7cce071-3c03-423f-88d3-f1045d02552b}";

        const string STRING_DEVINTERFACE_BYTESTREAM_INPUT = "{504be32c-ccf6-4d2c-b73f-6f8b3747e22b}";
        const string STRING_DEVINTERFACE_BYTESTREAM_OUTPUT = "{6dc23320-ab33-4ce4-80d4-bbb3ebbf2814}";


        public static EndpointDirection GetUmpEndpointTypeFromInstanceId(string instanceId)
        {
            var id = instanceId.ToLower().Trim();

            if (id.EndsWith(STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_INPUT))
                return EndpointDirection.In;

            else if (id.EndsWith(STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_OUTPUT))
                return EndpointDirection.Out;

            else if (id.EndsWith(STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI))
                return EndpointDirection.Bidirectional;

            else
                throw new ArgumentException("Invalid instanceId. Does not contain a UMP device interface GUID");
        }

        public static LegacyEndpointDirection GetLegacyEndpointTypeFromInstanceId(string instanceId)
        {
            var id = instanceId.ToLower().Trim();

            if (id.EndsWith(STRING_DEVINTERFACE_BYTESTREAM_INPUT))
                return LegacyEndpointDirection.In;

            else if (id.EndsWith(STRING_DEVINTERFACE_BYTESTREAM_OUTPUT))
                return LegacyEndpointDirection.Out;

            else
                throw new ArgumentException("Invalid instanceId. Does not contain the byte stream (MIDI 1.0) device interface GUID");
        }









    }
}
