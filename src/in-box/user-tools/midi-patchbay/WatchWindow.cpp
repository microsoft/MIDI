// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "WatchWindow.h"

#include "AppSettings.h"
#include "PatchModel.h"
#include "RouteEngine.h"
#include "StringResources.h"
#include "UmpDescriber.h"
#include "resource.h"

#include <midi_timestamp.h>

namespace internal = ::WindowsMidiServicesInternal;
namespace numberformatting = ::winrt::Windows::Globalization::NumberFormatting;
namespace datatransfer = ::winrt::Windows::ApplicationModel::DataTransfer;

namespace midipatchbay
{
    namespace
    {
        constexpr double DefaultWidth = 560.0;
        constexpr double DefaultHeight = 420.0;

        // Each new window sits a little further down and left, so they don't hide each other.
        constexpr double CascadeStep = 28.0;

        constexpr double WentToColumnWidth = 150.0;

        // What one look takes from the buffer at a time, and at most per look.
        constexpr size_t DrainBatch = 512;
        constexpr size_t MostPerTick = 8192;

        constexpr std::chrono::milliseconds TickInterval{ 50 };

        // The same colors as the Windows MIDI Monitor's message name chiclets, by
        // midiapp::UmpMessageTypeColorIndex. They read on light and dark alike.
        constexpr std::array<winrt::Windows::UI::Color, 8> ChicletColors
        {
            winrt::Windows::UI::Color{ 0xFF, 0x6E, 0x76, 0x81 },
            winrt::Windows::UI::Color{ 0xFF, 0xB4, 0x69, 0x1A },
            winrt::Windows::UI::Color{ 0xFF, 0x2A, 0x6B, 0xB8 },
            winrt::Windows::UI::Color{ 0xFF, 0x6B, 0x4B, 0xB5 },
            winrt::Windows::UI::Color{ 0xFF, 0x2E, 0x8B, 0x57 },
            winrt::Windows::UI::Color{ 0xFF, 0x1F, 0x8A, 0x8A },
            winrt::Windows::UI::Color{ 0xFF, 0xA4, 0x3E, 0x85 },
            winrt::Windows::UI::Color{ 0xFF, 0x5A, 0x64, 0x72 },
        };

        // Each window's number and its connection's dot share one of these.
        constexpr std::array<winrt::Windows::UI::Color, 6> WindowColors
        {
            winrt::Windows::UI::Color{ 0xFF, 0x4C, 0xC2, 0xFF },
            winrt::Windows::UI::Color{ 0xFF, 0xFF, 0xB4, 0x54 },
            winrt::Windows::UI::Color{ 0xFF, 0x7F, 0xD6, 0x7A },
            winrt::Windows::UI::Color{ 0xFF, 0xE0, 0x8A, 0xD8 },
            winrt::Windows::UI::Color{ 0xFF, 0xFF, 0x8A, 0x80 },
            winrt::Windows::UI::Color{ 0xFF, 0xB4, 0xA7, 0xFF },
        };

        // The window's layout. It holds no words: every word shown is set from the resources.
        constexpr wchar_t LayoutXaml[] = LR"xaml(
<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <Grid.RowDefinitions>
    <RowDefinition Height="32" />
    <RowDefinition Height="Auto" />
    <RowDefinition Height="Auto" />
    <RowDefinition Height="Auto" />
    <RowDefinition Height="*" />
  </Grid.RowDefinitions>

  <Grid x:Name="Fill" Grid.RowSpan="5" />
  <Border x:Name="Tint" Grid.RowSpan="5" Visibility="Collapsed" IsHitTestVisible="False" />

  <Border x:Name="ValueBrushSource" Width="0" Height="0" Background="{ThemeResource AccentTextFillColorPrimaryBrush}" />
  <Border x:Name="SuccessBrushSource" Width="0" Height="0" Background="{ThemeResource SystemFillColorSuccessBrush}" />

  <Grid x:Name="TitleBar" Grid.Row="0" Height="32" Background="Transparent">
    <Grid.ColumnDefinitions>
      <ColumnDefinition x:Name="LeftInset" Width="0" />
      <ColumnDefinition Width="Auto" />
      <ColumnDefinition Width="*" />
      <ColumnDefinition Width="Auto" />
      <ColumnDefinition x:Name="RightInset" Width="0" />
    </Grid.ColumnDefinitions>
    <Grid Grid.Column="1" Width="18" Height="18" Margin="12,0,8,0" VerticalAlignment="Center" IsHitTestVisible="False">
      <Ellipse x:Name="TagDot" />
      <TextBlock x:Name="TagText" FontSize="11" FontWeight="Bold" Foreground="Black"
                 HorizontalAlignment="Center" VerticalAlignment="Center" />
    </Grid>
    <TextBlock x:Name="Caption" Grid.Column="2" VerticalAlignment="Center" TextTrimming="CharacterEllipsis"
               IsHitTestVisible="False" Style="{ThemeResource CaptionTextBlockStyle}" />
    <ToggleButton x:Name="PinButton" Grid.Column="3" Width="40" Height="32" Padding="0" BorderThickness="0"
                  Background="Transparent" IsChecked="True" VerticalAlignment="Top">
      <FontIcon Glyph="&#xE718;" FontSize="12" />
    </ToggleButton>
  </Grid>

  <Border Grid.Row="1" Margin="10,0,10,8" Padding="10,7" CornerRadius="6" BorderThickness="1"
          Background="{ThemeResource CardBackgroundFillColorDefaultBrush}"
          BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}">
    <Grid ColumnSpacing="8">
      <Grid.ColumnDefinitions>
        <ColumnDefinition Width="Auto" />
        <ColumnDefinition Width="Auto" MaxWidth="220" />
        <ColumnDefinition Width="Auto" />
        <ColumnDefinition Width="Auto" />
        <ColumnDefinition Width="*" />
        <ColumnDefinition Width="Auto" />
      </Grid.ColumnDefinitions>
      <TextBlock x:Name="FromLabel" Grid.Column="0" FontSize="12" VerticalAlignment="Center"
                 Foreground="{ThemeResource TextFillColorTertiaryBrush}" />
      <TextBlock x:Name="FromText" Grid.Column="1" FontSize="12" VerticalAlignment="Center" TextTrimming="CharacterEllipsis" />
      <FontIcon x:Name="Arrow" Grid.Column="2" Glyph="&#xE72A;" FontSize="11" VerticalAlignment="Center"
                Foreground="{ThemeResource TextFillColorSecondaryBrush}" />
      <TextBlock x:Name="ToLabel" Grid.Column="3" FontSize="12" VerticalAlignment="Center"
                 Foreground="{ThemeResource TextFillColorTertiaryBrush}" />
      <TextBlock x:Name="ToText" Grid.Column="4" FontSize="12" VerticalAlignment="Center" TextTrimming="CharacterEllipsis" />
      <StackPanel Grid.Column="5" Orientation="Horizontal" Spacing="6" VerticalAlignment="Center">
        <Ellipse x:Name="StateDot" Width="7" Height="7" VerticalAlignment="Center" />
        <TextBlock x:Name="StateText" FontSize="12" Foreground="{ThemeResource TextFillColorSecondaryBrush}" />
      </StackPanel>
    </Grid>
  </Border>

