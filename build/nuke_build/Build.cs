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
using System;
using System.Collections.Generic;
using System.Configuration;
using System.IO;
using System.Linq;
using System.Xml;
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

    enum MidiBuildType
    {
        Preview,
        Stable
    }

    MSBuildVerbosity BuildVerbosity => MSBuildVerbosity.Quiet;
    LogLevel LoggingLevel => LogLevel.Error;

    // --------------------------------------------------------------------------------------
    // Version information to change 
    // --------------------------------------------------------------------------------------

    MidiBuildType BuildType => MidiBuildType.Preview;        // Stable or Preview
    
    const UInt16 BuildVersionMajor = 1;
    const UInt16 BuildVersionMinor = 0;
    const UInt16 BuildVersionPatch = 12;

    const UInt16 BuildVersionPreviewNumber = 12;
    string VersionName => "Preview " + BuildVersionPreviewNumber;

    // --------------------------------------------------------------------------------------

    UInt16 BuildVersionBuildNumber = 0; // gets read from the version file and reset with each patch change

    readonly string BuildMajorMinorPatch = BuildVersionMajor.ToString() + "." + BuildVersionMinor.ToString() + "." + BuildVersionPatch.ToString();

    string BuildVersionPreviewString;
    string BuildVersionFullString = "";
    string BuildVersionAssemblyFullString = "";
    string BuildVersionFileFullString = "";

    string NugetPackageId => "Microsoft.Windows.Devices.Midi2";
    string NugetPackageVersion;
    string NugetFullPackageIdWithVersion = "";

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


    AbsolutePath AppSdkImplementationInstallerReleaseFolder => BuildRootFolder / "app-sdk-impl";

    AbsolutePath SourceRootFolder => NukeBuild.RootDirectory / "src";
    AbsolutePath AppSdkSolutionFolder => SourceRootFolder / "app-sdk";
    AbsolutePath ApiSolutionFolder => SourceRootFolder / "api";

    AbsolutePath AppSdkSetupSolutionFolder => AppSdkSolutionFolder / "sdk-runtime-installer";

    //AbsolutePath AppSdkSetupRuntimeFolder => AppSdkSetupSolutionFolder / "sdk-package";
    //AbsolutePath AppSdkSetupSettingsFolder => AppSdkSetupSolutionFolder / "settings-package";
    //AbsolutePath AppSdkSetupConsoleFolder => AppSdkSetupSolutionFolder / "console-package";
    //AbsolutePath AppSdkSetupPowerShellFolder => AppSdkSetupSolutionFolder / "powershell-package";


    AbsolutePath InBoxComponentsSetupSolutionFolder => SourceRootFolder / "oob-setup";

    AbsolutePath InDevelopmentServiceComponentsSetupSolutionFolder => SourceRootFolder / "oob-setup-in-dev";

    AbsolutePath ApiReferenceFolder => SourceRootFolder / "shared" / "api-ref";



    AbsolutePath UserToolsRootFolder => SourceRootFolder / "user-tools";


    AbsolutePath MidiPowerShellSolutionFolder => AppSdkSolutionFolder / "projections" / "powershell" ;
    AbsolutePath MidiPowerShellStagingFolder => StagingRootFolder / "midi-powershell";


    AbsolutePath MidiConsoleSolutionFolder => UserToolsRootFolder / "midi-console";
    AbsolutePath MidiConsoleStagingFolder => StagingRootFolder / "midi-console";
    AbsolutePath ConsoleSetupSolutionFolder => UserToolsRootFolder / "midi-console-setup";


    AbsolutePath MidiSettingsSolutionFolder => UserToolsRootFolder / "midi-settings";
    AbsolutePath MidiSettingsStagingFolder => StagingRootFolder / "midi-settings";
   // AbsolutePath MidiSettingsSetupSolutionFolder => UserToolsRootFolder / "midi-settings-setup";

    //    AbsolutePath RustWinRTSamplesRootFolder => SamplesRootFolder / "rust-winrt";
    //    AbsolutePath ElectronJSSamplesRootFolder => SamplesRootFolder / "electron-js";

    AbsolutePath SetupBundleInfoIncludeFile => StagingRootFolder / "version" / "BundleInfo.wxi";
    AbsolutePath BuildVersionFile => StagingRootFolder / "version" / "BuildNumber.txt";

    AbsolutePath SdkVersionFilesFolder => StagingRootFolder / "version";
    AbsolutePath SdkVersionHeaderFile => SdkVersionFilesFolder / "WindowsMidiServicesSdkRuntimeVersion.h";
    AbsolutePath NuGetVersionHeaderFile => SdkVersionFilesFolder / "WindowsMidiServicesVersion.h";
    AbsolutePath CommonVersionCSharpFile => SdkVersionFilesFolder / "WindowsMidiServicesVersion.cs";

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
            Logging.Level = LoggingLevel;


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


    Target CreateVersionIncludes => _ => _
        .DependsOn(Prerequisites)
        .Executes(() =>
        {
            string buildSource = "GitHub Preview";
            string versionName = VersionName;
            string versionString = BuildVersionFullString;

            string buildVersionMajor = BuildVersionMajor.ToString();
            string buildVersionMinor = BuildVersionMinor.ToString();
            string buildVersionPatch = BuildVersionPatch.ToString();

            string buildDate = BuildDate.ToString("yyyy-MM-dd");

            // create directories if they do not exist

            ThisReleaseFolder.CreateDirectory();
            SdkVersionFilesFolder.CreateDirectory();

            // json version for published SDK releases. This needs to be manually copied to /docs/version/sdk_version.json
            // if this ends up being a published release

            var SdkVersionJsonFile = ThisReleaseFolder / "sdk_version.json";

            using (StreamWriter writer = System.IO.File.CreateText(SdkVersionJsonFile))
            {
                writer.WriteLine("{");
                writer.WriteLine("    \"releases\":");
                writer.WriteLine("    [");
                writer.WriteLine("        {");
                writer.WriteLine($"            \"type\": \"{BuildType.ToString().ToLower()}\",");
                writer.WriteLine($"            \"source\": \"{buildSource}\",");
                writer.WriteLine($"            \"name\": \"{versionName}\",");
                writer.WriteLine($"            \"description\": \"Replace this text with a summary of this SDK update\",");
                writer.WriteLine($"            \"buildDate\": \"{buildDate}\",");
                writer.WriteLine($"            \"versionFull\": \"{versionString}\",");
                writer.WriteLine($"            \"versionMajor\": {buildVersionMajor},");
                writer.WriteLine($"            \"versionMinor\": {buildVersionMinor},");
                writer.WriteLine($"            \"versionPatch\": {buildVersionPatch},");
                writer.WriteLine($"            \"versionBuildNumber\": {BuildVersionBuildNumber},");
                writer.WriteLine($"            \"preview\": \"{BuildVersionPreviewString}\",");
                writer.WriteLine($"            \"releaseNotesUri\": \"https://github.com/microsoft/MIDI/releases/tag/ (TODO)\",");
                writer.WriteLine($"            \"directDownloadUriX64\": \"https://aka.ms/MidiServicesLatestSdkRuntimeInstaller_Directx64\",");
                writer.WriteLine($"            \"directDownloadUriArm64\": \"https://aka.ms/MidiServicesLatestSdkRuntimeInstaller_DirectArm64\"");
                writer.WriteLine("        }");
                writer.WriteLine("    ]");
                writer.WriteLine("}");
            }

            bool isPreview = BuildType == MidiBuildType.Preview;

            // create .h file for SDK and related tools

            using (StreamWriter writer = System.IO.File.CreateText(NuGetVersionHeaderFile))
            {
                writer.WriteLine("// this file has been auto-generated by the Windows MIDI Services build process");
                writer.WriteLine("// the version information here represents the NuGet package version your app was");
                writer.WriteLine("// built against");
                writer.WriteLine();
                writer.WriteLine("#ifndef WINDOWS_MIDI_SERVICES_NUGET_VERSION_INCLUDE");
                writer.WriteLine("#define WINDOWS_MIDI_SERVICES_NUGET_VERSION_INCLUDE");
                writer.WriteLine();
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_NUGET_BUILD_IS_PREVIEW                         {isPreview.ToString().ToLower()}");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_NUGET_BUILD_SOURCE                             L\"{buildSource}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_NUGET_BUILD_DATE                               L\"{buildDate}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_NUGET_BUILD_VERSION_NAME                       L\"{versionName}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_NUGET_BUILD_VERSION_FULL                       L\"{versionString}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_NUGET_BUILD_VERSION_MAJOR                      {buildVersionMajor}");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_NUGET_BUILD_VERSION_MINOR                      {buildVersionMinor}");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_NUGET_BUILD_VERSION_PATCH                      {buildVersionPatch}");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_NUGET_BUILD_VERSION_BUILD_NUMBER               {BuildVersionBuildNumber}");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_NUGET_BUILD_PREVIEW                            L\"{BuildVersionPreviewString}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_NUGET_BUILD_VERSION_FILE                       L\"{BuildVersionFileFullString}\"");
                writer.WriteLine();
                writer.WriteLine("#endif");
                writer.WriteLine();
            }

            using (StreamWriter writer = System.IO.File.CreateText(SdkVersionHeaderFile))
            {
                writer.WriteLine("// this file has been auto-generated by the Windows MIDI Services build process");
                writer.WriteLine("// SDK runtime version file");
                writer.WriteLine();
                writer.WriteLine("#ifndef WINDOWS_MIDI_SERVICES_SDK_RUNTIME_VERSION_INCLUDE");
                writer.WriteLine("#define WINDOWS_MIDI_SERVICES_SDK_RUNTIME_VERSION_INCLUDE");
                writer.WriteLine();
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_IS_PREVIEW                       {isPreview.ToString().ToLower()}");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_SOURCE                           L\"{buildSource}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_DATE                             L\"{buildDate}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_NAME                     L\"{versionName}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_FULL                     L\"{versionString}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_MAJOR                    {buildVersionMajor}");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_MINOR                    {buildVersionMinor}");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_PATCH                    {buildVersionPatch}");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_BUILD_NUMBER             {BuildVersionBuildNumber}");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_PREVIEW                          L\"{BuildVersionPreviewString}\"");
                writer.WriteLine($"#define WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_FILE                     L\"{BuildVersionFileFullString}\"");
                writer.WriteLine();
                writer.WriteLine("#endif");
                writer.WriteLine();
            }

            // create .cs file for console and settings app

            using (StreamWriter writer = System.IO.File.CreateText(CommonVersionCSharpFile))
            {
                writer.WriteLine("// this file has been auto-generated by the Windows MIDI Services build process");
                writer.WriteLine("// this information is the version from the SDK projection, not the SDK runtime");
                writer.WriteLine("// the version information here represents the NuGet package version your app was");
                writer.WriteLine("// built against");
                writer.WriteLine();

                writer.WriteLine("namespace Microsoft.Windows.Devices.Midi2.Common");
                writer.WriteLine("{");
                writer.WriteLine("\tpublic static class MidiNuGetBuildInformation");
                writer.WriteLine("\t{");
                writer.WriteLine($"\t\tpublic const bool IsPreview = {isPreview.ToString().ToLower()};");
                writer.WriteLine($"\t\tpublic const string Source = \"{buildSource}\";");
                writer.WriteLine($"\t\tpublic const string BuildDate = \"{buildDate}\";");
                writer.WriteLine($"\t\tpublic const string Name = \"{versionName}\";");
                writer.WriteLine($"\t\tpublic const string BuildFullVersion = \"{versionString}\";");
                writer.WriteLine($"\t\tpublic const ushort VersionMajor = {buildVersionMajor};");
                writer.WriteLine($"\t\tpublic const ushort VersionMinor = {buildVersionMinor};");
                writer.WriteLine($"\t\tpublic const ushort VersionPatch = {buildVersionPatch};");
                writer.WriteLine($"\t\tpublic const ushort VersionBuildNumber = {BuildVersionBuildNumber};");
                writer.WriteLine($"\t\tpublic const string Preview = \"{BuildVersionPreviewString}\";");
                writer.WriteLine($"\t\tpublic const string AssemblyFullVersion = \"{BuildVersionAssemblyFullString}\";");
                writer.WriteLine($"\t\tpublic const string FileFullVersion = \"{BuildVersionFileFullString}\";");
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

            referenceFiles.Add(intermediateFolder / "MidiKS" / sourcePlatform / Configuration.Release / "MidiKS.lib");

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
        //    bool wxsWritten = false;

            foreach (var platform in SdkPlatforms)
            {
                string solutionDir = AppSdkSolutionFolder.ToString() + @"\";

                var msbuildProperties = new Dictionary<string, object>();
                msbuildProperties.Add("Platform", platform);
                msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash
                msbuildProperties.Add("NoWarn", "MSB3271");             // winmd and dll platform mismatch with Arm64EC
                msbuildProperties.Add("Version", BuildVersionFullString);
                msbuildProperties.Add("VersionPrefix", BuildMajorMinorPatch);
                msbuildProperties.Add("AssemblyVersion", BuildVersionAssemblyFullString);
                msbuildProperties.Add("FileVersion", BuildVersionFileFullString);

                Console.Out.WriteLine($"----------------------------------------------------------------------");
                Console.Out.WriteLine($"SolutionDir:   {solutionDir}");
                Console.Out.WriteLine($"Platform:      {platform}");

                MSBuildTasks.MSBuild(_ => _
                    .SetTargetPath(AppSdkSolutionFolder / "app-sdk.sln")
                    .SetMaxCpuCount(null)
                    /*.SetOutDir(outputFolder) */
                    /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                    /*.SetTargets("Build") */
                    .SetProperties(msbuildProperties)
                    .SetConfiguration(Configuration.Release)
                    .SetVerbosity(BuildVerbosity)
                    .EnableNodeReuse()
                );

            }

            var sdkOutputRootFolder = AppSdkSolutionFolder / "vsfiles" / "out";

//        string rid = "net8.0-windows10.0.20348.0";

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

                Console.Out.WriteLine($"NuGet Version: {BuildVersionFullString}");

                NuGetTasks.NuGetPack(_ => _
                    .SetTargetPath(AppSdkSolutionFolder / "projections" / "dotnet-and-cpp" / "nuget" / "Microsoft.Windows.Devices.Midi2.nuspec")
                    .AddProperty("version", NugetPackageVersion)
                    .AddProperty("id", NugetPackageId)
                    .SetOutputDirectory(AppSdkNugetOutputFolder)
                );


                string stagingPlatform = sourcePlatform;
                if (sourcePlatform.ToLower() == "arm64ec")
                {
                    stagingPlatform = "arm64";
                }

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
           //     FileSystemTasks.CopyFileToDirectory(AppSdkSolutionFolder / "ExampleMidiApp.exe.manifest", AppSdkStagingFolder / stagingPlatform, FileExistsPolicy.Overwrite, true);

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
                msbuildProperties.Add("VersionPrefix", BuildMajorMinorPatch);
                msbuildProperties.Add("Version", BuildVersionFullString);
                msbuildProperties.Add("AssemblyVersion", BuildVersionAssemblyFullString);
                msbuildProperties.Add("FileVersion", BuildVersionFileFullString);

                Console.Out.WriteLine($"----------------------------------------------------------------------");
                Console.Out.WriteLine($"SolutionDir:   {solutionDir}");
                Console.Out.WriteLine($"Platform:      {platform}");

                string[] toolsDirectoriesNeedingSdkPackageUpdates =
                    {
                        Path.Combine(solutionDir, @"tools\mididiag"),
                        Path.Combine(solutionDir, @"tools\midiusbinfo"),
                        Path.Combine(solutionDir, @"tools\midimdnsinfo"),
                        //Path.Combine(solutionDir, @"tools\midifixreg"),

                        Path.Combine(solutionDir, @"tests\InitializationExe"),
                        Path.Combine(solutionDir, @"tests\Benchmarks"),
                        Path.Combine(solutionDir, @"tests\Offline.unittests"),
                        Path.Combine(solutionDir, @"tests\SdkInitialization.unittests"),
                        Path.Combine(solutionDir, @"tests\Service.integrationtests"),
                        /* Path.Combine(solutionDir, "midi1monitor"), */
                        /* Path.Combine(solutionDir, "midiksinfo"), */
                    };

                foreach (var projectFolder in toolsDirectoriesNeedingSdkPackageUpdates)
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
                    .SetMaxCpuCount(null)
                    /*.SetOutDir(outputFolder) */
                    /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                    /*.SetTargets("Build") */
                    .SetProperties(msbuildProperties)
                    .SetConfiguration(Configuration.Release)
                    .SetVerbosity(BuildVerbosity)
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

                // Simple MIDI utilities
                FileSystemTasks.CopyFileToDirectory(sdkOutputRootFolder / "mididiag" / stagingPlatform / Configuration.Release / $"mididiag.exe", AppSdkStagingFolder / stagingPlatform, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(sdkOutputRootFolder / "midiksinfo" / stagingPlatform / Configuration.Release / $"midiksinfo.exe", AppSdkStagingFolder / stagingPlatform, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(sdkOutputRootFolder / "midimdnsinfo" / stagingPlatform / Configuration.Release / $"midimdnsinfo.exe", AppSdkStagingFolder / stagingPlatform, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(sdkOutputRootFolder / "midi1monitor" / stagingPlatform / Configuration.Release / $"midi1monitor.exe", AppSdkStagingFolder / stagingPlatform, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(sdkOutputRootFolder / "midi1enum" / stagingPlatform / Configuration.Release / $"midi1enum.exe", AppSdkStagingFolder / stagingPlatform, FileExistsPolicy.Overwrite, true);
                FileSystemTasks.CopyFileToDirectory(sdkOutputRootFolder / "midifixreg" / stagingPlatform / Configuration.Release / $"midifixreg.exe", AppSdkStagingFolder / stagingPlatform, FileExistsPolicy.Overwrite, true);
            }
        });


    Target BuildAppSdkRuntimeAndToolsInstaller => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(CreateVersionIncludes)
        .DependsOn(BuildConsoleApp)
        .DependsOn(BuildSettingsApp)
        .DependsOn(BuildPowerShellProjection)
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
                msbuildProperties.Add("VersionPrefix", BuildMajorMinorPatch);
                msbuildProperties.Add("Version", BuildVersionFileFullString);
                msbuildProperties.Add("FileVersion", BuildVersionFileFullString);

                Console.Out.WriteLine($"----------------------------------------------------------------------");
                Console.Out.WriteLine($"SolutionDir: {solutionDir}");
                Console.Out.WriteLine($"Platform:    {platform}");

                //var setupSolutionFolder = AppSdkSolutionFolder / "sdk-runtime-installer";


                MSBuildTasks.MSBuild(_ => _
                    .SetTargetPath(AppSdkSetupSolutionFolder / "midi-services-app-sdk-runtime-setup.sln")
                    .SetMaxCpuCount(null)
                    /*.SetOutDir(outputFolder) */
                    /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                    /*.SetTargets("Build") */
                    .SetProperties(msbuildProperties)
                    .SetTargets("Clean", "Rebuild")
                    .SetConfiguration(Configuration.Release)
                    .SetVerbosity(BuildVerbosity)
                    .EnableNodeReuse()
                );

                // todo: it would be better to see if any of the sdk files have changed and only
                // do this copy if a new setup file was created. Maybe do a before/after date/time check?

                string newInstallerName = $"Windows MIDI Services (SDK Runtime and Tools) {BuildVersionFullString}-{platform.ToLower()}.exe";
                FileSystemTasks.CopyFile(
                    AppSdkSetupSolutionFolder / "main-bundle" / "bin" / platform / Configuration.Release / "WindowsMidiServicesSdkRuntimeSetup.exe",
                    ThisReleaseFolder / newInstallerName);

                BuiltSdkRuntimeInstallers[platform.ToLower()] = newInstallerName;
            }

        });


    void IncrementBuildNumber()
    {
        int newBuildNumber = 0;

        if (File.Exists(BuildVersionFile))
        {
            using (StreamReader reader = System.IO.File.OpenText(BuildVersionFile))
            {
                // first line is major/minor/revision. Second is build number.
                var versionLine = reader.ReadLine();
                var buildLine = reader.ReadLine();

                if (versionLine.Trim() == BuildMajorMinorPatch)
                {
                    if (int.TryParse(buildLine, out newBuildNumber))
                    {
                        newBuildNumber++;
                    }
                    else
                    {
                        newBuildNumber = 0;
                    }
                }
                else
                {
                    // we'll write the new info
                }

            }
        }

        using (StreamWriter writer = System.IO.File.CreateText(BuildVersionFile))
        {
            writer.WriteLine(BuildMajorMinorPatch);
            writer.WriteLine(newBuildNumber.ToString());
        }

        BuildVersionBuildNumber = (ushort)newBuildNumber;

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

            NuGetTasks.NuGetRestore(_ => _
                .SetProcessWorkingDirectory(solutionDir)
                .SetSource(@"https://api.nuget.org/v3/index.json")
                .SetSolutionDirectory(solutionDir)
            //.SetConfigFile(packagesConfigFullPath)
            );

            var output = MSBuildTasks.MSBuild(_ => _
                .SetTargetPath(InDevelopmentServiceComponentsSetupSolutionFolder / "midi-services-in-box-preview-setup.sln")
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

            string newInstallerName = $"Windows MIDI Services (Preview Service Plugins) {BuildVersionFullString}-{platform.ToLower()}.exe";

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
                .SetSource(AppSdkNugetOutputFolder, @"https://api.nuget.org/v3/index.json")
            );

            bool wxsWritten = false;

            // build x64 and Arm64, no Arm64EC
            foreach (var platform in ToolsPlatforms)
            {
                string solutionDir = MidiSettingsSolutionFolder.ToString() + @"\";

                //
                // TEMP! The MIDI Settings app is x64 right now due to conflict with WinUI Ro Detours with Arm64
                //
                //string rid = platform.ToLower() == "arm64" ? "win-arm64" : "win-x64";
                string rid = "win-x64";

                

                DotNetTasks.DotNetBuild(_ => _
                    .SetProjectFile(MidiSettingsSolutionFolder / "Microsoft.Midi.Settings" / "Microsoft.Midi.Settings.csproj")
                    .SetConfiguration(Configuration.Release)
                    .SetPublishSingleFile(false)
                    .SetPublishTrimmed(false)
                    .SetSelfContained(false)
                    .SetRuntime(rid)
                    .SetVersionPrefix(BuildMajorMinorPatch)
                    .SetVersion(BuildVersionFullString)
                    .SetFileVersion(BuildVersionFileFullString)
                    .SetAssemblyVersion(BuildVersionAssemblyFullString)
                    //.AddNoWarns(8618) // ignore CS8618 which I have no control over because it's in projection assemblies
                    //.AddNoWarns(45)     // will this stop "MVVMTK0045" ?
                );

                var settingsOutputFolder = MidiSettingsSolutionFolder / "Microsoft.Midi.Settings" / "bin" / Configuration.Release / "net8.0-windows10.0.22621.0" / rid;
                var stagingFolder = MidiSettingsStagingFolder / platform;

                stagingFolder.CreateOrCleanDirectory();


                // get all the community toolkit files
                var toolkitFiles = Globbing.GlobFiles(settingsOutputFolder, "CommunityToolkit*.dll");
                var msftExtensionsFiles = Globbing.GlobFiles(settingsOutputFolder, "Microsoft.Extensions*.dll");
                var midiSdkFiles = Globbing.GlobFiles(
                    settingsOutputFolder, 
                    "Microsoft.Windows.Devices.Midi2.NetProjection.dll"
                    );

                List<AbsolutePath> paths = new List<AbsolutePath>(toolkitFiles.Count + msftExtensionsFiles.Count + midiSdkFiles.Count + 40);
                paths.AddRange(toolkitFiles);
                paths.AddRange(msftExtensionsFiles);
                paths.AddRange(midiSdkFiles);


                // copy output to staging folder

                paths.Add(settingsOutputFolder / "MidiSettings.exe");
                paths.Add(settingsOutputFolder / "MidiSettings.dll");
                paths.Add(settingsOutputFolder / "MidiSettings.exe.manifest");
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

                paths.Add(settingsOutputFolder / "System.CodeDom.dll");
                paths.Add(settingsOutputFolder / "System.Diagnostics.EventLog.dll");
                paths.Add(settingsOutputFolder / "System.Diagnostics.EventLog.Messages.dll");
                paths.Add(settingsOutputFolder / "System.Management.dll");
                paths.Add(settingsOutputFolder / "System.ServiceProcess.ServiceController.dll");

             //   paths.Add(settingsOutputFolder / "System.Text.Json.dll");

                paths.Add(settingsOutputFolder / "Microsoft.Web.WebView2.Core.dll");
                paths.Add(settingsOutputFolder / "Microsoft.Web.WebView2.Core.Projection.dll");
                paths.Add(settingsOutputFolder / "WebView2Loader.dll");

                paths.Add(settingsOutputFolder / "WinRT.Runtime.dll");

                paths.Add(settingsOutputFolder / "Microsoft.WindowsAppRuntime.Bootstrap.dll");
                paths.Add(settingsOutputFolder / "Microsoft.WindowsAppRuntime.Bootstrap.Net.dll");

                paths.Add(settingsOutputFolder / "Microsoft.WinUI.dll");
  //              paths.Add(settingsOutputFolder / "Microsoft.Xaml.Interactions.dll");
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
                paths.Add(settingsOutputFolder / "Assets" / "LoopbackDiagram.svg");


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
                .SetSource(AppSdkNugetOutputFolder, @"https://api.nuget.org/v3/index.json")
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
                    .SetVersionPrefix(BuildMajorMinorPatch)
                    .SetVersion(BuildVersionFullString)
                    .SetFileVersion(BuildVersionFileFullString)
                    .SetAssemblyVersion(BuildVersionAssemblyFullString)
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


    Target BuildPowerShellProjection => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(BuildAndPackAllAppSDKs)
        .Executes(() =>
    {
        var solution = MidiPowerShellSolutionFolder / "midi-powershell.sln";

        // for the MIDI nuget package
        NuGetTasks.NuGetInstall(_ => _
            .SetProcessWorkingDirectory(MidiPowerShellSolutionFolder)
            .SetPreRelease(true)
            .SetSource(AppSdkNugetOutputFolder)
            .SetPackageID(NugetPackageId)
            .SetDependencyVersion(DependencyVersion.Highest)
        );

        NuGetTasks.NuGetRestore(_ => _
            .SetProcessWorkingDirectory(MidiPowerShellSolutionFolder)
            .SetSolutionDirectory(MidiPowerShellSolutionFolder)
            .SetSource(AppSdkNugetOutputFolder, @"https://api.nuget.org/v3/index.json")
        );

        // build x64 and Arm64, no Arm64EC
        foreach (var platform in ToolsPlatforms)
        {
            string solutionDir = MidiPowerShellSolutionFolder.ToString() + @"\";

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
                .SetProjectFile(MidiPowerShellSolutionFolder / "WindowsMidiServices.csproj")
                .SetConfiguration(Configuration.Release)
                .SetVersionPrefix(BuildMajorMinorPatch)
                .SetVersion(BuildVersionFullString)
                .SetFileVersion(BuildVersionFileFullString)
                .SetAssemblyVersion(BuildVersionAssemblyFullString)
                .SetPublishSingleFile(false)
                .SetPublishTrimmed(false)
                .SetSelfContained(false)
                .SetRuntime(rid)
            );

            // copy output to staging folder

            // TODO: This doesn't deal with any localization content

            var psOutputFolder = MidiPowerShellSolutionFolder / "bin" / Configuration.Release / "net8.0-windows10.0.20348.0" / rid;
            //var runtimesFolder = consoleOutputFolder / "runtimes" / rid / "native";
            var runtimesFolder = psOutputFolder;

            var stagingFolder = MidiPowerShellStagingFolder / platform;

            FileSystemTasks.CopyFileToDirectory(MidiPowerShellSolutionFolder / "WindowsMidiServices.psd1", stagingFolder, FileExistsPolicy.Overwrite, true);

            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "WindowsMidiServices.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "WindowsMidiServices.deps.json", stagingFolder, FileExistsPolicy.Overwrite, true);

            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "WinRT.Runtime.dll", stagingFolder, FileExistsPolicy.Overwrite, true);

            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "Microsoft.Windows.Devices.Midi2.NetProjection.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "Microsoft.Windows.SDK.NET.dll", stagingFolder, FileExistsPolicy.Overwrite, true);

            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "Microsoft.ApplicationInsights.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "Microsoft.Win32.Registry.AccessControl.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "Newtonsoft.Json.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "System.CodeDom.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "System.Configuration.ConfigurationManager.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "System.Diagnostics.EventLog.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "System.DirectoryServices.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "System.Management.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "System.Security.Cryptography.Pkcs.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "System.Security.Cryptography.ProtectedData.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "System.Security.Permissions.dll", stagingFolder, FileExistsPolicy.Overwrite, true);
            FileSystemTasks.CopyFileToDirectory(psOutputFolder / "System.Windows.Extensions.dll", stagingFolder, FileExistsPolicy.Overwrite, true);


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
                    .SetMaxCpuCount(null)
                    /*.SetOutDir(outputFolder) */
                    /*.SetProcessWorkingDirectory(ApiSolutionFolder)*/
                    /*.SetTargets("Build") */
                    .SetProperties(msbuildProperties)
                    .SetConfiguration(Configuration.Release)
                    .SetVerbosity(BuildVerbosity)
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
            .SetSource(AppSdkNugetOutputFolder, @"https://api.nuget.org/v3/index.json")
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
                .SetMaxCpuCount(null)
                /*.SetOutDir(outputFolder) */
                .SetProcessWorkingDirectory(SamplesCSWinRTSolutionFolder)
                /*.SetTargets("Build") */
                .SetProperties(msbuildProperties)
                .SetConfiguration(Configuration.Release)
                .SetVerbosity(BuildVerbosity)
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


    Target ZipSamples => _ => _
        .DependsOn(Prerequisites)
        .DependsOn(CreateVersionIncludes)
        .DependsOn(BuildAndPackAllAppSDKs)
        .DependsOn(BuildCSharpSamples)
        .DependsOn(BuildCppSamples)
        .Executes(() =>
        {
            var samplesFolder = RootDirectory / "samples" ;

            // have to do a string compare on the folders here because the type throws
            // an exception if you use the HasDirectory or similar methods, and the directory
            // does not exist
            samplesFolder.ZipTo(
                ThisReleaseFolder / $"samples.zip",
                filter: x =>
                    !x.HasExtension("nupkg") &&
                    !x.HasExtension("user") &&
                    !x.HasExtension("log") &&
                    !x.HasExtension("pfx") &&
                    !x.ToString().Contains(@"\packages\") &&
                    !x.ToString().Contains(@"\.vs\") &&
                    !x.ToString().Contains(@"\obj\") &&
                    !x.ToString().Contains(@"\bin\") &&
                    !x.ToString().Contains(@"\intermediate\") &&
                    !x.ToString().Contains(@"\vsfiles\") &&
                    !x.ToString().Contains(@"\node_modules\")
                //filter: x =>
                //    x.HasExtension("ps1") ||

                //    x.HasExtension("md") ||

                //    x.HasExtension("sln") ||
                //    x.HasExtension("props") ||
                //    x.HasExtension("pfx") ||

                //    x.HasExtension("vcxproj") ||
                //    x.HasExtension("cpp") ||
                //    x.HasExtension("h") ||
                //    x.HasExtension("config") ||
                //    x.HasExtension("filters") ||

                //    x.HasExtension("csproj") ||
                //    x.HasExtension("cs") ||
                //    x.HasExtension("xaml") ||
                //    x.HasExtension("json") ||

                //    x.HasExtension("js") ||
                //    x.HasExtension("html") 
                );
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

            CopyFile(ApiStagingFolder / "arm64" / driverFile, arm64 / driverFile, FileExistsPolicy.Fail, false);
            CopyFile(ApiStagingFolder / "arm64" / pdbFile, arm64 / pdbFile, FileExistsPolicy.Fail, false);
            CopyFile(regHelperCmdFileFullPath, arm64 / regHelperCmdFileName, FileExistsPolicy.Fail, false);
            CopyFile(regHelperPs1FileFullPath, arm64 / regHelperPs1FileName, FileExistsPolicy.Fail, false);
            CopyFile(readmeFileFullPath, arm64 / readmeFileName, FileExistsPolicy.Fail, false);


            CopyFile(ApiStagingFolder / "x64" / driverFile, x64 / driverFile, FileExistsPolicy.Fail, false);
            CopyFile(ApiStagingFolder / "x64" / pdbFile, x64 / pdbFile, FileExistsPolicy.Fail, false);
            CopyFile(regHelperCmdFileFullPath, x64 / regHelperCmdFileName, FileExistsPolicy.Fail, false);
            CopyFile(regHelperPs1FileFullPath, x64 / regHelperPs1FileName, FileExistsPolicy.Fail, false);
            CopyFile(readmeFileFullPath, x64 / readmeFileName, FileExistsPolicy.Fail, false);


            CopyFile(ApiStagingFolder / "Win32" / driverFile, x86 / driverFile, FileExistsPolicy.Fail, false);
            CopyFile(ApiStagingFolder / "Win32" / pdbFile, x86 / pdbFile, FileExistsPolicy.Fail, false);
            CopyFile(regHelperx86CmdFileFullPath, x86 / regHelperx86CmdFileName, FileExistsPolicy.Fail, false);
            CopyFile(regHelperx86Ps1FileFullPath, x86 / regHelperx86Ps1FileName, FileExistsPolicy.Fail, false);
            CopyFile(readmeFileFullPath, x86 / readmeFileName, FileExistsPolicy.Fail, false);


            x64.ZipTo(ThisReleaseFolder / $"wdmaud2-winmm-x64.zip");
            arm64.ZipTo(ThisReleaseFolder / $"wdmaud2-winmm-arm64.zip");
            x86.ZipTo(ThisReleaseFolder / $"wdmaud2-winmm-x86-win32.zip");

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
        .DependsOn(ZipSamples)
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
