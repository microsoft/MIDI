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
    
    const UInt16 BuildVersionMajor = 0;
    const UInt16 BuildVersionMinor = 0;
    const UInt16 BuildVersionPatch = 15;

    const UInt16 BuildVersionPreviewNumber = 15;
    string VersionName => "Plugin Preview " + BuildVersionPreviewNumber;

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

                foreach (var file in stagingFiles)
                {
                    file.CopyToDirectory(ApiStagingFolder / servicePlatform, ExistsPolicy.FileOverwrite);
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
                .SetProcessWorkingDirectory(solutionDir)
                .SetSource(@"https://api.nuget.org/v3/index.json")
                .SetSolutionDirectory(solutionDir)
            //.SetConfigFile(packagesConfigFullPath)
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
                    .SetProcessWorkingDirectory(solutionDir)
                    .SetSource(@"https://api.nuget.org/v3/index.json")
                    .SetSolutionDirectory(solutionDir)
                //.SetConfigFile(packagesConfigFullPath)
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
            
        });


}
