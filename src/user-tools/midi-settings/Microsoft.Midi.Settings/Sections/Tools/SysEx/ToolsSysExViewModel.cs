// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

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
using Microsoft.Midi.Settings.Services;
using Microsoft.Midi.Settings.Contracts.Services;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class ToolsSysExViewModel : ObservableRecipient
    {
        private readonly IMidiSdkService _sdkService;
        private readonly IMidiEndpointEnumerationService _endpointEnumerationService;

        public DispatcherQueue? DispatcherQueue { get; set; }

        public ICommand SendSysExCommand
        {
            get; private set;
        }


        public ToolsSysExViewModel(
            IMidiSdkService sdkService,
            IMidiEndpointEnumerationService endpointEnumerationService
            )
        {
            _sdkService = sdkService;
            _endpointEnumerationService = endpointEnumerationService;

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

            SelectedFileName = null;
        }


        private async void SendSysEx()
        {
            if (SelectedEndpoint == null)
            {
                // TODO: Set error display
                return;
            }

            if (SelectedGroup == null)
            {
                // TODO: Set error display
                return;
            }

            if (SelectedFileName == null || SelectedFileName == string.Empty)
            {
                // TODO: Set error display
                return;
            }

            var selectedFile = await StorageFile.GetFileFromPathAsync(SelectedFileName);

            TransferInProgress = true;

            var stream = await selectedFile.OpenStreamForReadAsync();

            if (stream == null)
            {
                // TODO: set error display
                return;
            }

            var props = await selectedFile.GetBasicPropertiesAsync();

            if (props != null)
            {
                TransferTotalBytesInFile = (double)(props.Size);
            }


            await Task.Run(async () =>
            {
                var session = MidiSession.Create("SysEx Sender");

                if (session == null)
                {
                    // TODO: Set error display
                    return;
                }

                var connection = session.CreateEndpointConnection(SelectedEndpoint.Id);

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
                            DispatcherQueue?.TryEnqueue(() =>
                            {
                                TransferBytesRead = (double)(progress.BytesRead);
                                TransferMessagesSent = progress.MessagesSent;
                            });
                        });

                    await op;
                }
            });

        }

        [ObservableProperty]
        string? selectedFileName;


        [ObservableProperty]
        MidiEndpointWrapper? selectedEndpoint;

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


        public ObservableCollection<MidiEndpointWrapper> MidiEndpoints { get; } = [];


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
                MidiEndpoints.Clear();

                // now get all the endpoint devices and put them in groups by transport

                var endpoints = _endpointEnumerationService.GetEndpointsForPurpose(MidiEndpointDevicePurpose.NormalMessageEndpoint).OrderBy(x => x.Name);

                foreach (var endpoint in endpoints)
                {
                    MidiEndpoints.Add(endpoint);
                }
            });
        }



    }
}
