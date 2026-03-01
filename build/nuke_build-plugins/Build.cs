using Microsoft.ApplicationInsights.Extensibility;
using Microsoft.Build.Locator;
using Nuke.Common;
using Nuke.Common.CI;
using Nuke.Common.Execution;
using Nuke.Common.IO;
using Nuke.Common.ProjectModel;
using Nuke.Common.Tooling;
using Nuke.Common.Tools.DotNet;
using Nuke.Common.Tools.GitVersion;
using Nuke.Common.Tools.MSBuild;
using Nuke.Common.Tools.Npm;
using Nuke.Common.Tools.NuGet;
using Nuke.Common.Utilities.Collections;
using Serilog;
using System;
using System.Collections.Generic;
using System.Configuration;
using System.IO;
using System.Linq;
using System.Xml;
using static Nuke.Common.EnvironmentInfo;
//using static Nuke.Common.IO.FileSystemTasks;
using static Nuke.Common.IO.PathConstruction;

class Build : NukeBuild
{
    /// Support plugins are available for:
    ///   - JetBrains ReSharper        https://nuke.build/resharper
    ///   - JetBrains Rider            https://nuke.build/rider
    ///   - Microsoft VisualStudio     https://nuke.build/visualstudio
    ///   - Microsoft VSCode           https://nuke.build/vscode


    // ===========================================================
    //[GitVersion]
    //readonly GitVersion MasterBuildVersion;

    enum MidiBuildType
    {
        Preview,
        RC,
        Stable
    }

    MSBuildVerbosity BuildVerbosity => MSBuildVerbosity.Minimal;
    LogLevel LoggingLevel => LogLevel.Normal;

    // --------------------------------------------------------------------------------------
    // Version information to change 
    // --------------------------------------------------------------------------------------

    MidiBuildType BuildType = MidiBuildType.Preview;  // 从 Plugins_BuildNumber.txt 读取，默认为 Preview
    
    UInt16 BuildVersionMajor;             // 从 Plugins_BuildNumber.txt 读取
    UInt16 BuildVersionMinor;             // 从 Plugins_BuildNumber.txt 读取
    UInt16 BuildVersionPatch;             // 从 Plugins_BuildNumber.txt 读取
    UInt16 BuildVersionPreviewNumber;     // 从 Plugins_BuildNumber.txt 读取
    UInt16 BuildVersionPreviewMinorNumber; // 从 Plugins_BuildNumber.txt 读取
    UInt16 BuildVersionPreviewBuildNumber; // 从 Plugins_BuildNumber.txt 读取
    bool BuildVersionPreviewHasMinorNumber; // 用户是否设置了 Minor 版本号（区分 15.0 和 15.0.0 格式）
    string VersionName => BuildType == MidiBuildType.Stable 
        ? "Plugin " + BuildMajorMinorPatch 
        : BuildType == MidiBuildType.RC 
            ? "Plugin Release Candidate " + BuildVersionPreviewNumber 
            : "Plugin Preview " + BuildVersionPreviewNumber;

    // --------------------------------------------------------------------------------------

    string BuildMajorMinorPatch => BuildVersionMajor.ToString() + "." + BuildVersionMinor.ToString() + "." + BuildVersionPatch.ToString();

    string BuildVersionPreviewString = string.Empty;
    string BuildVersionFullString = string.Empty;
    string BuildVersionAssemblyFullString = string.Empty;
    string BuildVersionFileFullString = string.Empty;

    string NugetPackageId => "Microsoft.Windows.Devices.Midi2";
    string NugetPackageVersion = string.Empty;
    string NugetFullPackageIdWithVersion = string.Empty;


    const string TargetWindowsSdkVersion = "net10.0-windows";

    DateTime BuildDate;


    // can't release debug versions unless you copy over the debug runtimes
    // which makes a mess. So release-only.
    //readonly string ServiceBuildConfiguration = Configuration.Debug;
    readonly string ServiceBuildConfiguration = Configuration.Release;




    //string SetupBundleFullVersionString => BuildMajorMinorRevision + "-" + NuGetVersionName + "." + BuildDateNumber + "-" + BuildTimeNumber;


    AbsolutePath _thisReleaseFolder = null!;


    //string[] AppSdkAssemblies => new string[] {
    //    "Microsoft.Windows.Devices.Midi2",
    //    "Microsoft.Windows.Devices.Midi2.Initialization"
    //};


    // ===========================================================
    // directories

    AbsolutePath BuildRootFolder => NukeBuild.RootDirectory / "build";

    AbsolutePath StagingRootFolder => BuildRootFolder / "staging";
    AbsolutePath ApiStagingFolder => StagingRootFolder / "api";


    AbsolutePath ReleaseRootFolder => BuildRootFolder / "release";
    AbsolutePath AppSdkNugetOutputFolder => ReleaseRootFolder / "nuget";

    AbsolutePath ThisReleaseFolder => _thisReleaseFolder;


    AbsolutePath SourceRootFolder => NukeBuild.RootDirectory / "src";
    AbsolutePath AppSdkSolutionFolder => SourceRootFolder / "app-sdk";
    AbsolutePath ApiSolutionFolder => SourceRootFolder / "api";



    AbsolutePath NetworkMidiSetupSolutionFolder => SourceRootFolder / "oob-setup-network";
    AbsolutePath VirtualPatchBaySetupSolutionFolder => SourceRootFolder / "oob-setup-virtual-patch-bay";
    AbsolutePath BasicLoopbackSetupSolutionFolder => SourceRootFolder / "oob-setup-basic-loopback";


    AbsolutePath ApiReferenceFolder => SourceRootFolder / "shared" / "api-ref";

    AbsolutePath UserToolsRootFolder => SourceRootFolder / "user-tools";



