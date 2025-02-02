using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Xml;
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


    //string VersionName => "Developer Preview 9";
    string VersionName => "Customer Preview 1";
    string NuGetVersionName => "preview-10";

    // we set these here, especially the time, so it's the same for all platforms in the single build

    const string BuildVersionMajor = "1";
    const string BuildVersionMinor = "0";
    const string BuildVersionRevision = "3";
    const string BuildMajorMinorRevision = $"{BuildVersionMajor}.{BuildVersionMinor}.{BuildVersionRevision}";

    //string BuildDateNumber = DateTime.Now.ToString("yy") + DateTime.Now.DayOfYear.ToString("000");       // YYddd where ddd is the day number for the year
    string BuildDateNumber = DateTime.Now.ToString("yy") + DateTime.Now.Month.ToString("00") + DateTime.Now.Day.ToString("00");
    string BuildTimeNumber = UInt32.Parse(DateTime.Now.ToString("HHmm")).ToString();     // HHmm


    string NugetVersionString => BuildMajorMinorRevision + "-" + NuGetVersionName + "." + BuildDateNumber + "-" + BuildTimeNumber ;
    string NugetPackageId => "Microsoft.Windows.Devices.Midi2";

    string NugetFullPackageIdWithVersion => NugetPackageId + "." + NugetVersionString;


    string SetupBundleFullVersionString => BuildMajorMinorRevision + "-" + NuGetVersionName + "." + BuildDateNumber + "-" + BuildTimeNumber;


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

    AbsolutePath AppSdkSetupSolutionFolder => AppSdkSolutionFolder / "sdk-runtime-installer";

    AbsolutePath InBoxComponentsSetupSolutionFolder => SourceRootFolder / "oob-setup";

    AbsolutePath InDevelopmentServiceComponentsSetupSolutionFolder => SourceRootFolder / "oob-setup-in-dev";

    AbsolutePath ApiReferenceFolder => SourceRootFolder / "shared" / "api-ref";



    AbsolutePath UserToolsRootFolder => SourceRootFolder / "user-tools";

    AbsolutePath MidiConsoleSolutionFolder => UserToolsRootFolder / "midi-console";
    AbsolutePath MidiConsoleStagingFolder => StagingRootFolder / "midi-console";
    AbsolutePath ConsoleSetupSolutionFolder => UserToolsRootFolder / "midi-console-setup";


    AbsolutePath MidiSettingsSolutionFolder => UserToolsRootFolder / "midi-settings";
    AbsolutePath MidiSettingsStagingFolder => StagingRootFolder / "midi-settings";
   // AbsolutePath MidiSettingsSetupSolutionFolder => UserToolsRootFolder / "midi-settings-setup";

    //    AbsolutePath RustWinRTSamplesRootFolder => SamplesRootFolder / "rust-winrt";
    //    AbsolutePath ElectronJSSamplesRootFolder => SamplesRootFolder / "electron-js";

    AbsolutePath SetupBundleInfoIncludeFile => StagingRootFolder / "version" / "BundleInfo.wxi";

    AbsolutePath CommonVersionHeaderFile => StagingRootFolder / "version" / "WindowsMidiServicesVersion.h";
    AbsolutePath CommonVersionCSharpFile => StagingRootFolder / "version" / "WindowsMidiServicesVersion.cs";

    AbsolutePath SamplesRootFolder => NukeBuild.RootDirectory / "samples";
    AbsolutePath SamplesCppWinRTSolutionFolder => SamplesRootFolder / "cpp-winrt";

    AbsolutePath SamplesCSWinRTSolutionFolder => SamplesRootFolder / "csharp-net";


    string[] SdkPlatforms => new string[] { "x64", "Arm64EC"  };
    string[] ServiceAndApiPlatforms => new string[] { "x64", "Arm64" };
    string[] ServiceAndApiPlatformsAll => new string[] { "x64", "Arm64", "Arm64EC" };   // the order here matters because the dependencies in the solution aren't perfect
    string[] ToolsPlatforms => new string[] { "x64", "Arm64" };
    string[] NativeSamplesPlatforms => new string[] { "x64", "Arm64", "Arm64EC" };
    string[] ManagedSamplesPlatforms => new string[] { "x64", "Arm64" };
    string[] InstallerPlatforms => new string[] { "x64", "Arm64" };



    Dictionary<string, string> BuiltSdkRuntimeInstallers = new Dictionary<string, string>();
    Dictionary<string, string> BuiltInBoxInstallers = new Dictionary<string, string>();
    Dictionary<string, string> BuiltPreviewInBoxInstallers = new Dictionary<string, string>();





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

            _thisReleaseFolder = $"{ReleaseRootFolder / VersionName} ({DateTime.Now.ToString("yyyy-MM-dd HH-mm-ss")})";

        });


    Target CreateVersionIncludes => _ => _
        .DependsOn(Prerequisites)
        .Executes(() =>
        {
            string buildSource = "GitHub Preview";
            string versionName = VersionName;
            string versionString = SetupBundleFullVersionString;

            string buildVersionMajor = BuildVersionMajor;
            string buildVersionMinor = BuildVersionMinor;
            string buildVersionRevision = BuildVersionRevision;

            string buildVersionDateNumber = BuildDateNumber;
            string buildVersionTimeNumber = BuildTimeNumber;

            // create .h file for SDK and related tools

            using (StreamWriter writer = System.IO.File.CreateText(CommonVersionHeaderFile))
            {
                writer.WriteLine("// this file has been auto-generated by the Windows MIDI Services build process");
                writer.WriteLine();
                writer.WriteLine("#ifndef WINDOWS_MIDI_SERVICES_VERSION_INCLUDE");
                writer.WriteLine("#define WINDOWS_MIDI_SERVICES_VERSION_INCLUDE");
                writer.WriteLine();
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_BUILD_SOURCE               L\"{buildSource}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_BUILD_VERSION_NAME         L\"{versionName}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_BUILD_VERSION_FULL         L\"{versionString}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_BUILD_VERSION_MAJOR        L\"{buildVersionMajor}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_BUILD_VERSION_MINOR        L\"{buildVersionMinor}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_BUILD_VERSION_REVISION     L\"{buildVersionRevision}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_BUILD_VERSION_DATE_NUMBER  L\"{buildVersionDateNumber}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_BUILD_VERSION_TIME_NUMBER  L\"{buildVersionTimeNumber}\"");
                writer.WriteLine();
                writer.WriteLine("#endif");
                writer.WriteLine();
            }

            // create .cs file for console and settings app

            using (StreamWriter writer = System.IO.File.CreateText(CommonVersionCSharpFile))
            {
                writer.WriteLine("// this file has been auto-generated by the Windows MIDI Services build process");
                writer.WriteLine();

                writer.WriteLine("namespace Microsoft.Midi.Common");
                writer.WriteLine("{");
                writer.WriteLine("\tpublic static class MidiBuildInformation");
                writer.WriteLine("\t{");
                writer.WriteLine($"\t\tpublic const string Source = \"{buildSource}\";");
                writer.WriteLine($"\t\tpublic const string Name = \"{versionName}\";");
                writer.WriteLine($"\t\tpublic const string BuildFullVersion = \"{versionString}\";");
                writer.WriteLine($"\t\tpublic const string VersionMajor = \"{buildVersionMajor}\";");
                writer.WriteLine($"\t\tpublic const string VersionMinor = \"{buildVersionMinor}\";");
                writer.WriteLine($"\t\tpublic const string VersionRevision = \"{buildVersionRevision}\";");
                writer.WriteLine($"\t\tpublic const string VersionDateNumber = \"{buildVersionDateNumber}\";");
                writer.WriteLine($"\t\tpublic const string VersionTimeNumber = \"{buildVersionTimeNumber}\";");
                writer.WriteLine("\t}");
                writer.WriteLine("}");
                writer.WriteLine();
            }


        });

    Target BuildServiceAndPlugins => _ => _
        .DependsOn(CreateVersionIncludes)
        .DependsOn(Prerequisites)
        .Executes(() =>
    {
        foreach (var platform in ServiceAndApiPlatformsAll)
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

            // This transport gets compiled to Arm64X and x64. The Arm64X output is in the Arm64EC folder
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / Configuration.Release / $"Midi2.MidiSrvTransport.dll");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / platform / Configuration.Release / $"wdmaud2.drv");

            // only in-proc files, like the MidiSrvTransport, are Arm64EC. For all the others
            // any reference to Arm64EC is just Arm64. We don't use any of the Arm64X output
            var servicePlatform = (platform == "Arm64EC" || platform == "Arm64") ? "Arm64" : "x64";

            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / Configuration.Release / $"Midisrv.exe");

            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / Configuration.Release / $"Midi2.DiagnosticsTransport.dll");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / Configuration.Release / $"Midi2.KSTransport.dll");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / Configuration.Release / $"Midi2.KSAggregateTransport.dll");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / Configuration.Release / $"Midi2.VirtualMidiTransport.dll");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / Configuration.Release / $"Midi2.LoopbackMidiTransport.dll");

            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / Configuration.Release / $"Midi2.BS2UMPTransform.dll");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / Configuration.Release / $"Midi2.UMP2BSTransform.dll");
            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / Configuration.Release / $"Midi2.UmpProtocolDownscalerTransform.dll");

            stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / Configuration.Release / $"Midi2.SchedulerTransform.dll");

            foreach (var file in stagingFiles)
            {
                FileSystemTasks.CopyFileToDirectory(file, ApiStagingFolder / servicePlatform, FileExistsPolicy.Overwrite, true);
            }
        }

        // Copy reference files to reference folder. This requires
        // the SDK projects reference those instead of the current version
        // which references deep into the API project

        // copy x64 files to Arm64EC folder as well. No reason for the full
        // service and plugins to be Arm64EC just to provide a few headers

        var intermediateFolder = ApiSolutionFolder / "vsfiles" / "intermediate";

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

            // this is the only one that needs Arm64X
            referenceFiles.Add(intermediateFolder / "Midi2.MidiSrvTransport" / sourcePlatform / Configuration.Release / "Midi2MidiSrvTransport.h");

            // copy the files over to the reference location
            foreach (var file in referenceFiles)
            {
                FileSystemTasks.CopyFileToDirectory(file, ApiReferenceFolder / platform, FileExistsPolicy.Overwrite, true);
            }
        }

    });


    Target BuildInDevelopmentServicePlugins => _ => _
        .DependsOn(CreateVersionIncludes)
        .DependsOn(Prerequisites)
        .DependsOn(BuildServiceAndPlugins)
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


                MSBuildTasks.MSBuild(_ => _
                    .SetTargetPath(ApiSolutionFolder / "midi2-service-component-preview.sln")
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

                // only in-proc files, like the MidiSrvTransport, are Arm64EC
                //var servicePlatform = (platform == "Arm64EC" || platform == "Arm64") ? "Arm64" : "x64";
                var servicePlatform = platform;

                stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / Configuration.Release / $"Midi2.NetworkMidiTransport.dll");
                // stagingFiles.Add(ApiSolutionFolder / "vsfiles" / servicePlatform / Configuration.Release / $"Midi2.VirtualPatchBay.dll");

                foreach (var file in stagingFiles)
                {
                    FileSystemTasks.CopyFileToDirectory(file, ApiStagingFolder / servicePlatform, FileExistsPolicy.Overwrite, true);
                }
            }

        });



    Target BuildAndPackAllAppSDKs => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(CreateVersionIncludes)
        .DependsOn(BuildServiceAndPlugins)
        .DependsOn(BuildInDevelopmentServicePlugins)
        .Executes(() =>
        {
            //// for cswinrt
            //NuGetTasks.NuGetInstall(_ => _
            //    .SetProcessWorkingDirectory(AppSdkSolutionFolder)
            //    .SetSource("https://api.nuget.org/v3/index.json")
            //    .SetPackageID("Microsoft.Windows.CsWinRT")
            //    .SetDependencyVersion(DependencyVersion.Highest)
            //);



            foreach (var platform in SdkPlatforms)
            {
                string solutionDir = AppSdkSolutionFolder.ToString() + @"\";

                var msbuildProperties = new Dictionary<string, object>();
                msbuildProperties.Add("Platform", platform);
                msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash
                msbuildProperties.Add("NoWarn", "MSB3271");             // winmd and dll platform mismatch with Arm64EC

                Console.Out.WriteLine($"----------------------------------------------------------------------");
                Console.Out.WriteLine($"SolutionDir:   {solutionDir}");
                Console.Out.WriteLine($"Platform:      {platform}");

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

            foreach (var sourcePlatform in SdkPlatforms)
            {
                var sdkBinaries = new List<AbsolutePath>();

                sdkBinaries.Add(sdkOutputRootFolder / "Microsoft.Windows.Devices.Midi2" / sourcePlatform / Configuration.Release / $"Microsoft.Windows.Devices.Midi2.winmd");
                sdkBinaries.Add(sdkOutputRootFolder / "Microsoft.Windows.Devices.Midi2" / sourcePlatform / Configuration.Release / $"Microsoft.Windows.Devices.Midi2.dll");
                sdkBinaries.Add(sdkOutputRootFolder / "Microsoft.Windows.Devices.Midi2" / sourcePlatform / Configuration.Release / $"Microsoft.Windows.Devices.Midi2.pri");
                sdkBinaries.Add(sdkOutputRootFolder / "Microsoft.Windows.Devices.Midi2" / sourcePlatform / Configuration.Release / $"Microsoft.Windows.Devices.Midi2.pdb");

                // todo: if other DLLs are required here, add them
                //    sdkBinaries.Add(sdkOutputRootFolder / "WindowsMidiServicesClientInitialization" / sourcePlatform / Configuration.Release / $"WindowsMidiServicesClientInitialization.dll");


                // create the nuget package
                // todo: it would be better to see if any of the generated winmds have changed and only
                // do this step if those have changed. Maybe do a before/after date/time check?

                Console.Out.WriteLine($"NuGet Version: {NugetVersionString}");

                NuGetTasks.NuGetPack(_ => _
                    .SetTargetPath(AppSdkSolutionFolder / "projections" / "dotnet-and-cpp" / "nuget" / "Microsoft.Windows.Devices.Midi2.nuspec")
                    .AddProperty("version", NugetVersionString)
                    .AddProperty("id", NugetPackageId)
                    .SetOutputDirectory(AppSdkNugetOutputFolder)
                );


                string stagingPlatform = sourcePlatform;
                if (sourcePlatform.ToLower() == "arm64ec")
                {
                    stagingPlatform = "Arm64";
                }

                // copy the files over to the reference location
                foreach (var file in sdkBinaries)
                {
                    FileSystemTasks.CopyFileToDirectory(file, AppSdkStagingFolder / stagingPlatform, FileExistsPolicy.Overwrite, true);
                }

            }

            // copy the NuGet package over to this release

            FileSystemTasks.CopyFileToDirectory(
                AppSdkNugetOutputFolder /  $"{NugetFullPackageIdWithVersion}.nupkg", 
                ThisReleaseFolder, 
                FileExistsPolicy.Overwrite, 
                true);


            foreach (var sourcePlatform in SdkPlatforms)
            {
                // the solution compiles these apps to Arm64 when target is EC.
                string stagingPlatform = sourcePlatform;
                if (sourcePlatform.ToLower() == "arm64ec")
                {
                    stagingPlatform = "Arm64";
                }

                // sample manifest
                FileSystemTasks.CopyFileToDirectory(AppSdkSolutionFolder / "ExampleMidiApp.exe.manifest", AppSdkStagingFolder / stagingPlatform, FileExistsPolicy.Overwrite, true);

                // bootstrap files
                FileSystemTasks.CopyFileToDirectory(AppSdkSolutionFolder / "client-initialization-redist" / "Microsoft.Windows.Devices.Midi2.Initialization.hpp", AppSdkStagingFolder / stagingPlatform, FileExistsPolicy.Overwrite, true);
                //FileSystemTasks.CopyFileToDirectory(AppSdkSolutionFolder / "client-initialization-redist" / "MidiDesktopAppSdkBootstrapper.cs", AppSdkStagingFolder / stagingPlatform, FileExistsPolicy.Overwrite, true);
            }
        });


    Target BuildAppSDKToolsAndTests => _ => _
        .DependsOn(BuildAndPackAllAppSDKs)
        .Executes(() =>
        {

            foreach (var platform in SdkPlatforms)
            {
                string solutionDir = AppSdkSolutionFolder.ToString() + @"\";

                var msbuildProperties = new Dictionary<string, object>();
                msbuildProperties.Add("Platform", platform);
                msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash
                msbuildProperties.Add("NoWarn", "MSB3271");             // winmd and dll platform mismatch with Arm64EC

                Console.Out.WriteLine($"----------------------------------------------------------------------");
                Console.Out.WriteLine($"SolutionDir:   {solutionDir}");
                Console.Out.WriteLine($"Platform:      {platform}");

                string[] toolsDirectories =
                    {
                        Path.Combine(solutionDir, "mididiag"),
                        Path.Combine(solutionDir, "midiusbinfo"),
                        Path.Combine(solutionDir, "midimdnsinfo"),
                    };

                foreach (var projectFolder in toolsDirectories)
                {
                    // only send paths that have a packages config in them

                    var configFilePath = Path.Combine(projectFolder, "packages.config");

                    if (File.Exists(configFilePath))
                    {
                        Console.WriteLine($"Updating {configFilePath}");
                        UpdatePackagesConfigForCPPProject(configFilePath);

                        var vcxprojFilePaths = Directory.GetFiles(projectFolder, "*.vcxproj");

                        foreach (var path in vcxprojFilePaths)
                        {
                            Console.WriteLine($"Updating {path}");
                            UpdateWindowsMidiServicesSdkPackagePropertyForCppProject(path);

                            // nuget restore
                            RestoreNuGetPackagesForCPPProject(path, AppSdkSolutionFolder, configFilePath);

                        }
                    }
                    else
                    {
                        Console.WriteLine($"Not a project folder {projectFolder}");
                    }

                }


                MSBuildTasks.MSBuild(_ => _
                    .SetTargetPath(AppSdkSolutionFolder / "app-sdk-tools-and-tests.sln")
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

            foreach (var sourcePlatform in SdkPlatforms)
            {
                // the solution compiles these apps to Arm64 when target is EC.
                string stagingPlatform = sourcePlatform;
                if (sourcePlatform.ToLower() == "arm64ec")
                {
                    stagingPlatform = "Arm64";
                }

                // MIDI utilities
                FileSystemTasks.CopyFileToDirectory(sdkOutputRootFolder / "mididiag" / stagingPlatform / Configuration.Release / $"mididiag.exe", AppSdkStagingFolder / stagingPlatform, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(sdkOutputRootFolder / "midiksinfo" / stagingPlatform / Configuration.Release / $"midiksinfo.exe", AppSdkStagingFolder / stagingPlatform, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(sdkOutputRootFolder / "midimdnsinfo" / stagingPlatform / Configuration.Release / $"midimdnsinfo.exe", AppSdkStagingFolder / stagingPlatform, FileExistsPolicy.Overwrite, true);
            }
        });


    Target BuildAppSdkRuntimeAndToolsInstaller => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(CreateVersionIncludes)
        .DependsOn(BuildConsoleApp)
        //.DependsOn(BuildSettingsApp)
        .DependsOn(BuildAndPackAllAppSDKs)
        .DependsOn(BuildAppSDKToolsAndTests)
        .Executes(() =>
        {
            // we build for Arm64 and x64. No EC required here
            foreach (var platform in InstallerPlatforms)
            {
                UpdateSetupBundleInfoIncludeFile(platform);

                //string fullSetupVersionString = $"{SetupVersionName} {SetupBuildMajorMinor}.{SetupBuildDateNumber}.{SetupBuildTimeNumber}";               

                string solutionDir = AppSdkSolutionFolder.ToString() + @"\";

                var msbuildProperties = new Dictionary<string, object>();
                msbuildProperties.Add("Platform", platform);
                msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash

                Console.Out.WriteLine($"----------------------------------------------------------------------");
                Console.Out.WriteLine($"SolutionDir: {solutionDir}");
                Console.Out.WriteLine($"Platform:    {platform}");

                //var setupSolutionFolder = AppSdkSolutionFolder / "sdk-runtime-installer";


                MSBuildTasks.MSBuild(_ => _
                    .SetTargetPath(AppSdkSetupSolutionFolder / "midi-services-app-sdk-runtime-setup.sln")
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

                string newInstallerName = $"Windows MIDI Services (SDK Runtime and Tools) {SetupBundleFullVersionString}-{platform.ToLower()}.exe";
                FileSystemTasks.CopyFile(
                    AppSdkSetupSolutionFolder / "main-bundle" / "bin" / platform / Configuration.Release / "WindowsMidiServicesSdkRuntimeSetup.exe",
                    ThisReleaseFolder / newInstallerName);

                BuiltSdkRuntimeInstallers[platform.ToLower()] = newInstallerName;
            }

        });

    void UpdateSetupBundleInfoIncludeFile(string platform)
    {
        //string versionString = $"{SetupBuildMajorMinor}.{SetupBuildDateNumber}.{SetupBuildTimeNumber}";

        using (StreamWriter writer = System.IO.File.CreateText(SetupBundleInfoIncludeFile))
        {
            writer.WriteLine("<Include>");
            writer.WriteLine($"  <?define SetupVersionName=\"{VersionName} {platform}\" ?>");
            writer.WriteLine($"  <?define SetupVersionNumber=\"{SetupBundleFullVersionString}\" ?>");
            writer.WriteLine("</Include>");
        }
    }

    Target BuildServiceAndPluginsInstaller => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(BuildServiceAndPlugins)
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
                string newInstallerName = $"Windows MIDI Services (In-Box Service) {SetupBundleFullVersionString}-{platform.ToLower()}.exe";

                FileSystemTasks.CopyFile(
                    InBoxComponentsSetupSolutionFolder / "main-bundle" / "bin" / platform / Configuration.Release / "WindowsMidiServicesInBoxComponentsSetup.exe",
                    ThisReleaseFolder / newInstallerName);

                BuiltInBoxInstallers[platform.ToLower()] = newInstallerName;
            }
        });

    Target BuildInDevelopmentServicePluginsInstaller => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(BuildInDevelopmentServicePlugins)
        .Executes(() =>
    {
        // we build for Arm64 and x64. No EC required here
        foreach (var platform in InstallerPlatforms)
        {
            UpdateSetupBundleInfoIncludeFile(platform);

            //string fullSetupVersionString = $"{SetupVersionName} {SetupBuildMajorMinor}.{SetupBuildDateNumber}.{SetupBuildTimeNumber}";

            string solutionDir = InDevelopmentServiceComponentsSetupSolutionFolder.ToString() + @"\";

            var msbuildProperties = new Dictionary<string, object>();
            msbuildProperties.Add("Platform", platform);
            msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash

            Console.Out.WriteLine($"----------------------------------------------------------------------");
            Console.Out.WriteLine($"SolutionDir: {solutionDir}");
            Console.Out.WriteLine($"Platform:    {platform}");

            var output = MSBuildTasks.MSBuild(_ => _
                .SetTargetPath(InDevelopmentServiceComponentsSetupSolutionFolder / "midi-services-in-box-preview-setup.sln")
                .SetMaxCpuCount(14)
                /*.SetOutDir(outputFolder) */
                /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                /*.SetTargets("Build") */
                .SetProperties(msbuildProperties)
                .SetConfiguration(Configuration.Release)
                .EnableNodeReuse()
            );

            string newInstallerName = $"Windows MIDI Services (Preview Service Plugins) {SetupBundleFullVersionString}-{platform.ToLower()}.exe";

            FileSystemTasks.CopyFile(
                InDevelopmentServiceComponentsSetupSolutionFolder / "main-bundle" / "bin" / platform / Configuration.Release / "WindowsMidiServicesInDevelopmentServiceComponentsSetup.exe",
                ThisReleaseFolder / newInstallerName);

            BuiltPreviewInBoxInstallers[platform.ToLower()] = newInstallerName;

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
            var solution = MidiSettingsSolutionFolder / "midi-settings.sln";

            // for the MIDI nuget package
            NuGetTasks.NuGetInstall(_ => _
                .SetProcessWorkingDirectory(MidiSettingsSolutionFolder)
                .SetPreRelease(true)
                .SetSource(AppSdkNugetOutputFolder)
                .SetPackageID(NugetPackageId)
                .SetDependencyVersion(DependencyVersion.Highest)
            );

            NuGetTasks.NuGetRestore(_ => _
                .SetProcessWorkingDirectory(MidiSettingsSolutionFolder)
                .SetSolutionDirectory(MidiSettingsSolutionFolder)
                .SetSource(AppSdkNugetOutputFolder)
            );

            bool wxsWritten = false;

            // build x64 and Arm64, no Arm64EC
            foreach (var platform in ToolsPlatforms)
            {
                string solutionDir = MidiSettingsSolutionFolder.ToString() + @"\";

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
                    .SetProjectFile(MidiSettingsSolutionFolder / "Microsoft.Midi.Settings" / "Microsoft.Midi.Settings.csproj")
                    .SetConfiguration(Configuration.Release)
                    .SetPublishSingleFile(false)
                    .SetPublishTrimmed(false)
                    .SetSelfContained(false)
                    .SetRuntime(rid)
                    .AddNoWarns(8618) // ignore CS8618 which I have no control over because it's in projection assemblies 
                );

                // This just doesn't work. Even in Visual Studio, publishing the WinAppSdk app just fails for "unknown" reasons.
                //DotNetTasks.DotNetPublish(_ => _
                //    .SetProjectFile(MidiSettingsSolutionFolder / "Microsoft.Midi.Settings.csproj" / "Microsoft.Midi.Settings.csproj")
                //    .SetConfiguration(Configuration.Release)
                //    .SetPublishSingleFile(false)
                //    .SetPublishTrimmed(false)
                //    .SetSelfContained(false)
                //    .SetRuntime(rid)
                //);
                // folder is bin\rid\publish\

                var settingsOutputFolder = MidiSettingsSolutionFolder / "Microsoft.Midi.Settings" / "bin" / Configuration.Release / "net8.0-windows10.0.22621.0" / rid;
                var stagingFolder = MidiSettingsStagingFolder / platform;

                stagingFolder.CreateOrCleanDirectory();


                // get all the community toolkit files
                var toolkitFiles = Globbing.GlobFiles(settingsOutputFolder, "CommunityToolkit*.dll");
                var msftExtensionsFiles = Globbing.GlobFiles(settingsOutputFolder, "Microsoft.Extensions*.dll");
                var midiSdkFiles = Globbing.GlobFiles(
                    settingsOutputFolder, 
                    "Microsoft.Windows.Devices.Midi2.Initialization.dll",
                    /* "Microsoft.Windows.Devices.Midi2.Initialization.winmd", */
                    "Microsoft.Windows.Devices.Midi2.Initialization.pri"
                    );

                List<AbsolutePath> paths = new List<AbsolutePath>(toolkitFiles.Count + msftExtensionsFiles.Count + midiSdkFiles.Count + 40);
                paths.AddRange(toolkitFiles);
                paths.AddRange(msftExtensionsFiles);
                paths.AddRange(midiSdkFiles);


                // copy output to staging folder

                paths.Add(settingsOutputFolder / "MidiSettings.exe");
                paths.Add(settingsOutputFolder / "MidiSettings.dll");
                //paths.Add(settingsOutputFolder / "MidiSettings.exe.manifest");
                paths.Add(settingsOutputFolder / "MidiSettings.deps.json");
                paths.Add(settingsOutputFolder / "MidiSettings.runtimeconfig.json");

                paths.Add(settingsOutputFolder / "resources.pri");

                paths.Add(settingsOutputFolder / "Microsoft.Midi.Settings.Core.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Devices.Midi2.Tools.Shared.dll");

                paths.Add(settingsOutputFolder / "Microsoft.Windows.SDK.NET.dll");

                //paths.Add(settingsOutputFolder / "ColorCode.Core.dll");
                //paths.Add(settingsOutputFolder / "ColorCode.WinUI.dll");


                paths.Add(settingsOutputFolder / "Microsoft.InteractiveExperiences.Projection.dll");

                paths.Add(settingsOutputFolder / "Newtonsoft.Json.dll");

                //paths.Add(settingsOutputFolder / "System.CodeDom.dll");
                paths.Add(settingsOutputFolder / "System.Diagnostics.EventLog.dll");
                paths.Add(settingsOutputFolder / "System.Diagnostics.EventLog.Messages.dll");
                paths.Add(settingsOutputFolder / "System.Management.dll");
                paths.Add(settingsOutputFolder / "System.ServiceProcess.ServiceController.dll");

                paths.Add(settingsOutputFolder / "Microsoft.Web.WebView2.Core.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Web.WebView2.Core.Projection.dll");
                paths.Add(settingsOutputFolder / "WebView2Loader.dll");

                paths.Add(settingsOutputFolder / "WinRT.Runtime.dll");

                paths.Add(settingsOutputFolder / "Microsoft.WindowsAppRuntime.Bootstrap.dll");
                paths.Add(settingsOutputFolder / "Microsoft.WindowsAppRuntime.Bootstrap.Net.dll");

                paths.Add(settingsOutputFolder / "Microsoft.WinUI.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Xaml.Interactions.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Xaml.Interactivity.dll");

                paths.Add(settingsOutputFolder / "WinUIEx.dll");


                paths.Add(settingsOutputFolder / "Microsoft.Windows.ApplicationModel.DynamicDependency.Projection.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Windows.ApplicationModel.Resources.Projection.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Windows.ApplicationModel.WindowsAppRuntime.Projection.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Windows.AppLifecycle.Projection.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Windows.AppNotifications.Builder.Projection.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Windows.AppNotifications.Projection.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Windows.Management.Deployment.Projection.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Windows.PushNotifications.Projection.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Windows.Security.AccessControl.Projection.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Windows.Storage.Projection.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Windows.System.Power.Projection.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Windows.System.Projection.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Windows.Widgets.Projection.dll");


                // Add Assets folder with app icon. This ends up special-cased

                paths.Add(settingsOutputFolder / "Assets" / "AppIcon.ico");
                paths.Add(settingsOutputFolder / "Assets" / "AppIcon.png");


                // TODO: This doesn't deal with any localization content


                // copy all the globbed files

                foreach (var f in paths)
                {
                    FileSystemTasks.CopyFileToDirectory(f, stagingFolder, FileExistsPolicy.Overwrite);
                }

                // also write lines to the setup include file

                if (!wxsWritten)
                {
                    AbsolutePath SettingsAppFileListIncludeFile = AppSdkSetupSolutionFolder / "settings-package" / "_SetupFiles.wxs";

                    using (StreamWriter writer = System.IO.File.CreateText(SettingsAppFileListIncludeFile))
                    {
                        writer.WriteLine("<!-- This file was generated by a tool, and will be overwritten -->");
                        writer.WriteLine();
                        writer.WriteLine("<Wix xmlns=\"http://wixtoolset.org/schemas/v4/wxs\">");
                        writer.WriteLine();
                        writer.WriteLine("  <?define StagingSourceRootFolder=$(env.MIDI_REPO_ROOT)build\\staging ?>");
                        writer.WriteLine();
                        writer.WriteLine("  <Fragment>");
                        writer.WriteLine("    <Component Id=\"SettingsAppExe\" Bitness=\"always64\" Directory=\"SETTINGSAPP_INSTALLFOLDER\" Guid =\"de6c83ee-2d1d-4b21-a098-e4a4079b6872\">");

                        foreach (var f in paths)
                        {
                            if (f.ToString().ToLower().Contains("assets"))
                            {
                                // we don't create entries for assets files. They are also special-cased in the setup
                            }
                            else if (f.ToString().ToLower().EndsWith("midisettings.exe"))
                            {
                                // settings app exe so we can use to create icon
                                writer.WriteLine($"      <File Id=\"ShortcutTargetExe\" Source=\"$(StagingSourceRootFolder)\\midi-settings\\$(var.Platform)\\{f.Name}\" /> ");
                            }
                            else
                            {
                                // normal file
                                writer.WriteLine($"      <File Source=\"$(StagingSourceRootFolder)\\midi-settings\\$(var.Platform)\\{f.Name}\" /> ");
                            }
                        }

                        writer.WriteLine("    </Component>");
                        writer.WriteLine("  </Fragment>");
                        writer.WriteLine("</Wix>");
                    }

                    wxsWritten = true;
                }
            }

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
                .SetPackageID(NugetPackageId)
                .SetDependencyVersion(DependencyVersion.Highest)
            );

            NuGetTasks.NuGetRestore(_ => _
                .SetProcessWorkingDirectory(MidiConsoleSolutionFolder)
                .SetSolutionDirectory(MidiConsoleSolutionFolder)
                .SetSource(AppSdkNugetOutputFolder)
            );

            // build x64 and Arm64, no Arm64EC
            foreach (var platform in ToolsPlatforms)
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
                //FileSystemTasks.CopyFileToDirectory(consoleOutputFolder / "midi.exe.manifest", stagingFolder, FileExistsPolicy.Overwrite, true);

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


            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Initialization.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            //    FileSystemTasks.CopyFileToDirectory(runtimesFolder / "Microsoft.Windows.Devices.Midi2.Initialization.pri", stagingFolder, FileExistsPolicy.Overwrite, true);
                //FileSystemTasks.CopyFileToDirectory(runtimesFolder / ns + ".winmd", stagingFolder, FileExistsPolicy.Overwrite, true);

            }

        });


    void RestoreNuGetPackagesForCPPProject(string vcxprojFilePath, string solutionDir, string packagesConfigFullPath)
    {
        var projectDir = Path.GetDirectoryName(vcxprojFilePath);

        NuGetTasks.NuGetInstall(_ => _
            .SetProcessWorkingDirectory(projectDir)
            .SetPreRelease(true)
            .SetSource(AppSdkNugetOutputFolder)
            .SetPackageID(NugetPackageId)
            .SetDependencyVersion(DependencyVersion.Highest)
        );

        NuGetTasks.NuGetRestore(_ => _
            .SetProcessWorkingDirectory(projectDir)
            .SetSource(AppSdkNugetOutputFolder)
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

                element.SetAttribute("version", NugetVersionString);

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


    Target BuildCppSamples => _ => _
        .DependsOn(BuildAndPackAllAppSDKs)
        .Executes(() =>
        {
            var solution = SamplesCppWinRTSolutionFolder / "cpp-winrt-samples.sln";

            // update nuget packages for each project 
            foreach (var projectFolder in Directory.GetDirectories(SamplesCppWinRTSolutionFolder))
            {
                // only send paths that have a packages config in them

                var configFilePath = Path.Combine(projectFolder, "packages.config");

                if (File.Exists(configFilePath))
                {
                    Console.WriteLine($"Updating {configFilePath}");
                    UpdatePackagesConfigForCPPProject(configFilePath);

                    var vcxprojFilePaths = Directory.GetFiles(projectFolder, "*.vcxproj");

                    foreach (var path in vcxprojFilePaths)
                    {
                        Console.WriteLine($"Updating {path}");
                        UpdateWindowsMidiServicesSdkPackagePropertyForCppProject(path);

                        // nuget restore
                        RestoreNuGetPackagesForCPPProject(path, SamplesCppWinRTSolutionFolder, configFilePath);

                    }
                }
                else
                {
                    Console.WriteLine($"Not a project folder {projectFolder}");
                }

            }



            // for cppwinrt
            NuGetTasks.NuGetInstall(_ => _
                .SetProcessWorkingDirectory(SamplesCppWinRTSolutionFolder)
                .SetSource("https://api.nuget.org/v3/index.json")
                .SetPackageID("Microsoft.Windows.CppWinRT")
                .SetDependencyVersion(DependencyVersion.Highest)
            );

            // for WIL
            //NuGetTasks.NuGetInstall(_ => _
            //    .SetProcessWorkingDirectory(SamplesCppWinRTSolutionFolder)
            //    .SetPreRelease(true)
            //    .SetSource("https://api.nuget.org/v3/index.json")
            //    .SetPackageID("Microsoft.Windows.ImplementationLibrary")
            //);


            // for the MIDI nuget package
            // the install and restore should work with the above 
            // manual managing of the packages.config
            //NuGetTasks.NuGetInstall(_ => _
            //    .SetProcessWorkingDirectory(SamplesCppWinRTSolutionFolder)
            //    .SetPreRelease(true)
            //    .SetSource(AppSdkNugetOutputFolder)
            //    .SetPackageID(NugetFullPackageId)
            //    .SetDependencyVersion(DependencyVersion.Highest)
            //);

            //NuGetTasks.NuGetRestore(_ => _
            //    .SetProcessWorkingDirectory(SamplesCppWinRTSolutionFolder)
            //    .SetSolutionDirectory(SamplesCppWinRTSolutionFolder)
            //    .SetSource(AppSdkNugetOutputFolder)
            //);


            // make sure they compile
            foreach (var platform in NativeSamplesPlatforms)
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


    Target BuildCSharpSamples => _ => _
        .DependsOn(BuildAndPackAllAppSDKs)
        .Executes(() =>
    {
        var solution = SamplesCSWinRTSolutionFolder / "csharp-net-samples.sln";


        // for cswinrt
        NuGetTasks.NuGetInstall(_ => _
            .SetProcessWorkingDirectory(SamplesCSWinRTSolutionFolder)
            .SetSource("https://api.nuget.org/v3/index.json")
            .SetPackageID("Microsoft.Windows.CsWinRT")
            .SetDependencyVersion(DependencyVersion.Highest)
        );


        // for the MIDI nuget package
        // the install and restore should work with the above 
        // manual managing of the packages.config
        NuGetTasks.NuGetInstall(_ => _
            .SetProcessWorkingDirectory(SamplesCSWinRTSolutionFolder)
            .SetPreRelease(true)
            .SetSource(AppSdkNugetOutputFolder)
            .SetPackageID(NugetPackageId)
            .SetDependencyVersion(DependencyVersion.Highest)
        );

        NuGetTasks.NuGetRestore(_ => _
            .SetProcessWorkingDirectory(SamplesCSWinRTSolutionFolder)
            .SetSolutionDirectory(SamplesCSWinRTSolutionFolder)
            .SetSource(AppSdkNugetOutputFolder)
        );


        // make sure they compile
        foreach (var platform in ManagedSamplesPlatforms)
        {
            string rid = platform.ToLower() == "arm64" ? "win-arm64" : "win-x64";

            //DotNetTasks.DotNetBuild(_ => _
            //    .SetProjectFile(SamplesCSWinRTSolutionFolder / "Midi" / "Midi.csproj")
            //    .SetConfiguration(Configuration.Release)
            //    .SetPublishSingleFile(false)
            //    .SetPublishTrimmed(false)
            //    .SetSelfContained(false)
            //    .SetRuntime(rid)
            //);


            var msbuildProperties = new Dictionary<string, object>();
            msbuildProperties.Add("Platform", platform);
            msbuildProperties.Add("SolutionDir", SamplesCSWinRTSolutionFolder);      // to include trailing slash
                                                                    //msbuildProperties.Add("NoWarn", "MSB3271");             // winmd and dll platform mismatch with Arm64EC

            Console.Out.WriteLine($"----------------------------------------------------------------------");
            Console.Out.WriteLine($"Solution:    {solution}");
            Console.Out.WriteLine($"SolutionDir: {SamplesCSWinRTSolutionFolder}");
            Console.Out.WriteLine($"Platform:    {platform}");


            MSBuildTasks.MSBuild(_ => _
                .SetTargetPath(solution)
                .SetMaxCpuCount(14)
                /*.SetOutDir(outputFolder) */
                .SetProcessWorkingDirectory(SamplesCSWinRTSolutionFolder)
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
      //  var projectionOutputRoot = ElectronProjectionRootFolder / "projection";
      //  projectionOutputRoot.CreateOrCleanDirectory();

      ////  var projectionReleaseRoot = ElectronProjectionRootFolder / "projection_release";
      ////  projectionReleaseRoot.CreateOrCleanDirectory();


      //  foreach (var platform in OutOfProcPlatforms)
      //  {
      //      var platformOutputRootFolder = projectionOutputRoot / platform;
      //      platformOutputRootFolder.CreateDirectory();


      //      // copy the winmd and impl libs from staging

      //      var sdkFiles = (AppSdkStagingFolder / platform).GlobFiles("*.dll", "*.winmd", "*.pri");

      //      foreach (var sdkFile in sdkFiles)
      //      {
      //          FileSystemTasks.CopyFileToDirectory(
      //              sdkFile,
      //              platformOutputRootFolder,
      //              FileExistsPolicy.Overwrite,
      //              true);
      //      }


      //      // main windows metadata file
      //      //FileSystemTasks.CopyFileToDirectory(
      //      //    @"C:\Program Files (x86)\Windows Kits\10\UnionMetadata\10.0.20348.0\Windows.winmd",
      //      //    platformOutputRootFolder,
      //      //    FileExistsPolicy.Overwrite, true);

      //      foreach (var ns in AppSdkAssemblies)
      //      {
      //          var nsRootFolder = platformOutputRootFolder / ns.ToLower();
      //          nsRootFolder.CreateDirectory();

      //          var namespaceBuildFolder = nsRootFolder / "build";
      //          namespaceBuildFolder.CreateOrCleanDirectory();

      //          // the winmd files are in the parent folder
      //          //NodeRT($"--winmd {AppSdkStagingFolder / platform / ns + ".winmd"} --outdir {platformOutputRootFolder} --verbose");
      //          NodeRT($"--winmd {AppSdkStagingFolder / platform / ns + ".winmd"} --outdir {platformOutputRootFolder}");

      //          // todo: need to capture return code
      //      }

      //      // the NodeRt tool creates folders for each namespace, not for 
      //      // each winmd, so we then need to loop through and compile the
      //      // dll for each namespace.

      //      var platformNamespaceFolders = Globbing.GlobDirectories(platformOutputRootFolder, "microsoft.windows.devices.midi2*");
      //      //var namespaceFolders = Globbing.GlobDirectories(platformOutputRootFolder);

      //      foreach (var platformNamespaceFolder in platformNamespaceFolders)
      //      {
      //          // build the projection
      //          Console.Out.WriteLine("--------------");
      //          Console.Out.WriteLine($"Rebuilding: {platformNamespaceFolder}");

      //          //NodeGyp(
      //          //    arguments: $"rebuild --msvs_version=2022",
      //          //    workingDirectory: platformNamespaceFolder,
      //          //    logOutput: false
      //          //    );

      //          // node-gyp rebuild --target=32.0.0 --arch=x64 --dist-url=https://electronjs.org/headers

      //          //NpmTasks.Npm(
      //          //    "install electron --save-dev",
      //          //    platformNamespaceFolder);


      //          //NodeGyp(
      //          //    arguments: $"configure" +
      //          //        $" --node_use_v8_platform=false " +
      //          //        $" --node_use_bundled_v8=false" +
      //          //        $" --msvs_version=2022" +
      //          //        $" --target=32.0.0" +
      //          //        $" --dist-url=https://electronjs.org/headers" +
      //          //        
      //          //    workingDirectory: platformNamespaceFolder,
      //          //    logOutput: false
      //          //    );

      //          NodeGyp(
      //              arguments: $"rebuild" +
      //                  $" --openssl_fips=X" +
      //                  $" --arch={platform.ToLower()}" +
      //                 /* $" --verbose" + */
      //                  $" --msvs_version=2022",
      //              workingDirectory: platformNamespaceFolder,
      //              logOutput: true
      //              );




      //          // todo: need to capture return code

      //          //NpmTasks.Npm(
      //          //    "install --save-dev @electron/rebuild",
      //          //    nsFolder);

      //          // Next step is to execute the electron-rebuild.cmd
      //          // .\node_modules\.bin\electron-rebuild.cmd
      //          // but that path is different for each one, and nuke
      //          // doesn't seem to support that with their Tools.


      //          // now copy the output files

      //          //var nsReleaseRootFolder = projectionReleaseRoot / platform / nsFolder.Name.ToLower();
      //          //nsReleaseRootFolder.CreateDirectory();

      //          //binding.node is the binary

      //          // Because of electron versioning vs node versioning, it's easier for the
      //          // consumer if we just ship all the source, as crazy as that is for this.

      //          //FileSystemTasks.CopyDirectoryRecursively(
      //          //    platformNamespaceFolder,
      //          //    projectionReleaseRoot / platform,
      //          //    DirectoryExistsPolicy.Merge
      //          //    );


      //          //var sourceBinFolder = platformNamespaceFolder / "build" / "Release";

      //          //FileSystemTasks.CopyFileToDirectory(
      //          //    sourceBinFolder / "binding.node",
      //          //    nsReleaseRootFolder / "build" / "Release",
      //          //    FileExistsPolicy.Overwrite, true);

      //          //// we also want the three files in the lib folder

      //          //var sourceLibFolder = nsFolder / "lib";

      //          //var libFiles = sourceLibFolder.GlobFiles("*.js", "*.ts");

      //          //foreach (var libFile in libFiles)
      //          //{
      //          //    //Console.WriteLine($"Copying Projection File: {libFile}");

      //          //    FileSystemTasks.CopyFileToDirectory(
      //          //        libFile,
      //          //        nsReleaseRootFolder / "lib",
      //          //        FileExistsPolicy.Overwrite, true);
      //          //}

      //          //FileSystemTasks.CopyFileToDirectory(
      //          //    nsFolder / "package.json",
      //          //    nsReleaseRootFolder,
      //          //    FileExistsPolicy.Overwrite, true);


      //      }



      //      // copy the manifest over and name it the default for electron apps
      //      FileSystemTasks.CopyFile(
      //          AppSdkStagingFolder / platform / "ExampleMidiApp.exe.manifest",
      //          platformOutputRootFolder / "electron.exe.manifest" ,
      //          FileExistsPolicy.Overwrite, 
      //          true);

      //      // Remove windows.winmd



      //      // now, we zip it all up and put it over into the release folder

      //      platformOutputRootFolder.ZipTo(ThisReleaseFolder / $"electron-node-projection-{SetupVersionName} {SetupBuildMajorMinor}.{SetupBuildDateNumber}.{SetupBuildTimeNumber}-{platform.ToLower()}.zip");


      //  }

    });



    Target ZipPowershellDevUtilities => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(CreateVersionIncludes)
        .Executes(() =>
        {
            var regHelpersFolder = RootDirectory / "src" / "dev-tools" / "reg-helpers";

            regHelpersFolder.ZipTo(ThisReleaseFolder / $"dev-pre-setup-scripts.zip");
        });

    Target ZipWdmaud2 => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(BuildServiceAndPlugins)
        .Executes(() =>
        {
            var zipRoot = (StagingRootFolder / "wdmaud2").CreateOrCleanDirectory();

            var arm64 = (zipRoot / "arm64").CreateOrCleanDirectory();
            var x64 = (zipRoot / "x64").CreateOrCleanDirectory();

            string driverFile = "wdmaud2.drv";

            CopyFile(ApiStagingFolder / "arm64" / driverFile, arm64 / driverFile, FileExistsPolicy.Fail, false);
            CopyFile(ApiStagingFolder / "x64" / driverFile, x64 / driverFile, FileExistsPolicy.Fail, false);

            // todo: add takeown / copy scripts

            zipRoot.ZipTo(ThisReleaseFolder / $"wdmaud2-winmm-shim-driver.zip");

        });


    Target BuildAndPublishAll => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(CreateVersionIncludes)
        .DependsOn(BuildServiceAndPlugins)
        .DependsOn(ZipWdmaud2)
        .DependsOn(BuildServiceAndPluginsInstaller)
        .DependsOn(BuildInDevelopmentServicePlugins)
        .DependsOn(BuildInDevelopmentServicePluginsInstaller)
        .DependsOn(BuildAndPackAllAppSDKs)
        .DependsOn(BuildConsoleApp)
        .DependsOn(BuildSettingsApp)
        .DependsOn(BuildAppSdkRuntimeAndToolsInstaller)
      //  .DependsOn(BuildAndPackageElectronProjection)
        .DependsOn(BuildCppSamples)
        .DependsOn(BuildCSharpSamples)
        .DependsOn(ZipPowershellDevUtilities)
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

            if (BuiltPreviewInBoxInstallers.Count > 0)
            {
                Console.WriteLine("\nBuilt in-box preview installers:");

                foreach (var item in BuiltPreviewInBoxInstallers)
                {
                    Console.WriteLine($"  {item.Key.PadRight(5)} {item.Value}");
                }
            }
            else
            {
                Console.WriteLine("No in-box preview installers built.");
            }

            if (BuiltSdkRuntimeInstallers.Count > 0)
            {
                Console.WriteLine("\nBuilt SDK runtime installers:");

                foreach (var item in BuiltSdkRuntimeInstallers)
                {
                    Console.WriteLine($"  {item.Key.PadRight(5)} {item.Value}");
                }
            }
            else
            {
                Console.WriteLine("No SDK runtime installers built.");
            }

        });


}
