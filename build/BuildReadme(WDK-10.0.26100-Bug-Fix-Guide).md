# WDK 10.0.26100 Bug Fix Guide / WDK 10.0.26100 错误修复指南

## ⚠️ 项目规格 / Project Specifications

> **重要提示 / Important Notice**
> 
> 在应用 WDK Bug Fix 之前，请确保您已了解以下项目规格和构建要求。
> 
> Before applying the WDK Bug Fix, please ensure you understand the following project specifications and build requirements.

### 项目概述 / Project Overview

| 项目 | 说明 / Description |
|------|-------------------|
| **项目名称** | Windows MIDI Services |
| **项目根目录** | `X:\Path\To\Your\midi` (示例 / Example) |
| **构建系统** | Nuke Build (.NET) |
| **主要语言** | C++, C#, C++/WinRT |
| **目标平台** | Windows 10/11 (x64, Arm64, Arm64EC) |

### 构建工具要求 / Build Tool Requirements

| 工具 | 版本 | 用途 | 必需 |
|------|------|------|------|
| **Visual Studio** | 2022 或更高版本 | 主 IDE 和编译器 | ✅ |
| **Windows SDK** | 10.0.26100.0 或更高 | Windows API 和驱动开发 | ✅ |
| **WDK** | 10.0.26100 | Windows Driver Kit | ✅ |
| **.NET SDK** | 8.0 或更高 | 构建脚本和工具 | ✅ |
| **NuGet** | 最新版 | 包管理 | ✅ |
| **WiX Toolset** | 4.0.2 | 安装程序打包 | ✅ |
| **vcpkg** | 最新版 | C++ 包管理 | 推荐 |

### 环境变量 / Environment Variables

```powershell
# 必须设置 / Required
# 注意：请将路径修改为您的实际项目路径
# Note: Please change the path to your actual project location
$env:MIDI_REPO_ROOT = "X:\Path\To\Your\midi\"  # 项目根目录，用于 WiX 安装程序

# 可选但推荐 / Optional but recommended
# Windows SDK 工具路径
$env:PATH += ";C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64"

# Visual Studio MSBuild 路径 (根据您的 VS 版本调整)
# Visual Studio MSBuild path (adjust according to your VS version)

# Visual Studio 2022
$env:PATH += ";C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin"
# 或 Community 版本 / Or Community edition
# $env:PATH += ";C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin"
# 或 Enterprise 版本 / Or Enterprise edition
# $env:PATH += ";C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin"

# Visual Studio 2026 (版本号 18)
$env:PATH += ";C:\Program Files\Microsoft Visual Studio\18\Professional\MSBuild\Current\Bin"
# 或 Community 版本 / Or Community edition
# $env:PATH += ";C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin"
# 或 Enterprise 版本 / Or Enterprise edition
# $env:PATH += ";C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin"
```

### 构建顺序 / Build Order

```
nuke_build-service (Core service / 核心服务)
        │
        ├──→ nuke_build (SDK components / SDK 组件)
        │           │
        │           └──→ electron-projection (Electron projection / Electron 投影)
        │
        └──→ nuke_build-plugins (Plugin components / 插件组件)
```

> **Note:** Build from top to bottom due to dependencies.
> **注意：** 由于存在依赖关系，请从上到下依次构建。

#### 构建脚本 / Build Scripts
| 文件 | 路径 | 说明 |
|------|------|------|
| Service Build | `build\nuke_build-service\build.ps1` | 核心服务和驱动构建 |
| SDK Build | `build\nuke_build\build.ps1` | SDK 和应用程序构建 |
| Plugins Build | `build\nuke_build-plugins\build.ps1` | 插件组件构建 |

#### 版本配置文件 / Version Configuration Files
| 文件 | 路径 | 说明 |
|------|------|------|
| Service Version | `build\staging\version\Service_BuildNumber.txt` | 服务版本号配置 |
| SDK Version | `build\staging\version\SDK_BuildNumber.txt` | SDK 版本号配置 |
| Plugins Version | `build\staging\version\Plugins_BuildNumber.txt` | 插件版本号配置 |