    AbsolutePath SetupBundleInfoIncludeFile => StagingRootFolder / "version" / "BundleInfo.wxi";
    AbsolutePath BuildVersionFile => StagingRootFolder / "version" / "Plugins_BuildNumber.txt";

    AbsolutePath SdkVersionFilesFolder => StagingRootFolder / "version";

    AbsolutePath SamplesRootFolder => NukeBuild.RootDirectory / "samples";

    
    string[] ServiceAndApiPlatforms => new string[] { "x64", "Arm64" };
    string[] InstallerPlatforms => new string[] { "x64", "Arm64" };



    Dictionary<string, string> BuiltNetworkMidiInstallers = new Dictionary<string, string>();
    Dictionary<string, string> BuiltVirtualPatchBayInstallers = new Dictionary<string, string>();
    Dictionary<string, string> BuiltSimpleLoopbackInstallers = new Dictionary<string, string>();


    public static int Main () => Execute<Build>(x => x.BuildAndPublishAll);


    string MSBuildPath = string.Empty;

    void SetMSBuildVersion()
    {
        // 首先使用 vswhere 检测 VS 实例（支持所有版本，包括最新的 VS 2026+）
        string? vswhereMsBuildPath = GetMSBuildPathFromVsWhere();
        if (!string.IsNullOrEmpty(vswhereMsBuildPath))
        {
            Console.WriteLine($"Using VS from vswhere: {vswhereMsBuildPath}");
            MSBuildPath = vswhereMsBuildPath;
            MSBuildTasks.MSBuildPath = MSBuildPath;
            return;
        }

        // 如果 vswhere 没有找到，使用 MSBuildLocator 作为后备
        Console.WriteLine("vswhere did not find VS installation, falling back to MSBuildLocator...");

        var allInstances = MSBuildLocator.QueryVisualStudioInstances()
            .OrderByDescending(instance => instance.Version)
            .ToList();

        Console.WriteLine("All discovered instances:");
        foreach (var inst in allInstances)
        {
            Console.WriteLine($"Instance Found: {inst.Version.ToString()} (DiscoveryType: {inst.DiscoveryType})");
            Console.WriteLine($"- MSBuild Path: {inst.MSBuildPath}");
            Console.WriteLine($"- Visual Studio Path: {inst.VisualStudioRootPath}");
            Console.WriteLine();
        }

        // 优先选择 VisualStudioSetup 类型的实例
        var vsInstances = allInstances
            .Where(instance => instance.DiscoveryType == DiscoveryType.VisualStudioSetup);

        var selectedInstance = vsInstances.FirstOrDefault() ?? allInstances.FirstOrDefault();

        if (selectedInstance == null)
        {
            throw new InvalidOperationException("No MSBuild instance found. Please ensure Visual Studio or .NET SDK is installed.");
        }

        MSBuildPath = selectedInstance.MSBuildPath;

        Console.WriteLine($"Using: {MSBuildPath}");
        Console.WriteLine();

        MSBuildTasks.MSBuildPath = MSBuildPath;
    }

