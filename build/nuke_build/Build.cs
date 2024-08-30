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
using Nuke.Common.Tools.Npm;
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


    string SetupVersionName => "Developer Preview 7";
    string NuGetVersionName => "preview.7";

    // we set these here, especially the time, so it's the same for all platforms in the single build
    string SetupBuildMajorMinor = "1.0";
    string SetupBuildDateNumber = DateTime.Now.ToString("yy") + DateTime.Now.DayOfYear.ToString("000");       // YYddd where ddd is the day number for the year
    string SetupBuildTimeNumber = DateTime.Now.ToString("HHmm");       // HHmm


    string NugetFullVersionString => SetupBuildMajorMinor + "." + SetupBuildDateNumber + "." + SetupBuildTimeNumber + "-" + NuGetVersionName;
    string NugetFullPackageId => "Microsoft.Windows.Devices.Midi2";


    AbsolutePath _thisReleaseFolder;


    string[] AppSdkAssemblies => new string[] {
        "Microsoft.Windows.Devices.Midi2",
        "Microsoft.Windows.Devices.Midi2.Initialization"
    };


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
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / Configuration.Release / $"Midi2.VirtualPatchBayAbstraction.dll");

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

                foreach (var ns in AppSdkAssemblies)
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

            }



            foreach(var targetPlatform in InProcPlatforms)
            {
                string sourcePlatform;

                if (targetPlatform.ToLower() == "arm64ec")
                {
                    sourcePlatform = "Arm64";
                }
                else
                {
                    sourcePlatform = targetPlatform;
                }

                // MIDI diagnostics app
                FileSystemTasks.CopyFileToDirectory(sdkOutputRootFolder / "mididiag" / sourcePlatform / Configuration.Release / $"mididiag.exe", AppSdkStagingFolder / targetPlatform, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(sdkOutputRootFolder / "mididiag" / sourcePlatform / Configuration.Release / $"mididiag.exe.manifest", AppSdkStagingFolder / targetPlatform, FileExistsPolicy.Overwrite, true);

                // MIDI USB info utility
                FileSystemTasks.CopyFileToDirectory(sdkOutputRootFolder / "midiusbinfo" / sourcePlatform / Configuration.Release / $"midiusbinfo.exe", AppSdkStagingFolder / targetPlatform, FileExistsPolicy.Overwrite, true);

                // sample manifest
                FileSystemTasks.CopyFileToDirectory(AppSdkSolutionFolder / "ExampleMidiApp.exe.manifest", AppSdkStagingFolder / targetPlatform, FileExistsPolicy.Overwrite, true);

            }

        });



    Target BuildAppSdkRuntimeAndToolsInstaller => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(BuildConsoleApp)
        //.DependsOn(BuildSettingsApp)
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
                    ThisReleaseFolder / $"Windows MIDI Services (Tools and SDKs) {fullSetupVersionString}-{platform.ToLower()}.exe");

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
            //var solution = MidiSettingsSolutionFolder / "midi-settings.sln";

            //// for the MIDI nuget package
            //NuGetTasks.NuGetInstall(_ => _
            //    .SetProcessWorkingDirectory(MidiSettingsSolutionFolder)
            //    .SetPreRelease(true)
            //    .SetSource(AppSdkNugetOutputFolder)
            //    .SetPackageID(NugetFullPackageId)
            //);

            //NuGetTasks.NuGetRestore(_ => _
            //    .SetProcessWorkingDirectory(MidiSettingsSolutionFolder)
            //    .SetSolutionDirectory(MidiSettingsSolutionFolder)
            //    .SetSource(AppSdkNugetOutputFolder)
            //);

            //// build x64 and Arm64, no Arm64EC
            //foreach (var platform in OutOfProcPlatforms)
            //{
            //    string solutionDir = MidiSettingsSolutionFolder.ToString() + @"\";

            //    string rid = platform.ToLower() == "arm64" ? "win-arm64" : "win-x64";


            //    //DotNetTasks.DotNetBuild(_ => _
            //    //    .SetProjectFile(MidiSettingsSolutionFolder / "Microsoft.Devices.Midi2.Tools.Shared" / "Microsoft.Devices.Midi2.Tools.Shared.csproj")
            //    //    .SetConfiguration(Configuration.Release)
            //    //    .SetPublishSingleFile(false)
            //    //    .SetPublishTrimmed(false)
            //    //    .SetSelfContained(false)
            //    //    .SetRuntime(rid)
            //    //);

            //    DotNetTasks.DotNetBuild(_ => _
            //        .SetProjectFile(MidiSettingsSolutionFolder / "Microsoft.Midi.Settings" / "Microsoft.Midi.Settings.csproj")
            //        .SetConfiguration(Configuration.Release)
            //        .SetPublishSingleFile(false)
            //        .SetPublishTrimmed(false)
            //        .SetSelfContained(false)
            //        .SetRuntime(rid)
            //    );

            //    // copy output to staging folder

            //    // TODO: This doesn't deal with any localization content

            //    var consoleOutputFolder = MidiConsoleSolutionFolder / "Midi" / "bin" / Configuration.Release / "net8.0-windows10.0.20348.0" / rid;
            //    //var runtimesFolder = consoleOutputFolder / "runtimes" / rid / "native";
            //    var runtimesFolder = consoleOutputFolder;

            //    var stagingFolder = MidiConsoleStagingFolder / platform;

            //    FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "midi.exe", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "midi.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "midi.deps.json", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "midi.runtimeconfig.json", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "midi.exe.manifest", stagingFolder, FileExistsPolicy.Overwrite, true);

            //    FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "WinRT.Runtime.dll", stagingFolder, FileExistsPolicy.Overwrite, true);

            //    FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.NetProjection.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.SDK.NET.dll", stagingFolder, FileExistsPolicy.Overwrite, true);

            //    FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Spectre.Console.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Spectre.Console.Cli.dll", stagingFolder, FileExistsPolicy.Overwrite, true);

            //    FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "System.CodeDom.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "System.Diagnostics.EventLog.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "System.Diagnostics.EventLog.Messages.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "System.Management.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "System.ServiceProcess.ServiceController.dll", stagingFolder, FileExistsPolicy.Overwrite, true);

            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.ServiceConfig.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.CapabilityInquiry.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.ClientPlugins.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Diagnostics.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Messages.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Endpoints.Virtual.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Utilities.SysEx.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.VirtualPatchBay.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Initialization.dll", stagingFolder, FileExistsPolicy.Overwrite, true);

            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.ServiceConfig.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.CapabilityInquiry.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.ClientPlugins.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Diagnostics.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Messages.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Endpoints.Virtual.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.VirtualPatchBay.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Utilities.SysEx.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Initialization.pri", stagingFolder, FileExistsPolicy.Overwrite, true);

            //    //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.ServiceConfig.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.CapabilityInquiry.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.ClientPlugins.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.Diagnostics.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.Messages.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.Endpoints.Virtual.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.Initialization.winmd", stagingFolder, FileExistsPolicy.Overwrite, true);

            //}

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


                foreach (var ns in AppSdkAssemblies)
                {
                    FileSystemTasks.CopyFileToDirectory(runtimesFolder / ns + ".dll", stagingFolder, FileExistsPolicy.Overwrite, true);
                    FileSystemTasks.CopyFileToDirectory(runtimesFolder / ns + ".pri", stagingFolder, FileExistsPolicy.Overwrite, true);
                    //FileSystemTasks.CopyFileToDirectory(runtimesFolder / ns + ".winmd", stagingFolder, FileExistsPolicy.Overwrite, true);
                }

            }

        });

    //Target BuildConsoleAppInstaller => _ => _
    //    .DependsOn(BuildConsoleApp)
    //    .DependsOn(BuildServiceAndPluginsInstaller)
    //    .Executes(() =>
    //    {
    //        // we build for Arm64 and x64. No EC required here
    //        foreach (var platform in OutOfProcPlatforms)
    //        {
    //            //UpdateSetupBundleInfoIncludeFile(platform);   // happens as part of service installer

    //            string fullSetupVersionString = $"{SetupVersionName} {SetupBuildMajorMinor}.{SetupBuildDateNumber}.{SetupBuildTimeNumber}";

    //            string solutionDir = ConsoleSetupSolutionFolder.ToString() + @"\";

    //            var msbuildProperties = new Dictionary<string, object>();
    //            msbuildProperties.Add("Platform", platform);
    //            msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash

    //            Console.Out.WriteLine($"----------------------------------------------------------------------");
    //            Console.Out.WriteLine($"SolutionDir: {solutionDir}");
    //            Console.Out.WriteLine($"Platform:    {platform.ToLower()}");

    //            var output = MSBuildTasks.MSBuild(_ => _
    //                .SetTargetPath(ConsoleSetupSolutionFolder / "midi-console-setup.sln")
    //                .SetMaxCpuCount(14)
    //                /*.SetOutDir(outputFolder) */
    //                /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
    //                /*.SetTargets("Build") */
    //                .SetProperties(msbuildProperties)
    //                .SetConfiguration(Configuration.Release)
    //                .EnableNodeReuse()
    //            );


    //            // todo: it would be better to see if any of the sdk files have changed and only
    //            // do this copy if a new setup file was created. Maybe do a before/after date/time check?
    //            FileSystemTasks.CopyFile(
    //                ConsoleSetupSolutionFolder / "console-main-bundle" / "bin" / platform / Configuration.Release / "WindowsMidiServicesConsoleSetup.exe",
    //                ThisReleaseFolder / $"Windows MIDI Services (Console) - {fullSetupVersionString}-{platform.ToLower()}.exe");
    //        }


    //    });

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


    // hard-coded paths to just get around some runtime limitations. This should
    // be re-done to make this build more portable

    [LocalPath(@"g:\github\microsoft\midi\build\electron-projection\nodert\src\NodeRtCmd\bin\Debug\NodeRtCmd.exe")]
    readonly Tool NodeRT;


    [LocalPath(@"C:\Users\peteb\AppData\Roaming\npm\node-gyp.cmd")]
    readonly Tool NodeGyp;


    Target BuildAndPackageElectronProjection => _ => _
        .DependsOn(BuildAndPackAllAppSDKs)
        //.DependsOn(BuildAppSdkRuntimeAndToolsInstaller)
        .Executes(() =>
    {
        var projectionOutputRoot = ElectronProjectionRootFolder / "projection";
        projectionOutputRoot.CreateOrCleanDirectory();

        var projectionReleaseRoot = ElectronProjectionRootFolder / "projection_release";
        projectionReleaseRoot.CreateOrCleanDirectory();


        foreach (var platform in OutOfProcPlatforms)
        {
            var platformOutputRootFolder = projectionOutputRoot / platform;
            platformOutputRootFolder.CreateDirectory();

            // copy all winmds to the parent folder before we process any of them
            //foreach (var ns in AppSdkNamespaces)
            //{
            //    FileSystemTasks.CopyFileToDirectory(
            //        AppSdkStagingFolder / platform / ns + ".winmd",
            //        platformOutputRootFolder,
            //        FileExistsPolicy.Overwrite, true);
            //}

            // main windows metadata file
            FileSystemTasks.CopyFileToDirectory(
                @"C:\Program Files (x86)\Windows Kits\10\UnionMetadata\10.0.20348.0\Windows.winmd",
                platformOutputRootFolder,
                FileExistsPolicy.Overwrite, true);

            foreach (var ns in AppSdkAssemblies)
            {
                var nsRootFolder = platformOutputRootFolder / ns.ToLower();
                nsRootFolder.CreateDirectory();

                var namespaceBuildFolder = nsRootFolder / "build";
                namespaceBuildFolder.CreateOrCleanDirectory();

                // the winmd files are in the parent folder
                //NodeRT($"--winmd {AppSdkStagingFolder / platform / ns + ".winmd"} --outdir {platformOutputRootFolder} --verbose");
                NodeRT($"--winmd {AppSdkStagingFolder / platform / ns + ".winmd"} --outdir {platformOutputRootFolder}");

                // todo: need to capture return code
            }

            // the NodeRt tool creates folders for each namespace, not for 
            // each winmd, so we then need to loop through and compile the
            // dll for each namespace.

            var namespaceFolders = Globbing.GlobDirectories(platformOutputRootFolder, "microsoft.windows.devices.midi2*");
            //var namespaceFolders = Globbing.GlobDirectories(platformOutputRootFolder);

            foreach (var nsFolder in namespaceFolders)
            {
                // build the projection
                Console.Out.WriteLine("--------------");
                Console.Out.WriteLine($"Rebuilding: {nsFolder}");

                //NodeGyp(
                //    arguments: $"rebuild --msvs_version=2022",
                //    workingDirectory: nsFolder,
                //    logOutput: false
                //    );

                // node-gyp rebuild --target=32.0.0 --arch=x64 --dist-url=https://electronjs.org/headers

                //NpmTasks.Npm(
                //    "install electron --save-dev",
                //    nsFolder);


                //NodeGyp(
                //    arguments: $"configure" +
                //        $" --node_use_v8_platform=false " +
                //        $" --node_use_bundled_v8=false" +
                //        $" --msvs_version=2022" +
                //        $" --target=32.0.0" +
                //        $" --dist-url=https://electronjs.org/headers" +
                //        
                //    workingDirectory: nsFolder,
                //    logOutput: false
                //    );

                NodeGyp(
                    arguments: $"rebuild" +
                        $" --openssl_fips=X" +
                        $" --arch={platform.ToLower()}" +
                        $" --verbose" +
                        $" --msvs_version=2022",
                    workingDirectory: nsFolder,
                    logOutput: true
                    );




                // todo: need to capture return code

                //NpmTasks.Npm(
                //    "install --save-dev @electron/rebuild",
                //    nsFolder);

                // Next step is to execute the electron-rebuild.cmd
                // .\node_modules\.bin\electron-rebuild.cmd
                // but that path is different for each one, and nuke
                // doesn't seem to support that.


                // now copy the output files

                //var nsReleaseRootFolder = projectionReleaseRoot / platform / nsFolder.Name.ToLower();
                //nsReleaseRootFolder.CreateDirectory();

                //binding.node is the binary

                // Because of electron versioning vs node versioning, it's easier for the
                // consumer if we just ship all the source, as crazy as that is for this.

                FileSystemTasks.CopyDirectoryRecursively(
                    nsFolder,
                    projectionReleaseRoot / platform,
                    DirectoryExistsPolicy.Fail
                    );




                var sourceBinFolder = nsFolder / "build" / "Release";

                //FileSystemTasks.CopyFileToDirectory(
                //    sourceBinFolder / "binding.node",
                //    nsReleaseRootFolder / "build" / "Release",
                //    FileExistsPolicy.Overwrite, true);

                //// we also want the three files in the lib folder

                //var sourceLibFolder = nsFolder / "lib";

                //var libFiles = sourceLibFolder.GlobFiles("*.js", "*.ts");

                //foreach (var libFile in libFiles)
                //{
                //    //Console.WriteLine($"Copying Projection File: {libFile}");

                //    FileSystemTasks.CopyFileToDirectory(
                //        libFile,
                //        nsReleaseRootFolder / "lib",
                //        FileExistsPolicy.Overwrite, true);
                //}

                //FileSystemTasks.CopyFileToDirectory(
                //    nsFolder / "package.json",
                //    nsReleaseRootFolder,
                //    FileExistsPolicy.Overwrite, true);


            }

            // copy the example manifest from staging
            // copy the winmd and impl libs from staging

            var sdkFiles = (AppSdkStagingFolder / platform).GlobFiles("*.dll", "*.winmd", "*.pri");

            foreach (var sdkFile in sdkFiles)
            {
                FileSystemTasks.CopyFileToDirectory(
                    sdkFile,
                    projectionReleaseRoot / platform,
                    FileExistsPolicy.Overwrite, true);
            }

            // copy the manifest over and name it the default for electron apps
            FileSystemTasks.CopyFile(
                AppSdkStagingFolder / platform / "ExampleMidiApp.exe.manifest",
                projectionReleaseRoot / platform / "electron.exe.manifest" ,
                FileExistsPolicy.Overwrite, true);

            // now, we zip it all up and put it over into the release folder

            (projectionReleaseRoot / platform).ZipTo(ThisReleaseFolder / $"electron-projection-{platform.ToLower()}.zip");


        }

    });






    Target BuildAndPublishAll => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(BuildServiceAndPlugins)
        .DependsOn(BuildServiceAndPluginsInstaller)
        .DependsOn(BuildAndPackAllAppSDKs)
        .DependsOn(BuildConsoleApp)
        .DependsOn(BuildSettingsApp)
        .DependsOn(BuildAppSdkRuntimeAndToolsInstaller)
        .DependsOn(BuildCppSamples)
        .DependsOn(BuildAndPackageElectronProjection)
        .Executes(() =>
        {
        });


}