#### 解决方案文件 / Solution Files
| 解决方案 | 路径 | 说明 |
|---------|------|------|
| API/Service | `src\api\Midi2.sln` | 核心 API 和服务 |
| App SDK | `src\app-sdk\app-sdk.sln` | 应用程序 SDK |
| Plugins | `src\api\midi2-service-component-preview.sln` | 预览版插件 |

### 驱动项目 / Driver Projects

以下项目需要 WDK 10.0.26100 才能构建：

| 项目 | 路径 | 架构 |
|------|------|------|
| MinMidi | `src\api\Drivers\MinMidi\Driver\Driver.vcxproj` | x64, ARM64 |
| MinMidi2 | `src\api\Drivers\MinMidi2\Driver\MinMidi2.vcxproj` | x64, ARM64 |
| USBMIDI2 | `src\api\Drivers\USBMIDI2\Driver\USBMidi2.vcxproj` | x64, ARM64 |

### 支持的构建平台 / Supported Build Platforms

```csharp
// 来自 Build.cs
string[] SdkPlatforms => new string[] { "x64", "Arm64EC" };
string[] ServiceAndApiPlatforms => new string[] { "x64", "Arm64" };
string[] ServiceAndApiPlatformsAll => new string[] { "x64", "Arm64", "Arm64EC" };
string[] ToolsPlatforms => new string[] { "x64", "Arm64" };
string[] InstallerPlatforms => new string[] { "x64", "Arm64" };
```

### 构建配置 / Build Configuration

```csharp
// 默认构建配置
readonly string ServiceBuildConfiguration = Configuration.Release;  // 或 Debug

// 构建类型
enum MidiBuildType
{
    Preview,    // 预览版
    RC,         // 候选发布版
    Stable      // 稳定版
}
```

### 输出目录 / Output Directories

| 目录 | 路径 | 说明 |
|------|------|------|
| 构建输出 | `src\api\vsfiles\` | 编译后的二进制文件 |
| 中间文件 | `src\api\vsfiles\intermediate\` | 编译中间文件 |
| 暂存目录 | `build\staging\` | 打包前的临时文件 |
| 发布目录 | `build\release\` | 最终发布文件 |
| NuGet 包 | `build\release\nuget\` | NuGet 包输出 |

### 构建执行命令 / Build Execution Commands

> **注意 / Note:** 所有构建命令需要在脚本文件所在目录下，以**管理员权限**的命令提示符或 PowerShell 中运行。
> 
> All build commands must be run in the script file's directory, in a Command Prompt or PowerShell with **Administrator privileges**.

#### 1. 构建核心服务 / Build Core Service

```cmd
:: 以管理员身份打开 CMD
:: Open CMD as Administrator

cd "X:\Path\To\Your\midi\build\nuke_build-service"

:: 运行构建 (推荐使用 CMD 文件)
:: Run build (CMD file recommended)
build.cmd
```

#### 2. 构建 SDK 组件 / Build SDK Components

```cmd
:: 以管理员身份打开 CMD
:: Open CMD as Administrator

cd "X:\Path\To\Your\midi\build\nuke_build"

:: 运行构建 (推荐使用 CMD 文件)
:: Run build (CMD file recommended)
build.cmd
```

#### 3. 构建插件组件 / Build Plugin Components

```cmd
:: 以管理员身份打开 CMD
:: Open CMD as Administrator

cd "X:\Path\To\Your\midi\build\nuke_build-plugins"

:: 运行构建 (推荐使用 CMD 文件)
:: Run build (CMD file recommended)
build.cmd
```

#### 4. Electron 投影 (可选) / Electron Projection (Optional)

```cmd
:: 以管理员身份打开 CMD
:: Open CMD as Administrator

cd "X:\Path\To\Your\midi\build\electron-projection"

:: 运行生成脚本
:: Run generation script
gen-projection.cmd
```

#### 快速构建所有 / Quick Build All

```cmd
:: 以管理员身份运行以下命令（按顺序）
:: Run the following commands as Administrator (in order)

cd "X:\Path\To\Your\midi"