    string? GetMSBuildPathFromVsWhere()
    {
        try
        {
            var vswherePath = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
                "Microsoft Visual Studio", "Installer", "vswhere.exe");

            if (!File.Exists(vswherePath))
            {
                Console.WriteLine("vswhere.exe not found.");
                return null;
            }

            // 使用 vswhere 查找最新的 VS 安装
            var process = new System.Diagnostics.Process
            {
                StartInfo = new System.Diagnostics.ProcessStartInfo
                {
                    FileName = vswherePath,
                    Arguments = "-latest -products * -requires Microsoft.Component.MSBuild -property installationPath",
                    RedirectStandardOutput = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                }
            };

            process.Start();
            string output = process.StandardOutput.ReadToEnd().Trim();
            process.WaitForExit();

            if (string.IsNullOrEmpty(output) || !Directory.Exists(output))
            {
                Console.WriteLine("No Visual Studio installation found via vswhere.");
                return null;
            }

            // 构建 MSBuild 路径
            string msbuildPath = Path.Combine(output, "MSBuild", "Current", "Bin", "MSBuild.exe");

            if (File.Exists(msbuildPath))
            {
                Console.WriteLine($"Found VS installation via vswhere: {output}");
                return msbuildPath;
            }

            // 尝试 amd64 版本
            msbuildPath = Path.Combine(output, "MSBuild", "Current", "Bin", "amd64", "MSBuild.exe");
            if (File.Exists(msbuildPath))
            {
                Console.WriteLine($"Found VS installation (amd64) via vswhere: {output}");
                return msbuildPath;
            }

            Console.WriteLine("MSBuild.exe not found in VS installation.");
            return null;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error running vswhere: {ex.Message}");
            return null;
        }
    }

    Target T_Prerequisites => _ => _
        .Executes(() =>
        {
            Logging.Level = LoggingLevel;

            SetMSBuildVersion();

            BuildDate = DateTime.Today;

            // Need to verify that %MIDI_REPO_ROOT% is set (it's used by setup). If not, set it to the root \midi folder
            var rootVariableExists = !string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("MIDI_REPO_ROOT"));

            if (!rootVariableExists)
            {
                Environment.SetEnvironmentVariable("MIDI_REPO_ROOT", NukeBuild.RootDirectory.ToString().TrimEnd('/', '\\') + "\\");
            }

            // Setup vcpkg: Check environment variable, vcpkg existence, and VS integration
            SetupVcpkg();

            // this updates the build number and writes hte new value to the file
            IncrementBuildNumber();

            if (BuildType == MidiBuildType.Stable)
            {
                BuildVersionPreviewString = "";
                NugetPackageVersion = BuildMajorMinorPatch;
            }
            else if (BuildType == MidiBuildType.RC)
            {
                // 兼容两种格式：
                // - 15.0 格式 -> rc.15.0 (用户没有设置 Minor)
                // - 15.0.0 格式 -> rc.15.0.0 (用户设置了 Minor，即使为 0 也要显示)
                if (BuildVersionPreviewHasMinorNumber)
                {
                    BuildVersionPreviewString = "rc." + BuildVersionPreviewNumber + "." + BuildVersionPreviewMinorNumber + "." + BuildVersionPreviewBuildNumber;
                }
                else
                {
                    BuildVersionPreviewString = "rc." + BuildVersionPreviewNumber + "." + BuildVersionPreviewBuildNumber;
                }
                NugetPackageVersion = BuildMajorMinorPatch + "-" + BuildVersionPreviewString;
            }
            else if (BuildType == MidiBuildType.Preview)
            {
                // 兼容两种格式：
                // - 15.0 格式 -> preview.15.0 (用户没有设置 Minor)
                // - 15.0.0 格式 -> preview.15.0.0 (用户设置了 Minor，即使为 0 也要显示)
                if (BuildVersionPreviewHasMinorNumber)
                {
                    BuildVersionPreviewString = "preview." + BuildVersionPreviewNumber + "." + BuildVersionPreviewMinorNumber + "." + BuildVersionPreviewBuildNumber;
                }
                else
                {
                    BuildVersionPreviewString = "preview." + BuildVersionPreviewNumber + "." + BuildVersionPreviewBuildNumber;
                }
                NugetPackageVersion = BuildMajorMinorPatch + "-" + BuildVersionPreviewString;
            }

            // they are the same, for our use here.
            BuildVersionFullString = NugetPackageVersion;

            BuildVersionAssemblyFullString = BuildMajorMinorPatch + "." + BuildVersionPreviewBuildNumber;
            BuildVersionFileFullString = BuildMajorMinorPatch + "." + BuildVersionPreviewBuildNumber;

            NugetFullPackageIdWithVersion = NugetPackageId + "." + NugetPackageVersion;

            _thisReleaseFolder = $"{ReleaseRootFolder / VersionName} ({DateTime.Now.ToString("yyyy-MM-dd HH-mm-ss")})";

            // create the release folder
            Directory.CreateDirectory(ThisReleaseFolder.ToString());


            Console.WriteLine($"Building {BuildVersionFullString}");

        });


    Target T_CreateVersionIncludes => _ => _
        .DependsOn(T_Prerequisites)
        .Executes(() =>
        {
            string buildSource = "GitHub Preview";
            string versionName = VersionName;
            //string versionString = BuildVersionFullString;

            string buildVersionMajor = BuildVersionMajor.ToString();
            string buildVersionMinor = BuildVersionMinor.ToString();
            string buildVersionPatch = BuildVersionPatch.ToString();

            string buildDate = BuildDate.ToString("yyyy-MM-dd");

            // create directories if they do not exist

            ThisReleaseFolder.CreateDirectory();
            SdkVersionFilesFolder.CreateDirectory();

            var msbuildProperties = new Dictionary<string, object>();
            msbuildProperties.Add("MidiBuildType", $"{BuildType.ToString().ToLower()}");
            msbuildProperties.Add("MidiBuildSource", $"{buildSource}");
            msbuildProperties.Add("MidiVersionName", $"{versionName}");
            msbuildProperties.Add("MidiBuildVersionMajor", BuildVersionMajor);
            msbuildProperties.Add("MidiBuildVersionMinor", BuildVersionMinor);
            msbuildProperties.Add("MidiBuildVersionPatch", BuildVersionPatch);
            msbuildProperties.Add("MidiBuildVersionBuildNumber", BuildVersionPreviewBuildNumber);
            msbuildProperties.Add("MidiBuildVersionPreviewString", $"{BuildVersionPreviewString}");

            msbuildProperties.Add("MidiVersionOutputFolder", (StagingRootFolder / "version").ToString());

            MSBuildTasks.MSBuild(_ => _
                .SetTargetPath(SourceRootFolder / "build-gen-version-includes" / "GenVersionIncludes.csproj")
                .SetMaxCpuCount(null)
                .AddProcessAdditionalArguments("/t:TransformAll")
                .SetProperties(msbuildProperties)
                .SetVerbosity(MSBuildVerbosity.Normal)
                .EnableNodeReuse()
            );

        });


    Target T_BuildInDevelopmentServicePlugins => _ => _
        .DependsOn(T_CreateVersionIncludes)
        .DependsOn(T_Prerequisites)
        .Executes(() =>
        {
            foreach (var platform in ServiceAndApiPlatforms)
            {
                string solutionDir = ApiSolutionFolder.ToString() + @"\";

                var msbuildProperties = new Dictionary<string, object>();
                msbuildProperties.Add("Platform", platform);
                msbuildProperties.Add("SolutionDir", solutionDir);  // to include trailing slash
                msbuildProperties.Add("NoWarn", "MIDL2111");        // IDL identifier length warning

                Console.Out.WriteLine($"----------------------------------------------------------------------");
                Console.Out.WriteLine($"SolutionDir: {solutionDir}");
                Console.Out.WriteLine($"Platform:    {platform}");

                NuGetTasks.NuGetRestore(_ => _
                    .SetTargetPath(ApiSolutionFolder / "midi2-service-component-preview.sln")
                    .SetSource(@"https://api.nuget.org/v3/index.json")
                );

                MSBuildTasks.MSBuild(_ => _
                    .SetTargetPath(ApiSolutionFolder / "midi2-service-component-preview.sln")
                    .SetMaxCpuCount(null)
                    /*.SetOutDir(outputFolder) */
                    /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                    /*.SetTargets("Build") */
                    .SetProperties(msbuildProperties)
                    .SetConfiguration(ServiceBuildConfiguration)
                    .SetVerbosity(BuildVerbosity)
                    .EnableNodeReuse()
                );

                // copy binaries to staging folder
                var stagingFiles = new List<AbsolutePath>();

                // only in-proc files, like the MidiSrvTransport, are Arm64EC
                //var servicePlatform = (platform == "Arm64EC" || platform == "Arm64") ? "Arm64" : "x64";
                var servicePlatform = platform;

                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.NetworkMidiTransport.dll");
                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.NetworkMidiTransport.pdb");

                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.VirtualPatchBayTransport.dll");
                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.VirtualPatchBayTransport.pdb");

                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.BasicLoopbackMidiTransport.dll");
                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.BasicLoopbackMidiTransport.pdb");

                foreach (var file in stagingFiles)
                {
                    file.CopyToDirectory(ApiStagingFolder / servicePlatform, ExistsPolicy.FileOverwrite);
                }
            }
        });




    // 默认版本配置内容
    const string DefaultVersionFileContent = @"# Windows MIDI Services Plugins Version Configuration File
