# Windows MIDI Settings - 双语支持说明 (Bilingual Support)

## 概述 / Overview

Windows MIDI Settings 应用现已支持中英文双语界面。用户可以在设置页面中切换语言。

The Windows MIDI Settings app now supports bilingual interface (English and Chinese). Users can switch languages in the Settings page.

## 功能特性 / Features

### 1. 支持的语言 / Supported Languages
- **English (United States)** - 英语（美国）
- **中文 (简体)** - Chinese (Simplified)

### 2. 语言切换 / Language Switching
- 在设置页面选择语言后，应用会提示重启以应用新语言
- 语言设置会被保存，下次启动应用时自动使用上次选择的语言
- 首次启动时，应用会根据系统语言自动选择合适的界面语言

When you select a language in the Settings page, the app will prompt you to restart to apply the new language.
Language settings are saved and automatically used the next time you start the app.
On first launch, the app automatically selects the appropriate interface language based on your system language.

## 如何使用 / How to Use

### 切换语言 / Switch Language

1. 打开 Windows MIDI Settings 应用
2. 点击左侧导航栏的"设置" (Settings)
3. 在"语言" (Language) 部分，从下拉列表中选择您喜欢的语言
4. 应用会提示您重启以应用语言更改
5. 点击"立即重启" (Restart Now) 应用新语言

1. Open the Windows MIDI Settings app
2. Click "Settings" in the left navigation panel
3. In the "Language" section, select your preferred language from the dropdown list
4. The app will prompt you to restart to apply the language change
5. Click "Restart Now" to apply the new language

## 技术实现 / Technical Implementation

### 文件结构 / File Structure

```
Microsoft.Midi.Settings/
├── Strings/
│   ├── en-us/                    # 英文资源
│   │   ├── Resources.resw        # 英文字符串资源文件
│   │   └── Resources.Designer.cs # 自动生成的代码
│   └── zh-cn/                    # 中文资源
│       ├── Resources.resw        # 中文字符串资源文件
│       └── Resources.Designer.cs # 自动生成的代码
├── Services/
│   └── LanguageService.cs        # 语言服务
└── ViewModels/
    └── SettingsViewModel.cs      # 设置视图模型（包含语言选择）
```

### 核心组件 / Core Components

#### 1. LanguageService (语言服务)
- 管理当前语言设置
- 提供语言切换功能
- 保存和加载语言偏好

#### 2. 资源文件 (Resource Files)
- `Resources.resw` - 包含所有界面文本的 XML 资源文件
- 支持运行时动态语言切换

#### 3. 设置页面 (Settings Page)
- 添加了语言选择器 (ComboBox)
- 语言切换时自动提示重启应用

## 添加新语言 / Adding New Languages

如需添加其他语言，请遵循以下步骤：

To add additional languages, follow these steps:

1. **创建资源文件夹** / Create Resource Folder
   ```
   Strings/[language-code]/
   ```
   例如：`Strings/fr-fr/` (法语/French)

2. **复制并翻译资源文件** / Copy and Translate Resource Files
   - 复制 `en-us/Resources.resw` 到新文件夹
   - 翻译所有 `<value>` 元素的内容
   - 保持 `name` 属性不变

3. **生成 Designer.cs 文件** / Generate Designer.cs File
   - 可以使用 Visual Studio 自动生成
   - 或手动创建类似 `zh-cn/Resources.Designer.cs` 的文件

4. **更新 .csproj 文件** / Update .csproj File
   ```xml
   <PRIResource Update="Strings\fr-fr\Resources.resw">
     <Generator>ResXFileCodeGenerator</Generator>
     <LastGenOutput>Resources.Designer.cs</LastGenOutput>
     <CustomToolNamespace>Microsoft.Midi.Settings.Strings.fr_fr</CustomToolNamespace>
   </PRIResource>
   ```

5. **在 LanguageService 中添加语言** / Add Language in LanguageService
   ```csharp
   public List<CultureInfo> GetSupportedLanguages()
   {
       return new List<CultureInfo>
       {
           new CultureInfo("en-US"),
           new CultureInfo("zh-CN"),
           new CultureInfo("fr-FR")  // 添加新语言
       };
   }
   ```

6. **在设置页面添加语言选项** / Add Language Option in Settings Page
   ```xml
   <ComboBoxItem Content="Français (France)" Tag="fr-FR"/>
   ```

## 注意事项 / Notes

1. **重启要求** / Restart Requirement
   - 更改语言后需要重启应用才能完全应用新语言
   - 部分界面元素可能在重启前仍显示旧语言

2. **系统默认语言** / System Default Language
   - 首次启动时，应用会检测系统语言
   - 如果系统语言是中文，则默认使用中文界面
   - 否则默认使用英文界面

3. **配置文件位置** / Configuration File Location
   - 语言设置保存在：`%ProgramData%\Microsoft\MIDI\SettingsApp.appconfig.json`

## 翻译贡献 / Translation Contributions

如果您发现翻译问题或有改进建议，欢迎通过以下方式贡献：
- 在 GitHub 上提交 Issue
- 提交 Pull Request 改进翻译

If you find translation issues or have improvement suggestions, contributions are welcome:
- Submit an Issue on GitHub
- Submit a Pull Request to improve translations

## 技术支持 / Technical Support

如有问题，请联系：
- Discord: Windows MIDI and ASIO Discord Server
- GitHub: https://aka.ms/midirepo

For support, contact:
- Discord: Windows MIDI and ASIO Discord Server
- GitHub: https://aka.ms/midirepo
