using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.WinUI.UI.Controls;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.Devices.Midi;


namespace Microsoft.Midi.Settings.ViewModels
{
    public class Midi1DevicesViewModel : ObservableRecipient, INavigationAware
    {
        public Midi1DevicesViewModel()
        {

        }

        private List<Midi1ParentDeviceInformation> UnsortedMidi1Devices { get; } = [];
        public ObservableCollection<Midi1ParentDeviceInformation> Midi1Devices { get; } = [];


        public void OnNavigatedFrom()
        {
        }
        public void OnNavigatedTo(object parameter)
        {
            EnumerateDevices();
        }

        private void GetParentDevice(DeviceInformation di)
        {
            //
        }

        private void DumpProperties(DeviceInformation di)
        {
            System.Diagnostics.Debug.WriteLine("----");
            System.Diagnostics.Debug.WriteLine("MIDI props");
            foreach (var prop in di.Properties)
            {
                System.Diagnostics.Debug.WriteLine("midi " + prop.Key +  ": " + prop.Value);
            }

         //   TryDumpGenericProperties(di);

            System.Diagnostics.Debug.WriteLine("----");

        }

        private async void TryDumpGenericProperties(DeviceInformation di)
        {
            System.Diagnostics.Debug.WriteLine("   Generic props");

#pragma warning disable 8602
            string id = di.Properties.GetValueOrDefault("System.Devices.DeviceInstanceId", string.Empty).ToString().Replace("\\", "\\\\");
#pragma warning restore 8602
            string query = "System.Devices.DeviceInstanceId:=\"" + id + "\"";

            System.Diagnostics.Debug.WriteLine("   query:" + query);

            var devices = await DeviceInformation.FindAllAsync(query, null, DeviceInformationKind.DeviceInterface);

            if (devices != null && devices.Count > 0)
            {
                foreach (var device in devices)
                {
                    foreach (var prop in device.Properties)
                    {
                        System.Diagnostics.Debug.WriteLine("    gen " + prop.Key + ": " + prop.Value);
                    }
                }
            }
            else
            {
                System.Diagnostics.Debug.WriteLine("   no generic device matches");

            }

        }

        private async void TryFindAllParents()
        {
            System.Diagnostics.Debug.WriteLine("Begin Possible parent devices -------------------------------------- ");

            string classGuid = "{4d36e96c-e325-11ce-bfc1-08002be10318}";

            string query = "System.Devices.InterfaceClassGuid:=\"" + classGuid + "\"";

            System.Diagnostics.Debug.WriteLine("query:" + query);

            //var devices = await DeviceInformation.FindAllAsync(query, null, DeviceInformationKind.DeviceInterface);
            var devices = await DeviceInformation.FindAllAsync(
                null, new[] 
                { 
                    "System.Devices.Parent", 
                    "System.Devices.DeviceManufacturer", 
                    "System.Devices.HardwareIds", 
                    "System.Devices.ModelName", 
                    "System.Devices.InterfaceClassGuid" 
                }, 
                DeviceInformationKind.Device);

            if (devices != null && devices.Count > 0)
            {
                foreach (var device in devices)
                {
                    System.Diagnostics.Debug.WriteLine(device.Name);
                    System.Diagnostics.Debug.WriteLine(device.Id);
                    System.Diagnostics.Debug.WriteLine(device.Kind.ToString());

                    foreach (var prop in device.Properties)
                    {
                        System.Diagnostics.Debug.WriteLine(" " + prop.Key + ": " + prop.Value);
                    }
                    System.Diagnostics.Debug.WriteLine(" -- ");

                }
            }
            else
            {
                System.Diagnostics.Debug.WriteLine("No matching devices");

            }

            System.Diagnostics.Debug.WriteLine("End Possible parent devices -------------------------------------- ");

        }

