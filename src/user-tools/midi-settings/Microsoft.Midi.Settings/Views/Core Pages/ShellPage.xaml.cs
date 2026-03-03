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

        // 监听窗口大小变化
        App.MainWindow.SizeChanged += MainWindow_SizeChanged;
    }

    private void OnLoaded(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        //    TitleBarHelper.UpdateTitleBar(RequestedTheme);

        KeyboardAccelerators.Add(BuildKeyboardAccelerator(VirtualKey.Left, VirtualKeyModifiers.Menu));
        KeyboardAccelerators.Add(BuildKeyboardAccelerator(VirtualKey.GoBack));

        // prime the endpoint data
        App.GetService<IMidiEndpointEnumerationService>().GetEndpoints();

        // 初始化搜索框视觉状态
        UpdateSearchBoxVisualState();

        // 设置拖拽区域（排除搜索框区域）
        SetDragRegions();

        // 将焦点设置到导航菜单的 Home 项，避免搜索框默认获得焦点
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
        
        // 延迟更新拖拽区域，确保布局已完成
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

        // 获取搜索框实际区域在窗口中的位置（使用 SearchBox 而不是 SearchBoxContainer）
        var searchBoxRect = SearchBox.TransformToVisual(null).TransformBounds(
            new Rect(0, 0, SearchBox.ActualWidth, SearchBox.ActualHeight));

        // 转换为整数坐标
        int searchLeft = (int)(searchBoxRect.Left * scale);
        int searchRight = (int)(searchBoxRect.Right * scale);
        int windowWidth = (int)(App.MainWindow.Bounds.Width * scale);

        // 设置两个拖拽矩形：左侧（图标+标题）和右侧（搜索框右边缘到窗口右边缘）
        var dragRects = new RectInt32[]
        {
            // 左侧区域：从窗口左边缘到搜索框左边缘
            new RectInt32(0, 0, searchLeft, (int)(48 * scale)),
            // 右侧区域：从搜索框右边缘到窗口右边缘
            new RectInt32(searchRight, 0, windowWidth - searchRight, (int)(48 * scale))
        };

        titleBar.SetDragRectangles(dragRects);
    }

    private void UpdateSearchBoxVisualState()
    {
        double windowWidth = App.MainWindow.Bounds.Width;
        double titleWidth = AppTitleBarText.ActualWidth;
        double iconWidth = 48; // 图标列宽度
        double titleMargin = 16; // 标题右边距 8+8
        double searchBoxMargin = 4; // 搜索框边距
        double indicatorAreaWidth = 150; // 右侧指示器区域预留宽度

        // 计算标题区域总宽度（图标 + 标题文字 + 边距）
        double titleAreaWidth = iconWidth + titleWidth + titleMargin;

        // 计算搜索框可用的最大宽度
        double availableWidthForSearch = windowWidth - titleAreaWidth - indicatorAreaWidth - searchBoxMargin * 2;

        // 窗口宽度阈值
        const double WindowThreshold = 1124;
        // 搜索框宽度阈值（小于此值时切换为图标模式）
        const double SearchBoxCollapseThreshold = 650;

        // 当窗口宽度小于等于阈值时，进入紧凑模式
        if (windowWidth <= WindowThreshold)
        {
            // 紧凑模式：隐藏指示器区域，显示菜单图标按钮和搜索图标按钮
            RightTitleBarDragRegion.Visibility = Visibility.Collapsed;
            MenuToggleButton.Visibility = Visibility.Visible;
            SearchBox.Visibility = Visibility.Collapsed;
            SearchIconButton.Visibility = Visibility.Visible;
        }
        else
        {
            // 正常模式：显示指示器区域，隐藏菜单图标按钮
            RightTitleBarDragRegion.Visibility = Visibility.Visible;
            MenuToggleButton.Visibility = Visibility.Collapsed;

            // 计算搜索框可用宽度（含指示器区域）
            availableWidthForSearch = windowWidth - titleAreaWidth - indicatorAreaWidth - searchBoxMargin * 2;

            // 判断搜索框模式
            if (availableWidthForSearch < SearchBoxCollapseThreshold)
            {
                // 可用空间不足：显示搜索图标按钮
                SearchBox.Visibility = Visibility.Collapsed;
                SearchIconButton.Visibility = Visibility.Visible;
            }
            else
            {
                // 可用空间充足：显示完整搜索框
                SearchBox.Visibility = Visibility.Visible;
                SearchIconButton.Visibility = Visibility.Collapsed;
                SearchBox.MaxWidth = Math.Min(700, Math.Max(400, availableWidthForSearch));
            }
        }
    }

    private void MenuToggleButton_Click(object sender, RoutedEventArgs e)
    {
        // 切换导航菜单的展开/折叠状态
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

        // 当搜索框获得焦点时，如果有文字，显示搜索列表
        // 搜索数据已在应用启动时预加载，无需等待
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
        // 当点击搜索图标按钮时，刷新搜索数据
        ViewModel.RefreshSearchData();
    }




    private void KeyboardAccelerator_Invoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args)
    {

    }
}