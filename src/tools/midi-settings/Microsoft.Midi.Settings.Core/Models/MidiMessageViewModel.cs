using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection.Emit;
using System.Reflection.Metadata;
using System.Text;
using System.Threading.Channels;
using System.Threading.Tasks;
using Newtonsoft.Json.Linq;
using Windows.ApplicationModel.Preview.Notes;

namespace Microsoft.Midi.Settings.Core.Models;

// TODO: Move these to the ViewModels/Data folder and base on VM base class
// The model/data itself needs to come from the SDK. 

// this class is specifically to enable binding to the display controls in the settings app

public class MidiMessageViewModel
{
    
    
    // TODO: Also need incoming message timestamp

    public UInt32[] AllData 
    {
        get; private set;
    }

    // each field is listed by word, to make it easier to format on screen
    public List<MidiMessageWordFieldsViewModel> WordFields
    {
        get; private set;
    }


    public MidiMessageViewModel(long timestamp, UInt32[] dataWords, string sourceDeviceName, string sourceDeviceAddress = "")
    {
        AllData = new UInt32[dataWords.Length];

        Array.Copy(dataWords, AllData, dataWords.Length);

        SourceDeviceName = sourceDeviceName;
        Timestamp = timestamp;
        MessageType = MessageTypeFromFirstWord(AllData[0]);
        MessageTypeName = MidiFormattingUtility.MessageTypeNameFromMessageType(MessageType);
        SourceDeviceAddress = sourceDeviceAddress;

        // create fields
        WordFields = new List<MidiMessageWordFieldsViewModel>();

        BuildOutFields();
    }


    public uint MessageSizeInBits => (uint)(AllData.Length * 32);


    public byte MessageType
    {
        get; private set;
    }

    public string MessageInterpretation
    {
        get; private set;
    }


    // This is the MIDI device which sent this message
    public string SourceDeviceName { get; private set; }

    public string SourceDeviceAddress
    {
        get; private set;
    }

    // Internal timestamp from the service
    public long Timestamp { get; private set; }



    private void BuildOutFields()
    {
        // this could also use bitfields in C++, but we'd still need to provide descriptions and whatnot
        // May possibly change this approach later and incorporate at least the core parsing into the SDK
        // so it's done in as few places as possible and is usable by anyone.

        // note: not all fields are byte-aligned. Some fields cross message byte boundaries. At this time
        // no fields cross WORD boundaries, however, so we will format these on screen with the intent of 
        // displaying one full word per line. We'll readjust if that changes in the future. But given that
        // many embedded devices could have issues with numbers > 32 bits, it's unlikely that will change.

        // add fields for the first word
        WordFields.Add(new MidiMessageWordFieldsViewModel());


        // Message type bits 31-30 of first word
        WordFields[0].AddMessageTypeField(MessageType, FormatNibbleHex(MessageType) + " " + MessageTypeName + " (" + MessageSizeInBits + " bits)", FormatBinaryNoPrefix(MessageType, 4));

        switch (MessageType)
        {
            case 0x0:   // 32 bit utility
                AddMessageType0Fields();
                break;

            case 0x1:   // 32 bit System Real Time / Common
                AddMessageType1Fields();
                break;

            case 0x2:   // 32 bit MIDI 1.0 channel voice
                AddMessageType2Fields();
                break;

            case 0x3:   // 64 bit Data messages including SysEx
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                AddMessageType3Fields();
                break;

            case 0x4:   // 64 bit MIDI 2.0 channel voice
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                AddMessageType4Fields();
                break;

            case 0x5:   // 128 bit Data messages
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                AddMessageType5Fields();
                break;

            case 0x6:   // 32 bit undefined
                AddMessageType6Fields();
                break;

            case 0x7:   // 32 bit undefined
                AddMessageType7Fields();
                break;

            case 0x8:   // 64 bit undefined
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                AddMessageType8Fields();
                break;

            case 0x9:   // 64 bit undefined
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                AddMessageType9Fields();
                break;

            case 0xA:   // 64 bit undefined
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                AddMessageTypeAFields();
                break;

            case 0xB:   // 96 bit undefined
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                AddMessageTypeBFields();
                break;

            case 0xC:   // 96 bit undefined
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                AddMessageTypeCFields();
                break;

            case 0xD:   // 128 bit Flex Data
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                AddMessageTypeDFields();
                break;

            case 0xE:   // 128 bit undefined
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                AddMessageTypeEFields();
                break;

            case 0xF:   // 128 bit UMP Stream messages
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                WordFields.Add(new MidiMessageWordFieldsViewModel());
                AddMessageTypeFFields();
                break;

            default: 
                // just one big binary blob
                break;
        }

    }

    // Type 0: 32 bit Utility messages
    private void AddMessageType0Fields()
    {
        // message type already added

        // reserved
        var reserve01  = (byte)((AllData[0] & 0b00001111000000000000000000000000) >> 24);
        var status     = (byte)((AllData[0] & 0b00000000111100000000000000000000) >> 20);

        WordFields[0].AddReserved(reserve01, FormatBinaryNoPrefix(reserve01, 4));

        switch (status)
        {
            case 0x0: // NOOP
                {
                    MessageName = "NOOP";

                    var reserve02 = (UInt32)(AllData[0] & 0b00000000000011111111111111111111);
                    WordFields[0].AddField(status, "Status", "Specific type of Message", "NOOP", FormatBinaryNoPrefix(status, 4));
                    WordFields[0].AddReserved(reserve02, FormatBinaryNoPrefix(reserve02, 20));
                }
                break;

            case 0x1: // JR Clock
                {
                    MessageName = "JR Clock";

                    var reserve02 = (UInt32)((AllData[0] & 0b00000000000011110000000000000000) >> 16);
                    var clockTime = (UInt16)((AllData[0] & 0b00000000000000001111111111111111));
                    WordFields[0].AddField(status, "Status", "Specific type of Message", "JR Clock", FormatBinaryNoPrefix(status, 4));
                    WordFields[0].AddReserved(reserve02, FormatBinaryNoPrefix(reserve02, 16));
                    WordFields[0].AddField(clockTime, "Clock", "Clock", clockTime.ToString(), FormatBinaryNoPrefix(clockTime, 16));
                }
                break;

            case 0x2: // JR Timestamp
                {
                    MessageName = "JR Timestamp";

                    var reserve02 = (UInt32)((AllData[0] & 0b00000000000011110000000000000000) >> 16);
                    var clockStmp = (UInt16)((AllData[0] & 0b00000000000000001111111111111111));
                    WordFields[0].AddField(status, "Status", "Specific type of Message", "JR Timestamp", FormatBinaryNoPrefix(status, 4));
                    WordFields[0].AddReserved(reserve02, FormatBinaryNoPrefix(reserve02, 16));
                    WordFields[0].AddField(clockStmp, "Timestamp", "Timestamp", clockStmp.ToString(), FormatBinaryNoPrefix(clockStmp, 16));
                }
                break;

            case 0x3: // Delta Clockstamp Ticks Per Quarter Note
                {
                    MessageName = "Delta Clockstamp TPQN";

                    var reserve02 = (UInt32)((AllData[0] & 0b00000000000011110000000000000000) >> 16);
                    var ticksPQN  = (UInt16)((AllData[0] & 0b00000000000000001111111111111111));
                    WordFields[0].AddField(status, "Status", "Specific type of Message", "JR Timestamp", FormatBinaryNoPrefix(status, 4));
                    WordFields[0].AddReserved(reserve02, FormatBinaryNoPrefix(reserve02, 16));
                    WordFields[0].AddField(ticksPQN, "Ticks", "Ticks per Quarter Note", ticksPQN.ToString(), FormatBinaryNoPrefix(ticksPQN, 16));
                }
                break;

            case 0x4: // Delta Clockstamp Ticks Since Last Event
                {
                    MessageName = "Delta Clockstamp Ticks";

                    var ticksSLE  = (UInt32)((AllData[0] & 0b00000000000011111111111111111111));
                    WordFields[0].AddField(status, "Status", "Specific type of Message", "Delta Clockstamp", FormatBinaryNoPrefix(status, 4));
                    WordFields[0].AddField(ticksSLE, "Ticks", "Ticks since last event", ticksSLE.ToString(), FormatBinaryNoPrefix(ticksSLE, 20));
                }
                break;

            default:
                {
                    MessageName = "Unknown Type 0";

                    var unknown = (UInt32)((AllData[0] & 0b00000000111111111111111111111111));

                    WordFields[0].AddField(unknown, "Unknown", "Unknown", "Unknown", FormatBinaryNoPrefix(unknown, 24));
                }
                break;
        }


    }

