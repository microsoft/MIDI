using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Core.Models;

// TODO: Move these to the ViewModels/Data folder and base on VM base class
// The model/data itself needs to come from the SDK. 

public class MidiMessageWordFieldsViewModel
{
    public List<MidiMessageFieldViewModel> Fields { get; private set; } = new List<MidiMessageFieldViewModel>();

    // makes for shorter code but also generates the index and adds it
    public void AddField(UInt32 rawValue, string fieldName, string description, string interpretation, string formattedBinaryValue)
    {
        Fields.Add(new MidiMessageFieldViewModel(Fields.Count-1, fieldName, description, interpretation, formattedBinaryValue, rawValue, false));
    }

    public void AddMessageTypeField(UInt32 rawValue, string interpretation, string formattedBinaryValue)
    {
        Fields.Add(new MidiMessageFieldViewModel(Fields.Count - 1, "Message Type", "UMP Message Type", interpretation, formattedBinaryValue, rawValue, false, true, false, false, false));
    }

    public void AddGroupField(UInt32 rawValue, string interpretation, string formattedBinaryValue)
    {
        Fields.Add(new MidiMessageFieldViewModel(Fields.Count - 1, "Group", "UMP Group (1-16)", interpretation, formattedBinaryValue, rawValue, false, false, true, false, false));
    }

    public void AddOpcodeField(UInt32 rawValue, string interpretation, string formattedBinaryValue)
    {
        Fields.Add(new MidiMessageFieldViewModel(Fields.Count - 1, "Opcode", "Message opcode", interpretation, formattedBinaryValue, rawValue, false, false, false, true, false));
    }

    public void AddChannelField(UInt32 rawValue, string interpretation, string formattedBinaryValue)
    {
        Fields.Add(new MidiMessageFieldViewModel(Fields.Count - 1, "Channel", "MIDI Channel (1-16)", interpretation, formattedBinaryValue, rawValue, false, false, false, false, true));
    }

    public void AddReserved(UInt32 rawValue, string formattedBinaryValue)
    {
        string interpretation;

        if (rawValue > 0)
        {
            interpretation = "Reserved fields, unless this is an as-yet unhandled message type, should be set to all zeroes. This one is not.";
        }
        else
        {
            interpretation = "Reserved field properly set to all zeroes.";
        }

        Fields.Add(new MidiMessageFieldViewModel(Fields.Count - 1, "Reserved", "Reserved Field", interpretation, formattedBinaryValue, rawValue, true, false, false, false, false));
    }

    public MidiMessageWordFieldsViewModel()
    {
    }

}

public class MidiMessageFieldViewModel
{
    public MidiMessageFieldViewModel(int index, string name, string description, string interpretation, string formattedBinaryValue, UInt32 rawValue, bool isReserved = false, bool isMessageType = false, bool isGroup = false, bool isOpCode = false,  bool isChannel = false)
    {
        Index = index;
        Name = name;
        Description = description;
        Interpretation = interpretation;
        FormattedBinaryValue = formattedBinaryValue;
        RawValue = rawValue;

        IsReserved = isReserved;
        IsMessageType = isMessageType;
        IsGroup = isGroup;
        IsOpcode = isOpCode;
        IsChannel = isChannel;

    }

    public bool IsReserved
    {
        get; private set; 
    }

    public bool IsMessageType
    {
        get; private set;
    }

    public bool IsGroup
    {
        get; private set;
    }

    public bool IsOpcode
    {
        get; private set;
    }

    public bool IsChannel
    {
        get; private set;
    }

    public int Index
    {
        get; private set;    
    }

    // name of this field
    public string Name
    {
        get; private set;
    }

    // description of this field, typically used in a tooltip
    public string Description
    {
        get; private set; 
    }

    // what the value means. typically used in a tooltip
    public string Interpretation
    {
        get; private set; 
    }

    // binary value for display
    // these values aren't necessarily a single word. They can be multiple words
    public string FormattedBinaryValue
    {
        get; private set; 
    }

    public UInt32 RawValue
    {
        get; private set;
    }

    // TODO: May want to provide hex and decimal as well
}
