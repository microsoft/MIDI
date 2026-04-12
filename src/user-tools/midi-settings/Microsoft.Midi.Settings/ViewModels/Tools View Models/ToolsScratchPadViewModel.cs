// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Controls;
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class MidiController : ObservableRecipient
    {
        [ObservableProperty]
        private byte index;

        [ObservableProperty]
        private string description;

        [ObservableProperty]
        private bool hasDescription;

        public MidiController(byte controllerIndex)
        {
            Index = controllerIndex;
            Description = string.Empty;

            switch (controllerIndex)
            {
                case 0: Description = "Bank select"; break;
                case 1: Description = "Mod Wheel"; break;
                case 2: Description = "Breath"; break;
                case 4: Description = "Foot Pedal"; break;
                case 5: Description = "Portamento Time"; break;
                case 6: Description = "Data Entry (MSB)"; break;
                case 7: Description = "Volume"; break;
                case 8: Description = "Balance"; break;
                case 10: Description = "Stereo Pan"; break;
                case 11: Description = "Expression"; break;
                case 32: Description = "Bank Select (LSB)"; break;
                case 64: Description = "Sustain Pedal"; break;
                case 65: Description = "Portamento"; break;
                case 66: Description = "Sostenuto Pedal"; break;
                case 67: Description = "Soft Pedal"; break;
                case 68: Description = "Legato Footswitch"; break;
                case 69: Description = "Hold 2"; break;
                case 74: Description = "Brightness"; break;
                case 84: Description = "Portamento Amount"; break;
                case 88: Description = "High Resolution Velocity"; break;
                case 96: Description = "Data Increment"; break;
                case 97: Description = "Data Decrement"; break;
                case 98: Description = "NRPN (LSB)"; break;
                case 99: Description = "NRPN (MSB)"; break;
                case 100: Description = "RPN (LSB)"; break;
                case 101: Description = "RPN (MSB)"; break;
                case 120: Description = "All Sound Off"; break;
                case 121: Description = "Reset All Controllers"; break;
                case 122: Description = "Local Control On/Off"; break;
                case 123: Description = "All Notes Off"; break;
                case 124: Description = "Omni Mode Off"; break;
                case 125: Description = "Omni Mode On"; break;
                case 126: Description = "Mono Mode On"; break;
                case 127: Description = "Poly Mode On"; break;
            }

            HasDescription = Description != string.Empty;
        }
    }

    public partial class MidiNote : ObservableRecipient
    {
        [ObservableProperty]
        private byte noteIndex;

        [ObservableProperty]
        private string description;

        public MidiNote(byte noteIndex)
        {
            NoteIndex = noteIndex;
            Description = $"{MidiMessageHelper.GetNoteDisplayNameFromNoteIndex(noteIndex)} {MidiMessageHelper.GetNoteOctaveFromNoteIndex(noteIndex)}";
        }
    }

    public partial class ToolsScratchPadViewModel : ObservableRecipient, ISettingsSearchTarget, INavigationAware
    {
        public static IList<string> GetSearchKeywords()
        {
            // TODO: these need to be localized, so should refer to resources instead
            return new[] { "sysex", "system exclusive", "scratch pad" };
        }

        public static string GetSearchPageTitle()
        {
            return "MIDI 1.0 Scratch Pad";
        }

        public static string GetSearchPageDescription()
        {
            return "Send arbitrary MIDI 1.0 data to an endpoint.";
        }


        [ObservableProperty]
        MidiEndpointWrapper? selectedEndpoint;


        [ObservableProperty]
        private MidiGroupForDisplay? selectedGroup;

        partial void OnSelectedGroupChanged(MidiGroupForDisplay? value)
        {
            if (value != null)
            {
                IsValidDestinationSelected = true;
            }

            UpdateSendButtonAvailability(AllScratchPadText, value);
        }

        public ObservableCollection<MidiEndpointWrapper> MidiEndpoints { get; } = [];

        public ObservableCollection<MidiChannel> AllValidChannels { get; } = [];
        public ObservableCollection<MidiNote> AllValidNotes { get; } = [];
        public ObservableCollection<byte> AllValidVelocities { get; } = [];

        public ObservableCollection<MidiController> AllValidControllers { get; } = [];
        public ObservableCollection<byte> AllValidControllerValues { get; } = [];



        [ObservableProperty]
        private MidiChannel selectedChannel;

        [ObservableProperty]
        private MidiNote selectedNote;

        [ObservableProperty]
        private byte selectedVelocity;


        [ObservableProperty]
        private MidiController selectedController;

        [ObservableProperty]
        private byte selectedControllerValue;



        [ObservableProperty]
        private string allScratchPadText;
        partial void OnAllScratchPadTextChanged(string value)
        {
            UpdateSendButtonAvailability(value, SelectedGroup);
        }

        [ObservableProperty]
        string selectedScratchPadText;

        [ObservableProperty]
        bool allowRunningStatus;


        [ObservableProperty]
        bool isValidDestinationSelected;

        [ObservableProperty]
        bool isSendSelectedButtonEnabled;

        [ObservableProperty]
        bool isSendButtonEnabled;


        [ObservableProperty]
        bool busy;

        public ICommand SendAllCommand
        {
            get;
        }

        public ICommand SendSelectedCommand
        {
            get;
        }


        public ICommand AddSysExIdentityRequestCommand
        {
            get;
        }

        public ICommand AddBlankSysExBlockCommand
        {
            get;
        }

        public ICommand AddNoteOnCommand
        {
            get;
        }

        public ICommand AddNoteOffCommand
        {
            get;
        }

        public ICommand AddCCCommand
        {
            get;
        }


        private void UpdateSendButtonAvailability(string scratchPadtext, MidiGroupForDisplay? group)
        {
            IsSendButtonEnabled = group != null && !string.IsNullOrWhiteSpace(scratchPadtext);
        }



        private void AddNewLineIfNeeded()
        {
            if (!string.IsNullOrWhiteSpace(AllScratchPadText) && !AllScratchPadText.EndsWith('\n'))
            {
                AllScratchPadText += "\n";
            }
        }

        private bool SendMidi1TextWithValidation(string text)
        {
            if (string.IsNullOrWhiteSpace(AllScratchPadText))
            {
                _messageBoxService.ShowInfo("No message bytes to send");

                return false;
            }

            if (SelectedEndpoint == null || SelectedGroup == null)
            {
                _messageBoxService.ShowInfo("Please select an endpoint and group/port");

                return false;
            }

            Busy = true;

            var bytes = MidiMessageConverter.ConvertHexByteStringToByteArray(text);

            bool success = false;

            if (bytes == null || bytes.Count == 0)
            {
                _messageBoxService.ShowInfo("Unable to convert text to an array of MIDI 1.0 bytes");
            }
            else
            {
                var messages = MidiMessageConverter.ConvertMidi1CompleteMessageBytesToUmpWords(SelectedGroup.Group, bytes, AllowRunningStatus);

                if (messages == null || messages.Count == 0)
                {
                    success = false;
                    _messageBoxService.ShowInfo("Bytes did not convert to valid MIDI messages");
                }
                else
                {
                    // open the connection and send the messages

                    var conn = _sessionService.GetConnection(SelectedEndpoint.DeviceInformation.EndpointDeviceId);

                    if (conn != null)
                    {
                        if (conn.Open())
                        {
                            if (MidiEndpointConnection.SendMessageSucceeded(conn.SendMultipleMessagesWordList(MidiClock.TimestampConstantSendImmediately, messages)))
                            {
                                success = true;
                            }
                            else
                            {
                                _messageBoxService.ShowInfo("Unable to send MIDI messages.");
                                success = false;
                            }
                        }
                        else
                        {
                            _messageBoxService.ShowInfo("Unable to open connection.");
                            success = false;
                        }
                    }
                    else
                    {
                        _messageBoxService.ShowInfo("Endpoint not found.");
                        success = false;
                    }
                }

            }

            Busy = false;

            return success;
        }

        private void PopulateValidChannels()
        {
            for (byte i = 0; i < 16; i++)
            {
                AllValidChannels.Add(new MidiChannel(i));
            }
        }

        private void PopulateValidNotes()
        {
            for (byte i = 0; i < 128; i++)
            {
                AllValidNotes.Add(new MidiNote(i));
            }
        }

        private void PopulateValidVelocities()
        {
            for (byte i = 0; i < 128; i++)
            {
                AllValidVelocities.Add(i);
            }
        }


        private void PopulateValidControllers()
        {
            for (byte i = 0; i < 128; i++)
            {
                AllValidControllers.Add(new MidiController(i));
            }
        }

        private void PopulateValidControllerValues()
        {
            for (byte i = 0; i < 128; i++)
            {
                AllValidControllerValues.Add(i);
            }
        }


        private readonly IMidiSdkService _sdkService;
        private readonly IMessageBoxService _messageBoxService;
        private readonly IMidiEndpointEnumerationService _endpointEnumerationService;
        private readonly IMidiSessionService _sessionService;
        private readonly ISynchronizationContextService _synchronizationContextService;

        public ToolsScratchPadViewModel(
            IMidiSdkService sdkService,
            IMidiSessionService sessionService,
            IMidiEndpointEnumerationService endpointEnumerationService,
            ISynchronizationContextService synchronizationContextService,
            IMessageBoxService messageBoxService
            )
        {
            _messageBoxService = messageBoxService;
            _sessionService = sessionService;
            _sdkService = sdkService;
            _endpointEnumerationService = endpointEnumerationService;
            _synchronizationContextService = synchronizationContextService;

            AllowRunningStatus = false;
            AllScratchPadText = string.Empty;
            SelectedScratchPadText = string.Empty;

            PopulateValidChannels();
            PopulateValidNotes();
            PopulateValidVelocities();
            PopulateValidControllers();
            PopulateValidControllerValues();

            SelectedChannel = AllValidChannels[0];
            SelectedNote = AllValidNotes[60];
            SelectedVelocity = AllValidVelocities[63];

            SelectedController = AllValidControllers[0];
            SelectedControllerValue = AllValidControllerValues[0];


            AddSysExIdentityRequestCommand = new RelayCommand(
            () =>
            {
                AddNewLineIfNeeded();
                AllScratchPadText += "F0 7E 7F 06 01 F7\n";
            });

            AddBlankSysExBlockCommand = new RelayCommand(
            () =>
            {
                AddNewLineIfNeeded();
                AllScratchPadText += "F0 00 00 00 00 00 00 F7\n";
            });


            SendAllCommand = new RelayCommand(
            () =>
            {
                var success = SendMidi1TextWithValidation(AllScratchPadText);

                // TODO: Update UI to show that messages were sent
            });

            SendSelectedCommand = new RelayCommand(
            () =>
            {
                var success = SendMidi1TextWithValidation(SelectedScratchPadText);

                // TODO: Update UI to show that messages were sent
            });

            AddNoteOnCommand = new RelayCommand(
            () =>
            {
                AddNewLineIfNeeded();
                AllScratchPadText += $"9{SelectedChannel.Index.ToString("X1")} {SelectedNote.NoteIndex.ToString("X2")} {SelectedVelocity.ToString("X2")}\n";
            });

            AddNoteOffCommand = new RelayCommand(
            () =>
            {
                AddNewLineIfNeeded();
                AllScratchPadText += $"8{SelectedChannel.Index.ToString("X1")} {SelectedNote.NoteIndex.ToString("X2")} {SelectedVelocity.ToString("X2")}\n";
            });

            AddCCCommand = new RelayCommand(
            () =>
            {
                AddNewLineIfNeeded();
                AllScratchPadText += $"B{SelectedChannel.Index.ToString("X1")} {SelectedController.Index.ToString("X2")} {SelectedControllerValue.ToString("X2")}\n";
            });

            //AddProgramChangeCommand = new RelayCommand(
            //() =>
            //{
            //    AddNewLineIfNeeded();
            //    AllScratchPadText += $"C{SelectedChannel.Index.ToString("X1")} {SelectedProgram.ToString("X2")}\n";
            //});

        }


        public void RefreshDeviceCollection()
        {
            _synchronizationContextService.GetUIContext().Post(_ =>
            {
                MidiEndpoints.Clear();

                // now get all the endpoint devices and put them in groups by transport

                var endpoints = _endpointEnumerationService.GetEndpointsForPurpose(MidiEndpointDevicePurpose.NormalMessageEndpoint).OrderBy(x => x.Name);

                foreach (var endpoint in endpoints)
                {
                    MidiEndpoints.Add(endpoint);
                }

            }, null);

        }

        public void OnNavigatedTo(object parameter)
        {
            RefreshDeviceCollection();
        }

        public void OnNavigatedFrom()
        {

        }




    }
}