:: 步骤 1: 构建核心服务
:: Step 1: Build Core Service
cd "build\nuke_build-service"
build.cmd

:: 步骤 2: 构建 SDK
:: Step 2: Build SDK
cd "..\nuke_build"
build.cmd

:: 步骤 3: 构建插件（可选，如果需要）
:: Step 3: Build Plugins (optional, if needed)
cd "..\nuke_build-plugins"
build.cmd
```

---

## Overview / 概述

When building Windows MIDI Services with Windows Driver Kit (WDK) version 10.0.26100, you may encounter the following error:

在使用 Windows 驱动程序工具包 (WDK) 版本 10.0.26100 构建 Windows MIDI 服务时，您可能会遇到以下错误：

**Error 1: Missing DLL / 错误 1：缺少 DLL**
```
error MSB4062: 未能从程序集 C:\Program Files (x86)\Windows Kits\10\build\10.0.26100.0\bin\Microsoft.DriverKit.Build.Tasks.18.0.dll 
加载任务"ValidateNTTargetVersion"。未能加载文件或程序集"C:\Program Files (x86)\Windows Kits\10\build\10.0.26100.0\bin\Microsoft.DriverKit.Build.Tasks.18.0.dll" 
或它的某一个依赖项。系统找不到指定的文件。
```

**Error 2: INF Verification Exception / 错误 2：INF 验证异常**
```
INF verification exception: 试图加载格式不正确的程序。 (异常来自 HRESULT:0x8007000B)
```

These are known issues with WDK 10.0.26100 where required DLL files are missing or incompatible.

这些是 WDK 10.0.26100 的已知问题，所需的 DLL 文件缺失或不兼容。

---

## Solution / 解决方案

### Copy from Project's Bug Fix Directory (Recommended) / 从项目的 Bug Fix 目录复制（推荐）

This project includes the necessary DLL files in the `WDK 10.0.26100 bug fix file` directory. You can copy them directly to your WDK installation:

本项目在 `WDK 10.0.26100 bug fix file` 目录中包含了必要的 DLL 文件。您可以直接将它们复制到 WDK 安装目录：

#### Step 1: Check if the bug fix files exist / 步骤 1：检查 Bug Fix 文件是否存在

```powershell
Test-Path "D:\DVE\Projects\C++\MIDI\build\WDK 10.0.26100 bug fix file\Microsoft.DriverKit.Build.Tasks.18.0.dll"
```

#### Step 2: Copy the DLL files / 步骤 2：复制 DLL 文件

**PowerShell (Administrator) / PowerShell（管理员）:**

```powershell
# Source directory (relative to this script location) / 源目录（相对于脚本位置）
$sourceDir = "$PSScriptRoot\WDK 10.0.26100 bug fix file"

# Target directory / 目标目录
$targetDir = "C:\Program Files (x86)\Windows Kits\10\build\10.0.26100.0\bin"

# Copy the main DLL files / 复制主 DLL 文件
Copy-Item -Path "$sourceDir\Microsoft.DriverKit.Build.Tasks.18.0.dll" `
            -Destination $targetDir `
            -Force

Copy-Item -Path "$sourceDir\Microsoft.DriverKit.Build.Tasks.PackageVerifier.18.0.dll" `
            -Destination $targetDir `
            -Force

# Copy architecture-specific files (if needed) / 复制架构特定文件（如需要）
Copy-Item -Path "$sourceDir\x64\*" `
            -Destination "$targetDir\x64\" `
            -Force -Recurse

Copy-Item -Path "$sourceDir\x86\*" `
            -Destination "$targetDir\x86\" `
            -Force -Recurse

Copy-Item -Path "$sourceDir\arm64\*" `
            -Destination "$targetDir\arm64\" `
            -Force -Recurse
```

#### Step 3: Verify the fix / 步骤 3：验证修复

```powershell
Test-Path "C:\Program Files (x86)\Windows Kits\10\build\10.0.26100.0\bin\Microsoft.DriverKit.Build.Tasks.18.0.dll"
```

---

## Verification / 验证

After applying the fix, rebuild the project:

应用修复后，重新构建项目：

```powershell
# Navigate to the build directory (relative to project root) / 导航到构建目录（相对于项目根目录）
cd "..\nuke_build-service"