    // Type 1: 32 bit System Real Time and System Common Messages
    private void AddMessageType1Fields()
    {
        // message type already added

        // group

        // status (opcode, channel)

        MessageName = "Unknown Type 1";


        // TEMP!
        var data = AllData[0] & 0b00001111111111111111111111111111;

        WordFields[0].AddReserved(data, FormatBinaryNoPrefix(data, 28));


    }

    // Type 2: 32 bit MIDI 1.0 Channel Voice Messages
    private void AddMessageType2Fields()
    {
        // message type already added

        var groupIndex   = (byte)((AllData[0] & 0b00001111000000000000000000000000) >> 24);
        var opcode       = (byte)((AllData[0] & 0b00000000111100000000000000000000) >> 20);
        var channelIndex = (byte)((AllData[0] & 0b00000000000011110000000000000000) >> 16);

        // side-effects, but necessary
        Opcode = opcode;
        ChannelIndex = channelIndex;
        GroupIndex = groupIndex;
        MessageName = MidiFormattingUtility.Midi1ChannelVoiceMessageNameFromOpcode(opcode);

        WordFields[0].AddGroupField(groupIndex, MidiFormattingUtility.FormatGroup(groupIndex), FormatBinaryNoPrefix(groupIndex, 4));

        switch (opcode)
        {
            case 0x8: // Note off
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var noteNumber = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);

                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000010000000) >> 7);
                    var velocity   = (byte)((AllData[0] & 0b00000000000000000000000001111111));

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi1ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(noteNumber, "Note Number", "MIDI Note Number (0-127)", "Note " + noteNumber + " : " + MidiFormattingUtility.NoteNameFromMidi1StyleNoteNumber(noteNumber), FormatBinaryNoPrefix(noteNumber, 7));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 1));
                    WordFields[0].AddField(velocity, "Velocity", "Velocity for the note", velocity.ToString(), FormatBinaryNoPrefix(velocity, 7));
                }
                break;

            case 0x9:   // note on
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var noteNumber = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);

                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000010000000) >> 7);
                    var velocity   = (byte)((AllData[0] & 0b00000000000000000000000001111111));

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi1ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(noteNumber, "Note Number", "MIDI Note Number (0-127)", "Note " + noteNumber + " : " + MidiFormattingUtility.NoteNameFromMidi1StyleNoteNumber(noteNumber), FormatBinaryNoPrefix(noteNumber, 7));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 1));
                    WordFields[0].AddField(velocity, "Velocity", "Velocity for the note", velocity.ToString(), FormatBinaryNoPrefix(velocity, 7));
                }
                break;

            case 0xA: // poly pressure
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var noteNumber = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);

                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000010000000) >> 7);
                    var data       = (byte)((AllData[0] & 0b00000000000000000000000001111111));

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi1ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(noteNumber, "Note Number", "MIDI Note Number (0-127)", "Note " + noteNumber + " : " + MidiFormattingUtility.NoteNameFromMidi1StyleNoteNumber(noteNumber), FormatBinaryNoPrefix(noteNumber, 7));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 1));
                    WordFields[0].AddField(data, "Data", "Data for the note", data.ToString(), FormatBinaryNoPrefix(data, 7));
                }
                break;

            case 0xB: // control change
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var index      = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);

                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000010000000) >> 7);
                    var data       = (byte)((AllData[0] & 0b00000000000000000000000001111111));

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi1ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(index, "Index", "Controller Number", index.ToString(), FormatBinaryNoPrefix(index, 7));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 1));
                    WordFields[0].AddField(data, "Data", "Data for the note", data.ToString(), FormatBinaryNoPrefix(data, 7));
                }
                break;

            case 0xC: // program change
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var program    = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);

                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000010000000));

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi1ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(program, "Program", "Program Number", program.ToString(), FormatBinaryNoPrefix(program, 7));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 8));
                }
                break;

            case 0xD: // channel pressure
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var data       = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);
                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000010000000));

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi1ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(data, "Data", "Pressure Data", data.ToString(), FormatBinaryNoPrefix(data, 7));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 8));
                }
                break;

            case 0xE: // pitch bend
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var lsbData    = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);

                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000010000000) >> 7);
                    var msbData    = (byte)((AllData[0] & 0b00000000000000000000000001111111));

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi1ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(lsbData, "LSB", "Pitch Bend LSB", lsbData.ToString(), FormatBinaryNoPrefix(lsbData, 7));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 1));
                    WordFields[0].AddField(msbData, "MSB", "Pitch Bend MSB", msbData.ToString(), FormatBinaryNoPrefix(msbData, 7));
                }
                break;

            default:
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001111111111111111));

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi1ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 16));
                }
                break;
        }
    }

    // Type 3: 64 bit Data Messages (including SysEx)
    private void AddMessageType3Fields()
    {
        // message type already added


        // TEMP!

        var reserved01 = AllData[0] & 0b00001111111111111111111111111111;
        var reserved02 = AllData[1];

        WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 28));
        WordFields[1].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 32));

    }

    // Type 4: 64 bit MIDI 2.0 Channel Voice Messages
    private void AddMessageType4Fields()
    {
        var groupIndex   = (byte)((AllData[0] & 0b00001111000000000000000000000000) >> 24);
        var opcode       = (byte)((AllData[0] & 0b00000000111100000000000000000000) >> 20);
        var channelIndex = (byte)((AllData[0] & 0b00000000000011110000000000000000) >> 16);

        // side-effects, but necessary
        Opcode = opcode;
        ChannelIndex = channelIndex;
        GroupIndex = groupIndex;
        MessageName = MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode);

        WordFields[0].AddGroupField(groupIndex, MidiFormattingUtility.FormatGroup(groupIndex), FormatBinaryNoPrefix(groupIndex, 4));

        switch (opcode)
        {
            case 0x0: // Registered per-note controller
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var noteNumber = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);
                    var index      = (byte)((AllData[0] & 0b00000000000000000000000011111111));
                    var data = (UInt32)(AllData[1]);

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(noteNumber, "Note Number", "MIDI Note Number (0-127)", "Note " + noteNumber + " : " + MidiFormattingUtility.NoteNameFromMidi1StyleNoteNumber(noteNumber), FormatBinaryNoPrefix(noteNumber, 7));
                    WordFields[0].AddField(index, "Index", "Index of the note", index.ToString(), FormatBinaryNoPrefix(index, 8));

                    WordFields[1].AddField(data, "Data", "Controller data", data.ToString(), FormatBinaryNoPrefix(data, 32));
                }
                break;

            case 0x1: // Assignable per-note controller
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var noteNumber = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);
                    var index      = (byte)((AllData[0] & 0b00000000000000000000000011111111));
                    var data       = (UInt32)(AllData[1]);

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(noteNumber, "Note Number", "MIDI Note Number (0-127)", "Note " + noteNumber + " : " + MidiFormattingUtility.NoteNameFromMidi1StyleNoteNumber(noteNumber), FormatBinaryNoPrefix(noteNumber, 7));
                    WordFields[0].AddField(index, "Index", "Index of the note", index.ToString(), FormatBinaryNoPrefix(index, 8));

                    WordFields[1].AddField(data, "Data", "Controller data", data.ToString(), FormatBinaryNoPrefix(data, 32));
                }
                break;

            case 0x2: // Registered Controller - RPN
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var bank       = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);
                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000010000000) >> 7);
                    var index      = (byte)((AllData[0] & 0b00000000000000000000000001111111));
                    var data       = (UInt32)(AllData[1]);

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(bank, "Bank", "Bank (RPN MSB)", bank.ToString(), FormatBinaryNoPrefix(bank, 7));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 1));
                    WordFields[0].AddField(index, "Index", "Index (RPN LSB)", index.ToString(), FormatBinaryNoPrefix(index, 7));

                    WordFields[1].AddField(data, "Data", "Parameter Data", data.ToString(), FormatBinaryNoPrefix(data, 32));
                }
                break;

            case 0x3: // Assignable Controller - NRPN
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var bank       = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);
                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000010000000) >> 7);
                    var index      = (byte)((AllData[0] & 0b00000000000000000000000001111111));
                    var data       = (UInt32)(AllData[1]);

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(bank, "Bank", "Bank (NRPN MSB)", bank.ToString(), FormatBinaryNoPrefix(bank, 7));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 1));
                    WordFields[0].AddField(index, "Index", "Index (NRPN LSB)", index.ToString(), FormatBinaryNoPrefix(index, 7));

                    WordFields[1].AddField(data, "Data", "Parameter Data", data.ToString(), FormatBinaryNoPrefix(data, 32));
                }
                break;

            case 0x4: // Relative Registered Controller (RPN)
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var bank       = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);
                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000010000000) >> 7);
                    var index      = (byte)((AllData[0] & 0b00000000000000000000000001111111));
                    var data       = (UInt32)(AllData[1]);

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(bank, "Bank", "Bank", bank.ToString(), FormatBinaryNoPrefix(bank, 7));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 1));
                    WordFields[0].AddField(index, "Index", "Index", index.ToString(), FormatBinaryNoPrefix(index, 7));

                    WordFields[1].AddField(data, "Data", "Parameter Data", data.ToString(), FormatBinaryNoPrefix(data, 32));
                }
                break;

            case 0x5: // Relative Assignable Controller (NRPN)
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var bank       = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);
                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000010000000) >> 7);
                    var index      = (byte)((AllData[0] & 0b00000000000000000000000001111111));
                    var data       = (UInt32)(AllData[1]);

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(bank, "Bank", "Bank", bank.ToString(), FormatBinaryNoPrefix(bank, 7));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 1));
                    WordFields[0].AddField(index, "Index", "Index", index.ToString(), FormatBinaryNoPrefix(index, 7));

                    WordFields[1].AddField(data, "Data", "Parameter Data", data.ToString(), FormatBinaryNoPrefix(data, 32));
                }
                break;

            case 0x6: // Per-Note Pitch Bend
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var noteNumber = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);
                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000011111111));
                    var data       = (UInt32)(AllData[1]);

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(noteNumber, "Note Number", "MIDI Note Number (0-127)", "Note " + noteNumber + " : " + MidiFormattingUtility.NoteNameFromMidi1StyleNoteNumber(noteNumber), FormatBinaryNoPrefix(noteNumber, 7));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 8));

                    WordFields[1].AddField(data, "Data", "Pitch Bend data", data.ToString(), FormatBinaryNoPrefix(data, 32));
                }
                break;

            case 0x8: // Note off
                {
                    var reserved01    = (byte)((AllData[0]   & 0b00000000000000001000000000000000) >> 15);
                    var noteNumber    = (byte)((AllData[0]   & 0b00000000000000000111111100000000) >> 8);
                    var attributeType = (byte)((AllData[0]   & 0b00000000000000000000000011111111));
                    var velocity      = (UInt16)((AllData[1] & 0b11111111111111110000000000000000) >> 16);
                    var attribute     = (UInt16)((AllData[1] & 0b00000000000000001111111111111111));

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(noteNumber, "Note Number", "MIDI Note Number (0-127)", "Note " + noteNumber + " : " + MidiFormattingUtility.NoteNameFromMidi1StyleNoteNumber(noteNumber), FormatBinaryNoPrefix(noteNumber, 7));
                    WordFields[0].AddField(attributeType, "Attribute Type", "How to interpret the attribute", attributeType.ToString(), FormatBinaryNoPrefix(attributeType, 8));

                    WordFields[1].AddField(velocity, "Velocity", "Velocity of the note", velocity.ToString(), FormatBinaryNoPrefix(velocity, 16));
                    WordFields[1].AddField(attribute, "Attribute", "Note attribute data", attribute.ToString(), FormatBinaryNoPrefix(attribute, 16));
                }
                break;

            case 0x9:   // note on
                {
                    var reserved01    = (byte)((AllData[0]   & 0b00000000000000001000000000000000) >> 15);
                    var noteNumber    = (byte)((AllData[0]   & 0b00000000000000000111111100000000) >> 8);
                    var attributeType = (byte)((AllData[0]   & 0b00000000000000000000000011111111));
                    var velocity      = (UInt16)((AllData[1] & 0b11111111111111110000000000000000) >> 16);
                    var attribute     = (UInt16)((AllData[1] & 0b00000000000000001111111111111111));

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(noteNumber, "Note Number", "MIDI Note Number (0-127)", "Note " + noteNumber + " : " + MidiFormattingUtility.NoteNameFromMidi1StyleNoteNumber(noteNumber), FormatBinaryNoPrefix(noteNumber, 7));
                    WordFields[0].AddField(attributeType, "Attribute Type", "How to interpret the attribute", attributeType.ToString(), FormatBinaryNoPrefix(attributeType, 8));

                    WordFields[1].AddField(velocity, "Velocity", "Velocity of the note", velocity.ToString(), FormatBinaryNoPrefix(velocity, 16));
                    WordFields[1].AddField(attribute, "Attribute", "Note attribute data", attribute.ToString(), FormatBinaryNoPrefix(attribute, 16));
                }
                break;

            case 0xA: // poly pressure
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var noteNumber = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);
                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000011111111));
                    var data       = (UInt32)(AllData[1]);

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(noteNumber, "Note Number", "MIDI Note Number (0-127)", "Note " + noteNumber + " : " + MidiFormattingUtility.NoteNameFromMidi1StyleNoteNumber(noteNumber), FormatBinaryNoPrefix(noteNumber, 7));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 8));

                    WordFields[1].AddField(data, "Data", "Data for poly pressure", data.ToString(), FormatBinaryNoPrefix(data, 32));
                }
                break;

            case 0xB: // control change
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var index      = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);
                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000011111111));
                    var data       = (UInt32)(AllData[1]);

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(index, "Index", "Controller Number", index.ToString(), FormatBinaryNoPrefix(index, 7));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 8));

                    WordFields[1].AddField(data, "Data", "Data for the controller", data.ToString(), FormatBinaryNoPrefix(data, 32));
                }
                break;

            case 0xC: // program change
                {
                    var reserved01    = (byte)((AllData[0] & 0b00000000000000001111111100000000) >> 8);
                    var flagsReserved = (byte)((AllData[0] & 0b00000000000000000000000011111110) >> 1);
                    var flagsB        = (byte)((AllData[0] & 0b00000000000000000000000000000001));

                    var reserved02    = (byte)((AllData[1] & 0b10000000000000000000000000000000) >> 31);
                    var program       = (byte)((AllData[1] & 0b01111111000000000000000000000000) >> 24);
                    var reserved03    = (byte)((AllData[1] & 0b00000000111111110000000000000000) >> 16);
                    var reserved04    = (byte)((AllData[1] & 0b00000000000000001000000000000000) >> 15);
                    var bankMSB       = (byte)((AllData[1] & 0b00000000000000000111111100000000) >> 8);
                    var reserved05    = (byte)((AllData[1] & 0b00000000000000000000000010000000) >> 7);
                    var bankLSB       = (byte)((AllData[1] & 0b00000000000000000000000001111111));

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 8));
                    WordFields[0].AddReserved(flagsReserved, FormatBinaryNoPrefix(flagsReserved, 7));
                    WordFields[0].AddField(flagsB, "Bank Valid", "Set if bank is part of program change", flagsB.ToString(), FormatBinaryNoPrefix(flagsB, 1));

                    WordFields[1].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 1));
                    WordFields[1].AddField(program, "Program", "Program Number", program.ToString(), FormatBinaryNoPrefix(program, 7));
                    WordFields[1].AddReserved(reserved03, FormatBinaryNoPrefix(reserved03, 8));
                    WordFields[1].AddReserved(reserved04, FormatBinaryNoPrefix(reserved04, 1));
                    WordFields[1].AddField(bankMSB, "Bank MSB", "Bank MSB", bankMSB.ToString(), FormatBinaryNoPrefix(bankMSB, 7));
                    WordFields[1].AddReserved(reserved05, FormatBinaryNoPrefix(reserved05, 1));
                    WordFields[1].AddField(bankLSB, "Bank LSB", "Bank LSB", bankLSB.ToString(), FormatBinaryNoPrefix(bankLSB, 7));
                }
                break;

            case 0xD: // channel pressure
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001111111100000000) >> 8);
                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000011111111));
                    var data       = (UInt32)(AllData[1]);

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 8));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 8));

                    WordFields[1].AddField(data, "Data", "Data for channel pressure", data.ToString(), FormatBinaryNoPrefix(data, 32));
                }
                break;

            case 0xE: // pitch bend
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001111111100000000) >> 8);
                    var reserved02 = (byte)((AllData[0] & 0b00000000000000000000000011111111));
                    var data       = (UInt32)(AllData[1]);

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 8));
                    WordFields[0].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 8));

                    WordFields[1].AddField(data, "Data", "Data for pitch bend", data.ToString(), FormatBinaryNoPrefix(data, 32));
                }
                break;

            case 0xF: // Per-note management message
                {
                    var reserved01    = (byte)((AllData[0] & 0b00000000000000001000000000000000) >> 15);
                    var noteNumber    = (byte)((AllData[0] & 0b00000000000000000111111100000000) >> 8);
                    var flagsReserved = (byte)((AllData[0] & 0b00000000000000000000000011111100) >> 2);
                    var flagsD        = (byte)((AllData[0] & 0b00000000000000000000000000000010) >> 1);
                    var flagsS        = (byte)((AllData[0] & 0b00000000000000000000000000000001));
                    var reserved02    = (UInt32)(AllData[1]);

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 1));
                    WordFields[0].AddField(noteNumber, "Note Number", "MIDI Note Number (0-127)", "Note " + noteNumber + " : " + MidiFormattingUtility.NoteNameFromMidi1StyleNoteNumber(noteNumber), FormatBinaryNoPrefix(noteNumber, 7));
                    WordFields[0].AddReserved(flagsReserved, FormatBinaryNoPrefix(flagsReserved, 6));
                    WordFields[0].AddField(flagsD, "Detach Flag", "Detach Per-Note Controllers", flagsD.ToString(), FormatBinaryNoPrefix(flagsD, 1));
                    WordFields[0].AddField(flagsS, "Reset Flag", "Set or Reset Per-Note Controllers", flagsD.ToString(), FormatBinaryNoPrefix(flagsS, 1));

                    WordFields[1].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 32));
                }
                break;

            default:
                {
                    var reserved01 = (byte)((AllData[0] & 0b00000000000000001111111111111111));
                    var reserved02 = (byte)(AllData[1]);

                    WordFields[0].AddOpcodeField(opcode, MidiFormattingUtility.Midi2ChannelVoiceMessageNameFromOpcode(opcode), FormatBinaryNoPrefix(opcode, 4));
                    WordFields[0].AddChannelField(channelIndex, MidiFormattingUtility.FormatChannel(channelIndex), FormatBinaryNoPrefix(channelIndex, 4));
                    WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 16));

                    WordFields[1].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 32));
                }
                break;

        }

    }

    // Type 5: 128 bit Data Messages
    private void AddMessageType5Fields()
    {
        // message type already added

        // TEMP!

        var reserved01 = AllData[0] & 0b00001111111111111111111111111111;
        var reserved02 = AllData[1];
        var reserved03 = AllData[2];
        var reserved04 = AllData[3];

        WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 28));
        WordFields[1].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 32));
        WordFields[2].AddReserved(reserved03, FormatBinaryNoPrefix(reserved03, 32));
        WordFields[3].AddReserved(reserved04, FormatBinaryNoPrefix(reserved04, 32));

    }

    // Type 6: 32 bit - Reserved for future use
    private void AddMessageType6Fields()
    {
        // message type already added

        var reserved01 = AllData[0] & 0b00001111111111111111111111111111;
        WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 28));
    }

    // Type 7: 32 bit - Reserved for future use
    private void AddMessageType7Fields()
    {
        // message type already added

        var reserved01 = AllData[0] & 0b00001111111111111111111111111111;
        WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 28));
    }

    // Type 8: 64 bit - Reserved for future use
    private void AddMessageType8Fields()
    {
        // message type already added

        // TEMP
        var reserved01 = AllData[0] & 0b00001111111111111111111111111111;
        var reserved02 = AllData[1];

        WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 28));
        WordFields[1].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 32));
    }

    // Type 9: 64 bit - Reserved for future use
    private void AddMessageType9Fields()
    {
        // message type already added

        // TEMP
        var reserved01 = AllData[0] & 0b00001111111111111111111111111111;
        var reserved02 = AllData[1];

        WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 28));
        WordFields[1].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 32));
    }

    // Type A: 64 bit - Reserved for future use
    private void AddMessageTypeAFields()
    {
        // message type already added

        // TEMP
        var reserved01 = AllData[0] & 0b00001111111111111111111111111111;
        var reserved02 = AllData[1];

        WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 28));
        WordFields[1].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 32));
    }

    // Type B: 96 bit - Reserved for future use
    private void AddMessageTypeBFields()
    {
        // message type already added

        // TEMP
        var reserved01 = AllData[0] & 0b00001111111111111111111111111111;
        var reserved02 = AllData[1];
        var reserved03 = AllData[2];

        WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 28));
        WordFields[1].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 32));
        WordFields[2].AddReserved(reserved03, FormatBinaryNoPrefix(reserved03, 32));
    }

    // Type C: 96 bit - Reserved for future use
    private void AddMessageTypeCFields()
    {
        // message type already added

        // TEMP
        var reserved01 = AllData[0] & 0b00001111111111111111111111111111;
        var reserved02 = AllData[1];
        var reserved03 = AllData[2];

        WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 28));
        WordFields[1].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 32));
        WordFields[2].AddReserved(reserved03, FormatBinaryNoPrefix(reserved03, 32));
    }

    // Type D: 128 bit - Flex Data Messages
    private void AddMessageTypeDFields()
    {
        // message type already added

        // TEMP
        var reserved01 = AllData[0] & 0b00001111111111111111111111111111;
        var reserved02 = AllData[1];
        var reserved03 = AllData[2];
        var reserved04 = AllData[3];

        WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 28));
        WordFields[1].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 32));
        WordFields[2].AddReserved(reserved03, FormatBinaryNoPrefix(reserved03, 32));
        WordFields[3].AddReserved(reserved04, FormatBinaryNoPrefix(reserved04, 32));
    }

    // Type E: 128 bit - Reserved for future use
    private void AddMessageTypeEFields()
    {
        // message type already added

        // TEMP
        var reserved01 = AllData[0] & 0b00001111111111111111111111111111;
        var reserved02 = AllData[1];
        var reserved03 = AllData[2];
        var reserved04 = AllData[3];

        WordFields[0].AddReserved(reserved01, FormatBinaryNoPrefix(reserved01, 28));
        WordFields[1].AddReserved(reserved02, FormatBinaryNoPrefix(reserved02, 32));
        WordFields[2].AddReserved(reserved03, FormatBinaryNoPrefix(reserved03, 32));
        WordFields[3].AddReserved(reserved04, FormatBinaryNoPrefix(reserved04, 32));
    }

    // Type F: 128 bit - UMP Stream Messages
    private void AddMessageTypeFFields()
    {
        // message type already added

        // format is two bits after message type in the first byte in the first word

        var format = (byte)((AllData[0] & 0b00001100000000000000000000000000) >> 26);
        string formatInterpretation;

        switch (format)
        {
            case 0:
                formatInterpretation = "Complete message in one UMP";
                break;
            case 1:
                formatInterpretation = "Start of a message which spans two or more UMPs";
                break;
            case 2:
                formatInterpretation = "Continuing a message which spans three or more UMP";
                break;
            case 3:
                formatInterpretation = "End of message which spans two or more UMPs";
                break;
            default:
                formatInterpretation = "Unknown";
                break;
        }

        WordFields[0].AddField(format, "Format", "Indicates if message spans more than one UMP", formatInterpretation, FormatBinaryNoPrefix(format, 2));

        // Status helps us figure out what the rest of the message means.
        // Status is the remaining 10 bits of the high 16 bits of the first word

        var status = (UInt16)((AllData[0] & 0b0000001111111111_0000000000000000) >> 16);
        var formattedStatus = FormatBinaryNoPrefix(status, 10);

        var statusFieldName = "Status";
        var statusFieldDescription = "The type of stream message";


        MessageName = MidiFormattingUtility.Midi2StreamMessageNameFromStatus(status);


        switch (status)
        {
            case 0x0:   // endpoint discovery message
                {
                    // second half of first word contains ump major and minor version
                    WordFields[0].AddField(status, statusFieldName, statusFieldDescription, "Endpoint Discovery", formattedStatus);

                    var umpVersionMajor = (byte)((AllData[0] & 0b0000000000000000_1111111100000000) >> 8);
                    var umpVersionMinor = (byte)((AllData[0] & 0b0000000000000000_0000000011111111));

                    // least significant byte of second word contains the filter bitmap
                    var filterBitmap =    (byte)((AllData[1] & 0b0000000000000000_0000000011111111));

                    var filterBitmapInterpretation = string.Empty;

                    if ((bool)((filterBitmap & 0b00000001) > 0)) filterBitmapInterpretation += "Endpoint Info Notification, ";
                    if ((bool)((filterBitmap & 0b00000010) > 0)) filterBitmapInterpretation += "Device Identity Notification, ";
                    if ((bool)((filterBitmap & 0b00000100) > 0)) filterBitmapInterpretation += "Endpoint Name Notification, ";
                    if ((bool)((filterBitmap & 0b00001000) > 0)) filterBitmapInterpretation += "Product Instance Id Notification, ";
                    if ((bool)((filterBitmap & 0b00010000) > 0)) filterBitmapInterpretation += "Stream Configuration Notification, ";

                    if (filterBitmapInterpretation == string.Empty)
                    {
                        filterBitmapInterpretation = "No flags";
                    }
                    else
                    {
                        // add a prefix, and remove the comma and space from the end, just to neaten it up
                        filterBitmapInterpretation = "Requesting: " + filterBitmapInterpretation.Substring(0, filterBitmapInterpretation.Length - 2) + ".";
                    }

                    WordFields[0].AddField(umpVersionMajor, "UMP Major", "Supported UMP version", umpVersionMajor.ToString(), FormatBinaryNoPrefix(umpVersionMajor, 8));
                    WordFields[0].AddField(umpVersionMinor, "UMP Minor", "Supported UMP version", umpVersionMinor.ToString(), FormatBinaryNoPrefix(umpVersionMinor, 8));

                    WordFields[1].AddReserved(AllData[1] >> 8, FormatBinaryNoPrefix(AllData[1] >> 8, 24));
                    WordFields[1].AddField(filterBitmap, "Filter", "The type of request being made", filterBitmapInterpretation, FormatBinaryNoPrefix(filterBitmap, 8));

                    WordFields[2].AddReserved(AllData[2], FormatBinaryNoPrefix(AllData[2], 32));
                    WordFields[3].AddReserved(AllData[3], FormatBinaryNoPrefix(AllData[3], 32));
                }
                break;

            case 0x1:   // endpoint info notification message
                {
                    WordFields[0].AddField(status, statusFieldName, statusFieldDescription, "Endpoint Info Notification", formattedStatus);

                    var umpVersionMajor = (byte)((AllData[0] & 0b0000000000000000_1111111100000000) >> 8);
                    var umpVersionMinor = (byte)((AllData[0] & 0b0000000000000000_0000000011111111));

                    WordFields[0].AddField(umpVersionMajor, "UMP Major", "Supported UMP version", umpVersionMajor.ToString(), FormatBinaryNoPrefix(umpVersionMajor, 8));
                    WordFields[0].AddField(umpVersionMinor, "UMP Minor", "Supported UMP version", umpVersionMinor.ToString(), FormatBinaryNoPrefix(umpVersionMinor, 8));

                    var staticFunctionBlocks = (byte)((MostSignificantByte(AllData[0]) & 0b10000000) >> 7);
                    var numFunctionBlocks = (byte)(MostSignificantByte(AllData[0] & 0b01111111));
                    string staticFunctionBlocksInterpretation;

                    if (staticFunctionBlocks > 0)
                    {
                        staticFunctionBlocksInterpretation = "Function Blocks are static";
                    }
                    else
                    {
                        staticFunctionBlocksInterpretation = "Function Blocks are dynamic";
                    }

                    var reserved1 = (UInt16)((AllData[1] & 0b0000000011111111_1111110000000000) >> 10);
                    var m2        = (byte)((AllData[1]   & 0b0000000000000000_0000001000000000) >> 9);
                    var m1        = (byte)((AllData[1]   & 0b0000000000000000_0000000100000000) >> 8);
                    var reserved2 = (byte)((AllData[1]   & 0b0000000000000000_0000000011111100) >> 2);
                    var rxjr      = (byte)((AllData[1]   & 0b0000000000000000_0000000000000010) >> 1); ;
                    var txjr      = (byte)(AllData[1]    & 0b0000000000000000_0000000000000001);

                    WordFields[1].AddField(staticFunctionBlocks, "Static", "Indicates Static Function Blocks", staticFunctionBlocksInterpretation, FormatBinaryNoPrefix(staticFunctionBlocks, 1));
                    WordFields[1].AddField(numFunctionBlocks, "Num FBs", "Number of function blocks for endpoint", numFunctionBlocks.ToString(), FormatBinaryNoPrefix(numFunctionBlocks, 7));
                    WordFields[1].AddReserved(reserved1, FormatBinaryNoPrefix(reserved1, 14));
                    WordFields[1].AddField(m2, "M2", "MIDI 2.0 Support", m2.ToString(), FormatBinaryNoPrefix(m2, 1));
                    WordFields[1].AddField(m1, "M1", "MIDI 1.0 Support", m1.ToString(), FormatBinaryNoPrefix(m1, 1));
                    WordFields[1].AddReserved(reserved2, FormatBinaryNoPrefix(reserved2, 6));
                    WordFields[1].AddField(rxjr, "RxJR", "Receives Jitter Reduction Timestamps", rxjr.ToString(), FormatBinaryNoPrefix(rxjr, 1));
                    WordFields[1].AddField(txjr, "RxJR", "Sends Jitter Reduction Timestamps", txjr.ToString(), FormatBinaryNoPrefix(txjr, 1));

                    WordFields[2].AddReserved(AllData[2], FormatBinaryNoPrefix(AllData[2], 32));
                    WordFields[3].AddReserved(AllData[3], FormatBinaryNoPrefix(AllData[3], 32));

                }
                break;

            case 0x2:   // Device identity notification message
                {
                    var reserve01 = (UInt16)(AllData[0] & 0b00000000000000001111111111111111);

                    WordFields[0].AddField(status, statusFieldName, statusFieldDescription, "Device Identity Notification", formattedStatus);
                    WordFields[0].AddReserved(reserve01, FormatBinaryNoPrefix(reserve01, 16));

                    var reserve02 = (byte)((AllData[1] & 0b1111111100000000_0000000000000000) >> 24);
                    var reserve03 = (byte)((AllData[1] & 0b0000000010000000_0000000000000000) >> 23);
                    var sxIdByte1 = (byte)((AllData[1] & 0b0000000001111111_0000000000000000) >> 16);
                    var reserve04 = (byte)((AllData[1] & 0b0000000000000000_1000000000000000) >> 15);
                    var sxIdByte2 = (byte)((AllData[1] & 0b0000000000000000_0111111100000000) >> 8);
                    var reserve05 = (byte)((AllData[1] & 0b0000000000000000_0000000010000000) >> 7);
                    var sxIdByte3 = (byte)((AllData[1] & 0b0000000000000000_0000000001111111));

                    WordFields[1].AddReserved(reserve02, FormatBinaryNoPrefix(reserve02, 8));
                    WordFields[1].AddReserved(reserve03, FormatBinaryNoPrefix(reserve03, 1));
                    WordFields[1].AddField(sxIdByte1, "SysEx ID 1", "Manufacturer SysEx ID Byte 1", sxIdByte1.ToString(), FormatBinaryNoPrefix(sxIdByte1, 7));
                    WordFields[1].AddReserved(reserve04, FormatBinaryNoPrefix(reserve04, 1));
                    WordFields[1].AddField(sxIdByte2, "SysEx ID 2", "Manufacturer SysEx ID Byte 1", sxIdByte2.ToString(), FormatBinaryNoPrefix(sxIdByte2, 7));
                    WordFields[1].AddReserved(reserve05, FormatBinaryNoPrefix(reserve05, 1));
                    WordFields[1].AddField(sxIdByte3, "SysEx ID 3", "Manufacturer SysEx ID Byte 1", sxIdByte3.ToString(), FormatBinaryNoPrefix(sxIdByte3, 7));


                    var reserve06 = (byte)((AllData[2] & 0b1000000000000000_0000000000000000) >> 31);
                    var devFamLsb = (byte)((AllData[2] & 0b0111111100000000_0000000000000000) >> 24);
                    var reserve07 = (byte)((AllData[2] & 0b0000000010000000_0000000000000000) >> 23);
                    var devFamMsb = (byte)((AllData[2] & 0b0000000001111111_0000000000000000) >> 16);
                    var reserve08 = (byte)((AllData[2] & 0b0000000000000000_1000000000000000) >> 15);
                    var devModLsb = (byte)((AllData[2] & 0b0000000000000000_0111111100000000) >> 8);
                    var reserve09 = (byte)((AllData[2] & 0b0000000000000000_0000000010000000) >> 7);
                    var devModMsb = (byte)((AllData[2] & 0b0000000000000000_0000000001111111));

                    WordFields[2].AddReserved(reserve06, FormatBinaryNoPrefix(reserve06, 1));
                    WordFields[2].AddField(devFamLsb, "Family LSB", "Device Family LSB", FormatByteHex(devFamLsb), FormatBinaryNoPrefix(devFamLsb, 7));
                    WordFields[2].AddReserved(reserve07, FormatBinaryNoPrefix(reserve07, 1));
                    WordFields[2].AddField(devFamMsb, "Family MSB", "Device Family MSB", FormatByteHex(devFamMsb), FormatBinaryNoPrefix(devFamMsb, 7));
                    WordFields[2].AddReserved(reserve08, FormatBinaryNoPrefix(reserve08, 1));
                    WordFields[2].AddField(devModLsb, "Model LSB", "Device Family Model LSB", FormatByteHex(devModLsb), FormatBinaryNoPrefix(devModLsb, 7));
                    WordFields[2].AddReserved(reserve09, FormatBinaryNoPrefix(reserve09, 1));
                    WordFields[2].AddField(devModMsb, "Model MSB", "Device Family Model MSB", FormatByteHex(devModMsb), FormatBinaryNoPrefix(devModMsb, 7));


                    var reserve10 = (byte)((AllData[3] & 0b1000000000000000_0000000000000000) >> 31);
                    var srevByte1 = (byte)((AllData[3] & 0b0111111100000000_0000000000000000) >> 24);
                    var reserve11 = (byte)((AllData[3] & 0b0000000010000000_0000000000000000) >> 23);
                    var srevByte2 = (byte)((AllData[3] & 0b0000000001111111_0000000000000000) >> 16);
                    var reserve12 = (byte)((AllData[3] & 0b0000000000000000_1000000000000000) >> 15);
                    var srevByte3 = (byte)((AllData[3] & 0b0000000000000000_0111111100000000) >> 8);
                    var reserve13 = (byte)((AllData[3] & 0b0000000000000000_0000000010000000) >> 7);
                    var srevByte4 = (byte)((AllData[3] & 0b0000000000000000_0000000001111111));

                    WordFields[3].AddReserved(reserve10, FormatBinaryNoPrefix(reserve10, 1));
                    WordFields[3].AddField(srevByte1, "Family LSB", "Device Family LSB", FormatByteHex(srevByte1), FormatBinaryNoPrefix(srevByte1, 7));
                    WordFields[3].AddReserved(reserve11, FormatBinaryNoPrefix(reserve11, 1));
                    WordFields[3].AddField(srevByte2, "Family MSB", "Device Family MSB", FormatByteHex(srevByte2), FormatBinaryNoPrefix(srevByte2, 7));
                    WordFields[3].AddReserved(reserve12, FormatBinaryNoPrefix(reserve12, 1));
                    WordFields[3].AddField(srevByte3, "Model LSB", "Device Family Model LSB", FormatByteHex(srevByte3), FormatBinaryNoPrefix(srevByte3, 7));
                    WordFields[3].AddReserved(reserve13, FormatBinaryNoPrefix(reserve13, 1));
                    WordFields[3].AddField(srevByte4, "Model MSB", "Device Family Model MSB", FormatByteHex(srevByte4), FormatBinaryNoPrefix(srevByte4, 7));

                }
                break;

            case 0x3:   // Endpoint name notification
                {
                    WordFields[0].AddField(status, statusFieldName, statusFieldDescription, "Endpoint Name Notification", formattedStatus);

                    var byte01 = (byte)((AllData[0] & 0b00000000_00000000_11111111_00000000) >> 8);
                    var byte02 = (byte)((AllData[0] & 0b00000000_00000000_00000000_11111111));

                    int wordIndex = 0;
                    var nameBytes = new byte[14];

                    // starts at 2 due to first data byte being the third byte in the first word
                    for (int i = 2; i < 14; i++)
                    {
                        if (i % 4 == 0)
                            wordIndex++;

                        int shift = (24 - (8 * (i % 4)));
                        UInt32 mask = (0xFF000000 >> shift);

                        nameBytes[i] = (byte)((AllData[wordIndex] & mask) >> shift);

                        WordFields[wordIndex].AddField(nameBytes[i], "Name byte " + (i - 1), "Part of the endpoint name", 
                            System.Text.Encoding.UTF8.GetString(new[] { nameBytes[i] }), FormatBinaryNoPrefix(nameBytes[i], 8));
                    }

                    MessageInterpretation = System.Text.Encoding.UTF8.GetString(nameBytes);

                    // todo: Set message info to be full string
                }
                break;

            case 0x4:   // Product instance Id notification message
                {
                    WordFields[0].AddField(status, statusFieldName, statusFieldDescription, "Product Instance Id Notification", formattedStatus);

                    // TEMP
                    var temp = (byte)((AllData[0] & 0b00000000000000001111111111111111));
                    WordFields[0].AddReserved(temp, FormatBinaryNoPrefix(temp, 16));

                    WordFields[1].AddReserved(AllData[1], FormatBinaryNoPrefix(AllData[1], 32));
                    WordFields[2].AddReserved(AllData[2], FormatBinaryNoPrefix(AllData[2], 32));
                    WordFields[3].AddReserved(AllData[3], FormatBinaryNoPrefix(AllData[3], 32));
                }
                break;

            case 0x5:   // Stream configuration request
                {
                    WordFields[0].AddField(status, statusFieldName, statusFieldDescription, "Stream Configuration Request", formattedStatus);

                    // TEMP
                    var temp = (byte)((AllData[0] & 0b00000000000000001111111111111111));
                    WordFields[0].AddReserved(temp, FormatBinaryNoPrefix(temp, 16));

                    WordFields[1].AddReserved(AllData[1], FormatBinaryNoPrefix(AllData[1], 32));
                    WordFields[2].AddReserved(AllData[2], FormatBinaryNoPrefix(AllData[2], 32));
                    WordFields[3].AddReserved(AllData[3], FormatBinaryNoPrefix(AllData[3], 32));
                }
                break;

            case 0x6:   // Stream configuration notification
                {
                    WordFields[0].AddField(status, statusFieldName, statusFieldDescription, "Stream Configuration Notification", formattedStatus);

                    // TEMP
                    var temp = (byte)((AllData[0] & 0b00000000000000001111111111111111));
                    WordFields[0].AddReserved(temp, FormatBinaryNoPrefix(temp, 16));

                    WordFields[1].AddReserved(AllData[1], FormatBinaryNoPrefix(AllData[1], 32));
                    WordFields[2].AddReserved(AllData[2], FormatBinaryNoPrefix(AllData[2], 32));
                    WordFields[3].AddReserved(AllData[3], FormatBinaryNoPrefix(AllData[3], 32));

                }
                break;

            case 0x10:  // Function block discovery message
                {
                    WordFields[0].AddField(status, statusFieldName, statusFieldDescription, "Function Block Discovery", formattedStatus);

                    // TEMP
                    var temp = (byte)((AllData[0] & 0b00000000000000001111111111111111));
                    WordFields[0].AddReserved(temp, FormatBinaryNoPrefix(temp, 16));

                    WordFields[1].AddReserved(AllData[1], FormatBinaryNoPrefix(AllData[1], 32));
                    WordFields[2].AddReserved(AllData[2], FormatBinaryNoPrefix(AllData[2], 32));
                    WordFields[3].AddReserved(AllData[3], FormatBinaryNoPrefix(AllData[3], 32));
                }
                break;

            case 0x11:  // Function block info notification
                {
                    WordFields[0].AddField(status, statusFieldName, statusFieldDescription, "Function Block Info Notification", formattedStatus);

                    // TEMP
                    var temp = (byte)((AllData[0] & 0b00000000000000001111111111111111));
                    WordFields[0].AddReserved(temp, FormatBinaryNoPrefix(temp, 16));

                    WordFields[1].AddReserved(AllData[1], FormatBinaryNoPrefix(AllData[1], 32));
                    WordFields[2].AddReserved(AllData[2], FormatBinaryNoPrefix(AllData[2], 32));
                    WordFields[3].AddReserved(AllData[3], FormatBinaryNoPrefix(AllData[3], 32));
                }
                break;

            case 0x12:  // Function block name notification
                {
                    WordFields[0].AddField(status, statusFieldName, statusFieldDescription, "Function Block Name Notification", formattedStatus);

                    // TEMP
                    var temp = (byte)((AllData[0] & 0b00000000000000001111111111111111));
                    WordFields[0].AddReserved(temp, FormatBinaryNoPrefix(temp, 16));

                    WordFields[1].AddReserved(AllData[1], FormatBinaryNoPrefix(AllData[1], 32));
                    WordFields[2].AddReserved(AllData[2], FormatBinaryNoPrefix(AllData[2], 32));
                    WordFields[3].AddReserved(AllData[3], FormatBinaryNoPrefix(AllData[3], 32));
                }
                break;

            case 0x20:  // Start of clip message
                {
                    WordFields[0].AddField(status, statusFieldName, statusFieldDescription, "Start of Clip", formattedStatus);

                    // TEMP
                    var temp = (byte)((AllData[0] & 0b00000000000000001111111111111111));
                    WordFields[0].AddReserved(temp, FormatBinaryNoPrefix(temp, 16));

                    WordFields[1].AddReserved(AllData[1], FormatBinaryNoPrefix(AllData[1], 32));
                    WordFields[2].AddReserved(AllData[2], FormatBinaryNoPrefix(AllData[2], 32));
                    WordFields[3].AddReserved(AllData[3], FormatBinaryNoPrefix(AllData[3], 32));

                }
                break;

            case 0x21:  // End of clip message
                {
                    WordFields[0].AddField(status, statusFieldName, statusFieldDescription, "End of Clip", formattedStatus);

                    // TEMP
                    var temp = (byte)((AllData[0] & 0b00000000000000001111111111111111));
                    WordFields[0].AddReserved(temp, FormatBinaryNoPrefix(temp, 16));

                    WordFields[1].AddReserved(AllData[1], FormatBinaryNoPrefix(AllData[1], 32));
                    WordFields[2].AddReserved(AllData[2], FormatBinaryNoPrefix(AllData[2], 32));
                    WordFields[3].AddReserved(AllData[3], FormatBinaryNoPrefix(AllData[3], 32));

                }
                break;

            default:    // unknown
                {
                    WordFields[0].AddField(status, statusFieldName, statusFieldDescription, "(Unknown)", formattedStatus);

                    // TEMP
                    var temp = (byte)((AllData[0] & 0b00000000000000001111111111111111));
                    WordFields[0].AddReserved(temp, FormatBinaryNoPrefix(temp, 16));

                    WordFields[1].AddReserved(AllData[1], FormatBinaryNoPrefix(AllData[1], 32));
                    WordFields[2].AddReserved(AllData[2], FormatBinaryNoPrefix(AllData[2], 32));
                    WordFields[3].AddReserved(AllData[3], FormatBinaryNoPrefix(AllData[3], 32));
                }
                break;
        }



    }

    public byte GroupIndex
    {
        get; private set;
    }

    public byte Opcode
    {
        get; private set;
    }

    public byte ChannelIndex
    {
        get; private set;
    }

    public string MessageTypeName
    {
        get; private set;
    }

    public string MessageName
    {
        get; private set;
    }


    public bool HasGroup
    {
        get
        {
            switch (MessageType)
            {
                case 0x0:   // 32 bit utility
                    return false;

                case 0x1:   // 32 bit System Real Time / Common
                    return true;

                case 0x2:   // 32 bit MIDI 1.0 channel voice
                    return true;

                case 0x3:   // 64 bit Data messages including SysEx
                    return true;

                case 0x4:   // 64 bit MIDI 2.0 channel voice
                    return true;

                case 0x5:   // 128 bit Data messages
                    return true;

                case 0xD:   // 128 bit Flex Data
                    return true;

                case 0xF:   // 128 bit UMP Stream messages
                    return false;

                default: return false;
            }
        }
    }

    public bool HasChannel
    {
        get
        {
            // TODO: Double-check these assumptions

            switch (MessageType)
            {
                case 0x0:   // 32 bit utility
                    return false;
                case 0x1:   // 32 bit System Real Time / Common
                    return true;
                case 0x2:   // 32 bit MIDI 1.0 channel voice
                    return true;
                case 0x3:   // 64 bit Data messages including SysEx
                    return true;
                case 0x4:   // 64 bit MIDI 2.0 channel voice
                    return true;
                case 0x5:   // 128 bit Data messages
                    return true;

                case 0xD:   // 128 bit Flex Data
                    return true;

                case 0xF:   // 128 bit UMP Stream messages
                    return false;

                default: return false;
            }
        }
    }

    public bool HasOpcode
    {
        get
        {
            // TODO: Double-check these assumptions

            switch (MessageType)
            {
                case 0x0:   // 32 bit utility
                    return true;
                case 0x1:   // 32 bit System Real Time / Common
                    return true;
                case 0x2:   // 32 bit MIDI 1.0 channel voice
                    return true;
                case 0x3:   // 64 bit Data messages including SysEx
                    return true;
                case 0x4:   // 64 bit MIDI 2.0 channel voice
                    return true;
                case 0x5:   // 128 bit Data messages
                    return true;

                case 0xD:   // 128 bit Flex Data
                    return true;

                case 0xF:   // 128 bit UMP Stream messages
                    return false;

                default: return false;
            }
        }
    }


    public string FormattedMessageType => FormatNibbleHex(MessageType);
    //public string FormattedGroup => FormatNibbleDecimal((byte)(Group + 1));
    //public string FormattedOpcode => FormatNibbleBinary(Opcode);
    //public string FormattedChannel => FormatNibbleDecimal((byte)(Channel + 1));
    public string FormattedTimestamp => Timestamp.ToString();






    private UInt16 UInt16FromBytes(byte msb, byte lsb)
    {
        return (UInt16)(msb << 8 | lsb);
    }

    private UInt32 UInt32FromBytes(byte msb, byte innerMsb1, byte innerLsb1, byte lsb)
    {
        return (UInt16)(msb << 24 | innerMsb1 << 16 | innerLsb1 << 8 | lsb);
    }

    // A crumb is 2 bits. Learned something new today.
    private byte MostSignificantCrumbFromNibble(byte value)
    {
        return (byte)((value >> 2) & 0x07);
    }

    private byte LeastSignificantCrumbFromNibble(byte value)
    {
        return (byte)(value & 0x07);
    }



    private byte MostSignificantByte(UInt32 word)
    {
        return (byte)((word & 0xFF000000) >> 24);
    }

    private byte MostSignificantNibble(byte value)
    {
        return (byte)((value >> 4) & 0x0F);
    }

    private byte MessageTypeFromFirstWord(UInt32 word)
    {
        return MostSignificantNibble((byte)((word & 0xFF000000) >> 24));
    }


    private byte LeastSignificantNibble(byte value)
    {
        return (byte)(value & 0x0F);
    }


    private string FormatNibbleDecimal(byte value)
    {
        return string.Format("{0:D2}", value);
    }

    private string FormatNibbleHex(byte value)
    {
        return string.Format("0x{0:X1}", value);
    }

    private string FormatNibbleBinary(byte value)
    {
        return "b" + Convert.ToString(value, 2).PadLeft(4, '0');
    }


    private string FormatByteHex(byte value)
    {
        return string.Format("0x{0:X2}", value);
    }

    private string FormatUInt16Hex(UInt16 value)
    {
        return string.Format("0x{0:X4}", value);
    }

    private string FormatUInt32Hex(UInt32 value)
    {
        return string.Format("0x{0:X8}", value);
    }


    private string FormatBinaryNoPrefix(UInt32 value, int numberOfDigits = 32)
    {
        var padded = Convert.ToString(value, 2).PadLeft(numberOfDigits, '0');

        if (padded.Length > numberOfDigits)
        {
            return padded.Substring(padded.Length - numberOfDigits, numberOfDigits);
        }
        else
        {
            return padded; 
        }


    }


}
