using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Midi.Settings.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Documents;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Xml.Linq;
using Windows.Foundation;
using Windows.Foundation.Collections;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Microsoft.Midi.Settings.Controls
{
    // this contains everything including function block names etc.
    public class MidiGroupForDisplay
    {
        public MidiGroup Group { get; internal set; }

        public List<string> BlockNames = [];

        public MidiGroupForDisplay (MidiGroup group)
        {
            Group = group;
        }

        public override string ToString()
        {
            if (BlockNames.Count == 1)
            {
                return MidiGroup.LongLabel + " " + (uint)Group.DisplayValue + " (" + BlockNames[0] + ")";
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

                name = MidiGroup.LongLabelPlural + " " + (uint)Group.DisplayValue + " (" + name + ")";

                return name;
            }
            else
            {
                return MidiGroup.LongLabel + " " + (uint)Group.DisplayValue;
            }
        }
    }

    [ObservableObject]
    public sealed partial class MidiEndpointAndGroupPickerControl : UserControl
    {
        [ObservableProperty]
        private ObservableCollection<MidiEndpointWrapper> endpoints = [];


        public static readonly DependencyProperty SelectedEndpointProperty = DependencyProperty.Register(
            "SelectedEndpoint",
            typeof(MidiEndpointWrapper),
            typeof(MidiEndpointAndGroupPickerControl),
            new PropertyMetadata(null, new PropertyChangedCallback(OnSelectedEndpointChanged))
            );

        public MidiEndpointWrapper? SelectedEndpoint
        {
            get { return (MidiEndpointWrapper)GetValue(SelectedEndpointProperty); }
            set { SetValue(SelectedEndpointProperty, value); }
        }
        private static void OnSelectedEndpointChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            var control = (MidiEndpointAndGroupPickerControl)d;

            control.RebuildGroupList();
        }



        public static readonly DependencyProperty SelectedGroupProperty = DependencyProperty.Register(
            "SelectedGroup",
            typeof(MidiGroupForDisplay),
            typeof(MidiEndpointAndGroupPickerControl),
            new PropertyMetadata(null, new PropertyChangedCallback(OnSelectedGroupChanged))
            );
        public MidiGroupForDisplay? SelectedGroup
        {
            get { return (MidiGroupForDisplay)GetValue(SelectedGroupProperty); }
            set { SetValue(SelectedGroupProperty, value); }
        }
        private static void OnSelectedGroupChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {

            
        }

        [ObservableProperty]
        private bool showMessageDestinationGroups;

        [ObservableProperty]
        private bool showMessageSourceGroups;

        public ObservableCollection<MidiGroupForDisplay> Groups { get; } = [];

        public MidiEndpointAndGroupPickerControl()
        {
            showMessageDestinationGroups = true;
            showMessageSourceGroups = true;

            this.InitializeComponent();
        }

        public void RebuildGroupList()
        {
            if (SelectedEndpoint == null)
            {
                Groups.Clear();
                return;
            }

            Dictionary<uint, MidiGroupForDisplay> groupsForDisplay = [];

            var functionBlocks = SelectedEndpoint.DeviceInformation.GetDeclaredFunctionBlocks();

            // do we have function blocks? If so, use them
            if (functionBlocks != null && functionBlocks.Count > 0)
            {
                // todo: filter by direction
            }
            else
            {
                var groupBlocks = SelectedEndpoint.DeviceInformation.GetGroupTerminalBlocks();

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
                                    var group = new MidiGroup((byte)(groupNumber - 1));

                                    groupsForDisplay[groupNumber] = new MidiGroupForDisplay(group);

                                    if (block.Name != string.Empty)
                                    {
                                        groupsForDisplay[groupNumber].BlockNames.Add(block.Name);
                                    }
                                }

                            }
                        }
                    }
                }
                else
                {
                    // if no function blocks or group terminal blocks, show all 16 in and 16 out

                    for (byte groupIndex = 0; groupIndex < 16; groupIndex++)
                    {
                        var group = new MidiGroup(groupIndex);

                        groupsForDisplay[group.DisplayValue] = new MidiGroupForDisplay(group);
                        
                        if (ShowMessageDestinationGroups)
                        {
                            groupsForDisplay[group.DisplayValue].BlockNames.Add("Output");
                        }

                        if (ShowMessageSourceGroups)
                        {
                            groupsForDisplay[group.DisplayValue].BlockNames.Add("Input");
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
