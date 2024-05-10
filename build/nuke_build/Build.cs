using System;
using System.Linq;
using Nuke.Common;
using Nuke.Common.CI;
using Nuke.Common.Execution;
using Nuke.Common.IO;
using Nuke.Common.ProjectModel;
using Nuke.Common.Tooling;
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

    const string RepoRootFolder = @"G:\GitHub\microsoft\midi";  // todo change to %midi_repo_root%
    const string BuildRootFolder = RepoRootFolder + @"\build";
    
    const string StagingRootFolder = BuildRootFolder + @"\staging";
    const string MidiConsoleStagingFolder = StagingRootFolder + @"\midi-console";
    const string MidiSettingsStagingFolder = StagingRootFolder + @"\midi-settings";
    const string MidiSdkStagingFolder = StagingRootFolder + @"\sdk";

    const string ReleaseRootFolder = BuildRootFolder + @"\release";
    const string AppSdkNugetReleaseFolder = BuildRootFolder + @"\nuget";
    const string AppSdkInstallerReleaseFolder = BuildRootFolder + @"\app-sdk";

    const string SourceRootFolder = RepoRootFolder + @"\src";
    const string AppSdkSolutionFolder = SourceRootFolder + @"\app-sdk";

    const string UserToolsRootFolder = SourceRootFolder + @"\user-tools";
    const string MidiConsoleSolutionFolder = UserToolsRootFolder + @"\midi-console";
    const string MidiSettingsSolutionFolder = UserToolsRootFolder + @"\midi-settings";


    public static int Main () => Execute<Build>(x => x.BuildAndPublishAll);

    [Parameter("Configuration to build - Default is 'Debug' (local) or 'Release' (server)")]
    readonly Configuration Configuration = IsLocalBuild ? Configuration.Debug : Configuration.Release;

    Target Clean => _ => _
        .Before(Restore)
        .Executes(() =>
        {
        });

    Target Restore => _ => _
        .Executes(() =>
        {
        });





    Target BuildSDK => _ => _
        .DependsOn(Restore)
        .Executes(() =>
        {

            

        });


    Target BuildSettingsApp => _ => _
        .DependsOn(BuildSDK)
        .Executes(() =>
        {

        });

    Target BuildConsoleApp => _ => _
        .DependsOn(BuildSDK)
        .Executes(() =>
        {





        });



    Target BuildSamples => _ => _
        .DependsOn(BuildSDK)
        .Executes(() =>
        {

        });



    Target BuildAndPublishAll => _ => _
        .DependsOn(BuildSDK)
        .DependsOn(BuildSettingsApp)
        .DependsOn(BuildConsoleApp)
        .DependsOn(BuildSamples)
        .Executes(() =>
        {

        });


}
