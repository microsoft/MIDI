using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using System.Xml.Linq;
using Microsoft.UI.Xaml.Documents;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Microsoft.Midi.Settings.Controls
{
    // this contains everything including function block names etc.
    public class MidiGroupForDisplay
    {
        public uint GroupNumberForDisplay = 0;
        public List<string> BlockNames = [];

        public override string ToString()
        {
            if (GroupNumberForDisplay == 0)
            {
                return "Invalid Group";
            }

            if (BlockNames.Count == 1)
            {
                return MidiGroup.LongLabel + " " + GroupNumberForDisplay + " (" + BlockNames[0] + ")";
            }
            else if (BlockNames.Count > 1)
            {
                string name = string.Empty;

                foreach (string blockName in BlockNames)
                {
                    if (name != string.Empty)
                    {
                        name += ", ";
                    }

                    name += blockName;
                }

                name = MidiGroup.LongLabelPlural + " " + GroupNumberForDisplay + " (" + name + ")";

                return name;
            }
            else
            {
                return MidiGroup.LongLabel + " " + GroupNumberForDisplay;
            }
        }
    }

    [ObservableObject]
    public sealed partial class MidiEndpointAndGroupPickerControl : UserControl
    {
        [ObservableProperty]
        public ObservableCollection<MidiEndpointDeviceInformation>? endpoints;

        private MidiEndpointDeviceInformation? _selectedEndpoint = null;
        public MidiEndpointDeviceInformation? SelectedEndpoint
        {
            get { return _selectedEndpoint; }
            set
            {
                SetProperty(ref _selectedEndpoint, value);

                RebuildGroupList();
            }
        }

        [ObservableProperty]
        public MidiGroupForDisplay? selectedGroup;

        [ObservableProperty]
        public bool showMessageDestinationGroups;

        [ObservableProperty]
        public bool showMessageSourceGroups;

        public ObservableCollection<MidiGroupForDisplay> Groups { get; } = [];

        public MidiEndpointAndGroupPickerControl()
        {
            showMessageDestinationGroups = true;
            showMessageSourceGroups = true;

            this.InitializeComponent();
        }

        private void RebuildGroupList()
        {
            if (_selectedEndpoint == null)
            {
                Groups.Clear();
                return;
            }

            Dictionary<uint, MidiGroupForDisplay> groupsForDisplay = [];

            var functionBlocks = _selectedEndpoint.GetDeclaredFunctionBlocks();

            // do we have function blocks? If so, use them
            if (functionBlocks != null && functionBlocks.Count > 0)
            {
                // todo: filter by direction
            }
            else
            {
                var groupBlocks = _selectedEndpoint.GetGroupTerminalBlocks();

                // no function blocks. Do we have group terminal blocks?
                if (groupBlocks != null && groupBlocks.Count > 0)
                {
                    // todo: filter by direction, and also need to handle bidi

                    foreach (var block in groupBlocks)
                    {
                        if ((block.Direction == MidiGroupTerminalBlockDirection.Bidirectional) ||
                            (block.Direction == MidiGroupTerminalBlockDirection.BlockInput && ShowMessageDestinationGroups) ||
                            (block.Direction == MidiGroupTerminalBlockDirection.BlockOutput && ShowMessageSourceGroups))
                        {
                            for (var groupNumber = block.FirstGroup.DisplayValue; groupNumber < block.FirstGroup.DisplayValue + block.GroupCount; groupNumber++)
                            {
                                if (!groupsForDisplay.ContainsKey(groupNumber))
                                {
                                    groupsForDisplay[groupNumber] = new MidiGroupForDisplay();
                                    groupsForDisplay[groupNumber].GroupNumberForDisplay = groupNumber;
                                    groupsForDisplay[groupNumber].BlockNames.Add(block.Name);
                                }

                            }
                        }
                    }
                }
                else
                {
                    // if no function blocks or group terminal blocks, show all 16 in and 16 out

                    for (uint groupNumber = 1; groupNumber <= 16; groupNumber++)
                    {
                        groupsForDisplay[groupNumber] = new MidiGroupForDisplay();
                        
                        if (ShowMessageDestinationGroups)
                        {
                            groupsForDisplay[groupNumber].BlockNames.Add("Output");
                        }

                        if (ShowMessageSourceGroups)
                        {
                            groupsForDisplay[groupNumber].BlockNames.Add("Input");
                        }
                    }
                }
            }


            Groups.Clear();
            foreach (var display in groupsForDisplay.Values)
            {
                Groups.Add(display);
            }

            if (Groups.Count > 0)
            {
                SelectedGroup = Groups[0];
            }

        }


    }
}
