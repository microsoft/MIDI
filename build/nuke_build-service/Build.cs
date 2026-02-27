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

    MidiBuildType BuildType => MidiBuildType.Preview; 
    
    const UInt16 BuildVersionMajor = 1;
    const UInt16 BuildVersionMinor = 0;
    const UInt16 BuildVersionPatch = 15;

    UInt16 BuildVersionPreviewNumber = 14; // 从版本文件中读取，默认值为 14
    string VersionName => "Service Preview " + BuildVersionPreviewNumber;

    // --------------------------------------------------------------------------------------

    UInt16 BuildVersionBuildNumber = 0; // 从版本文件中读取

    readonly string BuildMajorMinorPatch = BuildVersionMajor.ToString() + "." + BuildVersionMinor.ToString() + "." + BuildVersionPatch.ToString();

    string BuildVersionPreviewString;
    string BuildVersionFullString = "";
    string BuildVersionAssemblyFullString = "";
    string BuildVersionFileFullString = "";

    string NugetPackageId => "Microsoft.Windows.Devices.Midi2";
    string NugetPackageVersion;


    DateTime BuildDate;


    // can't release debug versions unless you copy over the debug runtimes
    // which makes a mess. So release-only.
    //readonly string ServiceBuildConfiguration = Configuration.Debug;
    readonly string ServiceBuildConfiguration = Configuration.Release;




    //string SetupBundleFullVersionString => BuildMajorMinorRevision + "-" + NuGetVersionName + "." + BuildDateNumber + "-" + BuildTimeNumber;


    AbsolutePath _thisReleaseFolder;


    //string[] AppSdkAssemblies => new string[] {
    //    "Microsoft.Windows.Devices.Midi2",
    //    "Microsoft.Windows.Devices.Midi2.Initialization"
    //};


    // ===========================================================
    // directories

    AbsolutePath BuildRootFolder => NukeBuild.RootDirectory / "build";

    AbsolutePath ElectronProjectionRootFolder => BuildRootFolder / "electron-projection";

    AbsolutePath StagingRootFolder => BuildRootFolder / "staging";
    AbsolutePath AppSdkStagingFolder => StagingRootFolder / "app-sdk";
    AbsolutePath ApiStagingFolder => StagingRootFolder / "api";


    AbsolutePath ReleaseRootFolder => BuildRootFolder / "release";
    AbsolutePath AppSdkNugetOutputFolder => ReleaseRootFolder / "nuget";

    AbsolutePath ThisReleaseFolder => _thisReleaseFolder;


    AbsolutePath SourceRootFolder => NukeBuild.RootDirectory / "src";
    AbsolutePath ApiSolutionFolder => SourceRootFolder / "api";


    AbsolutePath InBoxComponentsSetupSolutionFolder => SourceRootFolder / "oob-setup";

    AbsolutePath ApiReferenceFolder => SourceRootFolder / "shared" / "api-ref";



    AbsolutePath SetupBundleInfoIncludeFile => StagingRootFolder / "version" / "BundleInfo.wxi"; // TODO Need to change path of this
    AbsolutePath BuildVersionFile => StagingRootFolder / "version" / "Service_BuildNumber.txt";

    AbsolutePath SdkVersionFilesFolder => StagingRootFolder / "version";
    AbsolutePath SdkVersionHeaderFile => SdkVersionFilesFolder / "WindowsMidiServicesSdkRuntimeVersion.h";
    AbsolutePath NuGetVersionHeaderFile => SdkVersionFilesFolder / "WindowsMidiServicesVersion.h";
    AbsolutePath CommonVersionCSharpFile => SdkVersionFilesFolder / "WindowsMidiServicesVersion.cs";

    
    string[] SdkPlatformsIncludingAnyCpu => new string[] { "x64", "Arm64EC", "AnyCPU" };
    string[] SdkPlatforms => new string[] { "x64", "Arm64EC" };
    string[] ServiceAndApiPlatforms => new string[] { "x64", "Arm64" };
    string[] ServiceAndApiPlatformsAll => new string[] { "x64", "Arm64", "Arm64EC" };   // the order here matters because the dependencies in the solution aren't perfect
    string[] ToolsPlatforms => new string[] { "x64", "Arm64" };
    string[] NativeSamplesPlatforms => new string[] { "x64", "Arm64", "Arm64EC" };
    string[] ManagedSamplesPlatforms => new string[] { "x64", "Arm64" };
    string[] InstallerPlatforms => new string[] { "x64", "Arm64" };

    Dictionary<string, string> BuiltInBoxInstallers = new Dictionary<string, string>();

    public static int Main () => Execute<Build>(x => x.BuildAndPublishAll);

    string MSBuildPath;

    void SetMSBuildVersion()
    {
        // 首先使用 vswhere 检测 VS 实例（支持所有版本，包括最新的 VS 2026+）
        string vswhereMsBuildPath = GetMSBuildPathFromVsWhere();
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

    string GetMSBuildPathFromVsWhere()
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
                Environment.SetEnvironmentVariable("MIDI_REPO_ROOT", NukeBuild.RootDirectory);
            }

            // this updates the build number and writes hte new value to the file
            IncrementBuildNumber();

            if (BuildType == MidiBuildType.Stable)
            {
                BuildVersionPreviewString = "";
                NugetPackageVersion = BuildMajorMinorPatch;
            }
            else if (BuildType == MidiBuildType.RC)
            {
                BuildVersionPreviewString = "rc." + BuildVersionPreviewNumber + "." + BuildVersionBuildNumber;
                NugetPackageVersion = BuildMajorMinorPatch + "-" + BuildVersionPreviewString;
            }
            else if (BuildType == MidiBuildType.Preview)
            {
                BuildVersionPreviewString = "preview." + BuildVersionPreviewNumber + "." + BuildVersionBuildNumber;
                NugetPackageVersion = BuildMajorMinorPatch + "-" + BuildVersionPreviewString;
            }

            // they are the same, for our use here.
            BuildVersionFullString = NugetPackageVersion;

            BuildVersionAssemblyFullString = BuildMajorMinorPatch + "." + BuildVersionBuildNumber;
            BuildVersionFileFullString = BuildMajorMinorPatch + "." + BuildVersionBuildNumber;

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
            msbuildProperties.Add("MidiBuildVersionBuildNumber", BuildVersionBuildNumber);
            msbuildProperties.Add("MidiBuildVersionPreviewString", $"{BuildVersionPreviewString}");

            msbuildProperties.Add("MidiVersionOutputFolder", (StagingRootFolder / "version").ToString());

            SetMSBuildVersion();

            MSBuildTasks.MSBuild(_ => _
                .SetTargetPath(SourceRootFolder / "build-gen-version-includes" / "GenVersionIncludes.csproj")
                .SetMaxCpuCount(null)
                .AddProcessAdditionalArguments("/t:TransformAll")
                .SetProperties(msbuildProperties)
                .SetVerbosity(MSBuildVerbosity.Normal)
                .EnableNodeReuse()
            );

            // WDK is broken for VS 2026, so have to use old VS 2022 MSBuild tools
            SetMSBuildVersion();

        });

    Target T_BuildServiceAndPlugins => _ => _
        .DependsOn(T_CreateVersionIncludes)
        .DependsOn(T_Prerequisites)
        .Executes(() =>
    {
        // this needs to build for Release before building for debug. 
        // Some of the IDL and other references point to Release only, and
        // the reference files come from Release

        var platforms = new List<string>();
        platforms.AddRange(ServiceAndApiPlatformsAll);
        platforms.Add("Win32");

        foreach (var platform in platforms)
        {
            string solutionDir = ApiSolutionFolder.ToString() + @"\";

            Console.Out.WriteLine($"----------------------------------------------------------------------");
            Console.Out.WriteLine($"SolutionDir: {solutionDir}");
            Console.Out.WriteLine($"Platform:    {platform}");

            var configs = new[] { Configuration.Debug, Configuration.Release };
            foreach (var buildConfiguration in configs)
            {
                Console.WriteLine($"Building Service and Plugins {platform} {buildConfiguration}");

                var msbuildProperties = new Dictionary<string, object>();
                msbuildProperties.Add("Platform", platform);
                msbuildProperties.Add("SolutionDir", solutionDir);  // to include trailing slash
                msbuildProperties.Add("NoWarn", "MIDL2111");        // IDL identifier length warning
                msbuildProperties.Add("Configuration", buildConfiguration.ToString());

                MSBuildTasks.MSBuild(_ => _
                    .SetTargetPath(ApiSolutionFolder / "midi2.sln")
                    .SetMaxCpuCount(null)
                    //.SetConfiguration(buildConfiguration)
                    /*.SetOutDir(outputFolder) */
                    /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                    //.SetTargets("Rebuild") 
                    .SetProperties(msbuildProperties)
                    .SetVerbosity(BuildVerbosity)
                    .EnableNodeReuse()
                );
            }

            // copy binaries to staging folder
            var stagingFiles = new List<AbsolutePath>();


            // This transport gets compiled to Arm64X and x64. The Arm64X output is in the Arm64EC folder
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / ServiceBuildConfiguration / $"Midi2.MidiSrvTransport.dll");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / ServiceBuildConfiguration / $"Midi2.MidiSrvTransport.pdb");

            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / ServiceBuildConfiguration / $"wdmaud2.drv");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / ServiceBuildConfiguration / $"wdmaud2.pdb");

            var servicePlatform = (platform == "Arm64EC" || platform == "Arm64") ? "Arm64" : platform;

            if (platform != "Win32")
            {
                // only in-proc files, like the MidiSrvTransport, are Arm64EC. For all the others
                // any reference to Arm64EC is just Arm64. We don't use any of the Arm64X output

                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midisrv.exe");
                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midisrv.pdb");

                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.DiagnosticsTransport.dll");
                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.DiagnosticsTransport.pdb");

                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.KSTransport.dll");
                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.KSTransport.pdb");

                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.KSAggregateTransport.dll");
                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.KSAggregateTransport.pdb");

                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.VirtualMidiTransport.dll");
                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.VirtualMidiTransport.pdb");

                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.LoopbackMidiTransport.dll");
                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.LoopbackMidiTransport.pdb");

                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.BS2UMPTransform.dll");
                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.BS2UMPTransform.pdb");

                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.UMP2BSTransform.dll");
                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.UMP2BSTransform.pdb");

                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.UmpProtocolDownscalerTransform.dll");
                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.UmpProtocolDownscalerTransform.pdb");

                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.SchedulerTransform.dll");
                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.SchedulerTransform.pdb");
            }

            foreach (var file in stagingFiles)
            {
                file.CopyToDirectory(ApiStagingFolder / servicePlatform, ExistsPolicy.FileOverwrite);
            }
        }

        // Copy reference files to reference folder. This requires
        // the SDK projects reference those instead of the current version
        // which references deep into the API project

        // copy x64 files to Arm64EC folder as well. No reason for the full
        // service and plugins to be Arm64EC just to provide a few headers

        var intermediateFolder = ApiSolutionFolder / "vsfiles" / "intermediate";
        var apiReleaseFolder = ApiSolutionFolder / "vsfiles";
        var apiIncludeFolder = ApiSolutionFolder / "inc";
        var networkTransportFolder = ApiSolutionFolder / "Transport" / "UdpNetworkMidi2Transport";

        foreach (var platform in ServiceAndApiPlatformsAll)
        {
            var referenceFiles = new List<AbsolutePath>();

            // for the destination platform, we use x64 files here since they're not
            // binaries anyway
            //string sourcePlatform = (platform == "Arm64EC" ? "x64" : platform);
            string sourcePlatform = platform;

            referenceFiles.Add(intermediateFolder / "idl" / sourcePlatform / Configuration.Release / "WindowsMidiServices.h");
            referenceFiles.Add(intermediateFolder / "idl" / sourcePlatform / Configuration.Release / "WindowsMidiServices_i.c");
            referenceFiles.Add(intermediateFolder / "idl" / sourcePlatform / Configuration.Release / "WindowsMidiServices_i.c");

            referenceFiles.Add(intermediateFolder / "MidiKS" / sourcePlatform / Configuration.Release / "MidiKS.lib");
            referenceFiles.Add(apiReleaseFolder / sourcePlatform / Configuration.Release / "MidiPluginConfigurationLib.lib");
            referenceFiles.Add(apiReleaseFolder / sourcePlatform / Configuration.Release / "MidiEndpointNamingLib.lib");

            referenceFiles.Add(apiIncludeFolder / "MidiEndpointCustomProperties.h");
            referenceFiles.Add(apiIncludeFolder / "MidiEndpointMatchCriteria.h");
            referenceFiles.Add(apiIncludeFolder / "MidiEndpointNameTable.h");
            referenceFiles.Add(apiIncludeFolder / "MidiDefs.h");
            referenceFiles.Add(apiIncludeFolder / "hstring_util.h");
            referenceFiles.Add(apiIncludeFolder / "wstring_util.h");
            referenceFiles.Add(apiIncludeFolder / "json_defs.h");
            referenceFiles.Add(apiIncludeFolder / "json_helpers.h");
            referenceFiles.Add(apiIncludeFolder / "loopback_ids.h");
            referenceFiles.Add(apiIncludeFolder / "midi_group_terminal_blocks.h");

            // transport-specific include file
            referenceFiles.Add(networkTransportFolder / "network_json_defs.h");
            //referenceFiles.Add(ble1TransportFolder / "ble1_json_defs.h");



            // this is the only one that needs Arm64X
            referenceFiles.Add(intermediateFolder / "Midi2.MidiSrvTransport" / sourcePlatform / Configuration.Release / "Midi2MidiSrvTransport.h");

            // copy the files over to the reference location
            foreach (var file in referenceFiles)
            {
                file.CopyToDirectory(ApiReferenceFolder / platform, ExistsPolicy.FileOverwrite);
            }


        }

    });



    // 默认版本配置内容
    const string DefaultVersionFileContent = @"# Windows MIDI Services Service Version Configuration File
