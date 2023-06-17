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

        public ObservableCollection<Midi1ParentDeviceInformation> Midi1Devices { get; } = new ObservableCollection<Midi1ParentDeviceInformation>();


        public void OnNavigatedFrom()
        {
        }
        public void OnNavigatedTo(object parameter)
        {
            EnumerateDevices();
        }

        private async void GetParentDevice(DeviceInformation di)
        {
            //
        }

        private async void DumpProperties(DeviceInformation di)
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

            string id = di.Properties["System.Devices.DeviceInstanceId"].ToString().Replace("\\", "\\\\");

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
            var devices = await DeviceInformation.FindAllAsync(null, new[] { "System.Devices.Parent", "System.Devices.DeviceManufacturer", "System.Devices.HardwareIds", "System.Devices.ModelName", "System.Devices.InterfaceClassGuid" }, DeviceInformationKind.Device);

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

        private async Task<Midi1ParentDeviceInformation> GetMidiDeviceParentAsync(string midiDeviceInstanceId)
        {
            const string systemContainerId = "00000000-0000-0000-ffff-ffffffffffff";
            const string localSystemName = "Local System";
            const string unknownParentName = "Unknown";

            var actualDevice = await DeviceInformation.CreateFromIdAsync(
                midiDeviceInstanceId, 
                new[] { "System.Devices.Parent", "System.Devices.DeviceManufacturer", "System.Devices.ContainerId" }, 
                DeviceInformationKind.Device);

            //    DumpProperties(device);

            string containerId = actualDevice.Properties["System.Devices.ContainerId"]?.ToString();
            string parentId = actualDevice.Properties["System.Devices.Parent"]?.ToString();

            Midi1ParentDeviceInformation? parent = null;

            // if the container id is system and we have a parent id, use the parent id instead.
            // this is true of, for example, the MOTU Express 128
            if (!string.IsNullOrEmpty(parentId) && containerId == systemContainerId)
            {
                System.Diagnostics.Debug.WriteLine("ParentId = " + parentId);

                // parent was specified, so use that
                parent = Midi1Devices.Where(device => device.Id == parentId).FirstOrDefault();

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

                    Midi1Devices.Add(parent);
                }
            }
            else if (containerId != systemContainerId)
            {
                // this is not the system container id so 

                System.Diagnostics.Debug.WriteLine("ContainerId = " + containerId);

                parent = Midi1Devices.Where(device => device.Id == containerId).FirstOrDefault();

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

                    Midi1Devices.Add(parent);
                }
            }
            else if (containerId == systemContainerId)
            {
                // system container id 

                System.Diagnostics.Debug.WriteLine("ContainerId = " + containerId);

                parent = Midi1Devices.Where(device => device.Id == containerId).FirstOrDefault();

                if (parent == null)
                {
                    parent = new Midi1ParentDeviceInformation() { Id = containerId };
                    parent.Name = localSystemName;

                    Midi1Devices.Add(parent);
                }
            }

            return parent;
        }

        private async void EnumerateDevices()
        {
            Midi1Devices.Clear();

            // input devices

            var inputDevices = await DeviceInformation.FindAllAsync(MidiInPort.GetDeviceSelector());

            foreach (var device in inputDevices)
            {
                var parent = await GetMidiDeviceParentAsync(device.Properties["System.Devices.DeviceInstanceId"].ToString());

                var port = new Midi1PortInformation()
                {
                    ContainerId = parent.Id,
                    Id = device.Id,
                    Name = device.Name,
                    PortDirection = Midi1PortDirection.Input,
                    DeviceInstanceId = device.Properties["System.Devices.DeviceInstanceId"].ToString()

                };

                parent.Ports.Add(port);

            }

            // output devices

            var outputDevices = await DeviceInformation.FindAllAsync(MidiOutPort.GetDeviceSelector());

            foreach (var device in outputDevices)
            {
                var parent = await GetMidiDeviceParentAsync(device.Properties["System.Devices.DeviceInstanceId"].ToString());

                var port = new Midi1PortInformation()
                { 
                    ContainerId = parent.Id, 
                    Id = device.Id, 
                    Name = device.Name, 
                    PortDirection = Midi1PortDirection.Output,
                    DeviceInstanceId = device.Properties["System.Devices.DeviceInstanceId"].ToString()
                };

                parent.Ports.Add(port);
            }
        }

    }

    public enum Midi1PortDirection
    {
        Input = 0,
        Output = 1
    }

    public class Midi1ParentDeviceInformation
    {
        public string Id { get; internal set; }

        public string Name { get; internal set; }
        public ObservableCollection<Midi1PortInformation> Ports { get; internal set; } = new ObservableCollection<Midi1PortInformation>();

    }

    public class Midi1PortInformation
    {
        public string Name { get; internal set; }
        public string Id { get; internal set; }
        public string DeviceInstanceId { get; internal set; }
        public Midi1PortDirection PortDirection { get; internal set; }

        public string ContainerId { get; internal set; }

    }



}