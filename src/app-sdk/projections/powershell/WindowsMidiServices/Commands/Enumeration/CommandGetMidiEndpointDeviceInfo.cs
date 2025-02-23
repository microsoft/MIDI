using System.Management.Automation;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.Get, "MidiEndpointDeviceInfo")]
    public class CommandGetMidiEndpointDeviceInfo : Cmdlet
    {
        [Parameter(Mandatory = true, Position = 0)]
        public string EndpointDeviceId
        {
            get; set;
        }

        protected override void ProcessRecord()
        {
            var sdkDevice = Microsoft.Windows.Devices.Midi2.MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(EndpointDeviceId);

            var device = new MidiEndpointDeviceInfo(sdkDevice);

            WriteObject(device);
        }
    }

}
