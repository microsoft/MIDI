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
    public void AddField(string fieldName, string description, string interpretation, string formattedBinaryValue)
    {
        Fields.Add(new MidiMessageFieldViewModel(Fields.Count-1, fieldName, description, interpretation, formattedBinaryValue, false));
    }

    public void AddMessageTypeField(string interpretation, string formattedBinaryValue)
    {
        Fields.Add(new MidiMessageFieldViewModel(Fields.Count - 1, "Message Type", "UMP Message Type", interpretation, formattedBinaryValue, false, true, false, false, false));
    }

    public void AddGroupField(string interpretation, string formattedBinaryValue)
    {
        Fields.Add(new MidiMessageFieldViewModel(Fields.Count - 1, "Group", "UMP Group (1-16)", interpretation, formattedBinaryValue, false, false, true, false, false));
    }

    public void AddOpcodeField(string interpretation, string formattedBinaryValue)
    {
        Fields.Add(new MidiMessageFieldViewModel(Fields.Count - 1, "Opcode", "Message opcode", interpretation, formattedBinaryValue, false, false, false, true, false));
    }

    public void AddChannelField(string interpretation, string formattedBinaryValue)
    {
        Fields.Add(new MidiMessageFieldViewModel(Fields.Count - 1, "Channel", "MIDI Channel (1-16)", interpretation, formattedBinaryValue, false, false, false, false, true));
    }

    public void AddReserved(string formattedBinaryValue)
    {
        Fields.Add(new MidiMessageFieldViewModel(Fields.Count - 1, "Reserved", "Reserved", "Unused space", formattedBinaryValue, true, false, false, false, false));
    }

    public MidiMessageWordFieldsViewModel()
    {
    }

}

public class MidiMessageFieldViewModel
{
    public MidiMessageFieldViewModel(int index, string name, string description, string interpretation, string formattedBinaryValue, bool isReserved = false, bool isMessageType = false, bool isGroup = false, bool isOpCode = false,  bool isChannel = false)
    {
        Index = index;
        Name = name;
        Description = description;
        Interpretation = interpretation;
        FormattedBinaryValue = formattedBinaryValue;
        
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

    // TODO: May want to provide hex and decimal as well
}