  <InfoBar x:Name="RemovedBar" Grid.Row="2" Margin="10,0,10,8" Severity="Warning" IsClosable="False" IsOpen="False" />

  <Grid x:Name="Toolbar" Grid.Row="3" Margin="10,0,10,8" ColumnSpacing="4">
    <Grid.ColumnDefinitions>
      <ColumnDefinition Width="Auto" />
      <ColumnDefinition Width="Auto" />
      <ColumnDefinition Width="Auto" />
      <ColumnDefinition Width="Auto" />
      <ColumnDefinition Width="*" />
    </Grid.ColumnDefinitions>
    <ToggleButton x:Name="PauseButton" Grid.Column="0" Width="34" Height="30" Padding="0" BorderThickness="0" Background="Transparent">
      <FontIcon x:Name="PauseIcon" Glyph="&#xE769;" FontSize="14" />
    </ToggleButton>
    <Button x:Name="ClearButton" Grid.Column="1" Width="34" Height="30" Padding="0" BorderThickness="0" Background="Transparent">
      <FontIcon Glyph="&#xE74D;" FontSize="14" />
    </Button>
    <ToggleButton x:Name="ClockButton" Grid.Column="2" Width="34" Height="30" Padding="0" BorderThickness="0" Background="Transparent">
      <FontIcon Glyph="&#xE823;" FontSize="14" />
    </ToggleButton>
    <ToggleButton x:Name="SensingButton" Grid.Column="3" Width="34" Height="30" Padding="0" BorderThickness="0" Background="Transparent">
      <FontIcon Glyph="&#xEA8F;" FontSize="14" />
    </ToggleButton>
    <TextBlock x:Name="CountText" Grid.Column="4" FontSize="12" HorizontalAlignment="Right" VerticalAlignment="Center"
               Foreground="{ThemeResource TextFillColorSecondaryBrush}" />
  </Grid>

  <Border x:Name="Table" Grid.Row="4" Margin="10,0,10,10" CornerRadius="6" BorderThickness="1"
          Background="{ThemeResource CardBackgroundFillColorDefaultBrush}"
          BorderBrush="{ThemeResource CardStrokeColorDefaultBrush}">
    <Grid>
      <Grid.RowDefinitions>
        <RowDefinition Height="Auto" />
        <RowDefinition Height="*" />
      </Grid.RowDefinitions>
      <Grid x:Name="Header" Padding="10,8,10,6" ColumnSpacing="10">
        <Grid.ColumnDefinitions>
          <ColumnDefinition Width="96" />
          <ColumnDefinition Width="0" />
          <ColumnDefinition Width="200" />
          <ColumnDefinition Width="*" />
        </Grid.ColumnDefinitions>
        <TextBlock x:Name="HeaderTime" Grid.Column="0" FontSize="12" FontWeight="SemiBold" />
        <TextBlock x:Name="HeaderWentTo" Grid.Column="1" FontSize="12" FontWeight="SemiBold" />
        <TextBlock x:Name="HeaderMessage" Grid.Column="2" FontSize="12" FontWeight="SemiBold" />
        <TextBlock x:Name="HeaderDetails" Grid.Column="3" FontSize="12" FontWeight="SemiBold" />
      </Grid>
      <ListView x:Name="Messages" Grid.Row="1" SelectionMode="None" IsItemClickEnabled="False">
        <ListView.ItemContainerStyle>
          <Style TargetType="ListViewItem">
            <Setter Property="Padding" Value="10,0,10,0" />
            <Setter Property="MinHeight" Value="38" />
            <Setter Property="HorizontalContentAlignment" Value="Stretch" />
          </Style>
        </ListView.ItemContainerStyle>
        <ListView.ItemTemplate>
          <DataTemplate>
            <Grid ColumnSpacing="10" Padding="0,3,0,3">
              <Grid.ColumnDefinitions>
                <ColumnDefinition Width="96" />
                <ColumnDefinition Width="0" />
                <ColumnDefinition Width="200" />
                <ColumnDefinition Width="*" />
              </Grid.ColumnDefinitions>
              <StackPanel Grid.Column="0" Orientation="Horizontal" Spacing="6" VerticalAlignment="Center">
                <TextBlock FontSize="12" />
                <Border CornerRadius="3" Padding="5,1,5,1" VerticalAlignment="Center" Visibility="Collapsed"
                        Background="{ThemeResource AccentFillColorDefaultBrush}">
                  <TextBlock FontSize="10" Foreground="{ThemeResource TextOnAccentFillColorPrimaryBrush}" />
                </Border>
              </StackPanel>
              <TextBlock Grid.Column="1" FontSize="12" VerticalAlignment="Center" TextTrimming="CharacterEllipsis" />
              <StackPanel Grid.Column="2" VerticalAlignment="Center" Spacing="1">
                <TextBlock FontSize="12" FontFamily="Consolas" FontWeight="SemiBold" />
                <Border CornerRadius="2" Padding="4,0,4,1" HorizontalAlignment="Left">
                  <TextBlock FontSize="10" Foreground="White" />
                </Border>
              </StackPanel>
              <TextBlock Grid.Column="3" FontSize="12" VerticalAlignment="Center" TextTrimming="CharacterEllipsis" />
            </Grid>
          </DataTemplate>
        </ListView.ItemTemplate>
      </ListView>
    </Grid>
  </Border>