# Run the build / 运行构建
.\build.ps1
```

If the build succeeds without the `MSB4062` error, the fix is working correctly.

如果构建成功且没有 `MSB4062` 错误，则修复工作正常。

---

## Affected Projects / 受影响的项目

The following driver projects are affected by this issue:

以下驱动程序项目受此问题影响：

- `MinMidi\Driver\Driver.vcxproj`
- `MinMidi2\Driver\MinMidi2.vcxproj`
- `USBMIDI2\Driver\USBMidi2.vcxproj`

---

## Additional Notes / 附加说明

### Why This Happens / 为什么会发生

The WDK 10.0.26100 release appears to have a packaging issue where the `Microsoft.DriverKit.Build.Tasks.18.0.dll` and `infverif.dll` (x86) were not included in the distribution. These DLLs are required for validating the NT target version during driver builds.

WDK 10.0.26100 版本似乎存在一个打包问题，`Microsoft.DriverKit.Build.Tasks.18.0.dll` 和 `infverif.dll` (x86) 未包含在分发包中。这些 DLL 是驱动程序构建期间验证 NT 目标版本所必需的。

### Compatibility / 兼容性

The DLL files provided in this project's `WDK 10.0.26100 bug fix file` directory have been tested and work for building Windows MIDI Services. However, always verify your drivers work correctly in a test environment.

本项目 `WDK 10.0.26100 bug fix file` 目录中提供的 DLL 文件已经过测试，可用于构建 Windows MIDI 服务。但是，请始终在测试环境中验证您的驱动程序是否正常工作。

---

## Troubleshooting / 故障排除

### Issue: Access Denied / 问题：访问被拒绝

If you get "Access Denied" when copying files:

如果复制文件时出现"访问被拒绝"：

```powershell
# Take ownership of the directory / 获取目录所有权
takeown /F "C:\Program Files (x86)\Windows Kits\10\build\10.0.26100.0\bin" /R /D Y

# Grant permissions / 授予权限
icacls "C:\Program Files (x86)\Windows Kits\10\build\10.0.26100.0\bin" /grant %username%:F /T

