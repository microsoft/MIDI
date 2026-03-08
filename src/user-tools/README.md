# Tools

These are generally written in .NET (7 or 8) and provide end-user functionality outside of the DAW or other third-party tools.

| Component | Description |
| --------- | ------------------- |
| midi-settings | The main MIDI Settings app for Windows |
| midi-settings-extensibility | Extensibility model (client plugin model) for the MIDI Settings app |

---

## Build Notes (`user-tools-Compilation-issue-fix` branch)

### Requirements
- .NET 10 SDK
- Windows App SDK 1.8+
- Windows SDK 10.0.26100.0

### Fixed Issues

#### 1. Missing `CommunityToolkit.WinUI.Controls.TitleBar` Package
- **Issue**: The `TitleBar` control from CommunityToolkit was referenced but the package does not exist on NuGet
- **Solution**: Replaced the `controls:TitleBar` with a standard Grid layout in `ShellPage.xaml`
- **Files Modified**:
  - `Microsoft.Midi.Settings/Views/Core Pages/ShellPage.xaml`
  - `Microsoft.Midi.Settings/Views/Core Pages/ShellPage.xaml.cs`

#### 2. NU1510 Warnings - Redundant Package References
- **Issue**: `System.Text.Json` and `System.Text.RegularExpressions` are built-in to .NET 10 and should not be explicitly referenced
- **Solution**: Removed the explicit package references
- **Files Modified**:
  - `Microsoft.Midi.Settings/Microsoft.Midi.Settings.csproj`

#### 3. WMC1506 Warnings - OneWay Binding Notifications
- **Issue**: OneWay bindings require properties to support change notifications, but the MIDI library types do not implement `INotifyPropertyChanged`
- **Solution**: Changed binding mode from `OneWay` to `OneTime` for read-only port information
- **Files Modified**:
  - `Microsoft.Midi.Settings/Views/Endpoint Management Pages/EndpointsAllPage.xaml`

### Build Output
- **Executable**: `MidiSettings.exe`
- **Output Directory**: `Microsoft.Midi.Settings/bin/x64/Release/net10.0-windows10.0.26100.0/`
- **Status**: Build successful with 0 warnings

---

## Changes (`Interface-adjustment` branch)

### UI Changes

#### Settings Button in NavigationView
- **Description**: Moved the Settings button from `PaneFooter` to `FooterMenuItems` for better navigation support and added bottom margin
- **Files Modified**:
  - `Microsoft.Midi.Settings/Views/Core Pages/ShellPage.xaml`
  - `Microsoft.Midi.Settings/Strings/en-us/Resources.resw`

#### Back Button Visibility
- **Description**: Set `NavigationView.IsBackButtonVisible="Collapsed"` and set the custom BackButton `Visibility="Collapsed"` to completely hide the back button
- **Files Modified**:
  - `Microsoft.Midi.Settings/Views/Core Pages/ShellPage.xaml`

---

## Recent Changes (`Interface-repair-optimization-and-adjustment` branch)

### ShellPage Improvements

#### Search Box
- **Fixed**: Search box right corner being clipped when window is narrow
- **Changes**:
  - Changed `HorizontalAlignment` from `Left` to `Stretch`
  - Removed fixed `Width` property, now uses adaptive width (MaxWidth: 700)
  - Added compact mode: shows search icon button when window width < 1020px

#### Title Bar Drag Regions
- **Fixed**: Drag regions not working properly
- **Changes**:
  - Now correctly calculates drag areas based on actual search box position
  - Updates drag regions when window size changes (with delay for layout completion)

#### Navigation View
- **Improved**: Unified content margin between expanded and compact modes
- **Changes**:
  - Override `NavigationViewContentMargin` and `NavigationViewContentGridMargin` to remove extra top padding
  - Set `NavigationFrameContentMargin` to `40,24,40,0` for consistent spacing

#### Focus Management
- **Improved**: Set default focus to Home navigation item on startup
- **Benefit**: Prevents search box from automatically receiving focus

### HomePage Improvements

#### InfoBar Visibility
- **Changed**: Changed from `IsOpen` to `Visibility` binding
- **Benefit**: Three InfoBars (Service availability, First-run setup, SDK Update) now collapse when hidden, no longer occupy space when not visible

### Page Layout Fixes

