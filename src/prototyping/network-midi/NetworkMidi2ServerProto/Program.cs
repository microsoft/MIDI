
using NetworkMidi2ServerProto;
using Windows.Networking.Connectivity;
using Windows.Networking;
using Windows.Networking.ServiceDiscovery.Dnssd;
using Windows.Networking.Sockets;
using Spectre.Console;
using System.Security.Cryptography.X509Certificates;
using Windows.Globalization.DateTimeFormatting;
using Windows.Storage.Streams;
using System.Text.Json.Nodes;



// this prototype currently handles only a single endpoint






// needed for emoji support
System.Console.OutputEncoding = System.Text.Encoding.UTF8;

AnsiConsole.MarkupLine("[green]Network MIDI 2.0 Prototype app[/]");

//     .Where(x => x.IPInformation != null && x.IPInformation.NetworkAdapter != null)


var hostNames = NetworkInformation
    .GetHostNames()
     .Where(x => x.Type == HostNameType.DomainName && x.RawName.Contains(".local"))
    .Select(x => new KeyValuePair<string, HostName>(x.ToString(), x))
    .ToList();

KeyValuePair<string, HostName> selectedHostName;

if (hostNames.Count > 1)
{
    var prompt = new SelectionPrompt<string>()
        .Title("Please pick the host name for this service.")
        .PageSize(20)
        .MoreChoicesText("Move up and down to see all possible host names");

    foreach (var host in hostNames)
    {
        prompt.AddChoice(host.Key);
    }

    var hostNameString = AnsiConsole.Prompt(prompt);

    selectedHostName = hostNames.Where(x => x.Key == hostNameString).FirstOrDefault();
}
else
{
    selectedHostName = hostNames.FirstOrDefault();

}

string midiEndpointName = "Test Network Host UMP Endpoint";
string midiProductInstanceId = "3263827";

string serviceType = "_midi2._udp.local";
string serviceInstanceName = "windowsproto"; // + DateTime.Now.Ticks;

string fullServiceName = serviceInstanceName + "." + serviceType;
ushort port = 13337;

//AnsiConsole.MarkupLine($"Starting service [green]{fullServiceName}[/] on [blue]{selectedHostName.Key}[/] and port [white]{port}[/].");

var table = new Table();
table.AddColumn("");
table.AddColumn("[blue]Service Setting[/]");

table.BorderColor(Color.DarkSlateGray1);

table.AddRow("[blue]Host Name[/]", selectedHostName.Key);
table.AddRow("[blue]Service Name[/]", fullServiceName);
table.AddRow("[blue]Port[/]", port.ToString());
table.AddRow("[blue]MIDI Endpoint Name[/]", midiEndpointName);
table.AddRow("[blue]MIDI Product Instance Id[/]", midiProductInstanceId);

AnsiConsole.Write(table);




// just for testing, of course


// json properties for the endpoint:
// - Server or Client
// - Endpoint Name
// - Product Instance Id
// + Client Properties
//  - Remote Host Address
//  - Remote Host Port
// + Server Properties
//  - Advertise on DNSSD - Bool
//  - Service Instance Name without suffix
//  - Port Number (or empty/null/missing for auto)

// can use windows.data.json for this insteasd of building a string

Windows.Data.Json.JsonObject obj = new Windows.Data.Json.JsonObject();

obj["ConnectionType"] = Windows.Data.Json.JsonValue.CreateStringValue("HostServer");
obj["MidiEndpointName"] = Windows.Data.Json.JsonValue.CreateStringValue(midiEndpointName);
obj["MidiProductInstanceId"] = Windows.Data.Json.JsonValue.CreateStringValue(midiProductInstanceId);
obj["Server_Advertise"] = Windows.Data.Json.JsonValue.CreateBooleanValue(true);
obj["Server_HostName"] = Windows.Data.Json.JsonValue.CreateStringValue(selectedHostName.Value.RawName);
obj["Server_ServiceInstanceName"] = Windows.Data.Json.JsonValue.CreateStringValue(serviceInstanceName);
obj["Server_Port"] = Windows.Data.Json.JsonValue.CreateStringValue("13337"); // Windows.Data.Json.JsonValue.CreateNullValue();

string jsonParameters = obj.Stringify();

System.Diagnostics.Debug.WriteLine("-----------------");
System.Diagnostics.Debug.WriteLine(jsonParameters);
System.Diagnostics.Debug.WriteLine("-----------------");



AnsiConsole.MarkupLine("[green]Creating endpoint factory[/]");
var factory = new Windows.Devices.Midi.NetworkMidiTransportPlugin.NetworkMidiHostUmpEndpointFactory();

AnsiConsole.MarkupLine("[green]Creating endpoint[/]");
var endpoint = await factory.CreateNewUmpEndpoint(jsonParameters);

AnsiConsole.MarkupLine("[green]Starting listening for messages[/]");

while (true)
{
    //AnsiConsole.MarkupLine("[green]Looping...[/]");

    DataReader midiReader = new DataReader(endpoint.IncomingMessages);

    if (midiReader.UnconsumedBufferLength >= sizeof(UInt32))
    {
        var word = midiReader.ReadUInt32();

        AnsiConsole.MarkupLine("[green]0x{0:X4}[/]", word);
    }

    Thread.Sleep(1000);
}

//Console.ReadKey(true);
