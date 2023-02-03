using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

using MidiSettings.Core.Models;

namespace MidiSettings.Views;

public sealed partial class EndpointDetailControl : UserControl
{
    public SampleMidiEndpoint ListDetailsMenuItem
    {
        get => GetValue(ListDetailsMenuItemProperty) as SampleMidiEndpoint;
        set => SetValue(ListDetailsMenuItemProperty, value);
    }

    public static readonly DependencyProperty ListDetailsMenuItemProperty = 
        DependencyProperty.Register("ListDetailsMenuItem", typeof(SampleMidiEndpoint), typeof(EndpointDetailControl), new PropertyMetadata(null, OnListDetailsMenuItemPropertyChanged));

    public EndpointDetailControl()
    {
        InitializeComponent();
    }

    private static void OnListDetailsMenuItemPropertyChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
    {
        var control = d as EndpointDetailControl;
        control.ForegroundElement.ChangeView(0, 0, 1);
    }
}
