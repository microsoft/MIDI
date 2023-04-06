using System;
using System.Threading.Channels;
using Microsoft.Midi.Settings.Core.Contracts.Services;
using Microsoft.Midi.Settings.Core.Models;
using static System.Runtime.InteropServices.JavaScript.JSType;

namespace Microsoft.Midi.Settings.Core.Services;

// This class holds sample data used by some generated pages to show how they can be used.
// TODO: The following classes have been created to display sample data. Delete these files once your app is using real data.
// 1. Contracts/Services/ISampleDataService.cs
// 2. Services/SampleDataService.cs
// 3. Models/SampleCompany.cs
// 4. Models/SampleOrder.cs
// 5. Models/SampleOrderDetail.cs
public class SampleDataService : ISampleDataService
{
    private List<string> _allUmpEndpointNames;
    private List<MidiMessageViewModel> _allMessages;
    private List<DisplayFriendlyMidiDevice> _allDevices;


    public SampleDataService()
    {

    }


    private static MidiMessageViewModel ConstructMidi2NoteOnMessage(long timestamp, string sourceDeviceName, byte group, byte channel, byte noteNumber, byte attributeType, UInt16 velocity, UInt16 attribute)
    {
        byte mt = 4;
        byte opcode = 9;

        var words = new UInt32[2];

        words[0] = (UInt32)(mt & 0xF) << 28 | ((UInt32)(group & 0xF) << 24) | ((UInt32)(opcode & 0x0F) << 20) | ((UInt32)(channel & 0x0F) << 16) | (UInt32)noteNumber << 8 | (UInt32)attributeType;
        words[1] = (UInt32)(velocity & 0xFFFF) << 16 | (UInt32)(attribute & 0xFFFF);

        return new MidiMessageViewModel(timestamp, words, sourceDeviceName);
    }
    private static MidiMessageViewModel ConstructMidi2NoteOffMessage(long timestamp, string sourceDeviceName, byte group, byte channel, byte noteNumber, byte attributeType, UInt16 velocity, UInt16 attribute)
    {
        byte mt = 4;
        byte opcode = 8;

        var words = new UInt32[2];

        words[0] = (UInt32)(mt & 0xF) << 28 | ((UInt32)(group & 0xF) << 24) | ((UInt32)(opcode & 0x0F) << 20) | ((UInt32)(channel & 0x0F) << 16) | (UInt32)noteNumber << 8 | (UInt32)attributeType;
        words[1] = (UInt32)(velocity & 0xFFFF) << 16 | (UInt32)(attribute & 0xFFFF);

        return new MidiMessageViewModel(timestamp, words, sourceDeviceName);
    }
    private static MidiMessageViewModel ConstructMidi1NoteOnMessage(long timestamp, string sourceDeviceName, byte group, byte channel, byte data1, byte data2)
    {
        byte mt = 2;
        byte opcode = 9;

        var words = new UInt32[1];

        words[0] = (UInt32)(mt & 0xF) << 28 | ((UInt32)(group & 0xF) << 24) | ((UInt32)(opcode & 0x0F ) << 20) | ((UInt32)(channel & 0x0F) << 16) | (UInt32)data1 << 8 | (UInt32)data2;

        return new MidiMessageViewModel(timestamp, words, sourceDeviceName);
    }
    private static MidiMessageViewModel ConstructMidi1NoteOffMessage(long timestamp, string sourceDeviceName, byte group, byte channel, byte data1, byte data2)
    {
        byte mt = 2;
        byte opcode = 8;

        var words = new UInt32[1];

        words[0] = (UInt32)(mt & 0xF) << 28 | ((UInt32)(group & 0xF) << 24) | ((UInt32)(opcode << 4) & 0xF0) | ((UInt32)(channel) & 0x0F);

        return new MidiMessageViewModel(timestamp, words, sourceDeviceName);
    }
    private static MidiMessageViewModel ConstructTypeFMessage(long timestamp, string sourceDeviceName, byte form, UInt16 status, UInt16 data1, UInt32 data2, UInt32 data3, UInt32 data4)
    {
        byte mt = 0xF;

        //byte[] bytes = new byte[16];

        var words = new UInt32[4];

        // I should probably be publicly shamed for writing this this way.
        // C unions and bitfields would really help here

        words[0] = (UInt32)(mt & 0xF) << 28 | ((UInt32)(form & 0x7) << 26) | ((UInt32)(status >> 6) & 0x3) | data1;
        words[1] = data2;
        words[2] = data3;
        words[3] = data4;

        return new MidiMessageViewModel(timestamp, words, sourceDeviceName);
    }