  <StackPanel x:Name="Empty" Grid.Row="3" Grid.RowSpan="2" Spacing="8" Padding="30,0,30,0"
              HorizontalAlignment="Center" VerticalAlignment="Center">
    <FontIcon Glyph="&#xE7B3;" FontSize="28" Foreground="{ThemeResource TextFillColorTertiaryBrush}" />
    <TextBlock x:Name="EmptyTitle" FontSize="13" TextAlignment="Center" TextWrapping="Wrap"
               Foreground="{ThemeResource TextFillColorSecondaryBrush}" />
    <TextBlock x:Name="EmptyHint" FontSize="12" TextAlignment="Center" TextWrapping="Wrap"
               Foreground="{ThemeResource TextFillColorTertiaryBrush}" />
  </StackPanel>
</Grid>
)xaml";

        // Read once, the way the Monitor reads its own.
        midiapp::UmpDescriptionLabels const& Labels() noexcept
        {
            static midiapp::UmpDescriptionLabels labels
            {
                resources::GetString(L"DecodeLabelNote"),
                resources::GetString(L"DecodeLabelVelocity"),
                resources::GetString(L"DecodeLabelPressure"),
                resources::GetString(L"DecodeLabelController"),
                resources::GetString(L"DecodeLabelValue"),
                resources::GetString(L"DecodeLabelProgram"),
                resources::GetString(L"DecodeLabelBank"),
                resources::GetString(L"DecodeLabelBend"),
                resources::GetString(L"DecodeLabelIndex"),
                resources::GetString(L"DecodeLabelBytes"),
                resources::GetString(L"DecodeLabelStatus"),
                resources::GetString(L"DecodeLabelSong"),
                resources::GetString(L"DecodeLabelPosition"),
                resources::GetString(L"DecodeLabelFrame"),
                resources::GetString(L"DecodeLabelAttribute"),
                resources::GetString(L"DecodeLabelText"),
                resources::GetString(L"DecodeLabelName"),
                resources::GetString(L"DecodeLabelTicks"),
                resources::GetString(L"DecodeLabelTempo"),
                resources::GetString(L"DecodeLabelStreamId"),
                resources::GetString(L"DecodeLabelFunctionBlock"),
                resources::GetString(L"DecodeLabelForm"),
                resources::GetString(L"DecodeLabelTimeSignature"),
                resources::GetString(L"DecodeLabelKeySignature"),
                resources::GetString(L"DecodeLabelPerNote"),
                resources::GetString(L"DecodeLabelOptions"),
                resources::GetString(L"DecodeLabelNoteOffValue"),
                resources::GetString(L"DecodeTimeSignatureNonStandardFormat")
            };

            return labels;
        }

        winrt::hstring FormatCount(_In_ uint64_t value) noexcept
        {
            try
            {
                numberformatting::DecimalFormatter formatter{};
                formatter.IsGrouped(true);
                formatter.FractionDigits(0);

                return formatter.FormatUInt(value);
            }
            catch (...)
            {
                return winrt::to_hstring(value);
            }
        }

        // Each part of a connection's end: the node's name as the canvas shows it, and its group.
        struct EndText
        {
            std::wstring Name{};
            std::wstring Group{};
        };

        EndText DescribeEnd(
            _In_ PatchDocument const& patch,
            _In_ std::wstring const& nodeId,
            _In_ int32_t groupIndex,
            _In_ bool isSource)
        {
            if (auto const* block = patch.FindBlock(nodeId))
            {
                return EndText{ std::wstring{ BlockDisplayName(*block) }, {} };
            }

            auto const* endpoint = patch.FindEndpoint(nodeId);

            if (endpoint == nullptr)
            {
                return EndText{ nodeId, {} };
            }

            EndText text{ endpoint->DisplayName, {} };

            if (groupIndex != AllGroups)
            {
                std::wstring groupName{};

                if (auto const live = ResolveEndpoint(*endpoint))
                {
                    groupName = live->GroupName(groupIndex, isSource);
                }

                auto const number = groupIndex + 1;

                text.Group = groupName.empty()
                    ? std::wstring{ resources::FormatString(L"WatchGroupFormat", number) }
                    : std::wstring{ resources::FormatString(L"WatchNamedGroupFormat", number, groupName) };
            }

            return text;
        }

        std::wstring Joined(_In_ EndText const& end)
        {
            return end.Group.empty()
                ? end.Name
                : std::wstring{ resources::FormatString(L"WatchEndFormat", end.Name, end.Group) };
        }

        void SetTip(_In_ xaml::DependencyObject const& target, _In_ winrt::hstring const& text)
        {
            controls::ToolTipService::SetToolTip(target, winrt::box_value(text));
            xaml::Automation::AutomationProperties::SetName(target, text);
        }

        void CopyText(_In_ std::wstring const& text)
        {
            datatransfer::DataPackage package{};
            package.SetText(text);
            datatransfer::Clipboard::SetContent(package);
        }

        HWND HandleOf(_In_ xaml::Window const& window) noexcept
        {
            HWND handle{ nullptr };

            if (window != nullptr)
            {
                if (auto const native = window.try_as<::IWindowNative>())
                {
                    native->get_WindowHandle(&handle);
                }
            }

            return handle;
        }
    }

    _Use_decl_annotations_
    WatchWindow::WatchWindow(
        HWND owner,
        std::wstring patchKey,
        uint32_t number,
        winrt::Windows::UI::Color color,
        WatchSubject subject,
        std::function<void(uint32_t)> closed) noexcept :
        m_owner(owner),
        m_patchKey(std::move(patchKey)),
        m_number(number),
        m_color(color),
        m_subject(std::move(subject)),
        m_closed(std::move(closed))
    {
    }

    WatchWindow::~WatchWindow() noexcept
    {
        if (m_lease.Id != 0)
        {
            RouteEngine::Current().StopWatching(m_lease.Id);
        }
    }

    _Use_decl_annotations_
    bool WatchWindow::Open(WatchPatchState const& state) noexcept
    {
        try
        {
            m_ticksPerSecond = internal::GetMidiTimestampFrequency();
            m_origin = internal::GetCurrentMidiTimestamp();
            m_drained.resize(DrainBatch);
            m_items = winrt::single_threaded_observable_vector<foundation::IInspectable>();

            m_window = xaml::Window{};
            m_root = xaml::Markup::XamlReader::Load(LayoutXaml).as<controls::Grid>();
            m_window.Content(m_root);

            Build();
            Refresh(state);

            m_lease = RouteEngine::Current().StartWatching(Cells());

            auto const weak = weak_from_this();

            m_root.Loaded([weak](auto&&, auto&&)
                {
                    auto const self = weak.lock();

                    if (self == nullptr || self->m_closing)
                    {
                        return;
                    }

                    try
                    {
                        midiapp::WindowChromeElements elements{};

                        elements.Window = self->m_window;
                        elements.Root = self->m_root;
                        elements.Fill = self->m_root.FindName(L"Fill").as<xaml::UIElement>();
                        elements.Tint = self->m_root.FindName(L"Tint").as<controls::Border>();
                        elements.TitleBar = self->m_root.FindName(L"TitleBar").as<xaml::UIElement>();
                        elements.LeftInset = self->m_root.FindName(L"LeftInset").as<controls::ColumnDefinition>();
                        elements.RightInset = self->m_root.FindName(L"RightInset").as<controls::ColumnDefinition>();

                        self->m_chrome.Initialize(elements, AppSettings::Current());
                        self->m_chrome.SetWindowIconFromResource(IDI_APPICON);

                        // The chrome keeps the app's own on-top setting; this window has its own pin.
                        self->ApplyAlwaysOnTop();
                    }
                    MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to set up a monitor window's chrome.")
                });

            m_window.Closed([weak](auto&&, auto&&)
                {
                    if (auto const self = weak.lock())
                    {
                        self->OnClosed();
                    }
                });

            // Owned by the editor: never in the taskbar on its own, and gone with the editor.
            if (auto const handle = HandleOf(m_window); handle != nullptr && m_owner != nullptr)
            {
                ::SetWindowLongPtrW(handle, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(m_owner));
            }

            if (auto const presenter = m_window.AppWindow().Presenter().try_as<windowing::OverlappedPresenter>())
            {
                presenter.IsMaximizable(false);
            }

            PlaceNearOwner();
            ApplyAlwaysOnTop();

            m_timer = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread().CreateTimer();
            m_timer.Interval(TickInterval);
            m_timer.Tick([weak](auto&&, auto&&)
                {
                    if (auto const self = weak.lock())
                    {
                        self->OnTick();
                    }
                });
            m_timer.Start();

            m_window.Activate();

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to open a monitor window.")

        return false;
    }

    void WatchWindow::Build() noexcept
    {
        try
        {
            auto const find = [this](wchar_t const* name) { return m_root.FindName(name); };
            auto const weak = weak_from_this();

            find(L"TagDot").as<shapes::Ellipse>().Fill(media::SolidColorBrush{ m_color });
            find(L"TagText").as<controls::TextBlock>().Text(winrt::to_hstring(m_number));

            m_caption = find(L"Caption").as<controls::TextBlock>();
            m_fromLabel = find(L"FromLabel").as<controls::TextBlock>();
            m_fromText = find(L"FromText").as<controls::TextBlock>();
            m_toLabel = find(L"ToLabel").as<controls::TextBlock>();
            m_toText = find(L"ToText").as<controls::TextBlock>();
            m_arrow = find(L"Arrow").as<xaml::UIElement>();
            m_stateDot = find(L"StateDot").as<shapes::Ellipse>();
            m_stateText = find(L"StateText").as<controls::TextBlock>();
            m_removedBar = find(L"RemovedBar").as<controls::InfoBar>();
            m_pauseButton = find(L"PauseButton").as<primitives::ToggleButton>();
            m_pauseIcon = find(L"PauseIcon").as<controls::FontIcon>();
            m_countText = find(L"CountText").as<controls::TextBlock>();
            m_list = find(L"Messages").as<controls::ListView>();
            m_table = find(L"Table").as<xaml::UIElement>();
            m_toolbar = find(L"Toolbar").as<xaml::UIElement>();
            m_empty = find(L"Empty").as<xaml::UIElement>();
            m_emptyTitle = find(L"EmptyTitle").as<controls::TextBlock>();
            m_valueBrushSource = find(L"ValueBrushSource").as<controls::Border>();
            m_successBrushSource = find(L"SuccessBrushSource").as<controls::Border>();

            auto const watchingNode = !m_subject.NodeId.empty();

            m_emptyTitle.Text(resources::GetString(watchingNode ? L"WatchEmptyNodeTitle" : L"WatchEmptyTitle"));
            find(L"EmptyHint").as<controls::TextBlock>().Text(resources::GetString(L"WatchEmptyHint"));

            find(L"HeaderTime").as<controls::TextBlock>().Text(resources::GetString(L"WatchColumnTime"));
            find(L"HeaderWentTo").as<controls::TextBlock>().Text(resources::GetString(L"WatchColumnWentTo"));
            find(L"HeaderMessage").as<controls::TextBlock>().Text(resources::GetString(L"WatchColumnMessage"));
            find(L"HeaderDetails").as<controls::TextBlock>().Text(resources::GetString(L"WatchColumnDetails"));

            // Watching a single connection, the From and To line already says where everything goes.
            if (watchingNode)
            {
                find(L"Header").as<controls::Grid>().ColumnDefinitions().GetAt(1).Width(xaml::GridLengthHelper::FromPixels(WentToColumnWidth));
            }

            xaml::Automation::AutomationProperties::SetName(m_list, resources::GetString(L"WatchListName"));

            auto const pin = find(L"PinButton").as<primitives::ToggleButton>();
            SetTip(pin, resources::GetString(L"WatchPinTip"));
            pin.Click([weak](foundation::IInspectable const& sender, auto&&)
                {
                    auto const self = weak.lock();
                    auto const button = sender.try_as<primitives::ToggleButton>();

                    if (self != nullptr && button != nullptr)
                    {
                        auto const checked = button.IsChecked();
                        self->m_alwaysOnTop = checked != nullptr && checked.Value();
                        self->ApplyAlwaysOnTop();
                    }
                });

            SetTip(m_pauseButton, resources::GetString(L"WatchPauseTip"));
            m_pauseButton.Click([weak](foundation::IInspectable const& sender, auto&&)
                {
                    auto const self = weak.lock();
                    auto const button = sender.try_as<primitives::ToggleButton>();

                    if (self != nullptr && button != nullptr)
                    {
                        auto const checked = button.IsChecked();
                        self->SetPaused(checked != nullptr && checked.Value());
                    }
                });

            auto const clear = find(L"ClearButton").as<controls::Button>();
            SetTip(clear, resources::GetString(L"WatchClearTip"));
            clear.Click([weak](auto&&, auto&&)
                {
                    if (auto const self = weak.lock())
                    {
                        self->Clear();
                    }
                });

            auto const clock = find(L"ClockButton").as<primitives::ToggleButton>();
            SetTip(clock, resources::GetString(L"WatchClockTip"));
            clock.Click([weak](foundation::IInspectable const& sender, auto&&)
                {
                    auto const self = weak.lock();
                    auto const button = sender.try_as<primitives::ToggleButton>();

                    if (self != nullptr && button != nullptr)
                    {
                        auto const checked = button.IsChecked();
                        self->m_showClock = checked != nullptr && checked.Value();
                    }
                });

            auto const sensing = find(L"SensingButton").as<primitives::ToggleButton>();
            SetTip(sensing, resources::GetString(L"WatchSensingTip"));
            sensing.Click([weak](foundation::IInspectable const& sender, auto&&)
                {
                    auto const self = weak.lock();
                    auto const button = sender.try_as<primitives::ToggleButton>();

                    if (self != nullptr && button != nullptr)
                    {
                        auto const checked = button.IsChecked();
                        self->m_showActiveSensing = checked != nullptr && checked.Value();
                    }
                });

            m_list.ItemsSource(m_items);

            m_list.ContainerContentChanging([weak](auto&&, controls::ContainerContentChangingEventArgs const& args)
                {
                    if (auto const self = weak.lock())
                    {
                        self->ShowRow(args);
                    }
                });

            m_list.RightTapped([weak](auto&&, input::RightTappedRoutedEventArgs const& args)
                {
                    if (auto const self = weak.lock())
                    {
                        self->ShowRowMenu(args);
                    }
                });

            m_root.KeyDown([weak](auto&&, input::KeyRoutedEventArgs const& args)
                {
                    if (auto const self = weak.lock())
                    {
                        self->OnKeyDown(args);
                    }
                });

            UpdateCount();
            UpdateEmptyState();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build a monitor window.")
    }

    void WatchWindow::PlaceNearOwner() noexcept
    {
        try
        {
            auto const handle = HandleOf(m_window);

            if (handle == nullptr)
            {
                return;
            }

            auto const dpi = ::GetDpiForWindow(handle);
            auto const scale = dpi == 0 ? 1.0 : dpi / 96.0;

            auto const width = static_cast<int32_t>(DefaultWidth * scale);
            auto const height = static_cast<int32_t>(DefaultHeight * scale);
            auto const step = static_cast<int32_t>(CascadeStep * scale * ((m_number - 1) % 8));

            RECT owner{};

            if (m_owner == nullptr || !::GetWindowRect(m_owner, &owner))
            {
                m_window.AppWindow().Resize({ width, height });
                return;
            }

            // Over the right side of the editor, where the inspector is, rather than the canvas.
            auto x = static_cast<int32_t>(owner.right - width - static_cast<int32_t>(24 * scale) - step);
            auto y = static_cast<int32_t>(owner.top + static_cast<int32_t>(110 * scale) + step);

            MONITORINFO monitor{};
            monitor.cbSize = sizeof(monitor);

            if (::GetMonitorInfoW(::MonitorFromWindow(m_owner, MONITOR_DEFAULTTONEAREST), &monitor))
            {
                auto const& work = monitor.rcWork;

                x = std::clamp(x, static_cast<int32_t>(work.left), (std::max)(static_cast<int32_t>(work.left), static_cast<int32_t>(work.right) - width));
                y = std::clamp(y, static_cast<int32_t>(work.top), (std::max)(static_cast<int32_t>(work.top), static_cast<int32_t>(work.bottom) - height));
            }

            m_window.AppWindow().MoveAndResize({ x, y, width, height });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to place a monitor window.")
    }

    void WatchWindow::ApplyAlwaysOnTop() noexcept
    {
        try
        {
            if (m_window == nullptr || m_closing)
            {
                return;
            }

            if (auto const appWindow = m_window.AppWindow())
            {
                if (auto const presenter = appWindow.Presenter().try_as<windowing::OverlappedPresenter>())
                {
                    presenter.IsAlwaysOnTop(m_alwaysOnTop);
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to keep a monitor window on top.")
    }

    void WatchWindow::OnClosed() noexcept
    {
        if (m_closing)
        {
            return;
        }

        m_closing = true;

        try
        {
            if (m_timer != nullptr)
            {
                m_timer.Stop();
            }

            // The timer has stopped, so nothing here takes from the buffer any more.
            if (m_lease.Id != 0)
            {
                RouteEngine::Current().StopWatching(m_lease.Id);
                m_lease = {};
            }

            m_chrome.Shutdown();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to close a monitor window.")

        if (m_closed)
        {
            try
            {
                m_closed(m_number);
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to forget a monitor window.")
        }
    }

    void WatchWindow::Activate() noexcept
    {
        try
        {
            if (m_window == nullptr || m_closing)
            {
                return;
            }

            if (auto const presenter = m_window.AppWindow().Presenter().try_as<windowing::OverlappedPresenter>();
                presenter != nullptr && presenter.State() == windowing::OverlappedPresenterState::Minimized)
            {
                presenter.Restore();
            }

            m_window.Activate();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to bring a monitor window forward.")
    }

    void WatchWindow::Close() noexcept
    {
        try
        {
            if (m_window != nullptr && !m_closing)
            {
                m_window.Close();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to close a monitor window.")

        // In case closing raised nothing.
        OnClosed();
    }

    std::vector<std::wstring> WatchWindow::MarkedConnections() const
    {
        if (m_subject.NodeId.empty())
        {
            return { m_subject.ConnectionId };
        }

        return m_links;
    }

    std::vector<std::wstring> WatchWindow::Cells() const
    {
        std::vector<std::wstring> cells{};

        for (auto const& link : m_links)
        {
            cells.push_back(m_patchKey + L'|' + link);
        }

        return cells;
    }

    _Use_decl_annotations_
    uint16_t WatchWindow::LinkIndex(std::wstring const& linkId)
    {
        auto const found = std::find(m_links.begin(), m_links.end(), linkId);

        if (found != m_links.end())
        {
            return static_cast<uint16_t>(found - m_links.begin());
        }

        m_links.push_back(linkId);
        m_linkNames.emplace_back();

        return static_cast<uint16_t>(m_links.size() - 1);
    }

    _Use_decl_annotations_
    void WatchWindow::Refresh(WatchPatchState const& state) noexcept
    {
        try
        {
            auto const* patch = state.Patch;
            auto const linksBefore = m_links.size();
            auto const namesBefore = m_linkNames;

            if (patch != nullptr)
            {
                m_patchName = patch->Name;
            }

            if (m_subject.NodeId.empty())
            {
                LinkIndex(m_subject.ConnectionId);

                auto const* connection = patch == nullptr ? nullptr : patch->FindConnection(m_subject.ConnectionId);

                if (connection == nullptr)
                {
                    m_state = State::Removed;
                }
                else
                {
                    auto const from = DescribeEnd(*patch, connection->SourceId, connection->SourceGroupIndex, true);
                    auto const to = DescribeEnd(*patch, connection->DestinationId, connection->DestinationGroupIndex, false);

                    m_fromName = from.Name;
                    m_fromGroup = from.Group;
                    m_toName = to.Name;
                    m_toGroup = to.Group;

                    auto const loopMuted = state.LoopMutedConnectionIds != nullptr &&
                        state.LoopMutedConnectionIds->count(connection->Id) != 0;

                    m_state = connection->Muted || loopMuted
                        ? State::Muted
                        : (state.Routing ? State::Routing : State::NotRouting);
                }
            }
            else
            {
                auto const exists = patch != nullptr &&
                    (patch->FindBlock(m_subject.NodeId) != nullptr || patch->FindEndpoint(m_subject.NodeId) != nullptr);

                if (!exists)
                {
                    m_state = State::Removed;
                }
                else
                {
                    m_fromName = DescribeEnd(*patch, m_subject.NodeId, AllGroups, true).Name;

                    size_t count{ 0 };

                    for (auto const& connection : patch->Connections)
                    {
                        if (connection.SourceId != m_subject.NodeId)
                        {
                            continue;
                        }

                        auto const index = LinkIndex(connection.Id);

                        m_linkNames[index] = Joined(DescribeEnd(*patch, connection.DestinationId, connection.DestinationGroupIndex, false));
                        count++;
                    }

                    m_toName = count == 1
                        ? std::wstring{ resources::GetString(L"WatchConnectionCountOne") }
                        : std::wstring{ resources::FormatString(L"WatchConnectionCountFormat", count) };

                    m_state = state.Routing ? State::Routing : State::NotRouting;
                }
            }

            if (m_lease.Id != 0 && m_links.size() != linksBefore)
            {
                RouteEngine::Current().ChangeWatching(m_lease.Id, Cells());
            }

            if (m_window == nullptr || m_closing)
            {
                return;
            }

            winrt::hstring caption{};
            winrt::hstring title{};

            if (m_subject.NodeId.empty())
            {
                caption = resources::FormatString(L"WatchCaptionFormat", m_fromName, m_toName);
                title = resources::FormatString(L"WatchTitleFormat", m_fromName, m_toName, m_patchName);
            }
            else
            {
                caption = resources::FormatString(L"WatchNodeCaptionFormat", m_fromName);
                title = resources::FormatString(L"WatchNodeTitleFormat", m_fromName, m_patchName);
            }

            m_window.Title(title);
            m_caption.Text(caption);

            auto const setEnd = [](controls::TextBlock const& text, std::wstring const& name, std::wstring const& group)
                {
                    text.Inlines().Clear();

                    xaml::Documents::Run nameRun{};
                    nameRun.Text(name);
                    nameRun.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                    text.Inlines().Append(nameRun);

                    if (!group.empty())
                    {
                        xaml::Documents::Run groupRun{};
                        groupRun.Text(L"  " + group);
                        groupRun.FontWeight(winrt::Microsoft::UI::Text::FontWeights::Normal());
                        text.Inlines().Append(groupRun);
                    }

                    xaml::Automation::AutomationProperties::SetName(text,
                        group.empty() ? winrt::hstring{ name } : winrt::hstring{ name + L" " + group });
                };

            if (m_subject.NodeId.empty())
            {
                m_fromLabel.Text(resources::GetString(L"WatchFromLabel"));
                m_toLabel.Text(resources::GetString(L"WatchToLabel"));
                m_toLabel.Visibility(xaml::Visibility::Visible);
                m_arrow.Visibility(xaml::Visibility::Visible);

                setEnd(m_fromText, m_fromName, m_fromGroup);
                setEnd(m_toText, m_toName, m_toGroup);
            }
            else
            {
                m_fromLabel.Text(resources::GetString(L"WatchEverythingOutOfLabel"));
                m_toLabel.Visibility(xaml::Visibility::Collapsed);
                m_arrow.Visibility(xaml::Visibility::Collapsed);

                setEnd(m_fromText, m_fromName, {});
                m_toText.Text(m_toName);
            }

            ShowState();

            // A row shows where it went by name, and a name changed.
            if (!m_subject.NodeId.empty() && m_items.Size() > 0 && m_linkNames != namesBefore)
            {
                m_list.ItemsSource(nullptr);
                m_list.ItemsSource(m_items);
                m_list.ScrollIntoView(m_items.GetAt(m_items.Size() - 1));
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to refresh a monitor window.")
    }

    void WatchWindow::ShowState() noexcept
    {
        try
        {
            wchar_t const* key{ L"WatchStateNotRouting" };

            switch (m_state)
            {
            case State::Routing:  key = L"WatchStateRouting"; break;
            case State::Muted:    key = L"WatchStateMuted"; break;
            case State::Removed:  key = L"WatchStateRemoved"; break;
            default:              break;
            }

            m_stateText.Text(resources::GetString(key));

            if (m_state == State::Routing && m_successBrushSource != nullptr)
            {
                m_stateDot.Fill(m_successBrushSource.Background());
            }
            else
            {
                m_stateDot.Fill(media::SolidColorBrush{ winrt::Windows::UI::Color{ 0x60, 0x80, 0x80, 0x80 } });
            }

            if (m_state == State::Removed)
            {
                m_removedBar.Message(resources::GetString(m_subject.NodeId.empty()
                    ? L"WatchRemovedConnectionMessage"
                    : L"WatchRemovedNodeMessage"));
            }

            m_removedBar.IsOpen(m_state == State::Removed);
            m_removedBar.Visibility(m_state == State::Removed ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show a monitor window's state.")
    }

    void WatchWindow::OnTick() noexcept
    {
        try
        {
            if (m_closing || m_lease.Buffer == nullptr)
            {
                return;
            }

            auto const before = m_history.LastSequence();
            size_t taken{ 0 };

            while (taken < MostPerTick)
            {
                auto const count = m_lease.Buffer->Drain(m_drained.data(), m_drained.size());

                for (size_t i = 0; i < count; i++)
                {
                    TakeIn(m_drained[i]);
                }

                taken += count;

                if (count < m_drained.size())
                {
                    break;
                }
            }

            if (m_history.LastSequence() != before && m_items.Size() > 0)
            {
                m_list.ScrollIntoView(m_items.GetAt(m_items.Size() - 1));
            }

            UpdateCount();
            UpdateEmptyState();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show what a watched connection carried.")
    }

    _Use_decl_annotations_
    void WatchWindow::TakeIn(WatchedMessage const& message) noexcept
    {
        try
        {
            m_total++;

            if (message.WordCount == 0)
            {
                return;
            }

            auto const kind = ClassifyWatched(message.Words[0]);

            if ((kind == WatchedKind::Clock && !m_showClock) ||
                (kind == WatchedKind::ActiveSensing && !m_showActiveSensing) ||
                m_paused)
            {
                return;
            }

            auto copy = message;

            // A message sent "now" carries no time of its own, so it gets the time it was seen.
            if (copy.Timestamp == 0)
            {
                copy.Timestamp = internal::GetCurrentMidiTimestamp();
            }

            if (m_history.Add(copy) > 0 && m_items.Size() > 0)
            {
                m_items.RemoveAt(0);
            }

            m_items.Append(winrt::box_value(m_history.LastSequence()));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to add a message to a monitor window.")
    }

    _Use_decl_annotations_
    void WatchWindow::AddTest(std::wstring const& linkId, TraceMessage const& message) noexcept
    {
        try
        {
            if (m_closing)
            {
                return;
            }

            auto const found = std::find(m_links.begin(), m_links.end(), linkId);

            if (found == m_links.end())
            {
                return;
            }

            WatchedMessage watched{};
            watched.Timestamp = internal::GetCurrentMidiTimestamp();
            watched.WordCount = (std::min)(message.WordCount, static_cast<uint8_t>(watched.Words.size()));
            watched.Link = static_cast<uint16_t>(found - m_links.begin());
            watched.FromTest = true;

            std::copy_n(message.Words.begin(), watched.WordCount, watched.Words.begin());

            auto const before = m_history.LastSequence();

            TakeIn(watched);

            if (m_history.LastSequence() != before && m_items.Size() > 0)
            {
                m_list.ScrollIntoView(m_items.GetAt(m_items.Size() - 1));
            }

            UpdateCount();
            UpdateEmptyState();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to add a test message to a monitor window.")
    }

    void WatchWindow::UpdateCount() noexcept
    {
        try
        {
            auto const dropped = m_lease.Buffer == nullptr ? 0 : m_lease.Buffer->Dropped() - (std::min)(m_droppedBefore, m_lease.Buffer->Dropped());

            if (m_total == m_shownTotal && dropped == m_shownDropped)
            {
                return;
            }

            m_shownTotal = m_total;
            m_shownDropped = dropped;

            // Everything that went along, hidden and dropped included.
            m_countText.Text(resources::FormatString(L"WatchCountFormat",
                std::wstring{ FormatCount(m_total + dropped) },
                std::wstring{ FormatCount(dropped) }));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to count a monitor window's messages.")
    }

    void WatchWindow::UpdateEmptyState() noexcept
    {
        try
        {
            auto const nothingYet = m_total == 0 && m_history.Size() == 0 && m_shownDropped == 0;

            m_empty.Visibility(nothingYet ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            m_table.Visibility(nothingYet ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
            m_toolbar.Visibility(nothingYet ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show a monitor window's empty state.")
    }

    void WatchWindow::Clear() noexcept
    {
        try
        {
            m_history.Clear();
            m_items.Clear();
            m_total = 0;
            m_droppedBefore = m_lease.Buffer == nullptr ? 0 : m_lease.Buffer->Dropped();

            UpdateCount();
            UpdateEmptyState();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to clear a monitor window.")
    }

    _Use_decl_annotations_
    void WatchWindow::SetPaused(bool paused) noexcept
    {
        try
        {
            m_paused = paused;

            m_pauseIcon.Glyph(paused ? L"\xE768" : L"\xE769");
            m_pauseButton.IsChecked(paused);
            SetTip(m_pauseButton, resources::GetString(paused ? L"WatchResumeTip" : L"WatchPauseTip"));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to pause a monitor window.")
    }

    _Use_decl_annotations_
    void WatchWindow::OnKeyDown(input::KeyRoutedEventArgs const& args) noexcept
    {
        try
        {
            using winrt::Windows::System::VirtualKey;

            auto const control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;

            if (args.Key() == VirtualKey::L && control)
            {
                Clear();
                args.Handled(true);
            }
            else if (args.Key() == VirtualKey::Space && !control &&
                args.OriginalSource().try_as<primitives::ButtonBase>() == nullptr)
            {
                // A focused button takes Space for itself.
                SetPaused(!m_paused);
                args.Handled(true);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to act on a key in a monitor window.")
    }

    _Use_decl_annotations_
    winrt::hstring WatchWindow::TimeText(WatchedMessage const& message) const
    {
        numberformatting::DecimalFormatter formatter{};
        formatter.IsGrouped(true);
        formatter.FractionDigits(3);
        formatter.IntegerDigits(1);

        return formatter.FormatDouble(WatchMilliseconds(message.Timestamp, m_origin, m_ticksPerSecond));
    }

    _Use_decl_annotations_
    void WatchWindow::ShowRow(controls::ContainerContentChangingEventArgs const& args) noexcept
    {
        try
        {
            if (args.InRecycleQueue())
            {
                return;
            }

            args.Handled(true);

            auto const root = args.ItemContainer().ContentTemplateRoot().try_as<controls::Grid>();
            auto const sequence = winrt::unbox_value_or<uint64_t>(args.Item(), 0);
            auto const* row = m_history.Find(sequence);

            if (root == nullptr || row == nullptr || root.Children().Size() < 4)
            {
                return;
            }

            auto const& message = row->Message;
            auto const children = root.Children();

            root.ColumnDefinitions().GetAt(1).Width(xaml::GridLengthHelper::FromPixels(m_subject.NodeId.empty() ? 0.0 : WentToColumnWidth));

            // Time, and the Test tag
            auto const timePanel = children.GetAt(0).as<controls::StackPanel>();
            timePanel.Children().GetAt(0).as<controls::TextBlock>().Text(TimeText(message));

            auto const testTag = timePanel.Children().GetAt(1).as<controls::Border>();
            testTag.Visibility(message.FromTest ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            testTag.Child().as<controls::TextBlock>().Text(resources::GetString(L"WatchTestTag"));

            // Went to
            auto const wentTo = children.GetAt(1).as<controls::TextBlock>();
            wentTo.Text(message.Link < m_linkNames.size() ? winrt::hstring{ m_linkNames[message.Link] } : winrt::hstring{});

            // Message: the words, and the name in the Monitor's colors
            auto const messagePanel = children.GetAt(2).as<controls::StackPanel>();
            messagePanel.Children().GetAt(0).as<controls::TextBlock>().Text(FormatWatchedWords(message.Words, message.WordCount));

            auto const chiclet = messagePanel.Children().GetAt(1).as<controls::Border>();
            auto const type = static_cast<uint8_t>(message.Words[0] >> 28);
            auto const colorIndex = midiapp::UmpMessageTypeColorIndex(type) % ChicletColors.size();

            chiclet.Background(media::SolidColorBrush{ ChicletColors[colorIndex] });
            chiclet.Child().as<controls::TextBlock>().Text(midiapp::UmpDisplayName(message.Words[0]));

            // Details, with the values in the accent color
            auto const details = children.GetAt(3).as<controls::TextBlock>();
            auto const description = midiapp::DescribeUmp(message.Words, message.WordCount, Labels());
            auto const valueBrush = m_valueBrushSource == nullptr ? nullptr : m_valueBrushSource.Background();

            details.Inlines().Clear();

            for (uint32_t i = 0; i < description.Count && i < description.Labels.size(); i++)
            {
                xaml::Documents::Run label{};
                label.Text((i == 0 ? std::wstring{} : std::wstring{ L"   " }) + std::wstring{ description.Labels[i] } + L" ");
                details.Inlines().Append(label);

                xaml::Documents::Run value{};
                value.Text(description.Values[i]);

                if (valueBrush != nullptr)
                {
                    value.Foreground(valueBrush);
                }

                details.Inlines().Append(value);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show a monitor window row.")
    }

    _Use_decl_annotations_
    std::wstring WatchWindow::RowText(WatchRow const& row) const
    {
        auto const& message = row.Message;

        std::wstring text{ TimeText(message) };

        if (message.FromTest)
        {
            text += L' ';
            text += resources::GetString(L"WatchTestTag");
        }

        if (!m_subject.NodeId.empty() && message.Link < m_linkNames.size())
        {
            text += L'\t';
            text += m_linkNames[message.Link];
        }

        text += L'\t';
        text += FormatWatchedWords(message.Words, message.WordCount);
        text += L'\t';
        text += midiapp::UmpDisplayName(message.Words[0]);

        auto const details = midiapp::DescribeUmp(message.Words, message.WordCount, Labels()).ToDisplayString();

        if (!details.empty())
        {
            text += L'\t';
            text += details;
        }

        return text;
    }

    _Use_decl_annotations_
    void WatchWindow::ShowRowMenu(input::RightTappedRoutedEventArgs const& args) noexcept
    {
        try
        {
            auto element = args.OriginalSource().try_as<xaml::DependencyObject>();
            controls::ListViewItem item{ nullptr };

            while (element != nullptr && item == nullptr)
            {
                item = element.try_as<controls::ListViewItem>();

                if (item == nullptr)
                {
                    element = media::VisualTreeHelper::GetParent(element);
                }
            }

            if (item == nullptr)
            {
                return;
            }

            auto const sequence = winrt::unbox_value_or<uint64_t>(m_list.ItemFromContainer(item), 0);
            auto const* row = m_history.Find(sequence);

            if (row == nullptr)
            {
                return;
            }

            args.Handled(true);

            auto const words = FormatWatchedWords(row->Message.Words, row->Message.WordCount);
            auto const whole = RowText(*row);

            controls::MenuFlyout menu{};

            controls::FontIcon copyIcon{};
            copyIcon.Glyph(L"\xE8C8");

            controls::MenuFlyoutItem copyWords{};
            copyWords.Text(resources::GetString(L"WatchCopyWords"));
            copyWords.Icon(copyIcon);
            copyWords.Click([words](auto&&, auto&&)
                {
                    try
                    {
                        CopyText(words);
                    }
                    MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to copy a message.")
                });

            controls::MenuFlyoutItem copyRow{};
            copyRow.Text(resources::GetString(L"WatchCopyRow"));
            copyRow.Click([whole](auto&&, auto&&)
                {
                    try
                    {
                        CopyText(whole);
                    }
                    MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to copy a message.")
                });

            menu.Items().Append(copyWords);
            menu.Items().Append(copyRow);

            primitives::FlyoutShowOptions options{};
            options.Position(args.GetPosition(item));

            menu.ShowAt(item, options);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show a monitor window row's menu.")
    }

    // ----------------------------------------------------------------- every window of one editor

    _Use_decl_annotations_
    WatchWindows::WatchWindows(HWND owner, std::wstring patchKey, std::function<void()> marksChanged) noexcept :
        m_owner(owner),
        m_patchKey(std::move(patchKey)),
        m_marksChanged(std::move(marksChanged))
    {
    }

    _Use_decl_annotations_
    void WatchWindows::Watch(WatchSubject const& subject, WatchPatchState const& state) noexcept
    {
        try
        {
            for (auto const& window : m_windows)
            {
                if (window != nullptr && window->Subject() == subject)
                {
                    window->Activate();
                    return;
                }
            }

            // The lowest number not in use, so numbers stay small as windows come and go.
            uint32_t number{ 1 };

            while (std::any_of(m_windows.begin(), m_windows.end(),
                [number](std::shared_ptr<WatchWindow> const& window) { return window != nullptr && window->Number() == number; }))
            {
                number++;
            }

            auto const color = WindowColors[(number - 1) % WindowColors.size()];

            auto window = std::make_shared<WatchWindow>(m_owner, m_patchKey, number, color, subject,
                [weak = weak_from_this()](uint32_t closedNumber)
                {
                    if (auto const self = weak.lock())
                    {
                        self->OnWindowClosed(closedNumber);
                    }
                });

            m_windows.push_back(window);

            if (!window->Open(state))
            {
                window->Close();
                return;
            }

            if (m_marksChanged)
            {
                m_marksChanged();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to open a monitor window.")
    }

    _Use_decl_annotations_
    void WatchWindows::Refresh(WatchPatchState const& state) noexcept
    {
        try
        {
            // A copy: a refresh can't close a window, but nothing here relies on that.
            auto const windows = m_windows;

            for (auto const& window : windows)
            {
                if (window != nullptr)
                {
                    window->Refresh(state);
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to refresh the monitor windows.")
    }

    _Use_decl_annotations_
    void WatchWindows::FeedTrace(TraceResult const& trace) noexcept
    {
        try
        {
            if (m_windows.empty())
            {
                return;
            }

            auto const windows = m_windows;

            for (auto const& step : trace.Steps)
            {
                for (auto const& carried : step.Carried)
                {
                    for (auto const& window : windows)
                    {
                        if (window != nullptr)
                        {
                            window->AddTest(carried.LinkId, carried.Message);
                        }
                    }
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show a test in the monitor windows.")
    }

    std::vector<PatchCanvas::WatchMark> WatchWindows::Marks() const noexcept
    {
        std::vector<PatchCanvas::WatchMark> marks{};

        try
        {
            for (auto const& window : m_windows)
            {
                if (window == nullptr)
                {
                    continue;
                }

                for (auto const& connectionId : window->MarkedConnections())
                {
                    marks.push_back(PatchCanvas::WatchMark{ connectionId, window->Number(), window->Color() });
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to list the watched connections.")

        return marks;
    }

    _Use_decl_annotations_
    void WatchWindows::BringForward(uint32_t number) noexcept
    {
        for (auto const& window : m_windows)
        {
            if (window != nullptr && window->Number() == number)
            {
                window->Activate();
                return;
            }
        }
    }

    void WatchWindows::CloseAll() noexcept
    {
        try
        {
            auto const windows = m_windows;

            for (auto const& window : windows)
            {
                if (window != nullptr)
                {
                    window->Close();
                }
            }

            m_windows.clear();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to close the monitor windows.")
    }

    _Use_decl_annotations_
    void WatchWindows::OnWindowClosed(uint32_t number) noexcept
    {
        try
        {
            std::erase_if(m_windows, [number](std::shared_ptr<WatchWindow> const& window)
                {
                    return window == nullptr || window->Number() == number;
                });

            if (m_marksChanged)
            {
                m_marksChanged();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to forget a monitor window.")
    }
}
