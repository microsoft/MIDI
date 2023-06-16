using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Midi.Settings.Core.Models;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Microsoft.Midi.Settings.Helpers;
public class MidiMessageFieldDataTemplateSelector : DataTemplateSelector

{
    public DataTemplate DataNormal
    {
        get; set;
    }
    public DataTemplate DataAlternate
    {
        get; set;
    }

    public DataTemplate MessageType
    {
        get; set;
    }
    public DataTemplate Opcode
    {
        get; set;
    }

    public DataTemplate Channel
    {
        get; set;
    }

    public DataTemplate Group
    {
        get; set;
    }

    public DataTemplate Reserved
    {
        get; set;
    }

    protected override DataTemplate SelectTemplateCore(object item)
    {
        var vm = item as MidiMessageFieldViewModel;

        if (vm != null)
        {
            if (vm.IsReserved) return Reserved;
            if (vm.IsMessageType) return MessageType; ;
            if (vm.IsGroup) return Group;
            if (vm.IsOpcode) return Opcode;
            if (vm.IsChannel) return Channel;

            if (vm.Index % 2 == 0)
            {
                return DataNormal;
            }
            else
            {
                return DataAlternate;
            }
        }
        else
        {
            return DataNormal;
        }
    }

}