# Windows MIDI Services Plugins 版本配置文件
# Modify this file to update the version number without changing the code
# 修改此文件即可更新版本号，无需修改代码
#
# Format:
# 格式说明：
# Line 1: Build Type (preview, rc, stable) [Required]
# 第1行：构建类型（preview、rc、stable）[必需]
# Line 2: Major.Minor.Patch (e.g.: 0.0.15) [Required]
# 第2行：主版本号.次版本号.补丁版本号（例如：0.0.15）[必需]
# Line 3: Preview.Minor.Build (e.g.: 15.0.0 or 15.0) [Optional]
# 第3行：预览版本号.次版本号.构建号（例如：15.0.0 或 15.0）[可选]
#
# Note:
# 注意：
# Lines starting with # are treated as comments and will be ignored
# 以 # 开头的行被视为注释，会被忽略
#
# Example:
# 示例：preview 0.0.15 15.0.0 -> 0.0.15-preview.15.0.0

preview
0.0.15
15.0.0
";

    void IncrementBuildNumber()
    {
        // 从版本文件中读取版本信息
        // 文件格式：
        // 第1行：构建类型 (preview, rc, stable) [必需]
        // 第2行：主版本号.次版本号.补丁版本号 (必需，例如: 0.0.15)
        // 第3行：预览版本号.次版本号.构建号 (可选，支持 15.0.0 或 15.0 格式，默认为 15.0.0)
        // 以 # 开头的行被视为注释，会被忽略
        
        // 如果版本文件不存在，创建默认版本文件
        if (!File.Exists(BuildVersionFile))
        {
            Log.Warning($"Version file not found: {BuildVersionFile}");
            Log.Warning("Creating default version file... / 正在创建默认版本文件...");
            
            try
            {
                // 确保目录存在
                Directory.CreateDirectory(Path.GetDirectoryName(BuildVersionFile)!);
                // 写入默认版本文件
                File.WriteAllText(BuildVersionFile, DefaultVersionFileContent);
                Log.Information($"Default version file created: {BuildVersionFile}");
            }
            catch (Exception ex)
            {
                Log.Error($"Failed to create version file: {ex.Message}");
                Log.Error("Using built-in default version / 使用内置默认版本");
            }
        }
        
        try
        {
            if (File.Exists(BuildVersionFile))
            {
                // 读取所有非注释行
                var validLines = File.ReadAllLines(BuildVersionFile)
                    .Where(line => !string.IsNullOrWhiteSpace(line) && !line.TrimStart().StartsWith("#"))
                    .ToArray();
                
                // 需要至少第1行和第2行
                if (validLines.Length >= 2)
                {
                    bool hasError = false;
                    
                    // 读取第1行：构建类型 (preview, rc, stable)
                    var buildTypeString = validLines[0].Trim().ToLower();
                    switch (buildTypeString)
                    {
                        case "preview":
                            BuildType = MidiBuildType.Preview;
                            break;
                        case "rc":
                            BuildType = MidiBuildType.RC;
                            break;
                        case "stable":
                            BuildType = MidiBuildType.Stable;
                            break;
                        default:
                            Log.Error($"Invalid build type in line 1: '{validLines[0]}'");
                            Log.Error("Expected: preview, rc, or stable / 期望：preview、rc 或 stable");
                            hasError = true;
                            break;
                    }
                    
                    // 读取第2行版本号 (Major.Minor.Patch)
                    var versionParts = validLines[1].Trim().Split('.');
                    if (versionParts.Length == 3 &&
                        ushort.TryParse(versionParts[0], out ushort major) &&
                        ushort.TryParse(versionParts[1], out ushort minor) &&
                        ushort.TryParse(versionParts[2], out ushort patch))
                    {
                        BuildVersionMajor = major;
                        BuildVersionMinor = minor;
                        BuildVersionPatch = patch;
                    }
                    else
                    {
                        Log.Error($"Invalid version format in line 2: '{validLines[1]}'");
                        Log.Error("Expected format: Major.Minor.Patch (e.g. 0.0.15) / 期望格式: 主版本.次版本.补丁 (例如 0.0.15)");
                        hasError = true;
                    }
                    
                    // 第3行是可选的，格式为 Preview.Minor.Build 或 Preview.Build
                    if (validLines.Length >= 3)
                    {
                        var previewBuildParts = validLines[2].Trim().Split('.');
                        
                        // 读取预览版本号（第1部分）
                        if (previewBuildParts.Length >= 1 && ushort.TryParse(previewBuildParts[0].Trim(), out ushort previewNumber))
                        {
                            BuildVersionPreviewNumber = previewNumber;
                        }
                        else
                        {
                            Log.Error($"Invalid preview number in line 3: '{validLines[2]}'");
                            Log.Error("Expected format: Preview.Minor.Build (e.g. 15.0.0 or 15.0) / 期望格式: 预览版本.次版本.构建号 (例如 15.0.0 或 15.0)");
                            hasError = true;
                        }
                        
                        // 读取次版本号和构建号
                        // 支持 Preview.Minor.Build (15.0.0) 或 Preview.Build (15.0) 格式
                        if (previewBuildParts.Length >= 3)
                        {
                            // 15.0.0 格式: Preview=15, Minor=0, Build=0
                            BuildVersionPreviewHasMinorNumber = true;
                            if (ushort.TryParse(previewBuildParts[1].Trim(), out ushort previewMinor))
                            {
                                BuildVersionPreviewMinorNumber = previewMinor;
                            }
                            else
                            {
                                BuildVersionPreviewMinorNumber = 0;
                            }
                            if (ushort.TryParse(previewBuildParts[2].Trim(), out ushort buildNumber))
                            {
                                BuildVersionPreviewBuildNumber = buildNumber;
                            }
                            else
                            {
                                BuildVersionPreviewBuildNumber = 0;
                            }
                        }
                        else if (previewBuildParts.Length == 2)
                        {
                            // 15.0 格式: Preview=15, Build=0 (不设置 Minor)
                            BuildVersionPreviewHasMinorNumber = false;
                            BuildVersionPreviewMinorNumber = 0;
                            if (ushort.TryParse(previewBuildParts[1].Trim(), out ushort buildNumber))
                            {
                                BuildVersionPreviewBuildNumber = buildNumber;
                            }
                            else
                            {
                                BuildVersionPreviewBuildNumber = 0;
                            }
                        }
                        else
                        {
                            BuildVersionPreviewHasMinorNumber = false;
                            BuildVersionPreviewMinorNumber = 0;
                            BuildVersionPreviewBuildNumber = 0;
                        }
                    }
                    
                    if (hasError)
                    {
                        Log.Error("Version file format error. Using default values / 版本文件格式错误。使用默认值");
                        Log.Error($"Default: Type={BuildType}, Version={BuildMajorMinorPatch}, Preview={BuildVersionPreviewNumber}, Build={BuildVersionPreviewBuildNumber}");
                    }
                    else
                    {
                        string buildTypeLabel = BuildType == MidiBuildType.Stable ? "" : BuildType == MidiBuildType.RC ? "-rc" : "-preview";
                        if (BuildType == MidiBuildType.Stable)
                        {
                            Log.Information($"Version loaded: {BuildMajorMinorPatch} (stable)");
                        }
                        else if (BuildVersionPreviewHasMinorNumber)
                        {
                            Log.Information($"Version loaded: {BuildMajorMinorPatch}{buildTypeLabel}.{BuildVersionPreviewNumber}.{BuildVersionPreviewMinorNumber}.{BuildVersionPreviewBuildNumber}");
                        }
                        else
                        {
                            Log.Information($"Version loaded: {BuildMajorMinorPatch}{buildTypeLabel}.{BuildVersionPreviewNumber}.{BuildVersionPreviewBuildNumber}");
                        }
                    }
                }
                else
                {
                    Log.Error("Version file has insufficient lines (at least 2 lines required: build type and version)");
                    Log.Error("版本文件行数不足（至少需要2行：构建类型和版本号）");
                }
            }
            else
            {
                Log.Error("Version file still not available after creation attempt");
                Log.Error("Using default values / 使用默认值");
            }
        }
        catch (Exception ex)
        {
            Log.Error($"Error reading version file: {ex.Message}");
            Log.Error("Using default values / 使用默认值");
        }
    }


    void UpdateSetupBundleInfoIncludeFile(string platform)
    {
        //string versionString = $"{SetupBuildMajorMinor}.{SetupBuildDateNumber}.{SetupBuildTimeNumber}";

        using (StreamWriter writer = System.IO.File.CreateText(SetupBundleInfoIncludeFile))
        {
            writer.WriteLine("<Include>");
            writer.WriteLine($"  <?define SetupVersionName=\"{VersionName} {platform}\" ?>");
            writer.WriteLine($"  <?define SetupVersionNumber=\"{BuildVersionFullString}\" ?>");
            writer.WriteLine($"  <?define MidiSdkAndToolsVersion=\"{BuildVersionFullString}\" ?>");
            writer.WriteLine("</Include>");
        }
    }


    Target T_BuildNetworkMidiInstaller => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_BuildInDevelopmentServicePlugins)
        .Executes(() =>
    {
        // we build for Arm64 and x64. No EC required here
        foreach (var platform in InstallerPlatforms)
        {
            UpdateSetupBundleInfoIncludeFile(platform);

            //string fullSetupVersionString = $"{SetupVersionName} {SetupBuildMajorMinor}.{SetupBuildDateNumber}.{SetupBuildTimeNumber}";

            string solutionDir = NetworkMidiSetupSolutionFolder.ToString() + @"\";

            var msbuildProperties = new Dictionary<string, object>();
            msbuildProperties.Add("Platform", platform);
            msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash

            Console.Out.WriteLine($"----------------------------------------------------------------------");
            Console.Out.WriteLine($"SolutionDir: {solutionDir}");
            Console.Out.WriteLine($"Platform:    {platform}");

            NuGetTasks.NuGetRestore(_ => _
                .SetTargetPath(NetworkMidiSetupSolutionFolder / "midi-services-network-midi-preview-setup.sln")
                .SetSource(@"https://api.nuget.org/v3/index.json")
            );

            var output = MSBuildTasks.MSBuild(_ => _
                .SetTargetPath(NetworkMidiSetupSolutionFolder / "midi-services-network-midi-preview-setup.sln")
                .SetMaxCpuCount(null)
                /*.SetOutDir(outputFolder) */
                /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                /*.SetTargets("Build") */
                .SetProperties(msbuildProperties)
                .SetConfiguration(Configuration.Release)
                .SetTargets("Clean", "Rebuild")
                .SetVerbosity(BuildVerbosity)
                .EnableNodeReuse()
            );

            string newInstallerName = $"Windows MIDI Services (Network MIDI 2.0 Preview) {BuildVersionFullString}-{platform.ToLower()}.exe";

            var setupFile = NetworkMidiSetupSolutionFolder / "main-bundle" / "bin" / platform / Configuration.Release / "WindowsMidiServicesNetworkMidiSetup.exe";

            setupFile.Copy(ThisReleaseFolder / newInstallerName);

            BuiltNetworkMidiInstallers[platform.ToLower()] = newInstallerName;

        }
    });

    Target T_BuildVirtualPatchBayPluginInstaller => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_BuildInDevelopmentServicePlugins)
        .Executes(() =>
        {
            // we build for Arm64 and x64. No EC required here
            foreach (var platform in InstallerPlatforms)
            {
                UpdateSetupBundleInfoIncludeFile(platform);

                //string fullSetupVersionString = $"{SetupVersionName} {SetupBuildMajorMinor}.{SetupBuildDateNumber}.{SetupBuildTimeNumber}";

                string solutionDir = VirtualPatchBaySetupSolutionFolder.ToString() + @"\";

                var msbuildProperties = new Dictionary<string, object>();
                msbuildProperties.Add("Platform", platform);
                msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash

                Console.Out.WriteLine($"----------------------------------------------------------------------");
                Console.Out.WriteLine($"SolutionDir: {solutionDir}");
                Console.Out.WriteLine($"Platform:    {platform}");

                NuGetTasks.NuGetRestore(_ => _
                    .SetTargetPath(VirtualPatchBaySetupSolutionFolder / "midi-services-virtual-patch-bay-setup.sln")
                    .SetSource(@"https://api.nuget.org/v3/index.json")
                );

                var output = MSBuildTasks.MSBuild(_ => _
                    .SetTargetPath(VirtualPatchBaySetupSolutionFolder / "midi-services-virtual-patch-bay-setup.sln")
                    .SetMaxCpuCount(null)
                    /*.SetOutDir(outputFolder) */
                    /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                    /*.SetTargets("Build") */
                    .SetProperties(msbuildProperties)
                    .SetConfiguration(Configuration.Release)
                    .SetTargets("Clean", "Rebuild")
                    .SetVerbosity(BuildVerbosity)
                    .EnableNodeReuse()
                );

                string newInstallerName = $"Windows MIDI Services (Virtual Patch Bay Preview) {BuildVersionFullString}-{platform.ToLower()}.exe";


                var setupFile = VirtualPatchBaySetupSolutionFolder / "main-bundle" / "bin" / platform / Configuration.Release / "WindowsMidiServicesVirtualPatchBaySetup.exe";
                setupFile.Copy(ThisReleaseFolder / newInstallerName);

                BuiltVirtualPatchBayInstallers[platform.ToLower()] = newInstallerName;

            }
        });

    Target T_BuildBasicLoopbackPluginInstaller => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_BuildInDevelopmentServicePlugins)
        .Executes(() =>
        {
            // we build for Arm64 and x64. No EC required here
            foreach (var platform in InstallerPlatforms)
            {
                UpdateSetupBundleInfoIncludeFile(platform);

                //string fullSetupVersionString = $"{SetupVersionName} {SetupBuildMajorMinor}.{SetupBuildDateNumber}.{SetupBuildTimeNumber}";

                string solutionDir = BasicLoopbackSetupSolutionFolder.ToString() + @"\";

                var msbuildProperties = new Dictionary<string, object>();
                msbuildProperties.Add("Platform", platform);
                msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash

                Console.Out.WriteLine($"----------------------------------------------------------------------");
                Console.Out.WriteLine($"SolutionDir: {solutionDir}");
                Console.Out.WriteLine($"Platform:    {platform}");

                NuGetTasks.NuGetRestore(_ => _
                    .SetTargetPath(SimpleLoopbackSetupSolutionFolder / "midi-services-simple-loopback-setup.sln")
                    .SetSource(@"https://api.nuget.org/v3/index.json")
                );

                var output = MSBuildTasks.MSBuild(_ => _
                    .SetTargetPath(BasicLoopbackSetupSolutionFolder / "midi-services-basic-loopback-setup.sln")
                    .SetMaxCpuCount(null)
                    /*.SetOutDir(outputFolder) */
                    /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                    /*.SetTargets("Build") */
                    .SetProperties(msbuildProperties)
                    .SetConfiguration(Configuration.Release)
                    .SetTargets("Clean", "Rebuild")
                    .SetVerbosity(BuildVerbosity)
                    .EnableNodeReuse()
                );

                string newInstallerName = $"Windows MIDI Services (Basic MIDI 1.0 Loopback Preview) {BuildVersionFullString}-{platform.ToLower()}.exe";


                var setupFile = BasicLoopbackSetupSolutionFolder / "main-bundle" / "bin" / platform / Configuration.Release / "WindowsMidiServicesBasicLoopbackSetup.exe";
                setupFile.Copy(ThisReleaseFolder / newInstallerName);

                BuiltSimpleLoopbackInstallers[platform.ToLower()] = newInstallerName;

            }
        });




    void RestoreNuGetPackagesForCPPProject(string vcxprojFilePath, string solutionDir, string packagesConfigFullPath)
    {
        var projectDir = Path.GetDirectoryName(vcxprojFilePath);

        NuGetTasks.NuGetInstall(_ => _
            .SetProcessWorkingDirectory(projectDir)
            .SetPreRelease(true)
            .SetSource(AppSdkNugetOutputFolder, @"https://api.nuget.org/v3/index.json")
            .SetPackageID(NugetPackageId)
            .SetDependencyVersion(DependencyVersion.Highest)
        );

        NuGetTasks.NuGetRestore(_ => _
            .SetProcessWorkingDirectory(projectDir)
            .SetSource(AppSdkNugetOutputFolder, @"https://api.nuget.org/v3/index.json")
            .SetSolutionDirectory(solutionDir)
            //.SetConfigFile(packagesConfigFullPath)
        );
    }

    void UpdateWindowsMidiServicesSdkPackagePropertyForCppProject(string vcxprojFilePath)
    {
        if (File.Exists(vcxprojFilePath))
        {
            // this is ugly and annoying to have to do.
            //   XmlTasks.XmlPoke(configFilePath, @"/packages/package/id", NugetFullPackageId)

            var doc = new XmlDocument();
            doc.Load(vcxprojFilePath);

            var manager = new XmlNamespaceManager(doc.NameTable);
            manager.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003");

            XmlElement? element = doc.SelectSingleNode($"/msb:Project/msb:PropertyGroup/msb:WindowsMidiServicesSdkPackage", manager) as XmlElement;

            // change the version attribute
            if (element != null)
            {
                element.InnerText = NugetFullPackageIdWithVersion;

                doc.Save(vcxprojFilePath);

                Console.WriteLine($"Updated {vcxprojFilePath}");
            }
            else
            {
                throw new ArgumentException($"Failed to update SDK Package Value for '{vcxprojFilePath}'.");
            }

        }
        else
        {
            throw new ArgumentException($"vcxproj file does not exist '{vcxprojFilePath}'.");
        }

    }

    void UpdatePackagesConfigForCPPProject(string configFilePath)
    {
        if (File.Exists(configFilePath))
        {
            // this is ugly and annoying to have to do.
            //   XmlTasks.XmlPoke(configFilePath, @"/packages/package/id", NugetFullPackageId)

            var doc = new XmlDocument();
            doc.Load(configFilePath);

            XmlElement? element = doc.SelectSingleNode($"/packages/package[@id = \"{NugetPackageId}\"]") as XmlElement;

            // change the version attribute
            if (element != null)
            {
                Console.WriteLine($"Updating {element}");

                element.SetAttribute("version", NugetPackageVersion);

                doc.Save(configFilePath);

                Console.WriteLine($"Updated {configFilePath}");
            }
            else
            {
                throw new ArgumentException($"Failed to update NuGet Package Value for '{configFilePath}'.");
            }

        }
        else
        {
            throw new ArgumentException($"Packages.config file does not exist '{configFilePath}'.");
        }
    }



    // hard-coded paths to just get around some runtime limitations. This should
    // be re-done to make this build more portable