        private async Task<Midi1ParentDeviceInformation?> GetMidiDeviceParentAsync(string midiDeviceInstanceId)
        {
            const string systemContainerId = "00000000-0000-0000-ffff-ffffffffffff";
            const string localSystemName = "Local System";
            const string unknownParentName = "Unknown";

            var actualDevice = await DeviceInformation.CreateFromIdAsync(
                midiDeviceInstanceId, 
                new[] { "System.Devices.Parent", "System.Devices.DeviceManufacturer", "System.Devices.ContainerId" }, 
                DeviceInformationKind.Device);

            //    DumpProperties(device);

            string containerId = actualDevice.Properties.GetValueOrDefault("System.Devices.ContainerId", Guid.Empty).ToString()!;
            string parentId = (string)actualDevice.Properties.GetValueOrDefault("System.Devices.Parent", string.Empty);

            Midi1ParentDeviceInformation? parent = null;

            // if the container id is system and we have a parent id, use the parent id instead.
            // this is true of, for example, the MOTU Express 128
            if (!string.IsNullOrEmpty(parentId) && containerId == systemContainerId)
            {
                System.Diagnostics.Debug.WriteLine("ParentId = " + parentId);

                // parent was specified, so use that
                parent = UnsortedMidi1Devices.Where(device => device.Id == parentId).FirstOrDefault();

                if (parent == null)
                {
                    parent = new Midi1ParentDeviceInformation() { Id = parentId };

                    var di = await DeviceInformation.CreateFromIdAsync(parentId, null, DeviceInformationKind.Device);

                    if (di != null)
                    {
                        if (string.IsNullOrEmpty(di.Name))
                        {
                            // this happens with rooted devices like the GS Wavetable Synth
                            parent.Name = localSystemName;
                        }
                        else
                        {
                            parent.Name = di.Name;

                        }

                    }
                    else
                    {
                        System.Diagnostics.Debug.WriteLine("Couldn't find parent device with id " + parentId);
                        parent.Name = unknownParentName;
                    }

                    UnsortedMidi1Devices.Add(parent);
                }
            }
            else if (containerId != systemContainerId)
            {
                // this is not the system container id so 

                System.Diagnostics.Debug.WriteLine("ContainerId = " + containerId);

                parent = UnsortedMidi1Devices.Where(device => device.Id == containerId).FirstOrDefault();

                if (parent == null)
                {
                    parent = new Midi1ParentDeviceInformation() { Id = containerId };

                    var di = await DeviceInformation.CreateFromIdAsync(containerId, null, DeviceInformationKind.DeviceContainer);

                    if (di != null)
                    {
                        parent.Name = di.Name;
                    }
                    else
                    {
                        System.Diagnostics.Debug.WriteLine("Couldn't find parent container with id " + containerId);
                    }

                    UnsortedMidi1Devices.Add(parent);
                }
            }
            else if (containerId == systemContainerId)
            {
                // system container id 

                System.Diagnostics.Debug.WriteLine("ContainerId = " + containerId);

                parent = UnsortedMidi1Devices.Where(device => device.Id == containerId).FirstOrDefault();

                if (parent == null)
                {
                    parent = new Midi1ParentDeviceInformation() { Id = containerId };
                    parent.Name = localSystemName;

                    UnsortedMidi1Devices.Add(parent);
                }
            }

            return parent;
        }

        private async void EnumerateDevices()
        {
            UnsortedMidi1Devices.Clear();

            // input devices

            var inputDevices = await DeviceInformation.FindAllAsync(MidiInPort.GetDeviceSelector());

            if (inputDevices != null)
            {
                foreach (var device in inputDevices)
                {
                    var parent = await GetMidiDeviceParentAsync(device.Properties.GetValueOrDefault("System.Devices.DeviceInstanceId", string.Empty).ToString()!);

                    var port = new Midi1PortInformation()
                    {
                        ContainerId = parent!.Id,
                        Id = device.Id,
                        Name = device.Name,
                        PortDirection = Midi1PortDirection.Input,
                        DeviceInstanceId = device.Properties.GetValueOrDefault("System.Devices.DeviceInstanceId", string.Empty).ToString()!

                    };

                    parent.Ports.Add(port);

                }
            }
            // output devices

            var outputDevices = await DeviceInformation.FindAllAsync(MidiOutPort.GetDeviceSelector());

            if (outputDevices != null)
            {
                foreach (var device in outputDevices)
                {
                    var parent = await GetMidiDeviceParentAsync(device.Properties.GetValueOrDefault("System.Devices.DeviceInstanceId", string.Empty).ToString()!);

                    var port = new Midi1PortInformation()
                    {
                        ContainerId = (string.IsNullOrEmpty(parent!.Id) ? string.Empty : parent.Id),
                        Id = device.Id,
                        Name = device.Name,
                        PortDirection = Midi1PortDirection.Output,
                        DeviceInstanceId = device.Properties.GetValueOrDefault("System.Devices.DeviceInstanceId", string.Empty).ToString()
                    };

                    parent.Ports.Add(port);
                }
            }

            // sort the results
            Midi1Devices.Clear();
            foreach (var p in UnsortedMidi1Devices.OrderBy(x=>x.Name))
            {
                Midi1Devices.Add(p);
            }
            UnsortedMidi1Devices.Clear();

        }
    }

    public enum Midi1PortDirection
    {
        Unknown = 0,
        Input = 1,
        Output = 2
    }

    public class Midi1ParentDeviceInformation
    {
        public Midi1ParentDeviceInformation()
        {
            Id = string.Empty;
            Name = string.Empty;
        }
        public string Id { get; internal set; }

        public string Name { get; internal set; }
        public ObservableCollection<Midi1PortInformation> Ports { get; internal init; } = new ObservableCollection<Midi1PortInformation>();

    }

    public class Midi1PortInformation
    {
        public Midi1PortInformation()
        {
            Name = string.Empty;
            Id = string.Empty;
            DeviceInstanceId = string.Empty;
            ContainerId = string.Empty;
            PortDirection = Midi1PortDirection.Unknown;
        }

        public string Name { get; internal init; }
        public string Id { get; internal init; }
        public string DeviceInstanceId { get; internal init; }
        public Midi1PortDirection PortDirection { get; internal init; }

        public string ContainerId { get; internal init; }

    }



}