# Try copying again / 再次尝试复制
```

### Issue: DLL Still Not Found / 问题：仍然找不到 DLL

If the build still fails after copying:

如果复制后构建仍然失败：

1. Verify the file was copied correctly / 验证文件是否正确复制
2. Check that the file is not blocked by Windows (right-click → Properties → Unblock) / 检查文件是否被 Windows 阻止（右键单击 → 属性 → 解除阻止）
3. Restart Visual Studio or your build terminal / 重新启动 Visual Studio 或构建终端
4. Clear the NuGet and MSBuild caches / 清除 NuGet 和 MSBuild 缓存：
   ```powershell
   dotnet nuget locals all --clear
   ```

---

## Compiler Warnings Configuration / 编译器警告配置

This section documents the compiler warnings that are intentionally disabled across different project types in Windows MIDI Services, along with the reasons for disabling them.

本节记录了 Windows MIDI 服务中不同类型项目中有意禁用的编译器警告，以及禁用它们的原因。

### C++ Projects / C++ 项目

#### Warning 4471 - Duplicate Definition / 重复定义警告

**Affected Projects / 受影响项目:**
- `USBMidi2\Driver\USBMidi2.vcxproj`
- `MinMidi2\Driver\MinMidi2.vcxproj`

**Reason for Disabling / 禁用原因:**
This warning occurs when header files are included multiple times or when macros are defined repeatedly. This is common in driver development because multiple Windows driver headers need to be included. The warning does not affect functionality.

此警告在头文件被多次包含或宏定义重复时发生。这在驱动程序开发中很常见，因为需要包含多个 Windows 驱动程序头文件。该警告不影响功能。

**Configuration / 配置:**
```xml
<DisableSpecificWarnings>4471;%(DisableSpecificWarnings)</DisableSpecificWarnings>
```

#### Warning 4324 - Structure Padding / 结构体填充警告

**Affected Projects / 受影响项目:**
All Transform and Transport projects / 所有转换器和传输层项目：
- Transform: BS2UMP, MidiMessageTypeFilter, MidiChannel, MidiGroup, Scheduler, UmpProtocolDownscaler, UMP2BS
- Transport: KSAggregate, KS, MidiSrv, Diagnostics, LoopbackMidi, SimpleLoopbackMidi, VirtualMidi

**Reason for Disabling / 禁用原因:**
This warning occurs when structure member alignment causes padding bytes to be inserted. In MIDI data processing, specific memory layouts are required by the MIDI protocol. The padding is intentional for proper data structure alignment.

此警告在结构体成员对齐导致填充字节插入时发生。在 MIDI 数据处理中，MIDI 协议需要特定的内存布局。填充是有意的，用于正确的数据结构对齐。

**Configuration / 配置:**
```xml
<DisableSpecificWarnings>4324</DisableSpecificWarnings>
```

#### Warning C1041 - PDB File Access / PDB 文件访问警告

**Affected Projects / 受影响项目:**
- `Transform\ByteStreamToUMP\Midi2.BS2UMPTransform.vcxproj`

**Reason for Disabling / 禁用原因:**
This error occurs when multiple compiler instances (CL.EXE) try to write to the same program database file (.idb/.pdb) simultaneously during parallel builds. The `/FS` flag (Force Synchronous PDB Writes) allows multiple CL.EXE instances to safely write to the same PDB file concurrently.

此错误在并行构建过程中，多个 CL.EXE 编译器实例同时尝试写入同一个程序数据库文件（.idb/.pdb）时发生。`/FS` 标志（强制同步 PDB 写入）允许多个 CL.EXE 实例安全地并发写入同一个 PDB 文件。

**Configuration / 配置:**
```xml
<AdditionalOptions>/FS %(AdditionalOptions)</AdditionalOptions>
```

### C# .NET Projects / C# .NET 项目

#### Warning CS8618 - Non-nullable Field Not Initialized / 非空字段未初始化

**Affected Projects / 受影响项目:**
- `build\nuke_build\Build.cs`
- `build\nuke_build-service\Build.cs`
- `build\nuke_build-plugins\Build.cs`

**Reason for Disabling / 禁用原因:**
In Nuke build scripts, fields marked with `[Parameter]` attribute are populated at runtime via dependency injection. The compiler cannot recognize this, so it reports the fields as "never assigned." These fields are properly initialized by the Nuke framework during execution.

在 Nuke 构建脚本中，标记有 `[Parameter]` 属性的字段由运行时的依赖注入填充。编译器无法识别这一点，因此报告字段"从未被赋值"。这些字段在运行时被 Nuke 框架正确初始化。

**Fix Applied / 修复方法:**
Initialize fields with default values / 使用默认值初始化字段：
```csharp
// Before / 修复前
string BuildVersionPreviewString;

