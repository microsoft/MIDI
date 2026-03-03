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
