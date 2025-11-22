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

    const UInt16 BuildVersionPreviewNumber = 14;
    string VersionName => "Service Preview " + BuildVersionPreviewNumber;

    // --------------------------------------------------------------------------------------

    UInt16 BuildVersionBuildNumber = 0; // gets read from the version file and reset with each patch change

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

    void SetMSBuildVersionOld()
    {

        // I hate this, but build was picking up the v17 tools no matter what I did.
        //MSBuildPath = @"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe";
        MSBuildPath = @"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe";

        MSBuildTasks.MSBuildPath = MSBuildPath;
    }
    void SetMSBuildVersionNew()
    {

        // I hate this, but build was picking up the v17 tools no matter what I did.
        MSBuildPath = @"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe";

        MSBuildTasks.MSBuildPath = MSBuildPath;
    }

    Target T_Prerequisites => _ => _
        .Executes(() =>
        {
            Logging.Level = LoggingLevel;

            SetMSBuildVersionNew();

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

            SetMSBuildVersionNew();

            MSBuildTasks.MSBuild(_ => _
                .SetTargetPath(SourceRootFolder / "build-gen-version-includes" / "GenVersionIncludes.csproj")
                .SetMaxCpuCount(null)
                .AddProcessAdditionalArguments("/t:TransformAll")
                .SetProperties(msbuildProperties)
                .SetVerbosity(MSBuildVerbosity.Normal)
                .EnableNodeReuse()
            );

            // WDK is broken for VS 2026, so have to use old VS 2022 MSBuild tools
            SetMSBuildVersionOld();

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
