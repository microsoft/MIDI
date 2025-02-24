using System.Management.Automation;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Get, "MidiEndpointGroups")]
    public class CommandGetMidiEndpointGroups : Cmdlet
    {
        protected override void ProcessRecord()
        {
            // take in the id, return all the active groups with their GTB or function block names


        }
    }

}