// After / 修复后
string BuildVersionPreviewString = string.Empty;
```

#### Warning CS8625 - Null Literal to Non-nullable / 无法将 null 转换为非空引用

**Affected Projects / 受影响项目:**
Same as CS8618 / 与 CS8618 相同

**Reason for Disabling / 禁用原因:**
Similar to CS8618, these warnings occur in Nuke build scripts where fields are initialized with `null!` to satisfy the compiler, but the actual values are provided at runtime by the Nuke framework.

与 CS8618 类似，这些警告发生在 Nuke 构建脚本中，字段使用 `null!` 初始化以满足编译器，但实际值由 Nuke 框架在运行时提供。

### .NET Projection Projects / .NET 投影项目

#### Warning CS8618 & CS8305 / 警告 CS8618 和 CS8305

**Affected Projects / 受影响项目:**
- `app-sdk\projections\dotnet-and-cpp\Microsoft.Windows.Devices.Midi2.NetProjection.csproj`

**Reason for Disabling / 禁用原因:**
- **CS8618**: Projection code is auto-generated by CsWinRT tool. The generated code may contain uninitialized non-nullable fields that are populated at runtime by the WinRT runtime. Manually fixing these warnings is not practical.
- **CS8305**: The projection project exposes APIs marked as `[Experimental]`. These are preview features of the MIDI library by design.

- **CS8618**: 投影代码由 CsWinRT 工具自动生成。生成的代码可能包含未初始化的非空字段，这些字段在运行时被 WinRT 运行时填充。手动修复这些警告不切实际。
- **CS8305**: 投影项目暴露了标记为 `[Experimental]` 的 API。这些是 MIDI 库的预览功能，是设计上的。

**Configuration / 配置:**
```xml
<NoWarn>$(NoWarn);CS8618;CS8305</NoWarn>
```

#### Warning CS8305 - Evaluation API Usage / 评估版 API 使用警告

**Affected APIs / 受影响 API:**
- `Microsoft.Windows.Devices.Midi2.Endpoints.Network` namespace
  - `MidiNetworkTransportManager`
  - `MidiNetworkHostCreationConfig`
  - `MidiNetworkClientConnectConfig`
  - `MidiNetworkAdvertisedHost`
  - `MidiNetworkAdvertisedHostWatcher`
  - Other Network MIDI 2.0 related types

**Warning Message / 警告信息:**
```
warning CS8305: 'MidiNetworkTransportManager' is for evaluation purposes only and is subject to change or removal in future updates.
```

**Reason / 原因:**
These APIs are part of the Network MIDI 2.0 feature set which is currently in preview/evaluation phase. Microsoft has marked these APIs with `[Experimental]` attribute to indicate:
1. The API surface may change in future releases
2. The feature is not yet finalized
3. Breaking changes may occur without notice

这些 API 是 Network MIDI 2.0 功能集的一部分，目前处于预览/评估阶段。Microsoft 已使用 `[Experimental]` 属性标记这些 API，以指示：
1. API 表面可能会在未来版本中更改
2. 功能尚未最终确定
3. 可能会在没有通知的情况下发生破坏性更改

**Impact / 影响:**
- **Functionality**: The APIs work correctly for evaluation and development
- **Stability**: APIs may change in future Windows updates
- **Production Use**: Not recommended for production applications without risk assessment

- **功能性**: API 在评估和开发中正常工作
- **稳定性**: API 可能会在未来的 Windows 更新中更改
- **生产使用**: 不建议用于生产应用程序，除非经过风险评估

**Recommended Actions / 建议操作:**
1. **For Evaluation/Development / 用于评估/开发:**
   - Safe to use with warning suppression
   - Monitor Windows MIDI Services updates
   - Test with preview Windows builds

2. **For Production / 用于生产:**
   - Evaluate stability requirements
   - Implement abstraction layers to isolate API dependencies
   - Plan for potential API changes
   - Subscribe to Windows MIDI Services announcements

3. **Staying Updated / 保持更新:**
   - Follow [Windows MIDI Services GitHub](https://github.com/microsoft/MIDI)
   - Join [Windows MIDI Services Discord](https://aka.ms/MidiDiscord)
   - Check [Windows Developer Blog](https://blogs.windows.com/windowsdeveloper/)

**Configuration / 配置:**
```xml
<PropertyGroup>
  <!-- Suppress evaluation API warnings / 抑制评估版 API 警告 -->
  <NoWarn>$(NoWarn);CS8305</NoWarn>
</PropertyGroup>
```

**Alternative: Using #pragma / 替代方法：使用 #pragma:**
```csharp
// Suppress warning for specific file / 为特定文件抑制警告
#pragma warning disable CS8305
using Microsoft.Windows.Devices.Midi2.Endpoints.Network;
#pragma warning restore CS8305