#### Commented Out Empty Description Text Blocks
- **Pages**:
  - **ManagementSessionsPage**: Commented out empty description text block
  - **EndpointsAllPage**: Commented out empty description text block (`DevicesPage_Description`)
  - **EndpointsBasicLoopPage**: Commented out empty description text block

### Performance Improvements

#### Search Data Preloading
- **Description**: Preload search data during app startup to prevent UI blocking
- **Changes**:
  - Moved `RefreshSearchData()` to `App.OnLaunched()`
  - Search data is now loaded in background thread
  - Search box cursor no longer delayed when entering search field

#### SDK Update Check Optimization
- **Description**: Moved SDK update check from ViewModel constructor to app startup
- **Changes**:
  - `CheckForSdkUpdatesAsync()` now called in `App.OnLaunched()`
  - Prevents HOME page navigation lag
  - Uses async/await to avoid blocking UI thread

#### Search Box Text Deletion
- **Fixed**: Search results list no longer shows when text is deleted
- **Changes**:
  - Added empty text check in `SettingsSearchBox_TextChanged`
  - Sets `ItemsSource = null` when text is empty

### Page Layout Fixes

#### SettingsPage Bottom Margin
- **Fixed**: Added bottom margin to prevent content from sticking to window bottom
- **Changes**:
  - Added `Margin="0,0,0,24"` to StackPanel in SettingsPage.xaml

### Technical Details
- Window threshold for compact mode: 1124px
- Search box collapse threshold: 650px available width
- Drag region height: 48px (title bar height)

---

## Recent Changes (`feature-navigation-and-layout` branch)

This branch focuses on **navigation improvements** and **page layout optimization**.

### Navigation Improvements

#### Mouse Button Navigation
- **Description**: Added support for mouse side buttons (XButton1/XButton2) for back/forward navigation
- **Changes**:
  - Added `WM_XBUTTONDOWN` message handling in `MainWindow.xaml.cs`
  - XButton1 (Back button) triggers `GoBack()` navigation
  - XButton2 (Forward button) triggers `GoForward()` navigation
  - Added `CanGoForward` property and `GoForward()` method to `INavigationService` interface
  - Implemented `GoForward()` in `NavigationService` class
- **Files Modified**:
  - `Microsoft.Midi.Settings/MainWindow.xaml.cs`
  - `Microsoft.Midi.Settings/Contracts/Services/INavigationService.cs`
  - `Microsoft.Midi.Settings/Services/NavigationService.cs`

#### Keyboard Navigation
- **Description**: Added keyboard shortcuts for forward navigation (Alt+Right, GoForward key)
- **Changes**:
  - Added `VirtualKey.GoForward` keyboard accelerator in `ShellPage.xaml.cs`
  - Updated `OnKeyboardAcceleratorInvoked` to handle forward navigation
- **Files Modified**:
  - `Microsoft.Midi.Settings/Views/Core Pages/ShellPage.xaml.cs`

### Page Layout Improvements

#### ManagementSessionsPage
- **Description**: Improved session connection display with conditional visibility
- **Changes**:
  - Commented out empty description text block
  - Added conditional visibility for connection list (shows "No active connections" when empty)
  - Added `HasConnections` property to `MidiServiceSessionInformationWrapper`
  - Added `ConnectionCount` observable property with change notifications
- **Files Modified**:
  - `Microsoft.Midi.Settings/Views/Core Pages/ManagementSessionsPage.xaml`
  - `Microsoft.Midi.Settings/ViewModels/Data/MidiServiceSessionInformationWrapper.cs`

#### DeviceDetailPage
- **Description**: Replaced CommandBar with StackPanel for action buttons
- **Changes**:
  - Replaced `CommandBar` with horizontal `StackPanel` containing Monitor and Panic buttons
  - Simplified button layout with consistent spacing
- **Files Modified**:
  - `Microsoft.Midi.Settings/Views/Endpoint Management Pages/DeviceDetailPage.xaml`

#### ForDevelopersPage
- **Description**: Added InfoBar warnings for service restart and reboot requirements
- **Changes**:
  - Added `InfoBar` for service restart required warning
  - Added `InfoBar` for reboot required warning
  - Both warnings use data binding to view model properties
- **Files Modified**:
  - `Microsoft.Midi.Settings/Views/Core Pages/ForDevelopersPage.xaml`

