using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

using MidiSettings.Core.Models;

namespace MidiSettings.Views;

public sealed partial class DeviceDetailControl : UserControl
{
    public SampleMidiDevice ListDetailsMenuItem
    {
        get => GetValue(ListDetailsMenuItemProperty) as SampleMidiDevice;
        set => SetValue(ListDetailsMenuItemProperty, value);
    }

    public static readonly DependencyProperty ListDetailsMenuItemProperty = 
        DependencyProperty.Register("ListDetailsMenuItem", typeof(SampleMidiDevice), typeof(DeviceDetailControl), new PropertyMetadata(null, OnListDetailsMenuItemPropertyChanged));

    public DeviceDetailControl()
    {
        InitializeComponent();
    }

    private static void OnListDetailsMenuItemPropertyChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
    {
        var control = d as DeviceDetailControl;
        control.ForegroundElement.ChangeView(0, 0, 1);
    }
}