    // the data in these is mostly garbage. Just for display. More detailed parsing could break.
    private static IEnumerable<MidiMessageViewModel> AllMessages()
    {
        var list = new List<MidiMessageViewModel>();

        Random rnd = new Random();

        string sourceDeviceName = "Contoso GX-1";

        // this is not the real timestamp we'll use as this is only valid to the millisecond. Ok for prototyping
        long timestamp = Environment.TickCount64;

        // ensure we have a type F in there
        list.Add(ConstructTypeFMessage(timestamp, sourceDeviceName, (byte)rnd.Next(0, 3), (UInt16)rnd.Next(0, 1023), (UInt16)rnd.Next(0, UInt16.MaxValue), (UInt32)rnd.Next(0, Int32.MaxValue), (UInt32)rnd.Next(0, Int32.MaxValue), (UInt32)rnd.Next(0, Int32.MaxValue)));

        for (int i = 0; i < 128; i++)
        {
            // artificially increment timestamp
            timestamp += rnd.Next(5,128);

            switch (rnd.Next(0, 10))
            {
                case 0:
                case 1:
                case 2:
                    list.Add(ConstructMidi1NoteOnMessage(timestamp, sourceDeviceName, (byte)rnd.Next(0, 15), (byte)rnd.Next(0, 15), (byte)rnd.Next(0, 127), (byte)rnd.Next(0, 127)));
                    break;
                case 3:
                case 4:
                case 5:
                    list.Add(ConstructMidi1NoteOffMessage(timestamp, sourceDeviceName, (byte)rnd.Next(0, 15), (byte)rnd.Next(0, 15), (byte)rnd.Next(0, 127), (byte)rnd.Next(0, 127)));
                    break;
                case 6:
                case 7:
                    list.Add(ConstructMidi2NoteOnMessage(timestamp, sourceDeviceName, (byte)rnd.Next(0, 15), (byte)rnd.Next(0, 15), (byte)rnd.Next(0, 127), (byte)rnd.Next(0, 127), (UInt16)rnd.Next(0, UInt16.MaxValue), (UInt16)rnd.Next(0, UInt16.MaxValue)));
                    break;
                case 8:
                case 9:
                    list.Add(ConstructMidi2NoteOffMessage(timestamp, sourceDeviceName, (byte)rnd.Next(0, 15), (byte)rnd.Next(0, 15), (byte)rnd.Next(0, 127), (byte)rnd.Next(0, 127), (UInt16)rnd.Next(0, UInt16.MaxValue), (UInt16)rnd.Next(0, UInt16.MaxValue)));
                    break;
                case 10:
                    list.Add(ConstructTypeFMessage(timestamp, sourceDeviceName, (byte)rnd.Next(0, 3), (UInt16)rnd.Next(0, 1023), (UInt16)rnd.Next(0, UInt16.MaxValue), (UInt32)rnd.Next(0, Int32.MaxValue), (UInt32)rnd.Next(0, Int32.MaxValue), (UInt32)rnd.Next(0, Int32.MaxValue)));
                    break;
            }
        }

        return list;
    }

    private static IEnumerable<DisplayFriendlyMidiDevice> AllDevices()
    {
        var list = new List<DisplayFriendlyMidiDevice>()
        {
            new DisplayFriendlyMidiDevice()
            {
                Name="Contoso GX-1"
            },
            new DisplayFriendlyMidiDevice()
            {
                Name="Fabrikam JD-800"
            },
            new DisplayFriendlyMidiDevice()
            {
                Name="Contoso M-1"
            },
            new DisplayFriendlyMidiDevice()
            {
                Name="Contoso MIDI x16"
            }
        };

        return list;
    }

    private static IEnumerable<string> AllUmpEndpointNames()
    {
        var list = new List<string>();

        foreach (DisplayFriendlyMidiDevice device in AllDevices())
        {
            list.Add(device.Name);
        }

        return list;
    }






    public async Task<IEnumerable<DisplayFriendlyMidiDevice>> GetDevicesAsync()
    {
        if (_allDevices == null)
        {
            _allDevices = new List<DisplayFriendlyMidiDevice>(AllDevices());
        }

        await Task.CompletedTask;
        return _allDevices;
    }


    public async Task<IEnumerable<MidiMessageViewModel>> GetUmpMonitorDataAsync()
    {
        if (_allMessages == null)
        {
            _allMessages = new List<MidiMessageViewModel>(AllMessages());
        }

        await Task.CompletedTask;
        return _allMessages;
    }


    public async Task<IEnumerable<string>> GetUmpEndpointNamesAsync()
    {
        if (_allUmpEndpointNames == null)
        {
            _allUmpEndpointNames = new List<string>(AllUmpEndpointNames());
        }

        await Task.CompletedTask;
        return _allUmpEndpointNames;
    }


















}
