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

    MidiBuildType BuildType => MidiBuildType.RC; 
    
    const UInt16 BuildVersionMajor = 1;
    const UInt16 BuildVersionMinor = 0;
    const UInt16 BuildVersionPatch = 23;

    const UInt16 BuildVersionPreviewNumber = 5;
    string VersionName => "SDK Release Candidate " + BuildVersionPreviewNumber;

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


    const string TargetWindowsSdkVersion = "10.0.26100.0";

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
    AbsolutePath BuildVersionFile => StagingRootFolder / "version" / "SDK_BuildNumber.txt";

    AbsolutePath SdkVersionFilesFolder => StagingRootFolder / "version";
    AbsolutePath SdkVersionHeaderFile => SdkVersionFilesFolder / "WindowsMidiServicesSdkRuntimeVersion.h";
    AbsolutePath NuGetVersionHeaderFile => SdkVersionFilesFolder / "WindowsMidiServicesVersion.h";
    AbsolutePath CommonVersionCSharpFile => SdkVersionFilesFolder / "WindowsMidiServicesVersion.cs";

    AbsolutePath SamplesRootFolder => NukeBuild.RootDirectory / "samples";
    AbsolutePath SamplesCppWinRTSolutionFolder => SamplesRootFolder / "cpp-winrt";

    AbsolutePath SamplesCSWinRTSolutionFolder => SamplesRootFolder / "csharp-net";

    
    string[] SdkPlatformsIncludingAnyCpu => new string[] { "AnyCPU", "x64", "Arm64EC"  };
    string[] SdkPlatforms => new string[] { "x64", "Arm64EC" };
    string[] ServiceAndApiPlatforms => new string[] { "x64", "Arm64" };
    string[] ServiceAndApiPlatformsAll => new string[] { "x64", "Arm64", "Arm64EC" };   // the order here matters because the dependencies in the solution aren't perfect
    string[] ToolsPlatforms => new string[] { "x64", "Arm64" };
    string[] NativeSamplesPlatforms => new string[] { "x64", "Arm64", "Arm64EC" };
    string[] ManagedSamplesPlatforms => new string[] { "x64", "Arm64" };
    string[] InstallerPlatforms => new string[] { "x64", "Arm64" };



    Dictionary<string, string> BuiltSdkRuntimeInstallers = new Dictionary<string, string>();

    public static int Main () => Execute<Build>(x => x.BuildAndPublishAll);

    string MSBuildPath;

    void SetMSBuildVersion()
    {
        //MSBuildLocator.RegisterInstance(MSBuildLocator.QueryVisualStudioInstances().OrderByDescending(
        //   instance => instance.Version).First());

        //var instances = MSBuildLocator.QueryVisualStudioInstances().OrderByDescending(instance => instance.Version);

        //foreach (var inst in instances)
        //{
        //    Console.WriteLine($"Instance Found: {inst.Version.ToString()}");
        //    Console.WriteLine($"- MSBuild Path: {inst.MSBuildPath}");
        //    Console.WriteLine();
        //}

        //MSBuildPath = MSBuildLocator.QueryVisualStudioInstances().OrderByDescending(
        //    instance => instance.Version).First().MSBuildPath;

        //Console.WriteLine($"Using: {MSBuildPath}");
        //Console.WriteLine();



        // I hate this, but build was picking up the v17 tools no matter what I did.
        MSBuildPath = @"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe";

        MSBuildTasks.MSBuildPath = MSBuildPath;
    }

    //protected override void OnBuildCreated()
    //{
    //    RegisterMSBuildVersion();

    //    base.OnBuildCreated();
    //}


    Target T_Prerequisites => _ => _
        .Executes(() =>
        {
            Logging.Level = LoggingLevel;

            SetMSBuildVersion();

            Console.WriteLine($"Found MSBuild here: {MSBuildPath}");

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
                .SetProcessToolPath(MSBuildTasks.MSBuildPath)
                .SetTargetPath(SourceRootFolder / "build-gen-version-includes" / "GenVersionIncludes.csproj")
                .SetMaxCpuCount(null)
                .AddProcessAdditionalArguments("/t:TransformAll")
                .SetProperties(msbuildProperties)
                .SetVerbosity(MSBuildVerbosity.Normal)
                .EnableNodeReuse()
            );

        });


    Target T_BuildAndPackAllAppSDKs => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_CreateVersionIncludes)
        .Executes(() =>
        {
        //    bool wxsWritten = false;

            foreach (var platform in SdkPlatformsIncludingAnyCpu)
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
                    .SetProcessToolPath(MSBuildTasks.MSBuildPath)
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
            var sdkIntermediateRootFolder = AppSdkSolutionFolder / "vsfiles" / "intermediate";

            // header files and similar
            foreach (var sourcePlatform in SdkPlatforms)
            {
                // the solution compiles these apps to Arm64 when target is EC.
                string stagingPlatform = sourcePlatform;
                if (sourcePlatform.ToLower() == "arm64ec")
                {
                    stagingPlatform = "Arm64";
                }

                // sample manifest
                //     (AppSdkSolutionFolder / "ExampleMidiApp.exe.manifest", AppSdkStagingFolder / stagingPlatform, ExistsPolicy.FileOverwrite);

                // bootstrap files
                (AppSdkSolutionFolder / "client-initialization-redist" / "Microsoft.Windows.Devices.Midi2.Initialization.hpp").CopyToDirectory(AppSdkStagingFolder / stagingPlatform, ExistsPolicy.FileOverwrite);
                //(AppSdkSolutionFolder / "client-initialization-redist" / "MidiDesktopAppSdkBootstrapper.cs", AppSdkStagingFolder / stagingPlatform, ExistsPolicy.FileOverwrite);

                // these are not architecture-specific
                (sdkIntermediateRootFolder / "com-extensions-idl" / sourcePlatform / Configuration.Release / "WindowsMidiServicesAppSdkComExtensions.h").CopyToDirectory(AppSdkStagingFolder, ExistsPolicy.FileOverwrite);
                (sdkIntermediateRootFolder / "com-extensions-idl" / sourcePlatform / Configuration.Release / "WindowsMidiServicesAppSdkComExtensions_i.c").CopyToDirectory(AppSdkStagingFolder, ExistsPolicy.FileOverwrite);
                (sdkIntermediateRootFolder / "com-extensions-idl" / sourcePlatform / Configuration.Release / "WindowsMidiServicesAppSdkComExtensions_p.c").CopyToDirectory(AppSdkStagingFolder, ExistsPolicy.FileOverwrite);
            }


            //        string rid = $"net8.0-windows{TargetWindowsSdkVersion}";

            foreach (var sourcePlatform in SdkPlatformsIncludingAnyCpu)
            {
                if (sourcePlatform == "AnyCPU") continue;

                var sdkBinaries = new List<AbsolutePath>();

                sdkBinaries.Add(sdkOutputRootFolder / "Microsoft.Windows.Devices.Midi2" / sourcePlatform / Configuration.Release / $"Microsoft.Windows.Devices.Midi2.winmd");
                sdkBinaries.Add(sdkOutputRootFolder / "Microsoft.Windows.Devices.Midi2" / sourcePlatform / Configuration.Release / $"Microsoft.Windows.Devices.Midi2.dll");
                sdkBinaries.Add(sdkOutputRootFolder / "Microsoft.Windows.Devices.Midi2" / sourcePlatform / Configuration.Release / $"Microsoft.Windows.Devices.Midi2.pri");
                sdkBinaries.Add(sdkOutputRootFolder / "Microsoft.Windows.Devices.Midi2" / sourcePlatform / Configuration.Release / $"Microsoft.Windows.Devices.Midi2.pdb");

                string stagingPlatform = sourcePlatform;
                if (sourcePlatform.ToLower() == "arm64ec")
                {
                    stagingPlatform = "arm64";
                }

                foreach (var file in sdkBinaries)
                {
                    file.CopyToDirectory(AppSdkStagingFolder / stagingPlatform, ExistsPolicy.FileOverwrite);
                }

                
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


            }

            // copy the NuGet package over to this release

            (AppSdkNugetOutputFolder / $"{NugetFullPackageIdWithVersion}.nupkg").CopyToDirectory(
                ThisReleaseFolder, 
                ExistsPolicy.FileOverwrite);


        });


    Target T_BuildAppSDKToolsAndTests => _ => _
        .DependsOn(T_BuildAndPackAllAppSDKs)
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
                    .SetProcessToolPath(MSBuildTasks.MSBuildPath)
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
                (sdkOutputRootFolder / "mididiag" / stagingPlatform / Configuration.Release / $"mididiag.exe").CopyToDirectory(AppSdkStagingFolder / stagingPlatform, ExistsPolicy.FileOverwrite);
                (sdkOutputRootFolder / "midiksinfo" / stagingPlatform / Configuration.Release / $"midiksinfo.exe").CopyToDirectory(AppSdkStagingFolder / stagingPlatform, ExistsPolicy.FileOverwrite);
                (sdkOutputRootFolder / "midimdnsinfo" / stagingPlatform / Configuration.Release / $"midimdnsinfo.exe").CopyToDirectory(AppSdkStagingFolder / stagingPlatform, ExistsPolicy.FileOverwrite);
                (sdkOutputRootFolder / "midi1monitor" / stagingPlatform / Configuration.Release / $"midi1monitor.exe").CopyToDirectory(AppSdkStagingFolder / stagingPlatform, ExistsPolicy.FileOverwrite);
                (sdkOutputRootFolder / "midi1enum" / stagingPlatform / Configuration.Release / $"midi1enum.exe").CopyToDirectory(AppSdkStagingFolder / stagingPlatform, ExistsPolicy.FileOverwrite);
                (sdkOutputRootFolder / "midifixreg" / stagingPlatform / Configuration.Release / $"midifixreg.exe").CopyToDirectory(AppSdkStagingFolder / stagingPlatform, ExistsPolicy.FileOverwrite);
                (sdkOutputRootFolder / "midicheckservice" / stagingPlatform / Configuration.Release / $"midicheckservice.exe").CopyToDirectory(AppSdkStagingFolder / stagingPlatform, ExistsPolicy.FileOverwrite);
            }
        });


    Target T_BuildAppSdkRuntimeAndToolsInstaller => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_CreateVersionIncludes)
        .DependsOn(T_BuildConsoleApp)
        .DependsOn(T_BuildSettingsApp)
        .DependsOn(T_BuildPowerShellProjection)
        .DependsOn(T_BuildAndPackAllAppSDKs)
        .DependsOn(T_BuildAppSDKToolsAndTests)
        .DependsOn(T_CopySharedDesignAssets)
        .DependsOn(T_CopyCollectMidiLogs)
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
                    .SetProcessToolPath(MSBuildTasks.MSBuildPath)
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
                (AppSdkSetupSolutionFolder / "main-bundle" / "bin" / platform / Configuration.Release / "WindowsMidiServicesSdkRuntimeSetup.exe")
                    .Copy(ThisReleaseFolder / newInstallerName);

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

    Target T_BuildUserToolsSharedComponents => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_BuildAndPackAllAppSDKs)
        .Executes(() =>
        {
            // build x64 and Arm64
        });


    Target T_BuildSettingsApp => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_BuildAndPackAllAppSDKs)
        .DependsOn(T_BuildUserToolsSharedComponents)
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

                var settingsOutputFolder = MidiSettingsSolutionFolder / "Microsoft.Midi.Settings" / "bin" / Configuration.Release / $"net10.0-windows{TargetWindowsSdkVersion}" / rid;
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

                paths.Add(settingsOutputFolder / "MidiSettings.pri");

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

                paths.Add(settingsOutputFolder / "Microsoft.Graphics.Imaging.Projection.dll");
                

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


                paths.Add(settingsOutputFolder / "App.xbf");
                paths.Add(settingsOutputFolder / "MainWindow.xbf");


                // TODO: This doesn't deal with any localization content

                // copy all the globbed files

                foreach (var f in paths)
                {
                    f.CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);
                }

                // Add Assets folder with app icon. This ends up special-cased
                List<AbsolutePath> assets = [];
                assets.AddRange(Globbing.GlobFiles(settingsOutputFolder / "Assets", "*.svg"));
                assets.AddRange(Globbing.GlobFiles(settingsOutputFolder / "Assets", "*.ico"));
                assets.AddRange(Globbing.GlobFiles(settingsOutputFolder / "Assets", "*.png"));

                foreach (var f in assets)
                {
                    f.CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);
                }

                // this is a really stupid way to do this, but just trying to fit into existing structure here
                // put on the backlog to make this more robust and less silly
                List<AbsolutePath> controlsXbf = [];
                controlsXbf.AddRange(Globbing.GlobFiles(settingsOutputFolder / "Controls" / "*.xbf"));

                List<AbsolutePath> stylesXbf = [];
                stylesXbf.AddRange(Globbing.GlobFiles(settingsOutputFolder / "Styles" / "*.xbf"));

                List<AbsolutePath> themesXbf = [];
                themesXbf.AddRange(Globbing.GlobFiles(settingsOutputFolder / "Themes" / "*.xbf"));

                List<AbsolutePath> viewsCorePagesXbf = [];
                viewsCorePagesXbf.AddRange(Globbing.GlobFiles(settingsOutputFolder / "Views" / "Core Pages" / "*.xbf"));

                List<AbsolutePath> viewsEndpointManagementPagesXbf = [];
                viewsEndpointManagementPagesXbf.AddRange(Globbing.GlobFiles(settingsOutputFolder / "Views" / "Endpoint Management Pages" / "*.xbf"));

                List<AbsolutePath> viewsToolsPagesXbf = [];
                viewsToolsPagesXbf.AddRange(Globbing.GlobFiles(settingsOutputFolder / "Views" / "Tools Pages" / "*.xbf"));

                List<AbsolutePath> viewsTransportSetupPagesXbf = [];
                viewsTransportSetupPagesXbf.AddRange(Globbing.GlobFiles(settingsOutputFolder / "Views" / "Transport Setup Pages" / "*.xbf"));

                foreach (var f in controlsXbf)
                {
                    f.CopyToDirectory(stagingFolder / "Controls", ExistsPolicy.FileOverwrite);
                }

                foreach (var f in stylesXbf)
                {
                    f.CopyToDirectory(stagingFolder / "Styles", ExistsPolicy.FileOverwrite);
                }

                foreach (var f in themesXbf)
                {
                    f.CopyToDirectory(stagingFolder / "Themes", ExistsPolicy.FileOverwrite);
                }

                foreach (var f in viewsCorePagesXbf)
                {
                    f.CopyToDirectory(stagingFolder / "Views" / "Core Pages", ExistsPolicy.FileOverwrite);
                }

                foreach (var f in viewsEndpointManagementPagesXbf)
                {
                    f.CopyToDirectory(stagingFolder / "Views" / "Endpoint Management Pages", ExistsPolicy.FileOverwrite);
                }

                foreach (var f in viewsToolsPagesXbf)
                {
                    f.CopyToDirectory(stagingFolder / "Views" / "Tools Pages", ExistsPolicy.FileOverwrite);
                }

                foreach (var f in viewsTransportSetupPagesXbf)
                {
                    f.CopyToDirectory(stagingFolder / "Views" / "Transport Setup Pages", ExistsPolicy.FileOverwrite);
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
                                writer.WriteLine($"      <File Source=\"$(StagingSourceRootFolder)\\midi-settings\\$(var.Platform)\\{f.Name}\" Vital=\"true\" PatchWholeFile=\"yes\"/> ");
                            }
                        }

                        writer.WriteLine("    </Component>");


                        // Controls XBF
                        writer.WriteLine("    <Component Id=\"ControlsXBF\" Bitness=\"always64\" Directory=\"SETTINGSAPP_CONTROLS_FOLDER\" Guid =\"11c285bc-ee74-46d0-bdf2-0031a584e8f8\">");
                        foreach (var f in controlsXbf)
                        {
                            writer.WriteLine($"      <File Source=\"$(StagingSourceRootFolder)\\midi-settings\\$(var.Platform)\\Controls\\{f.Name}\" Vital=\"true\" PatchWholeFile=\"yes\"/> ");
                        }
                        writer.WriteLine("    </Component>");

                        // Styles XBF
                        writer.WriteLine("    <Component Id=\"StylesXBF\" Bitness=\"always64\" Directory=\"SETTINGSAPP_STYLES_FOLDER\" Guid =\"50dfb652-ca6a-4c5d-827d-212d7904000e\">");
                        foreach (var f in stylesXbf)
                        {
                            writer.WriteLine($"      <File Source=\"$(StagingSourceRootFolder)\\midi-settings\\$(var.Platform)\\Styles\\{f.Name}\" Vital=\"true\" PatchWholeFile=\"yes\"/> ");
                        }
                        writer.WriteLine("    </Component>");

                        // Themes XBF
                        writer.WriteLine("    <Component Id=\"ThemesXBF\" Bitness=\"always64\" Directory=\"SETTINGSAPP_THEMES_FOLDER\" Guid =\"47f1c6b9-4dca-40ac-8fe8-57d02336ea6d\">");
                        foreach (var f in themesXbf)
                        {
                            writer.WriteLine($"      <File Source=\"$(StagingSourceRootFolder)\\midi-settings\\$(var.Platform)\\Themes\\{f.Name}\" Vital=\"true\" PatchWholeFile=\"yes\"/> ");
                        }
                        writer.WriteLine("    </Component>");

                        // Views\Core Pages XBF
                        writer.WriteLine("    <Component Id=\"ViewsCorePagesXBF\" Bitness=\"always64\" Directory=\"SETTINGSAPP_VIEWS_CORE_PAGES_FOLDER\" Guid =\"78e8fba4-5e7b-45c6-a353-302bb4cfc923\">");
                        foreach (var f in viewsCorePagesXbf)
                        {
                            writer.WriteLine($"      <File Source=\"$(StagingSourceRootFolder)\\midi-settings\\$(var.Platform)\\Views\\Core Pages\\{f.Name}\" Vital=\"true\" PatchWholeFile=\"yes\"/> ");
                        }
                        writer.WriteLine("    </Component>");

                        // Views\Endpoint Management Pages XBF
                        writer.WriteLine("    <Component Id=\"ViewsEndpointManagementPagesXBF\" Bitness=\"always64\" Directory=\"SETTINGSAPP_VIEWS_ENDPOINT_MANAGEMENT_PAGES_FOLDER\" Guid =\"ca35002b-b144-4d55-b28c-0a90685f9340\">");
                        foreach (var f in viewsEndpointManagementPagesXbf)
                        {
                            writer.WriteLine($"      <File Source=\"$(StagingSourceRootFolder)\\midi-settings\\$(var.Platform)\\Views\\Endpoint Management Pages\\{f.Name}\" Vital=\"true\" PatchWholeFile=\"yes\"/> ");
                        }
                        writer.WriteLine("    </Component>");

                        // Views\Tools Pages XBF
                        writer.WriteLine("    <Component Id=\"ViewsToolsPagesXBF\" Bitness=\"always64\" Directory=\"SETTINGSAPP_VIEWS_TOOLS_PAGES_FOLDER\" Guid =\"f83a3ae0-b76c-4437-a4d5-443d25c9ea65\">");
                        foreach (var f in viewsToolsPagesXbf)
                        {
                            writer.WriteLine($"      <File Source=\"$(StagingSourceRootFolder)\\midi-settings\\$(var.Platform)\\Views\\Tools Pages\\{f.Name}\" Vital=\"true\" PatchWholeFile=\"yes\"/> ");
                        }
                        writer.WriteLine("    </Component>");

                        // Views\Transport Setup Pages XBF
                        writer.WriteLine("    <Component Id=\"ViewsTransportSetupPagesXBF\" Bitness=\"always64\" Directory=\"SETTINGSAPP_VIEWS_TRANSPORT_SETUP_PAGES_FOLDER\" Guid =\"8ab2aeab-88b5-4075-816f-50a32ce18eef\">");
                        foreach (var f in viewsTransportSetupPagesXbf)
                        {
                            writer.WriteLine($"      <File Source=\"$(StagingSourceRootFolder)\\midi-settings\\$(var.Platform)\\Views\\Transport Setup Pages\\{f.Name}\" Vital=\"true\" PatchWholeFile=\"yes\"/> ");
                        }
                        writer.WriteLine("    </Component>");


                        writer.WriteLine("  </Fragment>");
                        writer.WriteLine("</Wix>");
                    }

                    wxsWritten = true;
                }
            }

        });



    Target T_CopySharedDesignAssets => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_BuildAndPackAllAppSDKs)
        .DependsOn(T_BuildUserToolsSharedComponents)
        .Executes(() =>
        {
            var designSourceFolder = RootDirectory / "design";

            var assetsStagingRoot = StagingRootFolder / "Assets";
            var transportAssetsStagingRoot = assetsStagingRoot / "Transports";
            var endpointAssetsStagingRoot = assetsStagingRoot / "Endpoints";

            (designSourceFolder / "APPPUB-small.svg").CopyToDirectory(transportAssetsStagingRoot, ExistsPolicy.FileOverwrite);
            (designSourceFolder / "BLE10-small.svg").CopyToDirectory(transportAssetsStagingRoot, ExistsPolicy.FileOverwrite);
            (designSourceFolder / "DIAG-small.svg").CopyToDirectory(transportAssetsStagingRoot, ExistsPolicy.FileOverwrite);
            (designSourceFolder / "KSA-small.svg").CopyToDirectory(transportAssetsStagingRoot, ExistsPolicy.FileOverwrite);
            (designSourceFolder / "KS-small.svg").CopyToDirectory(transportAssetsStagingRoot, ExistsPolicy.FileOverwrite);
            (designSourceFolder / "LOOP-small.svg").CopyToDirectory(transportAssetsStagingRoot, ExistsPolicy.FileOverwrite);
            (designSourceFolder / "NET2UDP-small.svg").CopyToDirectory(transportAssetsStagingRoot, ExistsPolicy.FileOverwrite);
            (designSourceFolder / "BLOOP-small.svg").CopyToDirectory(transportAssetsStagingRoot, ExistsPolicy.FileOverwrite);


            (designSourceFolder / "default-small.svg").CopyToDirectory(endpointAssetsStagingRoot, ExistsPolicy.FileOverwrite);
            (designSourceFolder / "default-apppub-small.svg").CopyToDirectory(endpointAssetsStagingRoot, ExistsPolicy.FileOverwrite);
            (designSourceFolder / "default-ble10-small.svg").CopyToDirectory(endpointAssetsStagingRoot, ExistsPolicy.FileOverwrite);
            (designSourceFolder / "default-diag-small.svg").CopyToDirectory(endpointAssetsStagingRoot, ExistsPolicy.FileOverwrite);
            (designSourceFolder / "default-ksa-small.svg").CopyToDirectory(endpointAssetsStagingRoot, ExistsPolicy.FileOverwrite);
            (designSourceFolder / "default-ks-small.svg").CopyToDirectory(endpointAssetsStagingRoot, ExistsPolicy.FileOverwrite);
            (designSourceFolder / "default-loop-small.svg").CopyToDirectory(endpointAssetsStagingRoot, ExistsPolicy.FileOverwrite);
            (designSourceFolder / "default-bloop-small.svg").CopyToDirectory(endpointAssetsStagingRoot, ExistsPolicy.FileOverwrite);
            (designSourceFolder / "default-net2udp-small.svg").CopyToDirectory(endpointAssetsStagingRoot, ExistsPolicy.FileOverwrite);

        });




    Target T_BuildConsoleApp => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_BuildAndPackAllAppSDKs)
        .DependsOn(T_BuildUserToolsSharedComponents)
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

                var consoleOutputFolder = MidiConsoleSolutionFolder / "Midi" / "bin" / Configuration.Release / $"net10.0-windows{TargetWindowsSdkVersion}" / rid ;
                //var runtimesFolder = consoleOutputFolder / "runtimes" / rid / "native";
                var runtimesFolder = consoleOutputFolder;

                var stagingFolder = MidiConsoleStagingFolder / platform;

                (consoleOutputFolder / "midi.exe").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);
                (consoleOutputFolder / "midi.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);
                (consoleOutputFolder / "midi.deps.json").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);
                (consoleOutputFolder / "midi.runtimeconfig.json").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);
                //(consoleOutputFolder / "midi.exe.manifest", stagingFolder, ExistsPolicy.FileOverwrite);
                (consoleOutputFolder / "Microsoft.Devices.Midi2.Tools.Shared.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);

                (consoleOutputFolder / "WinRT.Runtime.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);

                (consoleOutputFolder / "Microsoft.Windows.Devices.Midi2.NetProjection.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);
                (consoleOutputFolder / "Microsoft.Windows.SDK.NET.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);

                (consoleOutputFolder / "Spectre.Console.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);
                (consoleOutputFolder / "Spectre.Console.Cli.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);

                (consoleOutputFolder / "System.CodeDom.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);
                (consoleOutputFolder / "System.Diagnostics.EventLog.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);
                (consoleOutputFolder / "System.Diagnostics.EventLog.Messages.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);
                (consoleOutputFolder / "System.Management.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);
                (consoleOutputFolder / "System.ServiceProcess.ServiceController.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite, createDirectories: true);


            //    (runtimesFolder / "Microsoft.Windows.Devices.Midi2.Initialization.dll", stagingFolder, ExistsPolicy.FileOverwrite);
            //    (runtimesFolder / "Microsoft.Windows.Devices.Midi2.Initialization.pri", stagingFolder, ExistsPolicy.FileOverwrite);
                //(runtimesFolder / ns + ".winmd", stagingFolder, ExistsPolicy.FileOverwrite);

            }

        });


    Target T_BuildPowerShellProjection => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_BuildAndPackAllAppSDKs)
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

            var psOutputFolder = MidiPowerShellSolutionFolder / "bin" / Configuration.Release / $"net10.0-windows{TargetWindowsSdkVersion}" / rid;
            //var runtimesFolder = consoleOutputFolder / "runtimes" / rid / "native";
            var runtimesFolder = psOutputFolder;

            var stagingFolder = MidiPowerShellStagingFolder / platform;

            (MidiPowerShellSolutionFolder / "WindowsMidiServices.psd1").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);

            (psOutputFolder / "WindowsMidiServices.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);
            (psOutputFolder / "WindowsMidiServices.deps.json").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);

            (psOutputFolder / "WinRT.Runtime.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);

            (psOutputFolder / "Microsoft.Windows.Devices.Midi2.NetProjection.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);
            (psOutputFolder / "Microsoft.Windows.SDK.NET.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);

            (psOutputFolder / "Microsoft.ApplicationInsights.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);
            (psOutputFolder / "Microsoft.Win32.Registry.AccessControl.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);
            (psOutputFolder / "Newtonsoft.Json.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);
            (psOutputFolder / "System.CodeDom.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);
            (psOutputFolder / "System.Configuration.ConfigurationManager.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);
            (psOutputFolder / "System.Diagnostics.EventLog.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);
            (psOutputFolder / "System.DirectoryServices.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);
            (psOutputFolder / "System.Management.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);
            (psOutputFolder / "System.Security.Cryptography.Pkcs.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);
            (psOutputFolder / "System.Security.Cryptography.ProtectedData.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);
            (psOutputFolder / "System.Security.Permissions.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);
            (psOutputFolder / "System.Windows.Extensions.dll").CopyToDirectory(stagingFolder, ExistsPolicy.FileOverwrite);


            //    (runtimesFolder / "Microsoft.Windows.Devices.Midi2.Initialization.dll", stagingFolder, ExistsPolicy.FileOverwrite);
            //    (runtimesFolder / "Microsoft.Windows.Devices.Midi2.Initialization.pri", stagingFolder, ExistsPolicy.FileOverwrite);
            //(runtimesFolder / ns + ".winmd", stagingFolder, ExistsPolicy.FileOverwrite);

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


    Target T_BuildCppSamples => _ => _
        .DependsOn(T_BuildAndPackAllAppSDKs)
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
                    .SetProcessToolPath(MSBuildTasks.MSBuildPath)
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


    Target T_BuildCSharpSamples => _ => _
        .DependsOn(T_BuildAndPackAllAppSDKs)
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


    Target T_BuildAndPackageElectronProjection => _ => _
        .DependsOn(T_BuildAndPackAllAppSDKs)
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
      //          (
      //              sdkFile,
      //              platformOutputRootFolder,
      //              FileExistsPolicy.Overwrite,
      //              true);
      //      }


      //      // main windows metadata file
      //      //(
      //      //    @"C:\Program Files (x86)\Windows Kits\10\UnionMetadata\10.0.20348.0\Windows.winmd",
      //      //    platformOutputRootFolder,
      //      //    ExistsPolicy.FileOverwrite);

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

      //          //(
      //          //    sourceBinFolder / "binding.node",
      //          //    nsReleaseRootFolder / "build" / "Release",
      //          //    ExistsPolicy.FileOverwrite);

      //          //// we also want the three files in the lib folder

      //          //var sourceLibFolder = nsFolder / "lib";

      //          //var libFiles = sourceLibFolder.GlobFiles("*.js", "*.ts");

      //          //foreach (var libFile in libFiles)
      //          //{
      //          //    //Console.WriteLine($"Copying Projection File: {libFile}");

      //          //    (
      //          //        libFile,
      //          //        nsReleaseRootFolder / "lib",
      //          //        ExistsPolicy.FileOverwrite);
      //          //}

      //          //(
      //          //    nsFolder / "package.json",
      //          //    nsReleaseRootFolder,
      //          //    ExistsPolicy.FileOverwrite);


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


    Target T_CopyCollectMidiLogs => _ => _
        .DependsOn(T_Prerequisites)
        .Executes(() =>
        {
            var sourceFolder = ApiSolutionFolder / "CollectMidiLogs";

            var stagingRoot = StagingRootFolder / "CollectMidiLogs";

            (sourceFolder / "CollectMidiLogs.cmd").CopyToDirectory(stagingRoot, ExistsPolicy.FileOverwrite);
            (sourceFolder / "CollectMidiLogs.ps1").CopyToDirectory(stagingRoot, ExistsPolicy.FileOverwrite);
            (sourceFolder / "providers.wprp").CopyToDirectory(stagingRoot, ExistsPolicy.FileOverwrite);
            (sourceFolder / "tttraceall.psm1").CopyToDirectory(stagingRoot, ExistsPolicy.FileOverwrite);
        });




    Target T_ZipSamples => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_CreateVersionIncludes)
        .DependsOn(T_BuildAndPackAllAppSDKs)
        .DependsOn(T_BuildCSharpSamples)
        .DependsOn(T_BuildCppSamples)
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


    Target T_ZipPowershellDevUtilities => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_CreateVersionIncludes)
        .Executes(() =>
        {
            var regHelpersFolder = RootDirectory / "src" / "dev-tools" / "reg-helpers";

            regHelpersFolder.ZipTo(ThisReleaseFolder / $"dev-pre-setup-scripts.zip");
        });

    

    Target BuildAndPublishAll => _ => _
        .DependsOn(T_Prerequisites)
        .DependsOn(T_CreateVersionIncludes)
        .DependsOn(T_BuildAndPackAllAppSDKs)
        .DependsOn(T_BuildConsoleApp)
        .DependsOn(T_BuildSettingsApp)
        .DependsOn(T_BuildAppSdkRuntimeAndToolsInstaller)
      //  .DependsOn(BuildAndPackageElectronProjection)
        .DependsOn(T_BuildCppSamples)
        .DependsOn(T_BuildCSharpSamples)
        .DependsOn(T_ZipPowershellDevUtilities)
        .DependsOn(T_ZipSamples)
        .DependsOn(T_CopyCollectMidiLogs)
        .Executes(() =>
        {

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
