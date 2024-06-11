using System;
using System.Collections.Generic;
using System.Linq;
using Microsoft.ApplicationInsights.Extensibility;
using Nuke.Common;
using Nuke.Common.CI;
using Nuke.Common.Execution;
using Nuke.Common.IO;
using Nuke.Common.ProjectModel;
using Nuke.Common.Tooling;
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


    // directories

    AbsolutePath BuildRootFolder => NukeBuild.RootDirectory / "build";


    AbsolutePath StagingRootFolder => BuildRootFolder / "staging";
    AbsolutePath AppSdkStagingFolder => StagingRootFolder / "app-sdk";
    
    

    AbsolutePath ReleaseRootFolder => BuildRootFolder / "release";
    AbsolutePath AppSdkNugetReleaseFolder => BuildRootFolder / "app-sdk-nuget";

    AbsolutePath AppSdkImplementationInstallerReleaseFolder => BuildRootFolder / "app-sdk-impl";

    AbsolutePath SourceRootFolder => NukeBuild.RootDirectory / "src";
    AbsolutePath AppSdkSolutionFolder => SourceRootFolder / "app-sdk";
    AbsolutePath ApiSolutionFolder => SourceRootFolder / "api";


    AbsolutePath ApiReferenceFolder => SourceRootFolder / "shared" / "api-ref";



    AbsolutePath UserToolsRootFolder => SourceRootFolder / "user-tools";

    AbsolutePath MidiConsoleSolutionFolder => UserToolsRootFolder / "midi-console";
    AbsolutePath MidiConsoleStagingFolder => StagingRootFolder / "midi-console";


    AbsolutePath MidiSettingsSolutionFolder => UserToolsRootFolder / "midi-settings";
    AbsolutePath MidiSettingsStagingFolder => StagingRootFolder / "midi-settings";

    AbsolutePath SamplesRootFolder => NukeBuild.RootDirectory / "samples";
    AbsolutePath CppWinRTSamplesRootFolder => SamplesRootFolder / "cpp-winrt";
    AbsolutePath CSWinRTSamplesRootFolder => SamplesRootFolder / "csharp-net";
    //    AbsolutePath RustWinRTSamplesRootFolder => SamplesRootFolder / "rust-winrt";
    //    AbsolutePath ElectronJSSamplesRootFolder => SamplesRootFolder / "electron-js";


    string[] AllPlatforms => new string[] { "Arm64", "Arm64EC", "x64" };
    string[] ServicePlatforms => new string[] { "Arm64", "x64" };

    public static int Main () => Execute<Build>(x => x.BuildAndPublishAll);



    //[Parameter("Configuration to build - Default is 'Debug' (local) or 'Release' (server)")]
    //readonly Configuration Configuration = IsLocalBuild ? Configuration.Debug : Configuration.Release;

    //void BuildSdkProject(AbsolutePath projectRoot, string projectFileName, string platform, string config)
    //{
    //    var stagingFolder = AppSdkStagingFolder / platform / config;

    //    var msbuildProperties = new Dictionary<string, object>();
    //    msbuildProperties.Add("Platform", platform);
    //    msbuildProperties.Add("SolutionDir", AppSdkSolutionFolder.ToString() + @"\");  // to include trailing slash;

    //    MSBuildTasks.MSBuild(_ => _
    //        .SetTargetPath(projectRoot / projectFileName)
    //        /*.SetOutDir(AppSdkSolutionFolder) */
    //        /*.SetProcessWorkingDirectory(projectRoot) */
    //        /*.SetTargets("Build") */
    //        .SetProperties(msbuildProperties)
    //        .SetConfiguration(config)
    //        .SetVerbosity(MSBuildVerbosity.Minimal)
    //        .EnableNodeReuse()
    //    );

    //    AbsolutePath buildArtifactsFolder = projectRoot / "bin" / platform / config;

    //    var buildArtifacts = buildArtifactsFolder.GlobFiles("*.winmd", "*.dll", "*.pri");

    //    foreach (var file in buildArtifacts)
    //    {
    //        FileSystemTasks.CopyFileToDirectory(file, stagingFolder, FileExistsPolicy.Overwrite, true);
    //    }
    //}



    //void BuildSdkProjectAllPlatforms(AbsolutePath projectRoot, string projectFileName)
    //{
    //    foreach (var platform in AllPlatforms)
    //    {
    //        BuildSdkProject(projectRoot, projectFileName, platform, Configuration.Release);
    //    }
    //}







    Target Clean => _ => _
        .Before(Restore)
        .Executes(() =>
        {
        });

    Target Restore => _ => _
        .Executes(() =>
        {
        });





    Target BuildServiceAndPlugins => _ => _
        .DependsOn(Restore)
        .Executes(() =>
    {
        foreach (var platform in ServicePlatforms)
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

        }

        // Copy reference files to reference folder. This requires
        // the SDK projects reference those instead of the current version
        // which references deep into the API project

        // copy x64 files to Arm64EC folder as well. No reason for the full
        // service and plugins to be Arm64EC just to provide a few headers



        var intermediateFolder = ApiSolutionFolder / "vsfiles" / "intermediate";

        foreach (var platform in AllPlatforms)
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
        .DependsOn(BuildServiceAndPlugins)
        .Executes(() =>
        {
            foreach (var platform in AllPlatforms)
            {
                string solutionDir = AppSdkSolutionFolder.ToString() + @"\";

                var msbuildProperties = new Dictionary<string, object>();
                msbuildProperties.Add("Platform", platform);
                msbuildProperties.Add("SolutionDir", solutionDir);      // to include trailing slash
                //msbuildProperties.Add("NoWarn", "MIDL2111");            // IDL identifier length warning

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
                    .SetVerbosity(MSBuildVerbosity.Minimal)
                    .EnableNodeReuse()
                );

            }

            var sdkOutputRootFolder = AppSdkSolutionFolder / "vsfiles";

            foreach (var platform in AllPlatforms)
            {
                var sdkBinaries = new List<AbsolutePath>();

                foreach (var ns in new string[] {
                    "Microsoft.Windows.Devices.Midi2",
                    "Microsoft.Windows.Devices.Midi2.Service",
                    "Microsoft.Windows.Devices.Midi2.CapabilityInquiry",
                    "Microsoft.Windows.Devices.Midi2.ClientPlugins",
                    "Microsoft.Windows.Devices.Midi2.Diagnostics",
                    "Microsoft.Windows.Devices.Midi2.Messages",
                    "Microsoft.Windows.Devices.Midi2.Endpoints.Loopback",
                    "Microsoft.Windows.Devices.Midi2.Endpoints.Virtual"
                })
                {
                    sdkBinaries.Add(sdkOutputRootFolder / ns / platform / Configuration.Release / $"{ns}.winmd");
                    sdkBinaries.Add(sdkOutputRootFolder / ns / platform / Configuration.Release / $"{ns}.dll");
                    sdkBinaries.Add(sdkOutputRootFolder / ns / platform / Configuration.Release / $"{ns}.pri");

                    // todo: CS projection dll
                }

                sdkBinaries.Add(sdkOutputRootFolder / "Microsoft.Windows.Devices.Midi2.CapabilityInquiry" / platform / Configuration.Release / "Microsoft.Windows.Devices.Midi2.CapabilityInquiry.winmd");
                sdkBinaries.Add(sdkOutputRootFolder / "Microsoft.Windows.Devices.Midi2.CapabilityInquiry" / platform / Configuration.Release / "Microsoft.Windows.Devices.Midi2.CapabilityInquiry.dll");

                // copy the files over to the reference location
                foreach (var file in sdkBinaries)
                {
                    FileSystemTasks.CopyFileToDirectory(file, AppSdkStagingFolder / platform, FileExistsPolicy.Overwrite, true);
                }


                // build mididiag

                // copy over mididiag
            }
        });


    Target BuildSettingsApp => _ => _
        .DependsOn(BuildAndPackAllAppSDKs)
        .Executes(() =>
        {
            // update nuget packages

            // build x64 and Arm64
        });

    Target BuildConsoleApp => _ => _
        .DependsOn(BuildAndPackAllAppSDKs)
        .Executes(() =>
        {
            // update nuget packages

            // build x64 and Arm64

        });

    Target BuildSamples => _ => _
        .DependsOn(BuildAndPackAllAppSDKs)
        .Executes(() =>
        {
            // update nuget packages

            // make sure they compile

        });



    Target BuildAndPublishAll => _ => _
        .DependsOn(BuildAndPackAllAppSDKs)
        .DependsOn(BuildSettingsApp)
        .DependsOn(BuildConsoleApp)
        .DependsOn(BuildSamples)
        .Executes(() =>
        {

        });


}
