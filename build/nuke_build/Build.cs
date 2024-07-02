using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Microsoft.ApplicationInsights.Extensibility;
using Nuke.Common;
using Nuke.Common.CI;
using Nuke.Common.Execution;
using Nuke.Common.IO;
using Nuke.Common.ProjectModel;
using Nuke.Common.Tooling;
using Nuke.Common.Tools.DotNet;
using Nuke.Common.Tools.GitVersion;
using Nuke.Common.Tools.MSBuild;
using Nuke.Common.Tools.NuGet;
using Nuke.Common.Utilities.Collections;
using static Nuke.Common.EnvironmentInfo;
using static Nuke.Common.IO.FileSystemTasks;
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


    string SetupVersionName => "Developer Preview 6";
    string NuGetVersionName => "preview.6";

    // we set these here, especially the time, so it's the same for all platforms in the single build
    string SetupBuildMajorMinor = "1.0";
    string SetupBuildDateNumber = DateTime.Now.ToString("yy") + DateTime.Now.DayOfYear.ToString("000");       // YYddd where ddd is the day number for the year
    string SetupBuildTimeNumber = DateTime.Now.ToString("HHmm");       // HHmm


    string NugetFullVersionString => SetupBuildMajorMinor + "." + SetupBuildDateNumber + "." + SetupBuildTimeNumber + "-" + NuGetVersionName;
    string NugetFullPackageId => "Microsoft.Windows.Devices.Midi2";


    AbsolutePath _thisReleaseFolder;


    // ===========================================================

    // directories

    AbsolutePath BuildRootFolder => NukeBuild.RootDirectory / "build";


    AbsolutePath StagingRootFolder => BuildRootFolder / "staging";
    AbsolutePath AppSdkStagingFolder => StagingRootFolder / "app-sdk";
    AbsolutePath ApiStagingFolder => StagingRootFolder / "api";


    AbsolutePath ReleaseRootFolder => BuildRootFolder / "release";
    AbsolutePath AppSdkNugetOutputFolder => ReleaseRootFolder / "nuget";

    AbsolutePath ThisReleaseFolder => _thisReleaseFolder;


    AbsolutePath AppSdkImplementationInstallerReleaseFolder => BuildRootFolder / "app-sdk-impl";

    AbsolutePath SourceRootFolder => NukeBuild.RootDirectory / "src";
    AbsolutePath AppSdkSolutionFolder => SourceRootFolder / "app-sdk";
    AbsolutePath ApiSolutionFolder => SourceRootFolder / "api";

    AbsolutePath InBoxComponentsSetupSolutionFolder => SourceRootFolder / "oob-setup";

    AbsolutePath ApiReferenceFolder => SourceRootFolder / "shared" / "api-ref";



    AbsolutePath UserToolsRootFolder => SourceRootFolder / "user-tools";

    AbsolutePath MidiConsoleSolutionFolder => UserToolsRootFolder / "midi-console";
    AbsolutePath MidiConsoleStagingFolder => StagingRootFolder / "midi-console";
    AbsolutePath ConsoleSetupSolutionFolder => UserToolsRootFolder / "midi-console-setup";


    AbsolutePath MidiSettingsSolutionFolder => UserToolsRootFolder / "midi-settings";
    AbsolutePath MidiSettingsStagingFolder => StagingRootFolder / "midi-settings";
    AbsolutePath MidiSettingsSetupSolutionFolder => UserToolsRootFolder / "midi-settings-setup";

    //    AbsolutePath RustWinRTSamplesRootFolder => SamplesRootFolder / "rust-winrt";
    //    AbsolutePath ElectronJSSamplesRootFolder => SamplesRootFolder / "electron-js";

    AbsolutePath SetupBundleInfoIncludeFile => StagingRootFolder / "version" / "BundleInfo.wxi";


    AbsolutePath SamplesRootFolder => NukeBuild.RootDirectory / "samples";
    AbsolutePath SamplesCppWinRTSolutionFolder => SamplesRootFolder / "cpp-winrt";

    AbsolutePath SamplesCSWinRTSolutionFolder => SamplesRootFolder / "csharp-net";


    string[] InProcPlatforms => new string[] { "Arm64", "Arm64EC", "x64" };
    string[] OutOfProcPlatforms => new string[] { "Arm64", "x64" };

    public static int Main () => Execute<Build>(x => x.BuildAndPublishAll);


    Target Prerequisites => _ => _
        .Executes(() =>
        {
            // Need to verify that %MIDI_REPO_ROOT% is set (it's used by setup). If not, set it to the root \midi folder
            var rootVariableExists = !string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("MIDI_REPO_ROOT"));

            if (!rootVariableExists)
            {
                Environment.SetEnvironmentVariable("MIDI_REPO_ROOT", NukeBuild.RootDirectory);
            }

            _thisReleaseFolder = $"{ReleaseRootFolder / SetupVersionName} ({DateTime.Now.ToString("yyyy-MM-dd HH-mm-ss")})";

        });


    Target BuildServiceAndPlugins => _ => _
        .DependsOn(Prerequisites)
        .Executes(() =>
    {
        foreach (var platform in OutOfProcPlatforms)
        {
            string solutionDir = ApiSolutionFolder.ToString() + @"\";

            var msbuildProperties = new Dictionary<string, object>();
            msbuildProperties.Add("Platform", platform);
            msbuildProperties.Add("SolutionDir", solutionDir);  // to include trailing slash
            msbuildProperties.Add("NoWarn", "MIDL2111");        // IDL identifier length warning

            Console.Out.WriteLine($"----------------------------------------------------------------------");
            Console.Out.WriteLine($"SolutionDir: {solutionDir}");
            Console.Out.WriteLine($"Platform:    {platform}");


            MSBuildTasks.MSBuild(_ => _
                .SetTargetPath(ApiSolutionFolder / "midi2.sln")
                .SetMaxCpuCount(14)
                /*.SetOutDir(outputFolder) */
                /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                /*.SetTargets("Build") */
                .SetProperties(msbuildProperties)
                .SetConfiguration(Configuration.Release)
                .SetVerbosity(MSBuildVerbosity.Minimal)
                .EnableNodeReuse()
            );


            // copy binaries to staging folder
            var stagingFiles = new List<AbsolutePath>();

            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / Configuration.Release / $"Midisrv.exe");

            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / Configuration.Release / $"Midi2.MidiSrvAbstraction.dll");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / Configuration.Release / $"Midi2.DiagnosticsAbstraction.dll");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / Configuration.Release / $"Midi2.KSAbstraction.dll");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / Configuration.Release / $"Midi2.KSAggregateAbstraction.dll");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / Configuration.Release / $"Midi2.VirtualMidiAbstraction.dll");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / Configuration.Release / $"Midi2.LoopbackMidiAbstraction.dll");

            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / Configuration.Release / $"Midi2.BS2UMPTransform.dll");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / Configuration.Release / $"Midi2.UMP2BSTransform.dll");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / Configuration.Release / $"Midi2.UmpProtocolDownscalerTransform.dll");

            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / Configuration.Release / $"Midi2.SchedulerTransform.dll");

            foreach (var file in stagingFiles)
            {
                FileSystemTasks.CopyFileToDirectory(file, ApiStagingFolder / platform, FileExistsPolicy.Overwrite, true);
            }


        }

        // Copy reference files to reference folder. This requires
        // the SDK projects reference those instead of the current version
        // which references deep into the API project

        // copy x64 files to Arm64EC folder as well. No reason for the full
        // service and plugins to be Arm64EC just to provide a few headers

        var intermediateFolder = ApiSolutionFolder / "vsfiles" / "intermediate";

        foreach (var platform in InProcPlatforms)
        {
            var referenceFiles = new List<AbsolutePath>();

            // for the destination platform, we use x64 files here since they're not
            // binaries anyway
            string sourcePlatform = (platform == "Arm64EC" ? "x64" : platform);

            referenceFiles.Add(intermediateFolder / "idl" / sourcePlatform / Configuration.Release / "WindowsMidiServices.h");
            referenceFiles.Add(intermediateFolder / "idl" / sourcePlatform / Configuration.Release / "WindowsMidiServices_i.c");
            referenceFiles.Add(intermediateFolder / "idl" / sourcePlatform / Configuration.Release / "WindowsMidiServices_i.c");

            referenceFiles.Add(intermediateFolder / "Midi2.MidiSrvAbstraction" / sourcePlatform / Configuration.Release / "Midi2MidiSrvAbstraction.h");

            // copy the files over to the reference location
            foreach (var file in referenceFiles)
            {
                FileSystemTasks.CopyFileToDirectory(file, ApiReferenceFolder / platform, FileExistsPolicy.Overwrite, true);
            }
        }

    });


    Target BuildAndPackAllAppSDKs => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(BuildServiceAndPlugins)
        .Executes(() =>
        {
            foreach (var platform in InProcPlatforms)
            {
                string solutionDir = AppSdkSolutionFolder.ToString() + @"\";

                var msbuildProperties = new Dictionary<string, object>();
                msbuildProperties.Add("Platform", platform);
                msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash
                msbuildProperties.Add("NoWarn", "MSB3271");             // winmd and dll platform mismatch with Arm64EC

                Console.Out.WriteLine($"----------------------------------------------------------------------");
                Console.Out.WriteLine($"SolutionDir: {solutionDir}");
                Console.Out.WriteLine($"Platform:    {platform}");

                
                MSBuildTasks.MSBuild(_ => _
                    .SetTargetPath(AppSdkSolutionFolder / "app-sdk.sln")
                    .SetMaxCpuCount(14)
                    /*.SetOutDir(outputFolder) */
                    /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                    /*.SetTargets("Build") */
                    .SetProperties(msbuildProperties)
                    .SetConfiguration(Configuration.Release)
                    //.SetVerbosity(MSBuildVerbosity.Minimal)
                    .EnableNodeReuse()
                );

            }

            var sdkOutputRootFolder = AppSdkSolutionFolder / "vsfiles" / "out";

            foreach (var platform in InProcPlatforms)
            {
                var sdkBinaries = new List<AbsolutePath>();

                foreach (var ns in new string[] {
                    "Microsoft.Windows.Devices.Midi2",
                    "Microsoft.Windows.Devices.Midi2.ServiceConfig",
                    "Microsoft.Windows.Devices.Midi2.CapabilityInquiry",
                    "Microsoft.Windows.Devices.Midi2.ClientPlugins",
                    "Microsoft.Windows.Devices.Midi2.Diagnostics",
                    "Microsoft.Windows.Devices.Midi2.Messages",
                    "Microsoft.Windows.Devices.Midi2.Endpoints.Loopback",
                    "Microsoft.Windows.Devices.Midi2.Endpoints.Virtual",
                    "Microsoft.Windows.Devices.Midi2.Initialization"           // this last one gets packed 100% in the nuget, including the impl
                })
                {
                    sdkBinaries.Add(sdkOutputRootFolder / "coalesce" / platform / Configuration.Release / $"{ns}.winmd");
                    sdkBinaries.Add(sdkOutputRootFolder / "coalesce" / platform / Configuration.Release / $"{ns}.dll");
                    sdkBinaries.Add(sdkOutputRootFolder / "coalesce" / platform / Configuration.Release / $"{ns}.pri");

                    // todo: CS projection dll
                }

                // create the nuget package
                // todo: it would be better to see if any of the generated winmds have changed and only
                // do this step if those have changed. Maybe do a before/after date/time check?


                NuGetTasks.NuGetPack(_ => _
                    .SetTargetPath(AppSdkSolutionFolder / "projections" / "dotnet-and-cpp" / "nuget" / "Microsoft.Windows.Devices.Midi2.nuspec")
                    .AddProperty("version", NugetFullVersionString)
                    .AddProperty("id", NugetFullPackageId)
                    .SetOutputDirectory(AppSdkNugetOutputFolder)
                );

                // copy the files over to the reference location
                foreach (var file in sdkBinaries)
                {
                    FileSystemTasks.CopyFileToDirectory(file, AppSdkStagingFolder / platform, FileExistsPolicy.Overwrite, true);
                }

                // MIDI diagnostics app
                FileSystemTasks.CopyFileToDirectory(sdkOutputRootFolder / "mididiag" / platform / Configuration.Release / $"mididiag.exe", AppSdkStagingFolder / platform, FileExistsPolicy.Overwrite, true);

                // sample manifest
                FileSystemTasks.CopyFileToDirectory(AppSdkSolutionFolder / "MyMidiApp.exe.manifest", AppSdkStagingFolder / platform, FileExistsPolicy.Overwrite, true);
            }

        });



    Target BuildAppSdkRuntimeInstaller => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(BuildAndPackAllAppSDKs)
        .Executes(() =>
        {
            // we build for Arm64 and x64. No EC required here
            foreach (var platform in OutOfProcPlatforms)
            {
                UpdateSetupBundleInfoIncludeFile(platform);

                string fullSetupVersionString = $"{SetupVersionName} {SetupBuildMajorMinor}.{SetupBuildDateNumber}.{SetupBuildTimeNumber}";

                string solutionDir = AppSdkSolutionFolder.ToString() + @"\";

                var msbuildProperties = new Dictionary<string, object>();
                msbuildProperties.Add("Platform", platform);
                msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash

                Console.Out.WriteLine($"----------------------------------------------------------------------");
                Console.Out.WriteLine($"SolutionDir: {solutionDir}");
                Console.Out.WriteLine($"Platform:    {platform}");

                var setupSolutionFolder = AppSdkSolutionFolder / "sdk-runtime-installer";

                MSBuildTasks.MSBuild(_ => _
                    .SetTargetPath(setupSolutionFolder / "midi-services-app-sdk-runtime-setup.sln")
                    .SetMaxCpuCount(14)
                    /*.SetOutDir(outputFolder) */
                    /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                    /*.SetTargets("Build") */
                    .SetProperties(msbuildProperties)
                    .SetConfiguration(Configuration.Release)
                    .EnableNodeReuse()
                );

                // todo: it would be better to see if any of the sdk files have changed and only
                // do this copy if a new setup file was created. Maybe do a before/after date/time check?

                FileSystemTasks.CopyFile(
                    setupSolutionFolder / "main-bundle" / "bin" / platform / Configuration.Release / "WindowsMidiServicesSdkRuntimeSetup.exe",
                    ThisReleaseFolder / $"Windows MIDI Services (App SDK Runtime) - {fullSetupVersionString}-{platform.ToLower()}.exe");

            }

        });

    void UpdateSetupBundleInfoIncludeFile(string platform)
    {
        string versionString = $"{SetupBuildMajorMinor}.{SetupBuildDateNumber}.{SetupBuildTimeNumber}";

        using (StreamWriter writer = System.IO.File.CreateText(SetupBundleInfoIncludeFile))
        {
            writer.WriteLine("<Include>");
            writer.WriteLine($"  <?define SetupVersionName=\"{SetupVersionName} {platform}\" ?>");
            writer.WriteLine($"  <?define SetupVersionNumber=\"{versionString}\" ?>");
            writer.WriteLine("</Include>");
        }
    }

    Target BuildServiceAndPluginsInstaller => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(BuildServiceAndPlugins)
        .Executes(() =>
        {
            // we build for Arm64 and x64. No EC required here
            foreach (var platform in OutOfProcPlatforms)
            {
                UpdateSetupBundleInfoIncludeFile(platform);

                string fullSetupVersionString = $"{SetupVersionName} {SetupBuildMajorMinor}.{SetupBuildDateNumber}.{SetupBuildTimeNumber}";

                string solutionDir = InBoxComponentsSetupSolutionFolder.ToString() + @"\";

                var msbuildProperties = new Dictionary<string, object>();
                msbuildProperties.Add("Platform", platform);
                msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash

                Console.Out.WriteLine($"----------------------------------------------------------------------");
                Console.Out.WriteLine($"SolutionDir: {solutionDir}");
                Console.Out.WriteLine($"Platform:    {platform}");

                var output = MSBuildTasks.MSBuild(_ => _
                    .SetTargetPath(InBoxComponentsSetupSolutionFolder / "midi-services-in-box-setup.sln")
                    .SetMaxCpuCount(14)
                    /*.SetOutDir(outputFolder) */
                    /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                    /*.SetTargets("Build") */
                    .SetProperties(msbuildProperties)
                    .SetConfiguration(Configuration.Release)
                    .EnableNodeReuse()
                );


                // todo: it would be better to see if any of the sdk files have changed and only
                // do this copy if a new setup file was created. Maybe do a before/after date/time check?
                FileSystemTasks.CopyFile(
                    InBoxComponentsSetupSolutionFolder / "main-bundle" / "bin" / platform / Configuration.Release / "WindowsMidiServicesInBoxComponentsSetup.exe",
                    ThisReleaseFolder / $"Windows MIDI Services (In-Box Service) - {fullSetupVersionString}-{platform.ToLower()}.exe");
            }

        });


    Target BuildUserToolsSharedComponents => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(BuildAndPackAllAppSDKs)
        .Executes(() =>
        {
            // build x64 and Arm64
        });

    Target BuildSettingsApp => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(BuildAndPackAllAppSDKs)
        .DependsOn(BuildUserToolsSharedComponents)
        .Executes(() =>
        {
            // update nuget packages

            // build x64 and Arm64
        });

    Target BuildConsoleApp => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(BuildAndPackAllAppSDKs)
        .DependsOn(BuildUserToolsSharedComponents)
        .Executes(() =>
        {
            var solution = MidiConsoleSolutionFolder / "midi-console.sln";

            // for the MIDI nuget package
            NuGetTasks.NuGetInstall(_ => _
                .SetProcessWorkingDirectory(MidiConsoleSolutionFolder)
                .SetPreRelease(true)
                .SetSource(AppSdkNugetOutputFolder)
                .SetPackageID(NugetFullPackageId)
            );

            NuGetTasks.NuGetRestore(_ => _
                .SetProcessWorkingDirectory(MidiConsoleSolutionFolder)
                .SetSolutionDirectory(MidiConsoleSolutionFolder)
                .SetSource(AppSdkNugetOutputFolder)
            );

            // build x64 and Arm64, no Arm64EC
            foreach (var platform in OutOfProcPlatforms)
            {
                string solutionDir = MidiConsoleSolutionFolder.ToString() + @"\";

                string rid = platform.ToLower() == "arm64" ? "win-arm64" : "win-x64";


                //var msbuildProperties = new Dictionary<string, object>();
                //msbuildProperties.Add("Platform", platform);
                //msbuildProperties.Add("SolutionDir", solutionDir);          // to include trailing slash
                //msbuildProperties.Add("RuntimeIdentifier", rid);          
                ////msbuildProperties.Add("NoWarn", "MSB3271");             // winmd and dll platform mismatch with Arm64EC

                //Console.Out.WriteLine($"----------------------------------------------------------------------");
                //Console.Out.WriteLine($"Solution:    {solution}");
                //Console.Out.WriteLine($"SolutionDir: {solutionDir}");
                //Console.Out.WriteLine($"Platform:    {platform}");
                //Console.Out.WriteLine($"RID:         {rid}");


                DotNetTasks.DotNetBuild(_ => _
                    .SetProjectFile(MidiConsoleSolutionFolder / "Midi" / "Midi.csproj")
                    .SetConfiguration(Configuration.Release)
                    .SetPublishSingleFile(false)
                    .SetPublishTrimmed(false)
                    .SetSelfContained(false)
                    .SetRuntime(rid)
                );

                // copy output to staging folder

                // TODO: This doesn't deal with any localization content

                var consoleOutputFolder = MidiConsoleSolutionFolder / "Midi" / "bin" / Configuration.Release / "net8.0-windows10.0.20348.0" / rid ;
                //var runtimesFolder = consoleOutputFolder / "runtimes" / rid / "native";
                var runtimesFolder = consoleOutputFolder;

                var stagingFolder = MidiConsoleStagingFolder / platform;

                FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "midi.exe", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "midi.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "midi.deps.json", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "midi.runtimeconfig.json", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "midi.exe.manifest", stagingFolder, FileExistsPolicy.Overwrite, true);

                FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "WinRT.Runtime.dll", stagingFolder, FileExistsPolicy.Overwrite, true);

                FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.NetProjection.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.SDK.NET.dll", stagingFolder, FileExistsPolicy.Overwrite, true);

                FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Spectre.Console.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Spectre.Console.Cli.dll", stagingFolder, FileExistsPolicy.Overwrite, true);

                FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "System.CodeDom.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "System.Diagnostics.EventLog.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "System.Diagnostics.EventLog.Messages.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "System.Management.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "System.ServiceProcess.ServiceController.dll", stagingFolder, FileExistsPolicy.Overwrite, true);

                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.ServiceConfig.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.CapabilityInquiry.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.ClientPlugins.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Diagnostics.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Messages.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Endpoints.Virtual.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Initialization.dll", stagingFolder, FileExistsPolicy.Overwrite, true);

                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.ServiceConfig.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.CapabilityInquiry.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.ClientPlugins.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Diagnostics.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Messages.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Endpoints.Virtual.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Initialization.pri", stagingFolder, FileExistsPolicy.Overwrite, true);

                //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
                //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.ServiceConfig.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
                //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.CapabilityInquiry.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
                //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.ClientPlugins.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
                //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.Diagnostics.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
                //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.Messages.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
                //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
                //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.Endpoints.Virtual.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
                //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.Initialization.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);

            }

        });

    Target BuildConsoleAppInstaller => _ => _
        .DependsOn(BuildConsoleApp)
        .DependsOn(BuildServiceAndPluginsInstaller)
        .Executes(() =>
        {
            // we build for Arm64 and x64. No EC required here
            foreach (var platform in OutOfProcPlatforms)
            {
                //UpdateSetupBundleInfoIncludeFile(platform);   // happens as part of service installer

                string fullSetupVersionString = $"{SetupVersionName} {SetupBuildMajorMinor}.{SetupBuildDateNumber}.{SetupBuildTimeNumber}";

                string solutionDir = ConsoleSetupSolutionFolder.ToString() + @"\";

                var msbuildProperties = new Dictionary<string, object>();
                msbuildProperties.Add("Platform", platform);
                msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash

                Console.Out.WriteLine($"----------------------------------------------------------------------");
                Console.Out.WriteLine($"SolutionDir: {solutionDir}");
                Console.Out.WriteLine($"Platform:    {platform.ToLower()}");

                var output = MSBuildTasks.MSBuild(_ => _
                    .SetTargetPath(ConsoleSetupSolutionFolder / "midi-console-setup.sln")
                    .SetMaxCpuCount(14)
                    /*.SetOutDir(outputFolder) */
                    /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                    /*.SetTargets("Build") */
                    .SetProperties(msbuildProperties)
                    .SetConfiguration(Configuration.Release)
                    .EnableNodeReuse()
                );


                // todo: it would be better to see if any of the sdk files have changed and only
                // do this copy if a new setup file was created. Maybe do a before/after date/time check?
                FileSystemTasks.CopyFile(
                    ConsoleSetupSolutionFolder / "console-main-bundle" / "bin" / platform / Configuration.Release / "WindowsMidiServicesConsoleSetup.exe",
                    ThisReleaseFolder / $"Windows MIDI Services (Console) - {fullSetupVersionString}-{platform.ToLower()}.exe");
            }


        });

    Target BuildCppSamples => _ => _
        .DependsOn(BuildAndPackAllAppSDKs)
        .Executes(() =>
        {
            var solution = SamplesCppWinRTSolutionFolder / "cpp-winrt-samples.sln";

            // update nuget packages

            // for the MIDI nuget package
            NuGetTasks.NuGetInstall(_ => _
                .SetProcessWorkingDirectory(SamplesCppWinRTSolutionFolder)
                .SetPreRelease(true)
                .SetSource(AppSdkNugetOutputFolder)
                .SetPackageID(NugetFullPackageId)
            );

            // for cppwinrt
            NuGetTasks.NuGetInstall(_ => _
                .SetProcessWorkingDirectory(SamplesCppWinRTSolutionFolder)
                .SetPreRelease(true)
                .SetSource("https://api.nuget.org/v3/index.json")
                .SetPackageID("Microsoft.Windows.CppWinRT")
            );

            // for WIL
            //NuGetTasks.NuGetInstall(_ => _
            //    .SetProcessWorkingDirectory(SamplesCppWinRTSolutionFolder)
            //    .SetPreRelease(true)
            //    .SetSource("https://api.nuget.org/v3/index.json")
            //    .SetPackageID("Microsoft.Windows.ImplementationLibrary")
            //);


            NuGetTasks.NuGetRestore(_ => _
                .SetProcessWorkingDirectory(SamplesCppWinRTSolutionFolder)
                .SetSolutionDirectory(SamplesCppWinRTSolutionFolder)
                .SetSource(AppSdkNugetOutputFolder)
            /*.SetConfigFile(solution)*/
            );


            
            // make sure they compile
            foreach (var platform in InProcPlatforms)
            {
                string solutionDir = AppSdkSolutionFolder.ToString() + @"\";

                var msbuildProperties = new Dictionary<string, object>();
                msbuildProperties.Add("Platform", platform);
                msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash
                //msbuildProperties.Add("NoWarn", "MSB3271");             // winmd and dll platform mismatch with Arm64EC

                Console.Out.WriteLine($"----------------------------------------------------------------------");
                Console.Out.WriteLine($"Solution:    {solution}");
                Console.Out.WriteLine($"SolutionDir: {solutionDir}");
                Console.Out.WriteLine($"Platform:    {platform}");


                MSBuildTasks.MSBuild(_ => _
                    .SetTargetPath(solution)
                    .SetMaxCpuCount(14)
                    /*.SetOutDir(outputFolder) */
                    /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                    /*.SetTargets("Build") */
                    .SetProperties(msbuildProperties)
                    .SetConfiguration(Configuration.Release)
                    //.SetVerbosity(MSBuildVerbosity.Minimal)
                    .EnableNodeReuse()
                );




            }
        });



    Target BuildAndPublishAll => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(BuildServiceAndPlugins)
        .DependsOn(BuildServiceAndPluginsInstaller)
        .DependsOn(BuildAndPackAllAppSDKs)
        .DependsOn(BuildAppSdkRuntimeInstaller)
        .DependsOn(BuildCppSamples)
        .DependsOn(BuildSettingsApp)
        /*.DependsOn(BuildSettingsAppInstaller) */
        .DependsOn(BuildConsoleApp)
        .DependsOn(BuildConsoleAppInstaller)
        .Executes(() =>
        {
        });


}
