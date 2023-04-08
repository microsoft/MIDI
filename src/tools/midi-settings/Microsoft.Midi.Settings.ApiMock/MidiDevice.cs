using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Midi.Settings.SdkMock.Messages;
using Windows.Security.Cryptography.Core;

namespace Microsoft.Midi.Settings.SdkMock;
public class MidiDevice
{
    // temp. This is hard-coded to the Network one for the moment

    public string Id
    {
        get; set;
    }

    public string Name
    {
        get; set; 
    }

    public string TransportType
    {
        get; set; 
    }

    public string Address
    {
        get; set;
    }

    private Dictionary<string, object> _properties = new();
    public IDictionary<string, object> Properties
    {
        get
        {
            return _properties;
        }
    }


    // hard coded, but this would use an interface or base type
    public Windows.Devices.Midi.NetworkMidiTransportPlugin.NetworkMidiHostUmpEndpoint UmpEndpoint
    {
        get; set; 
    }

    // this is a polling approach for prototyping purposes. May offer events and other approaches
    private readonly UmpFactory _umpFactory = new();

    public ObservableCollection<Ump> Messages
    {
        get; private set;
    } = new();

    public bool IsReceiving
    {
        get; private set; 
    }

    public async Task StartReceivingAsync()
    {
        await Task.Run(() =>
        {
            IsReceiving = true;

            while (true)
            {
                if (UmpEndpoint.IncomingMidiMessages.HasData())
                {
                    var word = UmpEndpoint.IncomingMidiMessages.ReadWord();

                    // process into messages. We're using the service API directly
                    // instead of the client API or SDK, so there's no message processing for
                    // us here. All this copying is inefficient and is ONLY for prototyping
                    // Once I have Gary's buffer implementation, will revisit

                    _umpFactory.AddMidiWord(word);

                    if (_umpFactory.Umps.Count > 0)
                    {
                        var ump = _umpFactory.Umps.Dequeue();

                        // add to the observable collection
                        // Note, there's currently no timestamp. We need to add this to
                        // the mock. In the final version, it will come from the service.
                        Messages.Add(ump);
                    }
                }
                else
                {
                    // very much a temp hold
                    Thread.Sleep(200);
                }
            }
        });
    }


}