// Or for specific line / 或为特定行
#pragma warning disable CS8305
var host = MidiNetworkTransportManager.CreateNetworkHostAsync(config);
#pragma warning restore CS8305
```

### C# Sample Projects / C# 示例项目

#### Warning MSB3277 - Assembly Version Conflict / 程序集版本冲突

**Affected Projects / 受影响项目:**
- `samples\csharp-net\virtual-device-app-winui\virtual-device-app-winui-cs.csproj`

**Warning Message / 警告信息:**
```
warning MSB3277: 发现无法解析的"WindowsBase"的不同版本之间存在冲突。
"WindowsBase, Version=4.0.0.0"与"WindowsBase, Version=5.0.0.0"之间存在冲突。
```

**Reason / 原因:**
This warning occurs when different versions of the same assembly are referenced. In this case:
- The project targets `net10.0-windows10.0.26100.0` which references `WindowsBase, Version=4.0.0.0`
- The `Microsoft.Web.WebView2.Wpf.dll` (net5.0-windows10.0.17763.0) depends on `WindowsBase, Version=5.0.0.0`

MSBuild automatically resolves the conflict by selecting the primary version (4.0.0.0).

此警告在引用了同一程序集的不同版本时发生。在本例中：
- 项目目标框架 `net10.0-windows10.0.26100.0` 引用了 `WindowsBase, Version=4.0.0.0`
- `Microsoft.Web.WebView2.Wpf.dll` (net5.0-windows10.0.17763.0) 依赖于 `WindowsBase, Version=5.0.0.0`

MSBuild 通过选择主版本 (4.0.0.0) 自动解决冲突。

**Impact / 影响:**
- This is a **warning**, not an error - the build continues and succeeds
- The application functions correctly at runtime
- The conflict is automatically resolved by MSBuild

**Resolution / 解决方案:**
No action required. This warning is expected when using WebView2 in WinUI 3 applications targeting .NET 10. The MSBuild conflict resolution selects the appropriate version.

无需操作。在面向 .NET 10 的 WinUI 3 应用程序中使用 WebView2 时，预计会出现此警告。MSBuild 冲突解决会选择适当的版本。

---

### PowerShell Projection Projects / PowerShell 投影项目

#### Warning CS8012 - Assembly Reference Mismatch / 程序集引用不匹配

**Affected Projects / 受影响项目:**
- `app-sdk\projections\powershell\WindowsMidiServices.csproj`

**Reason for Disabling / 禁用原因:**
This warning occurs when referenced assemblies target different frameworks than the current project. PowerShell modules often need to reference assemblies with different target frameworks. This is standard practice in PowerShell module development.

此警告在引用的程序集目标框架与当前项目不匹配时发生。PowerShell 模块经常需要引用不同目标框架的程序集。这是 PowerShell 模块开发的标准做法。

**Configuration / 配置:**
```xml
<NoWarn>$(NoWarn);CS8012</NoWarn>
```

### MIDI Settings App / MIDI 设置应用

#### Warning CS8012 - Assembly Reference Mismatch / 程序集引用不匹配

**Affected Projects / 受影响项目:**
- `user-tools\midi-settings\Microsoft.Midi.Settings\Microsoft.Midi.Settings.csproj`

**Warning Message / 警告信息:**
```
warning CS8012: 引用程序集"Microsoft.Windows.Devices.Midi2.NetProjection, Version=1.0.15.0, Culture=neutral, PublicKeyToken=null"面向的是另一个处理器。
```

**Reason / 原因:**
This warning occurs during multi-platform builds (x64/ARM64) when referenced assemblies target different processor architectures. The `Microsoft.Windows.Devices.Midi2.NetProjection` assembly may be built for a specific architecture, causing this warning when the project is built for a different platform.

此警告在多平台构建（x64/ARM64）期间发生，当引用的程序集针对不同的处理器架构时。`Microsoft.Windows.Devices.Midi2.NetProjection` 程序集可能是为特定架构构建的，当项目为不同平台构建时会导致此警告。

**Impact / 影响:**
- This is a **warning**, not an error - the build continues and succeeds
- The application functions correctly at runtime
- Common in cross-platform WinUI 3 applications

**Resolution / 解决方案:**
Add CS8012 to the `NoWarn` property in the project file:

```xml
<PropertyGroup>
  <NoWarn>$(NoWarn);CS8012</NoWarn>
