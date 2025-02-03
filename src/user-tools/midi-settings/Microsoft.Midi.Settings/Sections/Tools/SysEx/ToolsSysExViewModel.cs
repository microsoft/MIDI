using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.UI.Dispatching;
using Microsoft.Midi.Settings;
using Microsoft.Midi.Settings.Models;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class ToolsSysExViewModel : ObservableRecipient
    {
        public DispatcherQueue? DispatcherQueue { get; set; }

        public ToolsSysExViewModel()
        {
        }

        public ObservableCollection<MidiEndpointDeviceInformation> MidiEndpointDevices { get; } = [];

        private const UInt16 DefaultDelayBetweenMessagesMilliseconds = 10;
        private const UInt16 MaxDelayBetweenMessagesMilliseconds = 2000;
        private UInt16 _delayBetweenMessagesMilliseconds = DefaultDelayBetweenMessagesMilliseconds;
        public string DelayBetweenMessagesText
        {
            get { return _delayBetweenMessagesMilliseconds.ToString(); }
            set
            {
                UInt16 temp;

                if (UInt16.TryParse(value, out temp))
                {
                    if (temp > MaxDelayBetweenMessagesMilliseconds)
                    {
                        temp = MaxDelayBetweenMessagesMilliseconds;
                    }

                    SetProperty(ref _delayBetweenMessagesMilliseconds, temp);
                }
                else
                {
                    SetProperty(ref _delayBetweenMessagesMilliseconds, DefaultDelayBetweenMessagesMilliseconds);
                }
            }
        }


        public void RefreshDeviceCollection()
        {
            if (DispatcherQueue == null) return;

            DispatcherQueue.TryEnqueue(DispatcherQueuePriority.Normal, () =>
            {
                MidiEndpointDevices.Clear();

                // now get all the endpoint devices and put them in groups by transport

                var enumeratedDevices = AppState.Current.MidiEndpointDeviceWatcher.EnumeratedEndpointDevices.Values.OrderBy(x => x.Name);

                foreach (var endpointDevice in enumeratedDevices)
                {
                    if (endpointDevice.EndpointPurpose == MidiEndpointDevicePurpose.NormalMessageEndpoint)
                    {
                        // check for input / output
                        MidiEndpointDevices.Add(endpointDevice);
                    }

                }

            });

        }



    }
}