#pragma warning disable CS0414 // 字段已被赋值但从未使用
    [LocalPath(@"g:\github\microsoft\midi\build\electron-projection\nodert\src\NodeRtCmd\bin\Debug\NodeRtCmd.exe")]
    readonly Tool NodeRT = null!;


    [LocalPath(@"C:\Users\peteb\AppData\Roaming\npm\node-gyp.cmd")]
    readonly Tool NodeGyp = null!;
#pragma warning restore CS0414


    Target BuildAndPublishAll => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_CreateVersionIncludes)
        .DependsOn(T_BuildInDevelopmentServicePlugins)
        .DependsOn(T_BuildNetworkMidiInstaller)
        .DependsOn(T_BuildVirtualPatchBayPluginInstaller)
        .DependsOn(T_BuildBasicLoopbackPluginInstaller)
        .Executes(() =>
        {

            if (BuiltNetworkMidiInstallers.Count > 0)
            {
                Console.WriteLine("\nBuilt Network Midi installers:");

                foreach (var item in BuiltNetworkMidiInstallers)
                {
                    Console.WriteLine($"  {item.Key.PadRight(5)} {item.Value}");
                }
            }
            else
            {
                Console.WriteLine("No network MIDI installers built.");
            }


            if (BuiltVirtualPatchBayInstallers.Count > 0)
            {
                Console.WriteLine("\nBuilt Virtual Patch Bay installers:");

                foreach (var item in BuiltVirtualPatchBayInstallers)
                {
                    Console.WriteLine($"  {item.Key.PadRight(5)} {item.Value}");
                }
            }
            else
            {
                Console.WriteLine("No Virtual Patch Bay installers built.");
            }

            if (BuiltSimpleLoopbackInstallers.Count > 0)
            {
                Console.WriteLine("\nBuilt Simple Loopback installers:");

                foreach (var item in BuiltSimpleLoopbackInstallers)
                {
                    Console.WriteLine($"  {item.Key.PadRight(5)} {item.Value}");
                }
            }
            else
            {
                Console.WriteLine("No Simple Loopback installers built.");
            }

        });
        
    void SetupVcpkg()
    {
        Log.Information("Setting up vcpkg...");
        
        // Step 1: Check VCPKG_ROOT and VS Integration
        var vcpkgRootFromEnv = Environment.GetEnvironmentVariable("VCPKG_ROOT");
        var defaultVcpkgPath = NukeBuild.RootDirectory / "build" / "vcpkg";
        
        // Check VS integration status
        var userPropsPath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "vcpkg", "vcpkg.user.props");
        
        bool vsIntegrationExists = File.Exists(userPropsPath);
        string? vsIntegrationPath = null;
        
        if (vsIntegrationExists)
        {
            try
            {
                var propsContent = File.ReadAllText(userPropsPath);
                // Extract path from: Project="...\vcpkg.props"
                var match = System.Text.RegularExpressions.Regex.Match(propsContent, 
                    @"Project=""([^""]+\\scripts\\buildsystems\\msbuild\\vcpkg\.props)""");
                if (match.Success)
                {
                    vsIntegrationPath = Path.GetDirectoryName(Path.GetDirectoryName(Path.GetDirectoryName(match.Groups[1].Value)));
                    Log.Information("VS Integration found at: {VsIntegrationPath}", vsIntegrationPath);
                }
            }
            catch { }
        }
        
        // Step 2: Determine which vcpkg to use
        string? vcpkgRoot = null;
        
        if (!string.IsNullOrWhiteSpace(vcpkgRootFromEnv))
        {
            // VCPKG_ROOT is set, use it
            vcpkgRoot = vcpkgRootFromEnv;
            Log.Information("Using VCPKG_ROOT: {VcpkgRoot}", vcpkgRoot);
        }
        else if (!string.IsNullOrWhiteSpace(vsIntegrationPath))
        {
            // No VCPKG_ROOT but VS integration exists, use integrated path
            vcpkgRoot = vsIntegrationPath;
            Log.Information("Using VS integrated vcpkg: {VcpkgRoot}", vcpkgRoot);
        }
        else
        {
            // Neither exists, use default path
            vcpkgRoot = defaultVcpkgPath;
            Log.Information("Using default vcpkg path: {VcpkgRoot}", vcpkgRoot);
        }
        
        // Step 3: Check if vcpkg exists, clone if needed
        var vcpkgExe = Path.Combine(vcpkgRoot, "vcpkg.exe");
        bool vcpkgExists = File.Exists(vcpkgExe);
        
        if (!vcpkgExists)
        {
            Log.Warning("vcpkg not found at: {VcpkgRoot}", vcpkgRoot);
            Log.Information("Cloning vcpkg repository...");
            
            // Clone to default location
            var buildDir = NukeBuild.RootDirectory / "build";
            buildDir.CreateDirectory();
            
            var cloneProcess = new System.Diagnostics.Process
            {
                StartInfo = new System.Diagnostics.ProcessStartInfo
                {
                    FileName = "git",
                    Arguments = "clone https://github.com/Microsoft/vcpkg.git",
                    WorkingDirectory = buildDir.ToString(),
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                }
            };
            
            cloneProcess.Start();
            string cloneOutput = cloneProcess.StandardOutput.ReadToEnd();
            string cloneError = cloneProcess.StandardError.ReadToEnd();
            cloneProcess.WaitForExit();
            
            if (cloneProcess.ExitCode != 0)
            {
                Log.Warning("Failed to clone vcpkg. Exit code: {ExitCode}", cloneProcess.ExitCode);
                Log.Warning("Error: {Error}", cloneError);
                return;
            }
            
            Log.Information("vcpkg cloned successfully. Bootstrapping...");
            
            // Run bootstrap
            var bootstrapProcess = new System.Diagnostics.Process
            {
                StartInfo = new System.Diagnostics.ProcessStartInfo
                {
                    FileName = "cmd.exe",
                    Arguments = "/c bootstrap-vcpkg.bat",
                    WorkingDirectory = defaultVcpkgPath.ToString(),
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                }
            };
            
            bootstrapProcess.Start();
            string bootstrapOutput = bootstrapProcess.StandardOutput.ReadToEnd();
            string bootstrapError = bootstrapProcess.StandardError.ReadToEnd();
            bootstrapProcess.WaitForExit();
            
            if (bootstrapProcess.ExitCode != 0)
            {
                Log.Warning("vcpkg bootstrap failed. Exit code: {ExitCode}", bootstrapProcess.ExitCode);
                Log.Warning("Error: {Error}", bootstrapError);
                return;
            }
            
            // Use the newly cloned vcpkg
            vcpkgRoot = defaultVcpkgPath;
            vcpkgExe = Path.Combine(vcpkgRoot, "vcpkg.exe");
            Log.Information("vcpkg initialized at: {VcpkgRoot}", vcpkgRoot);
        }
        else
        {
            Log.Information("vcpkg found at: {VcpkgRoot}", vcpkgRoot);
        }
        
        // Step 4: Set VCPKG_ROOT environment variable
        Environment.SetEnvironmentVariable("VCPKG_ROOT", vcpkgRoot);
        Log.Information("VCPKG_ROOT set to: {VcpkgRoot}", vcpkgRoot);
        
        // Step 5: Check and apply Visual Studio integration
        Log.Information("Checking Visual Studio integration...");
        
        bool needsIntegration = false;
        
        if (!File.Exists(userPropsPath))
        {
            needsIntegration = true;
            Log.Warning("Visual Studio integration not found.");
        }
        else
        {
            try
            {
                var propsContent = File.ReadAllText(userPropsPath);
                var expectedPath = Path.Combine(vcpkgRoot, "scripts", "buildsystems", "msbuild", "vcpkg.props");
                
                if (!propsContent.Contains(expectedPath))
                {
                    needsIntegration = true;
                    Log.Warning("Visual Studio integration points to different location.");
                }
            }
            catch
            {
                needsIntegration = true;
            }
        }
        
        if (needsIntegration)
        {
            Log.Information("Applying Visual Studio integration...");
            
            var integrateProcess = new System.Diagnostics.Process
            {
                StartInfo = new System.Diagnostics.ProcessStartInfo
                {
                    FileName = vcpkgExe,
                    Arguments = "integrate install",
                    WorkingDirectory = vcpkgRoot,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                }
            };
            
            integrateProcess.Start();
            string output = integrateProcess.StandardOutput.ReadToEnd();
            string error = integrateProcess.StandardError.ReadToEnd();
            integrateProcess.WaitForExit();
            
            if (integrateProcess.ExitCode == 0)
            {
                Log.Information("Visual Studio integration applied successfully.");
            }
            else
            {
                Log.Warning("Integration failed. Exit code: {ExitCode}", integrateProcess.ExitCode);
                Log.Warning("Error: {Error}", error);
            }
        }
        else
        {
            Log.Information("Visual Studio integration is up to date.");
        }
        
        Log.Information("vcpkg setup complete.");
    }

}
