using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

using MidiSettings.Core.Models;

namespace MidiSettings.Views;

public sealed partial class EndpointDetailControl : UserControl
{
    public SampleOrder ListDetailsMenuItem
    {
        get => GetValue(ListDetailsMenuItemProperty) as SampleOrder;
        set => SetValue(ListDetailsMenuItemProperty, value);
    }

    public static readonly DependencyProperty ListDetailsMenuItemProperty = DependencyProperty.Register("ListDetailsMenuItem", typeof(SampleOrder), typeof(EndpointDetailControl), new PropertyMetadata(null, OnListDetailsMenuItemPropertyChanged));

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
