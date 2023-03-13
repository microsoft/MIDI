using System;
using System.Threading.Channels;
using Microsoft.Midi.Settings.Core.Contracts.Services;
using Microsoft.Midi.Settings.Core.Models;

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
    private List<DisplayFriendlyMidiMessage> _allMessages;
    private List<DisplayFriendlyMidiDevice> _allDevices;


    public SampleDataService()
    {

    }


    private static DisplayFriendlyMidiMessage ConstructMidi2NoteOnMessage(long timestamp, string sourceDeviceName, byte group, byte channel, byte noteNumber, byte attributeType, UInt16 velocity, UInt16 attribute)
    {
        byte mt = 4;
        byte opcode = 9;

        byte[] bytes = new byte[8];

        bytes[0] = (byte)((mt << 4) | (group & 0xf));
        bytes[1] = (byte)((opcode << 4) | (channel & 0xf));
        bytes[2] = noteNumber;
        bytes[3] = attributeType;
        bytes[4] = (byte)((velocity & 0xF0) >> 8);
        bytes[5] = (byte)(velocity & 0x0F);
        bytes[6] = (byte)((attribute & 0xF0) >> 8);
        bytes[7] = (byte)(attribute & 0x0F);

        return new DisplayFriendlyMidiMessage(timestamp, bytes, 8, sourceDeviceName);
    }
    private static DisplayFriendlyMidiMessage ConstructMidi2NoteOffMessage(long timestamp, string sourceDeviceName, byte group, byte channel, byte noteNumber, byte attributeType, UInt16 velocity, UInt16 attribute)
    {
        byte mt = 4;
        byte opcode = 8;

        byte[] bytes = new byte[8];

        bytes[0] = (byte)((mt << 4) | (group & 0xf));
        bytes[1] = (byte)((opcode << 4) | (channel & 0xf));
        bytes[2] = noteNumber;
        bytes[3] = attributeType;
        bytes[4] = (byte)((velocity & 0xF0) >> 8);
        bytes[5] = (byte)(velocity & 0x0F);
        bytes[6] = (byte)((attribute & 0xF0) >> 8);
        bytes[7] = (byte)(attribute & 0x0F);

        return new DisplayFriendlyMidiMessage(timestamp, bytes, 8, sourceDeviceName);
    }
    private static DisplayFriendlyMidiMessage ConstructMidi1NoteOnMessage(long timestamp, string sourceDeviceName, byte group, byte channel, byte data1, byte data2)
    {
        byte mt = 2;
        byte opcode = 9;

        byte[] bytes = new byte[4];

        bytes[0] = (byte)((mt << 4) | (group & 0xf));
        bytes[1] = (byte)((opcode << 4) | (channel & 0xf));
        bytes[2] = data1;
        bytes[3] = data2;

        return new DisplayFriendlyMidiMessage(timestamp, bytes, 4, sourceDeviceName);
    }
    private static DisplayFriendlyMidiMessage ConstructMidi1NoteOffMessage(long timestamp, string sourceDeviceName, byte group, byte channel, byte data1, byte data2)
    {
        byte mt = 2;
        byte opcode = 8;

        byte[] bytes = new byte[4];

        bytes[0] = (byte)((mt << 4) | (group & 0xf));
        bytes[1] = (byte)((opcode << 4) | (channel & 0xf));
        bytes[2] = data1;
        bytes[3] = data2;

        return new DisplayFriendlyMidiMessage(timestamp, bytes, 4, sourceDeviceName);
    }
    private static DisplayFriendlyMidiMessage ConstructTypeFMessage(long timestamp, string sourceDeviceName, byte form, UInt16 status, UInt16 data1, UInt32 data2, UInt32 data3, UInt32 data4)
    {
        byte mt = 0xF;

        byte[] bytes = new byte[16];

        // I should probably be publicly shamed for writing this this way.
        // C unions and bitfields would really help here

        bytes[0] = (byte)((mt << 4) | ((form & 0x7) << 2) | ((status >> 8) & 0x3));
        bytes[1] = (byte)(status & 0xFF);
        bytes[2] = (byte)((data1 & 0xFF00) >> 8);
        bytes[3] = (byte)(data1 & 0x00FF);
        bytes[4] = (byte)((data2 & 0xFF000000) >> 24);
        bytes[5] = (byte)((data2 & 0x00FF0000) >> 16);
        bytes[6] = (byte)((data2 & 0x0000FF00) >> 8);
        bytes[7] = (byte)((data2 & 0x000000FF));
        bytes[8] = (byte)((data3 & 0xFF000000) >> 24);
        bytes[9] = (byte)((data3 & 0x00FF0000) >> 16);
        bytes[10] = (byte)((data3 & 0x0000FF00) >> 8);
        bytes[11] = (byte)((data3 & 0x000000FF));
        bytes[12] = (byte)((data4 & 0xFF000000) >> 24);
        bytes[13] = (byte)((data4 & 0x00FF0000) >> 16);
        bytes[14] = (byte)((data4 & 0x0000FF00) >> 8);
        bytes[15] = (byte)((data4 & 0x000000FF));

        return new DisplayFriendlyMidiMessage(timestamp, bytes, 16, sourceDeviceName);
    }

    // the data in these is mostly garbage. Just for display. More detailed parsing could break.
    private static IEnumerable<DisplayFriendlyMidiMessage> AllMessages()
    {
        var list = new List<DisplayFriendlyMidiMessage>();

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


    public async Task<IEnumerable<DisplayFriendlyMidiMessage>> GetUmpMonitorDataAsync()
    {
        if (_allMessages == null)
        {
            _allMessages = new List<DisplayFriendlyMidiMessage>(AllMessages());
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
