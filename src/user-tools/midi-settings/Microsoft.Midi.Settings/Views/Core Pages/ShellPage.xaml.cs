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
using Microsoft.UI.Input;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Windowing;
using Windows.Graphics;
using Windows.Foundation;

using Windows.System;
using Windows.UI.Popups;

using System.Threading.Tasks;

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

        // TitleBar control removed - title is now set directly in XAML
        AppTitleBarText.Text = "AppDisplayName".GetLocalized();

        ViewModel.NavigationService.Frame = NavigationFrame;
        ViewModel.NavigationViewService.Initialize(NavigationViewControl);

        App.MainWindow.ExtendsContentIntoTitleBar = true;
        App.MainWindow.Activated += MainWindow_Activated;
        //        AppTitleBarText.Text = "AppDisplayName".GetLocalized();

        // Listen for window size changes
        App.MainWindow.SizeChanged += MainWindow_SizeChanged;
    }

    private void OnLoaded(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        //    TitleBarHelper.UpdateTitleBar(RequestedTheme);

        KeyboardAccelerators.Add(BuildKeyboardAccelerator(VirtualKey.Left, VirtualKeyModifiers.Menu));
        KeyboardAccelerators.Add(BuildKeyboardAccelerator(VirtualKey.Right, VirtualKeyModifiers.Menu));
        KeyboardAccelerators.Add(BuildKeyboardAccelerator(VirtualKey.GoBack));
        KeyboardAccelerators.Add(BuildKeyboardAccelerator(VirtualKey.GoForward));

        // Listen for mouse side buttons (back/forward) - subscribe in OnLoaded to ensure Content is loaded
        if (App.MainWindow.Content != null)
        {
            App.MainWindow.Content.PointerPressed += Content_PointerPressed;
            App.MainWindow.Content.PointerReleased += Content_PointerReleased;
        }

        // Also listen for mouse side buttons at page level
        this.PointerPressed += ShellPage_PointerPressed;

        // prime the endpoint data
        App.GetService<IMidiEndpointEnumerationService>().GetEndpoints();

        // Initialize search box visual state
        UpdateSearchBoxVisualState();

        // Set drag regions (excluding search box area)
        SetDragRegions();

        // Set focus to the Home item in navigation menu to avoid search box getting default focus
        if (NavigationViewControl.MenuItems.Count > 0)
        {
            var homeItem = NavigationViewControl.MenuItems[0] as NavigationViewItem;
            if (homeItem != null)
            {
                homeItem.Focus(FocusState.Programmatic);
            }
        }
    }


    private void MainWindow_Activated(object sender, WindowActivatedEventArgs args)
    {
        var resource = args.WindowActivationState == WindowActivationState.Deactivated ? "WindowCaptionForegroundDisabled" : "WindowCaptionForeground";

        //        AppTitleBarText.Foreground = (SolidColorBrush)App.Current.Resources[resource];
    }

    private void MainWindow_SizeChanged(object sender, WindowSizeChangedEventArgs args)
    {
        UpdateSearchBoxVisualState();
        
        // Defer drag region update to ensure layout is complete
        DispatcherQueue.TryEnqueue(Microsoft.UI.Dispatching.DispatcherQueuePriority.Low, () =>
        {
            SetDragRegions();
        });
    }

    private void SetDragRegions()
    {
        if (App.MainWindow.AppWindow?.TitleBar == null) return;

        var titleBar = App.MainWindow.AppWindow.TitleBar;
        var scale = App.MainWindow.Content?.XamlRoot?.RasterizationScale ?? 1.0;
        int windowWidth = (int)(App.MainWindow.Bounds.Width * scale);
        int titleBarHeight = (int)(48 * scale);

        // Check current mode
        bool isCompactMode = SearchBox.Visibility != Visibility.Visible;

        if (isCompactMode)
        {
            // Compact mode: search box is hidden, calculate drag regions using search icon button and menu button positions
            var searchIconButtonRect = SearchIconButton.TransformToVisual(null).TransformBounds(
                new Rect(0, 0, SearchIconButton.ActualWidth, SearchIconButton.ActualHeight));
            var menuButtonRect = MenuToggleButton.TransformToVisual(null).TransformBounds(
                new Rect(0, 0, MenuToggleButton.ActualWidth, MenuToggleButton.ActualHeight));

            int searchIconButtonLeft = (int)(searchIconButtonRect.Left * scale);
            int menuButtonRight = (int)(menuButtonRect.Right * scale);

            // Compact mode: left draggable area (icon+title), middle non-draggable area (search icon button+menu button), right draggable area
            var dragRects = new RectInt32[]
            {
                // Left area: from window left edge to search button left edge
                new RectInt32(0, 0, searchIconButtonLeft, titleBarHeight),
                // Right area: from menu button right edge to window right edge
                new RectInt32(menuButtonRight, 0, windowWidth - menuButtonRight, titleBarHeight)
            };

            titleBar.SetDragRectangles(dragRects);
        }
        else
        {
            // Normal mode: search box is visible, calculate drag regions using search box position
            var searchBoxRect = SearchBox.TransformToVisual(null).TransformBounds(
                new Rect(0, 0, SearchBox.ActualWidth, SearchBox.ActualHeight));

            // Convert to integer coordinates
            int searchLeft = (int)(searchBoxRect.Left * scale);
            int searchRight = (int)(searchBoxRect.Right * scale);

            // Set two drag rectangles: left (icon+title) and right (from search box right edge to window right edge)
            var dragRects = new RectInt32[]
            {
                // Left area: from window left edge to search box left edge
                new RectInt32(0, 0, searchLeft, titleBarHeight),
                // Right area: from search box right edge to window right edge
                new RectInt32(searchRight, 0, windowWidth - searchRight, titleBarHeight)
            };

            titleBar.SetDragRectangles(dragRects);
        }
    }

    private void UpdateSearchBoxVisualState()
    {
        double windowWidth = App.MainWindow.Bounds.Width;
        double titleWidth = AppTitleBarText.ActualWidth;
        double iconWidth = 48; // Icon column width
        double titleMargin = 16; // Title right margin 8+8
        double searchBoxMargin = 4; // Search box margin
        double indicatorAreaWidth = 150; // Right indicator area reserved width

        // Calculate total title area width (icon + title text + margins)
        double titleAreaWidth = iconWidth + titleWidth + titleMargin;

        // Calculate maximum available width for search box
        double availableWidthForSearch = windowWidth - titleAreaWidth - indicatorAreaWidth - searchBoxMargin * 2;

        // Window width threshold
        const double WindowThreshold = 1124;
        // Search box width threshold (switch to icon mode when below this value)
        const double SearchBoxCollapseThreshold = 650;

        // Enter compact mode when window width is less than or equal to threshold
        if (windowWidth <= WindowThreshold)
        {
            // Compact mode: hide indicator area, show menu icon button and search icon button
            RightTitleBarDragRegion.Visibility = Visibility.Collapsed;
            MenuToggleButton.Visibility = Visibility.Visible;
            SearchBox.Visibility = Visibility.Collapsed;
            SearchIconButton.Visibility = Visibility.Visible;
        }
        else
        {
            // Normal mode: show indicator area, hide menu icon button
            RightTitleBarDragRegion.Visibility = Visibility.Visible;
            MenuToggleButton.Visibility = Visibility.Collapsed;

            // Calculate available width for search box (including indicator area)
            availableWidthForSearch = windowWidth - titleAreaWidth - indicatorAreaWidth - searchBoxMargin * 2;

            // Determine search box mode
            if (availableWidthForSearch < SearchBoxCollapseThreshold)
            {
                // Insufficient space available: show search icon button
                SearchBox.Visibility = Visibility.Collapsed;
                SearchIconButton.Visibility = Visibility.Visible;
            }
            else
            {
                // Sufficient space available: show full search box
                SearchBox.Visibility = Visibility.Visible;
                SearchIconButton.Visibility = Visibility.Collapsed;
                SearchBox.MaxWidth = Math.Min(700, Math.Max(400, availableWidthForSearch));
            }
        }
    }

    private void MenuToggleButton_Click(object sender, RoutedEventArgs e)
    {
        // Toggle navigation menu expand/collapse state
        NavigationViewControl.IsPaneOpen = !NavigationViewControl.IsPaneOpen;
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
        bool result = false;

        // Determine whether to go back or forward based on key pressed
        if (sender.Key == VirtualKey.Left || sender.Key == VirtualKey.GoBack)
        {
            result = navigationService.GoBack();
        }
        else if (sender.Key == VirtualKey.Right || sender.Key == VirtualKey.GoForward)
        {
            result = navigationService.GoForward();
        }

        args.Handled = result;
    }
    
    private void SettingsSearchBox_QuerySubmitted(AutoSuggestBox sender, AutoSuggestBoxQuerySubmittedEventArgs args)
    {

    }


    private void SettingsSearchBox_TextChanged(AutoSuggestBox sender, AutoSuggestBoxTextChangedEventArgs args)
    {
        if (args.Reason == AutoSuggestionBoxTextChangeReason.UserInput)
        {
            var searchText = sender.Text.ToLower().Trim();

            if (string.IsNullOrWhiteSpace(searchText))
            {
                sender.ItemsSource = null;
            }
            else
            {
                var results = ViewModel.GetSearchResults(searchText);
                sender.ItemsSource = results;
            }
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

        // When search box gets focus, if there is text, show search results list
        // Search data was preloaded at app startup, no waiting needed
        if (sender is AutoSuggestBox autoSuggestBox && !string.IsNullOrWhiteSpace(autoSuggestBox.Text))
        {
            var results = ViewModel.GetSearchResults(autoSuggestBox.Text.ToLower().Trim());
            autoSuggestBox.ItemsSource = results;
        }

        System.Diagnostics.Debug.WriteLine("AutoSuggestBox_GotFocus: Exit");
    }

    private void AutoSuggestBox_LostFocus(object sender, RoutedEventArgs e)
    {
        System.Diagnostics.Debug.WriteLine("AutoSuggestBox_LostFocus: Enter");

        System.Diagnostics.Debug.WriteLine("AutoSuggestBox_LostFocus: Exit");
    }

    private void SearchIconButton_Click(object sender, RoutedEventArgs e)
    {
        // Refresh search data when search icon button is clicked
        ViewModel.RefreshSearchData();
    }

    private void ShellPage_PointerPressed(object sender, PointerRoutedEventArgs e)
    {
        HandlePointerPressed(e);
    }

    private void Content_PointerPressed(object sender, PointerRoutedEventArgs e)
    {
        HandlePointerPressed(e);
    }

    private void HandlePointerPressed(PointerRoutedEventArgs e)
    {
        var pointer = e.GetCurrentPoint(this);

        // Detect mouse side button press
        if (pointer.PointerDeviceType == PointerDeviceType.Mouse)
        {
            var properties = pointer.Properties;

            // XButton1 = Back button
            if (properties.IsXButton1Pressed)
            {
                var navigationService = App.GetService<INavigationService>();
                if (navigationService.CanGoBack)
                {
                    navigationService.GoBack();
                    e.Handled = true;
                }
            }
            // XButton2 = Forward button
            else if (properties.IsXButton2Pressed)
            {
                var navigationService = App.GetService<INavigationService>();
                if (navigationService.CanGoForward)
                {
                    navigationService.GoForward();
                    e.Handled = true;
                }
            }
        }
    }

    private void Content_PointerReleased(object sender, PointerRoutedEventArgs e)
    {
        // Mouse side buttons are handled in PointerPressed, no additional action needed here
    }

    private void KeyboardAccelerator_Invoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args)
    {

    }
}