#### HomePage
- **Description**: Unified SettingsCard padding and margin for consistent layout
- **Changes**:
  - Added consistent `Padding="20,0,16,0"` to Common Tasks SettingsCards
  - Added consistent `Padding="25,0,0,0"` to Helpful Links and Useful tools SettingsCards
  - Wrapped TextBlock elements in Grid containers with vertical margin for better spacing
- **Files Modified**:
  - `Microsoft.Midi.Settings/Views/Core Pages/HomePage.xaml`

### Localization Updates

#### Resources.resw
- **Description**: Added new localization strings for UI elements
- **Changes**:
  - Added strings for DeviceDetailPage buttons and labels
  - Added strings for MIDI 1.0 port information labels
  - Added strings for ForDevelopersPage warnings
- **Files Modified**:
  - `Microsoft.Midi.Settings/Strings/en-us/Resources.resw`

### Minor Adjustments

#### PluginsTransportPage
- **Description**: Adjusted description text display settings
- **Changes**:
  - Increased `MaxLines` from 2 to 3 for description text
  - Added `TextTrimming="CharacterEllipsis"` for better text truncation
- **Files Modified**:
  - `Microsoft.Midi.Settings/Views/Core Pages/PluginsTransportPage.xaml`

#### Project File
- **Description**: Updated midi-console project configuration
- **Files Modified**:
  - `midi-console/Midi/Midi.csproj`

---

## Recent Changes (`fix-mouse-side-button-navigation` branch)

### Bug Fixes

#### Mouse Side Button Navigation in Settings Pages
- **Issue**: Mouse side buttons (XButton1/XButton2) for back/forward navigation were not working in SettingsPage and GlobalMidiSettingsPage
- **Root Cause**: ScrollViewer controls in these pages were intercepting pointer events, preventing them from bubbling up to ShellPage's PointerPressed handler
- **Solution**: ~~Added PointerPressed event handlers to ScrollViewer controls in affected pages to handle mouse side button navigation locally~~ Removed duplicate mouse side button handlers from individual pages. The MainWindow.xaml.cs already handles WM_XBUTTONDOWN messages globally for all pages. Having duplicate handlers in both XAML PointerPressed events and Windows message handling caused navigation to trigger twice.
- **Files Modified**:
  - `Microsoft.Midi.Settings/Views/Core Pages/SettingsPage.xaml` (reverted)
  - `Microsoft.Midi.Settings/Views/Core Pages/SettingsPage.xaml.cs` (reverted)
  - `Microsoft.Midi.Settings/Views/Core Pages/GlobalMidiSettingsPage.xaml` (reverted)
  - `Microsoft.Midi.Settings/Views/Core Pages/GlobalMidiSettingsPage.xaml.cs` (reverted)

#### Fixed Double Navigation on Mouse Side Button
- **Issue**: When pressing mouse side button to go back from Settings page, it would navigate back twice (e.g., Settings → Devices → Home instead of Settings → Devices)
- **Root Cause**: Mouse side button events were being handled multiple times:
  1. Page-level PointerPressed event handlers (in SettingsPage/GlobalMidiSettingsPage)
  2. ShellPage-level PointerPressed event handlers
  3. MainWindow-level WM_XBUTTONDOWN Windows message handling
- **Solution**: Removed page-level handlers, kept only MainWindow.xaml.cs global handler which uses Windows message subclassing to capture XBUTTON1/XBUTTON2
- **Files Modified**:
  - `Microsoft.Midi.Settings/Views/Core Pages/SettingsPage.xaml`
  - `Microsoft.Midi.Settings/Views/Core Pages/SettingsPage.xaml.cs`
  - `Microsoft.Midi.Settings/Views/Core Pages/GlobalMidiSettingsPage.xaml`
  - `Microsoft.Midi.Settings/Views/Core Pages/GlobalMidiSettingsPage.xaml.cs`

#### Menu Navigation Stack Behavior
- **Description**: Changed menu navigation to clear navigation history when switching between main sections
- **Behavior**:
  - **Menu clicks**: Navigation stack is cleared (`clearNavigation: true`), so pressing back won't return to previous menu section
  - **Content navigation**: Normal stack behavior, back/forward works within content pages
- **Example**: 
  - Click "Devices" menu → navigate to Devices page (stack cleared)
  - Click a device → navigate to Device Detail (stack: [Devices])
  - Press back → return to Devices (stack: [])
  - Press back again → won't go to previous menu (e.g., Home) because stack is empty
- **Files Modified**:
  - `Microsoft.Midi.Settings/Services/NavigationViewService.cs`
