using System.Collections.ObjectModel;
using Microsoft.Midi.Settings.SdkMock.TransportClientSdk;

namespace Microsoft.Midi.Settings.SdkMock;
public class MidiSession
{
    // this will be DeviceInformation later, and will come from enumeration
    public ObservableCollection<MidiDevice> Devices
    {
        get; private set; 
    }

    public MidiSession()
    {
        Devices = new ObservableCollection<MidiDevice>();
    }

    public async Task<MidiDevice> CreateNewUmpDeviceAsync(string deviceTransportProviderId, string jsonParameters)
    {
        // Temp: hard-code to return network MIDI device for now

        var factory = new Windows.Devices.Midi.NetworkMidiTransportPlugin.NetworkMidiHostUmpEndpointFactory();

        var endpoint = await factory.CreateNewUmpEndpoint(jsonParameters);

        var device = new MidiDevice();
        device.UmpEndpoint = endpoint;

        // this will come from the device
        device.Id = endpoint.Id;
        device.Name = endpoint.Properties[NetworkMidiServerClientPlugin.PKEY_MidiEndpointName].ToString();
        device.Address = endpoint.Properties[NetworkMidiServerClientPlugin.PKEY_Server_HostName].ToString() + " : " + endpoint.Properties[NetworkMidiServerClientPlugin.PKEY_Server_Port].ToString();
        device.TransportType = "Network MIDI 2.0 (Host)";

        Devices.Add(device);

        return device;
    }
}
