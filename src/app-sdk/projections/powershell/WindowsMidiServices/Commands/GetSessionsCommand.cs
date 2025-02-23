using System.Management.Automation;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Get, "MidiSessionList")]
    public class GetSessionsCommand : Cmdlet
    {
        protected override void ProcessRecord()
        {
            var sdkSessions = Microsoft.Windows.Devices.Midi2.Reporting.MidiReporting.GetActiveSessions();

            //List<MidiEndpointDeviceInformation> devices = [];

            //foreach (var sdkDevice in sdkDevices)
            //{
            //    devices.Add(new MidiEndpointDeviceInformation(sdkDevice));
            //}

            WriteObject(sdkSessions);
        }
    }


}