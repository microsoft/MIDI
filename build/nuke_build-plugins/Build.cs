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
    
    const UInt16 BuildVersionMajor = 0;
    const UInt16 BuildVersionMinor = 0;
    const UInt16 BuildVersionPatch = 15;

    UInt16 BuildVersionPreviewNumber = 15; // 从版本文件中读取，默认值为 15
    string VersionName => "Plugin Preview " + BuildVersionPreviewNumber;

    // --------------------------------------------------------------------------------------

    UInt16 BuildVersionBuildNumber = 0; // 从版本文件中读取

    readonly string BuildMajorMinorPatch = BuildVersionMajor.ToString() + "." + BuildVersionMinor.ToString() + "." + BuildVersionPatch.ToString();

    string BuildVersionPreviewString;
    string BuildVersionFullString = "";
    string BuildVersionAssemblyFullString = "";
    string BuildVersionFileFullString = "";

    string NugetPackageId => "Microsoft.Windows.Devices.Midi2";
    string NugetPackageVersion;
    string NugetFullPackageIdWithVersion = "";


    const string TargetWindowsSdkVersion = "10.0.22621.0";

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
    AbsolutePath SimpleLoopbackSetupSolutionFolder => SourceRootFolder / "oob-setup-simple-loopback";


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
                Environment.SetEnvironmentVariable("MIDI_REPO_ROOT", NukeBuild.RootDirectory.ToString().TrimEnd('/', '\\') + "\\");
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
            msbuildProperties.Add("MidiBuildVersionBuildNumber", BuildVersionBuildNumber);
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

                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.SimpleLoopbackMidiTransport.dll");
                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / ServiceBuildConfiguration / $"Midi2.SimpleLoopbackMidiTransport.pdb");

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
# Line 1: Major.Minor.Patch (e.g.: 0.0.15) [Required]
# 第1行：主版本号.次版本号.补丁版本号（例如：0.0.15）[必需]
# Line 2: Preview.Minor.Build (e.g.: 15.0.0 or 15.0) [Optional]
# 第2行：预览版本号.次版本号.构建号（例如：15.0.0 或 15.0）[可选]
#
# Note:
# 注意：
# Lines starting with # are treated as comments and will be ignored
# 以 # 开头的行被视为注释，会被忽略
#
# Example:
# 示例：0.0.15-preview.15.0.0

0.0.15
15.0.0
";

    void IncrementBuildNumber()
    {
        // 从版本文件中读取版本信息
        // 文件格式：
        // 第1行：主版本号.次版本号.补丁版本号 (必需，例如: 0.0.15)
        // 第2行：预览版本号.次版本号.构建号 (可选，支持 15.0.0 或 15.0 格式，默认为 15.0.0)
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
                        Logger.Error("Expected format: Major.Minor.Patch (e.g. 0.0.15) / 期望格式: 主版本.次版本.补丁 (例如 0.0.15)");
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
                            Logger.Error("Expected format: Preview.Minor.Build (e.g. 15.0.0 or 15.0) / 期望格式: 预览版本.次版本.构建号 (例如 15.0.0 或 15.0)");
                            hasError = true;
                        }
                        
                        // 读取构建号（最后一部分）
                        if (previewBuildParts.Length >= 2)
                        {
                            // 支持 Preview.Minor.Build (15.0.0) 或 Preview.Build (15.0) 格式
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

    Target T_BuildSimpleLoopbackPluginInstaller => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_BuildInDevelopmentServicePlugins)
        .Executes(() =>
        {
            // we build for Arm64 and x64. No EC required here
            foreach (var platform in InstallerPlatforms)
            {
                UpdateSetupBundleInfoIncludeFile(platform);

                //string fullSetupVersionString = $"{SetupVersionName} {SetupBuildMajorMinor}.{SetupBuildDateNumber}.{SetupBuildTimeNumber}";

                string solutionDir = SimpleLoopbackSetupSolutionFolder.ToString() + @"\";

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
                    .SetTargetPath(SimpleLoopbackSetupSolutionFolder / "midi-services-simple-loopback-setup.sln")
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

                string newInstallerName = $"Windows MIDI Services (Simple Loopback Preview) {BuildVersionFullString}-{platform.ToLower()}.exe";


                var setupFile = SimpleLoopbackSetupSolutionFolder / "main-bundle" / "bin" / platform / Configuration.Release / "WindowsMidiServicesSimpleLoopbackSetup.exe";
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

            XmlElement element = doc.SelectSingleNode($"/msb:Project/msb:PropertyGroup/msb:WindowsMidiServicesSdkPackage", manager) as XmlElement;

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

            XmlElement element = doc.SelectSingleNode($"/packages/package[@id = \"{NugetPackageId}\"]") as XmlElement;

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

    [LocalPath(@"g:\github\microsoft\midi\build\electron-projection\nodert\src\NodeRtCmd\bin\Debug\NodeRtCmd.exe")]
    readonly Tool NodeRT;


    [LocalPath(@"C:\Users\peteb\AppData\Roaming\npm\node-gyp.cmd")]
    readonly Tool NodeGyp;


    Target BuildAndPublishAll => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_CreateVersionIncludes)
        .DependsOn(T_BuildInDevelopmentServicePlugins)
        .DependsOn(T_BuildNetworkMidiInstaller)
        .DependsOn(T_BuildVirtualPatchBayPluginInstaller)
        .DependsOn(T_BuildSimpleLoopbackPluginInstaller)
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


}
