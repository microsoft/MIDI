using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Networking.Connectivity;
using Windows.Networking;

namespace Microsoft.Midi.Settings.SdkMock.TransportClientSdk;
public class NetworkMidiServerClientPlugin
{
    private Windows.Data.Json.JsonObject _jsonData;

    // TEMP: these are also defined in the server component, and need to stay in sync
    public const string PKEY_Id = "Id";
    public const string PKEY_ConnectionType = "ConnectionType";
    public const string PKEY_MidiEndpointName = "MidiEndpointName";
    public const string PKEY_MidiProductInstanceId = "MidiProductInstanceId";
    public const string PKEY_Server_Advertise = "Server_Advertise";
    public const string PKEY_Server_ServiceInstanceName = "Server_ServiceInstanceName";
    public const string PKEY_Server_Port = "Server_Port";
    public const string PKEY_Server_HostName = "Server_HostName";

    public object GetConfigurationUI()
    {
        return null;
    }

    public string GetJsonConfiguration()
    {
        // hard coded for the moment

        var selectedHostName = NetworkInformation
            .GetHostNames()
             .Where(x => x.Type == HostNameType.DomainName && x.RawName.Contains(".local"))
            .Select(x => new KeyValuePair<string, HostName>(x.ToString(), x))
            .FirstOrDefault();

        var midiEndpointName = "Test Windows Host";
        var midiProductInstanceId = "3263827";

        var serviceType = "_midi2._udp.local";
        var serviceInstanceName = "windowsprototype"; // + DateTime.Now.Ticks;

        var fullServiceName = serviceInstanceName + "." + serviceType;
        ushort port = 13337;

        Windows.Data.Json.JsonObject obj = new Windows.Data.Json.JsonObject();

        obj[PKEY_ConnectionType] = Windows.Data.Json.JsonValue.CreateStringValue("HostServer");
        obj[PKEY_MidiEndpointName] = Windows.Data.Json.JsonValue.CreateStringValue(midiEndpointName);
        obj[PKEY_MidiProductInstanceId] = Windows.Data.Json.JsonValue.CreateStringValue(midiProductInstanceId);
        obj[PKEY_Server_Advertise] = Windows.Data.Json.JsonValue.CreateBooleanValue(true);
        obj[PKEY_Server_HostName] = Windows.Data.Json.JsonValue.CreateStringValue(selectedHostName.Value.RawName);
        obj[PKEY_Server_ServiceInstanceName] = Windows.Data.Json.JsonValue.CreateStringValue(serviceInstanceName);
        obj[PKEY_Server_Port] = Windows.Data.Json.JsonValue.CreateStringValue(port.ToString()); // Windows.Data.Json.JsonValue.CreateNullValue();

        string jsonParameters = obj.Stringify();

        return jsonParameters;
    }


}
