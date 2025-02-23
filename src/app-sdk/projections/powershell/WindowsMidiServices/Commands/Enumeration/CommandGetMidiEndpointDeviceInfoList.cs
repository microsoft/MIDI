using System.Management.Automation;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Get, "MidiEndpointDeviceInfoList")]
    public class CommandGetMidiEndpointDeviceInfoList : Cmdlet
    {
        protected override void BeginProcessing()
        {

        }

        protected override void ProcessRecord()
        {
            var sdkDevices = Microsoft.Windows.Devices.Midi2.MidiEndpointDeviceInformation.FindAll();

            List<MidiEndpointDeviceInfo> devices = [];

            foreach (var sdkDevice in sdkDevices)
            {
                devices.Add(new MidiEndpointDeviceInfo(sdkDevice));
            }

            WriteObject(devices);
        }
    }

}
