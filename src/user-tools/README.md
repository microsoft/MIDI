# Tools / 工具

These are generally written in .NET (7 or 8) and provide end-user functionality outside of the DAW or other third-party tools.

这些通常使用 .NET（7 或 8）编写，提供 DAW 或其他第三方工具之外的最终用户功能。

| Component | Description / 描述 |
| --------- | ------------------- |
| midi-settings | The main MIDI Settings app for Windows / Windows 的主要 MIDI 设置应用程序 |
| midi-settings-extensibility | Extensibility model (client plugin model) for the MIDI Settings app / MIDI 设置应用程序的扩展性模型（客户端插件模型） |

---

## Build Notes / 构建说明

### Requirements / 要求
- .NET 10 SDK
- Windows App SDK 1.8+
- Windows SDK 10.0.26100.0

### Fixed Issues / 修复的问题

#### 1. Missing `CommunityToolkit.WinUI.Controls.TitleBar` Package / 缺少 `CommunityToolkit.WinUI.Controls.TitleBar` 包
- **Issue / 问题**: The `TitleBar` control from CommunityToolkit was referenced but the package does not exist on NuGet / 引用了 CommunityToolkit 的 `TitleBar` 控件，但该包在 NuGet 上不存在
- **Solution / 解决方案**: Replaced the `controls:TitleBar` with a standard Grid layout in `ShellPage.xaml` / 在 `ShellPage.xaml` 中将 `controls:TitleBar` 替换为标准 Grid 布局
- **Files Modified / 修改的文件**:
  - `Microsoft.Midi.Settings/Views/Core Pages/ShellPage.xaml`
  - `Microsoft.Midi.Settings/Views/Core Pages/ShellPage.xaml.cs`

#### 2. NU1510 Warnings - Redundant Package References / NU1510 警告 - 冗余的包引用
- **Issue / 问题**: `System.Text.Json` and `System.Text.RegularExpressions` are built-in to .NET 10 and should not be explicitly referenced / `System.Text.Json` 和 `System.Text.RegularExpressions` 是 .NET 10 的内置组件，不应显式引用
- **Solution / 解决方案**: Removed the explicit package references / 移除显式的包引用
- **Files Modified / 修改的文件**:
  - `Microsoft.Midi.Settings/Microsoft.Midi.Settings.csproj`

#### 3. WMC1506 Warnings - OneWay Binding Notifications / WMC1506 警告 - OneWay 绑定通知
- **Issue / 问题**: OneWay bindings require properties to support change notifications, but the MIDI library types do not implement `INotifyPropertyChanged` / OneWay 绑定需要属性支持变更通知，但 MIDI 库类型未实现 `INotifyPropertyChanged`
- **Solution / 解决方案**: Changed binding mode from `OneWay` to `OneTime` for read-only port information / 将只读端口信息的绑定模式从 `OneWay` 改为 `OneTime`
- **Files Modified / 修改的文件**:
  - `Microsoft.Midi.Settings/Views/Endpoint Management Pages/EndpointsAllPage.xaml`

### Build Output / 构建输出
- **Executable / 可执行文件**: `MidiSettings.exe`
- **Output Directory / 输出目录**: `Microsoft.Midi.Settings/bin/x64/Release/net10.0-windows10.0.26100.0/`
- **Status / 状态**: Build successful with 0 warnings / 构建成功，0 个警告

---

## Changes / 更改

### UI Changes / UI 更改

#### Settings Button in NavigationView / 导航视图中的设置按钮
- **Description / 描述**: Moved the Settings button from `PaneFooter` to `FooterMenuItems` for better navigation support and added bottom margin / 将设置按钮从 `PaneFooter` 移到 `FooterMenuItems` 以获得更好的导航支持，并添加底部边距
- **Files Modified / 修改的文件**:
  - `Microsoft.Midi.Settings/Views/Core Pages/ShellPage.xaml`
  - `Microsoft.Midi.Settings/Strings/en-us/Resources.resw`

#### Back Button Visibility / 返回按钮可见性
- **Description / 描述**: Set `NavigationView.IsBackButtonVisible="Collapsed"` and set the custom BackButton `Visibility="Collapsed"` to completely hide the back button / 设置 `NavigationView.IsBackButtonVisible="Collapsed"` 并将自定义 BackButton 的 `Visibility="Collapsed"` 以彻底隐藏返回按钮
- **Files Modified / 修改的文件**:
  - `Microsoft.Midi.Settings/Views/Core Pages/ShellPage.xaml`

---

## Recent Changes / 最近更改

### ShellPage Improvements / ShellPage 改进

#### Search Box / 搜索框
- **Fixed / 修复**: Search box right corner being clipped when window is narrow / 窗口较窄时搜索框右侧圆角被裁切的问题
- **Changes / 更改**:
  - Changed `HorizontalAlignment` from `Left` to `Stretch` / 将 `HorizontalAlignment` 从 `Left` 改为 `Stretch`
  - Removed fixed `Width` property, now uses adaptive width (MaxWidth: 700) / 移除固定的 `Width` 属性，现在使用自适应宽度（最大宽度：700）
  - Added compact mode: shows search icon button when window width < 1020px / 添加紧凑模式：当窗口宽度 < 1020px 时显示搜索图标按钮

