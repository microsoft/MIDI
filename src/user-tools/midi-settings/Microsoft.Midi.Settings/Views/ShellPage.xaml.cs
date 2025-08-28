// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Helpers;
using Microsoft.Midi.Settings.Services;
using Microsoft.Midi.Settings.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;

using Windows.System;
using Windows.UI.Popups;

namespace Microsoft.Midi.Settings.Views;

// TODO: Update NavigationViewItem titles and icons in ShellPage.xaml.
public sealed partial class ShellPage : Page
{
    public ShellViewModel ViewModel
    {
        get;
    }

    public ShellPage(ShellViewModel viewModel)
    {
        ViewModel = viewModel;
        InitializeComponent();

        AppTitleBarControl.Title = App.MainWindow.Title;

        ViewModel.NavigationService.Frame = NavigationFrame;
        ViewModel.NavigationViewService.Initialize(NavigationViewControl);

        // App.MainWindow.ExtendsContentIntoTitleBar = true;
        // App.MainWindow.SetTitleBar(AppTitleBar);
        App.MainWindow.Activated += MainWindow_Activated;
        //        AppTitleBarText.Text = "AppDisplayName".GetLocalized();
    }

    private void OnLoaded(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        //    TitleBarHelper.UpdateTitleBar(RequestedTheme);

        KeyboardAccelerators.Add(BuildKeyboardAccelerator(VirtualKey.Left, VirtualKeyModifiers.Menu));
        KeyboardAccelerators.Add(BuildKeyboardAccelerator(VirtualKey.GoBack));

        // prime the endpoint data
        App.GetService<IMidiEndpointEnumerationService>().GetEndpoints();

    }


    private void MainWindow_Activated(object sender, WindowActivatedEventArgs args)
    {
        var resource = args.WindowActivationState == WindowActivationState.Deactivated ? "WindowCaptionForegroundDisabled" : "WindowCaptionForeground";

        //        AppTitleBarText.Foreground = (SolidColorBrush)App.Current.Resources[resource];
    }

    private void NavigationViewControl_DisplayModeChanged(NavigationView sender, NavigationViewDisplayModeChangedEventArgs args)
    {
        //AppTitleBar.Margin = new Thickness()
        //{
        //    Left = sender.CompactPaneLength * (sender.DisplayMode == NavigationViewDisplayMode.Minimal ? 2 : 1),
        //    Top = AppTitleBar.Margin.Top,
        //    Right = AppTitleBar.Margin.Right,
        //    Bottom = AppTitleBar.Margin.Bottom
        //};
    }

    private static KeyboardAccelerator BuildKeyboardAccelerator(VirtualKey key, VirtualKeyModifiers? modifiers = null)
    {
        var keyboardAccelerator = new KeyboardAccelerator() { Key = key };

        if (modifiers.HasValue)
        {
            keyboardAccelerator.Modifiers = modifiers.Value;
        }

        keyboardAccelerator.Invoked += OnKeyboardAcceleratorInvoked;

        return keyboardAccelerator;
    }

    private static void OnKeyboardAcceleratorInvoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args)
    {
        var navigationService = App.GetService<INavigationService>();

        var result = navigationService.GoBack();

        args.Handled = result;
    }




    private void SettingsSearchBox_QuerySubmitted(AutoSuggestBox sender, AutoSuggestBoxQuerySubmittedEventArgs args)
    {

    }


    private void SettingsSearchBox_TextChanged(AutoSuggestBox sender, AutoSuggestBoxTextChangedEventArgs args)
    {
        if (args.Reason == AutoSuggestionBoxTextChangeReason.UserInput)
        {
            var results = ViewModel.GetSearchResults(sender.Text.ToLower().Trim());

            sender.ItemsSource = results;
        }
    }

    private void AutoSuggestBox_SuggestionChosen(AutoSuggestBox sender, AutoSuggestBoxSuggestionChosenEventArgs args)
    {
        var item = args.SelectedItem as MidiSettingsSearchResult;

        if (item != null)
        {
            sender.Text = item.DisplayText;

            App.GetService<INavigationService>().NavigateTo(item.DestinationKey, item.Parameter);
        }
    }

    private void AutoSuggestBox_GotFocus(object sender, RoutedEventArgs e)
    {
        System.Diagnostics.Debug.WriteLine("AutoSuggestBox_GotFocus: Enter");
        // very hacky. Should make this more deterministic and move outside of this event
        ViewModel.RefreshSearchData();

        SearchBox.Width = SearchBox.MaxWidth;

        System.Diagnostics.Debug.WriteLine("AutoSuggestBox_GotFocus: Exit");
    }

    private void AutoSuggestBox_LostFocus(object sender, RoutedEventArgs e)
    {
        System.Diagnostics.Debug.WriteLine("AutoSuggestBox_LostFocus: Enter");

        SearchBox.Width = SearchBox.MinWidth;

        System.Diagnostics.Debug.WriteLine("AutoSuggestBox_LostFocus: Exit");
    }




    private void KeyboardAccelerator_Invoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args)
    {

    }

    private void TitleBar_BackButtonClick(object sender, RoutedEventArgs e)
    {
        var navigationService = App.GetService<INavigationService>();

        navigationService.GoBack();
    }
}