# Windows MIDI Services Service 版本配置文件
# Modify this file to update the version number without changing the code
# 修改此文件即可更新版本号，无需修改代码
#
# Format:
# 格式说明：
# Line 1: Major.Minor.Patch (e.g.: 1.0.15) [Required]
# 第1行：主版本号.次版本号.补丁版本号（例如：1.0.15）[必需]
# Line 2: Preview.Minor.Build (e.g.: 14.0.0 or 14.0) [Optional]
# 第2行：预览版本号.次版本号.构建号（例如：14.0.0 或 14.0）[可选]
#
# Note:
# 注意：
# Lines starting with # are treated as comments and will be ignored
# 以 # 开头的行被视为注释，会被忽略
#
# Example:
# 示例：1.0.15-preview.14.0.0

1.0.15
14.0.0
";

    void IncrementBuildNumber()
    {
        // 从版本文件中读取版本信息
        // 文件格式：
        // 第1行：主版本号.次版本号.补丁版本号 (必需，例如: 1.0.15)
        // 第2行：预览版本号.次版本号.构建号 (可选，支持 14.0.0 或 14.0 格式，默认为 14.0.0)
        // 以 # 开头的行被视为注释，会被忽略
        
        // 如果版本文件不存在，创建默认版本文件
        if (!File.Exists(BuildVersionFile))
        {
            Logger.Warn($"Version file not found: {BuildVersionFile}");
            Logger.Warn("Creating default version file... / 正在创建默认版本文件...");
            
            try
            {
                // 确保目录存在
                Directory.CreateDirectory(Path.GetDirectoryName(BuildVersionFile)!);
                // 写入默认版本文件
                File.WriteAllText(BuildVersionFile, DefaultVersionFileContent);
                Logger.Info($"Default version file created: {BuildVersionFile}");
            }
            catch (Exception ex)
            {
                Logger.Error($"Failed to create version file: {ex.Message}");
                Logger.Error("Using built-in default version / 使用内置默认版本");
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
                
                // 只需要第1行存在即可
                if (validLines.Length >= 1)
                {
                    bool hasError = false;
                    
                    // 验证第1行版本号格式 (Major.Minor.Patch)
                    var versionParts = validLines[0].Trim().Split('.');
                    if (versionParts.Length != 3 || 
                        !ushort.TryParse(versionParts[0], out _) ||
                        !ushort.TryParse(versionParts[1], out _) ||
                        !ushort.TryParse(versionParts[2], out _))
                    {
                        Logger.Error($"Invalid version format in line 1: '{validLines[0]}'");
                        Logger.Error("Expected format: Major.Minor.Patch (e.g. 1.0.15) / 期望格式: 主版本.次版本.补丁 (例如 1.0.15)");
                        hasError = true;
                    }
                    
                    // 第2行是可选的，格式为 Preview.Minor.Build 或 Preview.Build
                    if (validLines.Length >= 2)
                    {
                        var previewBuildParts = validLines[1].Trim().Split('.');
                        
                        // 读取预览版本号（第1部分）
                        if (previewBuildParts.Length >= 1 && ushort.TryParse(previewBuildParts[0].Trim(), out ushort previewNumber))
                        {
                            BuildVersionPreviewNumber = previewNumber;
                        }
                        else
                        {
                            Logger.Error($"Invalid preview number in line 2: '{validLines[1]}'");
                            Logger.Error("Expected format: Preview.Minor.Build (e.g. 14.0.0 or 14.0) / 期望格式: 预览版本.次版本.构建号 (例如 14.0.0 或 14.0)");
                            hasError = true;
                        }
                        
                        // 读取构建号（最后一部分）
                        if (previewBuildParts.Length >= 2)
                        {
                            // 支持 Preview.Minor.Build (14.0.0) 或 Preview.Build (14.0) 格式
                            int lastIndex = previewBuildParts.Length - 1;
                            if (ushort.TryParse(previewBuildParts[lastIndex].Trim(), out ushort buildNumber))
                            {
                                BuildVersionBuildNumber = buildNumber;
                            }
                            else
                            {
                                BuildVersionBuildNumber = 0;
                            }
                        }
                        else
                        {
                            BuildVersionBuildNumber = 0;
                        }
                    }
                    
                    if (hasError)
                    {
                        Logger.Error("Version file format error. Using default values / 版本文件格式错误。使用默认值");
                        Logger.Error($"Default: Preview={BuildVersionPreviewNumber}, Build={BuildVersionBuildNumber}");
                    }
                    else
                    {
                        Logger.Info($"Version loaded: {validLines[0]}-preview.{BuildVersionPreviewNumber}.{BuildVersionBuildNumber}");
                    }
                }
                else
                {
                    Logger.Error($"Version file has insufficient lines: {validLines.Length} (expected at least 3)");
                    Logger.Error("Please check the file format / 请检查文件格式");
                }
            }
            else
            {
                Logger.Error("Version file still not available after creation attempt");
                Logger.Error("Using default values / 使用默认值");
            }
        }
        catch (Exception ex)
        {
            Logger.Error($"Error reading version file: {ex.Message}");
            Logger.Error("Using default values / 使用默认值");
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

    Target T_BuildServiceAndPluginsInstaller => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_BuildServiceAndPlugins)
        .Executes(() =>
        {
            // we build for Arm64 and x64. No EC required here
            foreach (var platform in InstallerPlatforms)
            {
                UpdateSetupBundleInfoIncludeFile(platform);

                //string fullSetupVersionString = $"{SetupVersionName} {SetupBuildMajorMinor}.{SetupBuildDateNumber}.{SetupBuildTimeNumber}";

                string solutionDir = InBoxComponentsSetupSolutionFolder.ToString() + @"\";

                var msbuildProperties = new Dictionary<string, object>();
                msbuildProperties.Add("Platform", platform);
                msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash

                Console.Out.WriteLine($"----------------------------------------------------------------------");
                Console.Out.WriteLine($"SolutionDir: {solutionDir}");
                Console.Out.WriteLine($"Platform:    {platform}");

                NuGetTasks.NuGetRestore(_ => _
                    .SetProcessWorkingDirectory(solutionDir)
                    .SetSource(@"https://api.nuget.org/v3/index.json")
                    .SetSolutionDirectory(solutionDir)
                //.SetConfigFile(packagesConfigFullPath)
                );

                var output = MSBuildTasks.MSBuild(_ => _
                    .SetTargetPath(InBoxComponentsSetupSolutionFolder / "midi-services-in-box-setup.sln")
                    .SetMaxCpuCount(null)
                    /*.SetOutDir(outputFolder) */
                    /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                    /*.SetTargets("Build") */
                    .SetProperties(msbuildProperties)
                    .SetConfiguration(Configuration.Release)
                    .SetVerbosity(BuildVerbosity)
                    .SetTargets("Clean", "Rebuild")
                    .EnableNodeReuse()
                );


                // todo: it would be better to see if any of the sdk files have changed and only
                // do this copy if a new setup file was created. Maybe do a before/after date/time check?

                string installerType = ServiceBuildConfiguration == Configuration.Debug ? "DEBUG" : "";


                string newInstallerName = $"Windows MIDI Services ({installerType}In-Box Service) {BuildVersionFullString}-{platform.ToLower()}.exe";

                var setupFile = InBoxComponentsSetupSolutionFolder / "main-bundle" / "bin" / platform / Configuration.Release / "WindowsMidiServicesInBoxComponentsSetup.exe";
                setupFile.Copy(ThisReleaseFolder / newInstallerName, ExistsPolicy.FileOverwrite);               

                BuiltInBoxInstallers[platform.ToLower()] = newInstallerName;
            }
        });

    Target T_ZipServicePdbs => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_BuildServiceAndPlugins)
        .Executes(() =>
    {
        foreach (var platform in new string[]{ "arm64", "x64"})
        {
            var folder = ApiStagingFolder / platform;

            folder.ZipTo(
                ThisReleaseFolder / $"service-pdbs-{platform}.zip",
                filter: x =>
                    x.HasExtension("pdb")
                );
            
        }

    });

    Target T_ZipPowershellDevUtilities => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_CreateVersionIncludes)
        .Executes(() =>
        {
            var regHelpersFolder = RootDirectory / "src" / "dev-tools" / "reg-helpers";

            regHelpersFolder.ZipTo(ThisReleaseFolder / $"dev-pre-setup-scripts.zip");
        });

    Target T_ZipWdmaud2 => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_BuildServiceAndPlugins)
        .Executes(() =>
        {
            var zipRoot = (StagingRootFolder / "wdmaud2").CreateOrCleanDirectory();

            var arm64 = (zipRoot / "arm64").CreateOrCleanDirectory();
            var x64 = (zipRoot / "x64").CreateOrCleanDirectory();
            var x86 = (zipRoot / "Win32").CreateOrCleanDirectory();

            var regHelpersLocation = RootDirectory / "src" / "dev-tools" / "reg-helpers";

            var regHelperCmdFileName = "dev-replace-wdmaud2.cmd";
            var regHelperPs1FileName = "midi-replace-wdmaud2-drv.ps1";
            var readmeFileName = "wdmaud2.drv - README.txt";

            var regHelperx86CmdFileName = "dev-replace-wdmaud2-x86.cmd";
            var regHelperx86Ps1FileName = "midi-replace-wdmaud2-drv-x86.ps1";


            var regHelperCmdFileFullPath = regHelpersLocation / regHelperCmdFileName;
            var regHelperPs1FileFullPath = regHelpersLocation / regHelperPs1FileName;
            var readmeFileFullPath = regHelpersLocation / readmeFileName;

            var regHelperx86CmdFileFullPath = regHelpersLocation / regHelperx86CmdFileName;
            var regHelperx86Ps1FileFullPath = regHelpersLocation / regHelperx86Ps1FileName;

            string driverFile = "wdmaud2.drv";
            string pdbFile = "wdmaud2.pdb";

            (ApiStagingFolder / "arm64" / driverFile).Copy(arm64 / driverFile, ExistsPolicy.Fail);
            (ApiStagingFolder / "arm64" / pdbFile).Copy(arm64 / pdbFile, ExistsPolicy.Fail); 
            (regHelperCmdFileFullPath).Copy(arm64 / regHelperCmdFileName, ExistsPolicy.Fail);
            (regHelperPs1FileFullPath).Copy(arm64 / regHelperPs1FileName, ExistsPolicy.Fail);
            (readmeFileFullPath).Copy(arm64 / readmeFileName, ExistsPolicy.Fail);


            (ApiStagingFolder / "x64" / driverFile).Copy(x64 / driverFile, ExistsPolicy.Fail);
            (ApiStagingFolder / "x64" / pdbFile).Copy(x64 / pdbFile, ExistsPolicy.Fail);
            (regHelperCmdFileFullPath).Copy(x64 / regHelperCmdFileName, ExistsPolicy.Fail);
            (regHelperPs1FileFullPath).Copy(x64 / regHelperPs1FileName, ExistsPolicy.Fail);
            (readmeFileFullPath).Copy(x64 / readmeFileName, ExistsPolicy.Fail);


            (ApiStagingFolder / "Win32" / driverFile).Copy(x86 / driverFile, ExistsPolicy.Fail);
            (ApiStagingFolder / "Win32" / pdbFile).Copy(x86 / pdbFile, ExistsPolicy.Fail);
            (regHelperx86CmdFileFullPath).Copy(x86 / regHelperx86CmdFileName, ExistsPolicy.Fail);
            (regHelperx86Ps1FileFullPath).Copy(x86 / regHelperx86Ps1FileName, ExistsPolicy.Fail);
            (readmeFileFullPath).Copy(x86 / readmeFileName, ExistsPolicy.Fail);


            x64.ZipTo(ThisReleaseFolder / $"wdmaud2-winmm-x64.zip");
            arm64.ZipTo(ThisReleaseFolder / $"wdmaud2-winmm-arm64.zip");
            x86.ZipTo(ThisReleaseFolder / $"wdmaud2-winmm-x86-win32.zip");

        });


    Target BuildAndPublishAll => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_CreateVersionIncludes)
        .DependsOn(T_BuildServiceAndPlugins)
        .DependsOn(T_BuildServiceAndPluginsInstaller)
        .DependsOn(T_ZipWdmaud2)
        .DependsOn(T_ZipPowershellDevUtilities)
        .DependsOn(T_ZipServicePdbs)
        .Executes(() =>
        {
            if (BuiltInBoxInstallers.Count > 0)
            {
                Console.WriteLine("\nBuilt in-box installers:");

                foreach (var item in BuiltInBoxInstallers)
                {
                    Console.WriteLine($"  {item.Key.PadRight(5)} {item.Value}");
                }
            }
            else
            {
                Console.WriteLine("No in-box installers built.");
            }

        });


}