#### Title Bar Drag Regions / 标题栏拖拽区域
- **Fixed / 修复**: Drag regions not working properly / 拖拽区域无法正常工作的问题
- **Changes / 更改**:
  - Now correctly calculates drag areas based on actual search box position / 现在根据搜索框实际位置正确计算拖拽区域
  - Updates drag regions when window size changes (with delay for layout completion) / 窗口大小变化时更新拖拽区域（带延迟以确保布局完成）

#### Navigation View / 导航视图
- **Improved / 改进**: Unified content margin between expanded and compact modes / 统一展开模式和紧凑模式下的内容边距
- **Changes / 更改**:
  - Override `NavigationViewContentMargin` and `NavigationViewContentGridMargin` to remove extra top padding / 覆盖 `NavigationViewContentMargin` 和 `NavigationViewContentGridMargin` 以移除额外的顶部边距
  - Set `NavigationFrameContentMargin` to `40,24,40,0` for consistent spacing / 设置 `NavigationFrameContentMargin` 为 `40,24,40,0` 以保持一致间距

#### Focus Management / 焦点管理
- **Improved / 改进**: Set default focus to Home navigation item on startup / 启动时将默认焦点设置到 Home 导航项
- **Benefit / 好处**: Prevents search box from automatically receiving focus / 防止搜索框自动获得焦点

### HomePage Improvements / HomePage 改进

#### InfoBar Visibility / InfoBar 可见性
- **Changed / 更改**: Changed from `IsOpen` to `Visibility` binding / 从 `IsOpen` 绑定改为 `Visibility` 绑定
- **Benefit / 好处**: Three InfoBars (Service availability, First-run setup, SDK Update) now collapse when hidden, no longer occupy space when not visible / 三个 InfoBar（服务可用性、首次运行设置、SDK 更新）在隐藏时现在会折叠，不可见时不再占用空间

### Page Layout Fixes / 页面布局修复

#### Commented Out Empty Description Text Blocks / 注释掉空的描述文本块
- **Pages / 页面**:
  - **ManagementSessionsPage**: Commented out empty description text block / 注释掉空的描述文本块
  - **EndpointsAllPage**: Commented out empty description text block (`DevicesPage_Description`) / 注释掉空的描述文本块 (`DevicesPage_Description`)
  - **EndpointsBasicLoopPage**: Commented out empty description text block / 注释掉空的描述文本块

### Performance Improvements / 性能改进

#### Search Data Preloading / 搜索数据预加载
- **Description / 描述**: Preload search data during app startup to prevent UI blocking / 在应用启动时预加载搜索数据，防止 UI 阻塞
- **Changes / 更改**:
  - Moved `RefreshSearchData()` to `App.OnLaunched()` / 将 `RefreshSearchData()` 移到 `App.OnLaunched()`
  - Search data is now loaded in background thread / 搜索数据现在在后台线程加载
  - Search box cursor no longer delayed when entering search field / 进入搜索框时光标不再延迟

#### SDK Update Check Optimization / SDK 更新检查优化
- **Description / 描述**: Moved SDK update check from ViewModel constructor to app startup / 将 SDK 更新检查从 ViewModel 构造函数移到应用启动
- **Changes / 更改**:
  - `CheckForSdkUpdatesAsync()` now called in `App.OnLaunched()` / `CheckForSdkUpdatesAsync()` 现在在 `App.OnLaunched()` 中调用
  - Prevents HOME page navigation lag / 防止 HOME 页面导航卡顿
  - Uses async/await to avoid blocking UI thread / 使用 async/await 避免阻塞 UI 线程

#### Search Box Text Deletion / 搜索框文本删除
- **Fixed / 修复**: Search results list no longer shows when text is deleted / 删除文本时不再显示搜索结果列表
- **Changes / 更改**:
  - Added empty text check in `SettingsSearchBox_TextChanged` / 在 `SettingsSearchBox_TextChanged` 中添加空文本检查
  - Sets `ItemsSource = null` when text is empty / 文本为空时设置 `ItemsSource = null`

### Page Layout Fixes / 页面布局修复

#### SettingsPage Bottom Margin / 设置页面底部边距
- **Fixed / 修复**: Added bottom margin to prevent content from sticking to window bottom / 添加底部边距防止内容紧贴窗口底部
- **Changes / 更改**:
  - Added `Margin="0,0,0,24"` to StackPanel in SettingsPage.xaml / 在 SettingsPage.xaml 的 StackPanel 添加 `Margin="0,0,0,24"`

### Technical Details / 技术细节
- Window threshold for compact mode: 1124px / 紧凑模式窗口阈值：1124px
- Search box collapse threshold: 650px available width / 搜索框折叠阈值：650px 可用宽度
- Drag region height: 48px (title bar height) / 拖拽区域高度：48px（标题栏高度）
