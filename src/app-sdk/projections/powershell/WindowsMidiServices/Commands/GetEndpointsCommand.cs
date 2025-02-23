using System.Management.Automation;

using Microsoft.Windows.Devices.Midi2;
using Microsoft.Windows.Devices.Midi2.Initialization;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Get, "MidiEndpoints")]
    public class GetEndpointsCommand : Cmdlet
    {
        protected override void ProcessRecord()
        {
            var devices = MidiEndpointDeviceInformation.FindAll();

            WriteObject(devices);
        }
    }


    [Cmdlet(VerbsCommon.Get, "MidiEndpointGroups")]
    public class GetEndpointGroupsCommand : Cmdlet
    {
        protected override void ProcessRecord()
        {
            // take in the id, return all the active groups with their GTB or function block names


        }
    }

}