</PropertyGroup>
```

---

#### Warning MVVMTK0045 - MVVM Toolkit AOT Compatibility / MVVM Toolkit AOT 兼容性

**Affected Projects / 受影响项目:**
- `user-tools\midi-settings\Microsoft.Midi.Settings\ViewModels\Data\MidiEndpointWrapper.cs`

**Reason for Disabling / 禁用原因:**
This warning occurs when using `[ObservableProperty]` fields in WinUI 3 applications, which are not AOT-compatible. The fix is to convert fields to partial properties.

此警告在 WinUI 3 应用程序中使用 `[ObservableProperty]` 字段时发生，这些字段不是 AOT 兼容的。修复方法是将字段转换为部分属性。

**Fix Applied / 修复方法:**
```csharp
// Before / 修复前
[ObservableProperty]
private bool hasManufacturerName;

// After / 修复后
[ObservableProperty]
public partial bool HasManufacturerName { get; set; }
```

#### Warning CS8305 - Preview/Evaluation API Usage / 预览版/评估版 API 使用

**Affected Projects / 受影响项目:**
- `user-tools\midi-settings\Microsoft.Midi.Settings\*.cs`
- Especially files using Network MIDI 2.0 APIs / 特别是使用 Network MIDI 2.0 API 的文件

**Specific APIs Causing Warnings / 导致警告的特定 API:**
```csharp
// Network MIDI 2.0 APIs (Evaluation) / Network MIDI 2.0 API（评估版）
using Microsoft.Windows.Devices.Midi2.Endpoints.Network;

// Types that trigger CS8305 / 触发 CS8305 的类型:
- MidiNetworkTransportManager
- MidiNetworkHostCreationConfig
- MidiNetworkClientConnectConfig
- MidiNetworkClientDisconnectConfig
- MidiNetworkAdvertisedHost
- MidiNetworkAdvertisedHostWatcher
- MidiNetworkConfiguredHost
- MidiNetworkConfiguredClient
- MidiLoopbackEndpointManager (if marked experimental)
- MidiBasicLoopbackEndpointManager (if marked experimental)
```

**Reason for Disabling / 禁用原因:**
The MIDI Settings app uses preview/evaluation APIs from the MIDI library (marked as `[Experimental]`). These are intentional preview features that:
1. Are subject to change in future Windows updates
2. May have different behavior across Windows versions
3. Are provided for early evaluation and feedback

MIDI 设置应用使用了 MIDI 库的预览版/评估版 API（标记为 `[Experimental]`）。这些是设计上的预览功能：
1. 可能会在未来的 Windows 更新中更改
2. 在不同 Windows 版本中可能有不同的行为
3. 用于早期评估和反馈

**Warning Examples / 警告示例:**
```
warning CS8305: 'MidiNetworkTransportManager' is for evaluation purposes only and is subject to change or removal in future updates.
warning CS8305: 'MidiNetworkHostCreationConfig' is for evaluation purposes only and is subject to change or removal in future updates.
warning CS8305: 'MidiNetworkAdvertisedHostWatcher' is for evaluation purposes only and is subject to change or removal in future updates.
```

**Configuration / 配置:**
```xml
<PropertyGroup>
  <!-- Suppress evaluation API warnings / 抑制评估版 API 警告 -->
  <NoWarn>$(NoWarn);CS8305;CS8618</NoWarn>
</PropertyGroup>
```

**Note / 注意:**
While these warnings are suppressed for the build to succeed, developers should be aware that:
- Network MIDI 2.0 features are actively evolving
- Code using these APIs may need updates when the API surface changes
- Testing with latest Windows Insider builds is recommended

虽然这些警告被抑制以使构建成功，但开发人员应注意：
- Network MIDI 2.0 功能正在积极开发中
- 当 API 表面更改时，使用这些 API 的代码可能需要更新
- 建议使用最新的 Windows Insider 版本进行测试

## References / 参考

- [Windows Driver Kit Documentation](https://docs.microsoft.com/en-us/windows-hardware/drivers/)
- [Windows MIDI Services Documentation](https://aka.ms/midi)
- [Windows MIDI Services Discord](https://aka.ms/MidiDiscord)

---

> **文档作者 / Document Author:** `https://github.com/Crostia/MIDI`

*Last Updated / 最后更新: 2026-03-01*