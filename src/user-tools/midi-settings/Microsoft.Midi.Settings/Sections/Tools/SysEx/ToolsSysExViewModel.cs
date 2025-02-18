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
using Microsoft.Midi.Settings.Controls;
using System.Windows.Input;
using CommunityToolkit.Mvvm.Input;
using Windows.Storage.Pickers;
using Windows.Storage;
using Microsoft.Windows.Devices.Midi2.Utilities.SysExTransfer;
using Microsoft.Extensions.Logging.Abstractions;
using Windows.Foundation;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class ToolsSysExViewModel : ObservableRecipient
    {
        public DispatcherQueue? DispatcherQueue { get; set; }

        public ICommand SendSysExCommand
        {
            get; private set;
        }


        public ToolsSysExViewModel()
        {
            DelayBetweenMessagesText = DefaultDelayBetweenMessagesMilliseconds.ToString();

            SendSysExCommand = new RelayCommand(
                () =>
                {
                    SendSysEx();
                });

            TransferInProgress = false;
            TransferCompleteSuccess = false;
            transferCompleteFailed = false;

            TransferBytesRead = 0;
            TransferTotalBytesInFile = 0;

            SelectedFile = null;
            SelectedFileName = null;
        }


        private async void SendSysEx()
        {
            if (SelectedEndpointDevice == null)
            {
                // TODO: Set error display
                return;
            }

            if (SelectedGroup == null)
            {
                // TODO: Set error display
                return;
            }

            if (SelectedFile == null)
            {
                // TODO: Set error display
                return;
            }


            TransferInProgress = true;

            var stream = await SelectedFile.OpenStreamForReadAsync();

            if (stream == null)
            {
                // TODO: set error display
                return;
            }

            var props = await SelectedFile.GetBasicPropertiesAsync();

            if (props != null)
            {
                TransferTotalBytesInFile = (double)(props.Size);
            }

            var session = MidiSession.Create("SysEx Sender");

            if (session == null)
            {
                // TODO: Set error display
                return;
            }

            var connection = session.CreateEndpointConnection(SelectedEndpointDevice.EndpointDeviceId);

            if (connection == null)
            {
                // TODO: Set error display
                return;
            }

            if (connection.Open())
            {
                var op = MidiSystemExclusiveSender.SendDataAsync(
                    connection,
                    stream.AsInputStream(),
                    MidiSystemExclusiveDataReaderFormat.Binary,
                    MidiSystemExclusiveDataFormat.ByteFormatSystemExclusive7,
                    _delayBetweenMessagesMilliseconds,
                    true,
                    SelectedGroup.Group);

                op.Progress = new AsyncOperationProgressHandler<bool, MidiSystemExclusiveSendProgress>(
                    (info, progress) =>
                    {
                        DispatcherQueue.TryEnqueue(() =>
                        {
                            TransferBytesRead = (double)(progress.BytesRead);
                            TransferMessagesSent = progress.MessagesSent;
                        });
                    });

                await op;
            }



        }

        [ObservableProperty]
        StorageFile? selectedFile;


        [ObservableProperty]
        string? selectedFileName;


        [ObservableProperty]
        MidiEndpointDeviceInformation? selectedEndpointDevice;

        [ObservableProperty]
        MidiGroupForDisplay? selectedGroup;


        [ObservableProperty]
        bool transferInProgress;

        [ObservableProperty]
        double transferBytesRead;

        [ObservableProperty]
        ulong transferMessagesSent;

        [ObservableProperty]
        double transferTotalBytesInFile;

        [ObservableProperty]
        bool transferCompleteSuccess;

        [ObservableProperty]
        bool transferCompleteFailed;


        public ObservableCollection<MidiEndpointDeviceInformation> MidiEndpointDevices { get; } = [];


        private const UInt16 DefaultDelayBetweenMessagesMilliseconds = 50;
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
