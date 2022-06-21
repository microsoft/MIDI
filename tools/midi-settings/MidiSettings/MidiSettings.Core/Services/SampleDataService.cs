using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

using MidiSettings.Core.Contracts.Services;
using MidiSettings.Core.Models;

namespace MidiSettings.Core.Services;

// This class holds sample data used by some generated pages to show how they can be used.
// TODO: The following classes have been created to display sample data. Delete these files once your app is using real data.
// 1. Contracts/Services/ISampleDataService.cs
// 2. Services/SampleDataService.cs
// 3. Models/SampleCompany.cs
// 4. Models/SampleOrder.cs
// 5. Models/SampleOrderDetail.cs
public class SampleDataService : ISampleDataService
{
    private List<SampleMidiDevice> _allDevices;

    public SampleDataService()
    {
    }

    private static IEnumerable<SampleMidiEndpoint> AllEndpoints()
    {
        // The following is order summary data
        var devices = AllDevices();
        return devices.SelectMany(c => c.MidiEndpoints);
    }


    private static IEnumerable<SampleMidiDevice> AllDevices()
    {
        return new List<SampleMidiDevice>()
        {
            new SampleMidiDevice()
            {
                Id = "{9d7debbc-c85d-11d1-9eb4-006008c3a19a}",
                Name = "iContoso StudioPort 8 (Above Display)",
                IconPath = "",
                TransportType = "USB",

                DeviceLocationInformation = "0000.0014.0000.003.003.001.000.000.000",
                DeviceManufacturer = "iContoso",
                DeviceSuppliedDisplayName = "iContoso StudioPort 8",
                DeviceInstancePath = "USB\\VID_1C75&PID_0288&MI_00\\A&118C224F&0&0000",

                DriverDate = DateTime.Parse("2022-04-28"),
                DriverDescription = "USB Audio Device",
                DriverProvider = "Microsoft",
                DriverVersion = "10.0.22000.653",
                ParentDevice = "USB\\VID_1C75&PID_0288\\00000000001A",

                MidiEndpoints=new List<SampleMidiEndpoint>()
                {
                    new SampleMidiEndpoint()
                    {
                        Id="",
                        Name="Sound Canvas",
                        ProtocolVersion="1.0",
                        DeviceSuppliedDisplayName="DIN MIDI 1",
                    },
                    new SampleMidiEndpoint()
                    {
                        Id="",
                        Name="Moog Voyager",
                        ProtocolVersion="1.0",
                        DeviceSuppliedDisplayName="DIN MIDI 2",
                    },
                    new SampleMidiEndpoint()
                    {
                        Id="",
                        Name="JP-8080",
                        ProtocolVersion="1.0",
                        DeviceSuppliedDisplayName="DIN MIDI 3",
                    },
                    new SampleMidiEndpoint()
                    {
                        Id="",
                        Name="Waldorf Pulse",
                        ProtocolVersion="1.0",
                        DeviceSuppliedDisplayName="DIN MIDI 4",
                    },
                    new SampleMidiEndpoint()
                    {
                        Id="",
                        Name="Korg EX-8000 1 (Lead)",
                        ProtocolVersion="1.0",
                        DeviceSuppliedDisplayName="DIN MIDI 5",
                    },
                    new SampleMidiEndpoint()
                    {
                        Id="",
                        Name="Korg EX-8000 2 (Pads)",
                        ProtocolVersion="1.0",
                        DeviceSuppliedDisplayName="DIN MIDI 6",
                    },
                    new SampleMidiEndpoint()
                    {
                        Id="",
                        Name="Ensoniq ESQ-M",
                        ProtocolVersion="1.0",
                        DeviceSuppliedDisplayName="DIN MIDI 7",
                    },
                    new SampleMidiEndpoint()
                    {
                        Id="",
                        Name="Kawai K3M",
                        ProtocolVersion="1.0",
                        DeviceSuppliedDisplayName="DIN MIDI 8",
                    },


                }
            },
        };
    }

    public async Task<IEnumerable<SampleMidiDevice>> GetDeviceListDetailsDataAsync()
    {
        if (_allDevices == null)
        {
            _allDevices = new List<SampleMidiDevice>(AllDevices());
        }

        await Task.CompletedTask;
        return _allDevices;
    }
}
