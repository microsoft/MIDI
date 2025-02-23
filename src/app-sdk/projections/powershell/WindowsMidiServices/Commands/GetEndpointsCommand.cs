using System.Management.Automation;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Get, "MidiEndpointList")]
    public class GetEndpointsCommand : Cmdlet
    {
        protected override void ProcessRecord()
        {
            var sdkDevices = Microsoft.Windows.Devices.Midi2.MidiEndpointDeviceInformation.FindAll();

            List<MidiEndpointDeviceInformation> devices = [];

            foreach (var sdkDevice in sdkDevices)
            {
                devices.Add(new MidiEndpointDeviceInformation(sdkDevice));
            }

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
