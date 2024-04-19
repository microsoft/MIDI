#tool nuget:?package=NuGet.CommandLine&version=5.10
//#addin nuget:?package=Cake.Compression&version=0.3.0

// ===========================================================================================
string setupVersionName = "Developer Preview 6";

// we set these here, especially the time, so it's the same for all platforms in the single build
string setupBuildMajorMinor = "1.0";
string setupBuildDateNumber = DateTime.Now.ToString("yy") + DateTime.Now.DayOfYear.ToString("000");       // YYddd where ddd is the day number for the year
string setupBuildTimeNumber = DateTime.Now.ToString("HHmm");       // HHmm
// ===========================================================================================




var target = Argument("target", "Default");


var ridX64 = "win10-x64";
var ridArm64 = "win10-arm64";
var frameworkVersion = "net8.0-windows10.0.20348.0";


var platformTargets = new[]{PlatformTarget.x64, PlatformTarget.ARM64};
//var platformTargets = new[]{PlatformTarget.x64};



// we don't actually support debug builds for this. 
var configuration = "Release";

var repoRootDir = MakeAbsolute(Directory("../")).ToString();

var srcDir = System.IO.Path.Combine(repoRootDir, "src");
var buildRootDir = System.IO.Path.Combine(repoRootDir, "build");

var stagingRootDir = System.IO.Path.Combine(buildRootDir, "staging");
var releaseRootDir = System.IO.Path.Combine(buildRootDir, "release");

var stagingX64Dir = System.IO.Path.Combine(stagingRootDir, ridX64);
var stagingArm64Dir = System.IO.Path.Combine(stagingRootDir, ridArm64);

string stagingDir;

//////////////////////////////////////////////////////////////////////
// PROJECT LOCATIONS
//////////////////////////////////////////////////////////////////////
///

var registrySettingsStagingDir = System.IO.Path.Combine(stagingRootDir, "reg");

var apiAndServiceSolutionDir = System.IO.Path.Combine(srcDir, "api");
var apiAndServiceSolutionFile = System.IO.Path.Combine(apiAndServiceSolutionDir, "Midi2.sln");
var apiAndServiceStagingDir = System.IO.Path.Combine(stagingRootDir, "api");


var sdkSolutionDir = System.IO.Path.Combine(srcDir, "app-dev-sdk");
var sdkSolutionFile = System.IO.Path.Combine(sdkSolutionDir, "midi-sdk.sln");
var sdkStagingDir = System.IO.Path.Combine(stagingRootDir, "app-dev-sdk");

var vsFilesDir = System.IO.Path.Combine(apiAndServiceSolutionDir, "VSFiles");

var consoleAppSolutionDir = System.IO.Path.Combine(srcDir, "user-tools", "midi-console");
var consoleAppSolutionFile = System.IO.Path.Combine(consoleAppSolutionDir, "midi-console.sln");
var consoleAppProjectDir = System.IO.Path.Combine(consoleAppSolutionDir, "Midi");
var consoleAppProjectFile = System.IO.Path.Combine(consoleAppProjectDir, "Midi.csproj");
var consoleAppStagingDir = System.IO.Path.Combine(stagingRootDir, "midi-console");

var settingsAppSolutionDir = System.IO.Path.Combine(srcDir, "user-tools", "midi-settings");
var settingsAppSolutionFile = System.IO.Path.Combine(settingsAppSolutionDir, "midi-settings.sln");

var settingsAppProjectDir = System.IO.Path.Combine(settingsAppSolutionDir, "Microsoft.Midi.Settings");
var settingsAppProjectFile = System.IO.Path.Combine(settingsAppProjectDir, "Microsoft.Midi.Settings.csproj");

var settingsAppStagingDir = System.IO.Path.Combine(stagingRootDir, "midi-settings");


var setupSolutionDir = System.IO.Path.Combine(srcDir, "oob-setup");
var setupSolutionFile = System.IO.Path.Combine(setupSolutionDir, "midi-services-setup.sln");
var setupBundleInfoIncludeFile = System.IO.Path.Combine(stagingRootDir, "version", "BundleInfo.wxi");


var setupReleaseDir = releaseRootDir;

var apiReleaseArtifactsFolder = System.IO.Path.Combine(releaseRootDir, "api");


// we always want the latest redist. These are permalinks.
var vcRedistArm64URL = new Uri("https://aka.ms/vs/17/release/vc_redist.arm64.exe");
var vcRedistX64URL = new Uri("https://aka.ms/vs/17/release/vc_redist.x64.exe");

var vcRedistDir = "";

var allowPreviewVersionOfBuildTools = true;

//////////////////////////////////////////////////////////////////////
// TASKS
//////////////////////////////////////////////////////////////////////

Task("Clean")
    .WithCriteria(c => HasArgument("rebuild"))
    .Does(() =>
{
    CleanDirectory(stagingRootDir);
    CleanDirectory(vsFilesDir);

    //CleanDirectory(buildX64Dir);
    //CleanDirectory(buildArm64Dir);
});


Task("VerifyDependencies")
    .Does(() =>
{
    // Grab latest VC runtime

    // https://aka.ms/vs/17/release/vc_redist.x64.exe


    // Grab lates t.NET runtime if the file isn't there. It will have a long name with a 
    // version number, like dotnet-runtime-8.0.0-rc.1.23419.4-win-x64 so you'llneed to 
    // rename that for the installer
    // https://aka.ms/dotnet/8.0/preview/windowsdesktop-runtime-win-x64.exe



});

Task("SetupEnvironment")
    .Does(()=>
{
    // TODO: Need to verify that %MIDI_REPO_ROOT% is set. If not, set it to the root \midi folder
    var rootVariableExists = !string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("MIDI_REPO_ROOT"));

    if (!rootVariableExists)
    {
        // this will only work if this folder is located in \midi\build or some other subfolder of the root \midi folder
        Environment.SetEnvironmentVariable("MIDI_REPO_ROOT", System.IO.Path.GetFullPath("..\\"));
    }
});


 Task("BuildServiceAndAPI")
    .DoesForEach(platformTargets, plat =>
{
    Information("\nBuilding service and API for " + plat.ToString());

    string abstractionRootDir = System.IO.Path.Combine(apiAndServiceSolutionDir, "Abstraction");

    // the projects, as configured for internal Razzle build, rely on this folder

    string workingDirectory = string.Empty;

    var buildSettings = new MSBuildSettings
    {
        MaxCpuCount = 0,
        Configuration = configuration,
        AllowPreviewVersion = allowPreviewVersionOfBuildTools,
        PlatformTarget = plat,
        Verbosity = Verbosity.Minimal,
    };

    MSBuild(System.IO.Path.Combine(apiAndServiceSolutionDir, "Midi2.sln"), buildSettings);

    // Copy key output files from VSFiles to staging to allow building installer
   
    var outputDir = System.IO.Path.Combine(apiAndServiceSolutionDir, "VSFiles", plat.ToString(), configuration);
    var generatedFilesDir = System.IO.Path.Combine(apiAndServiceSolutionDir, "VSFiles", "intermediate", "Windows.Devices.Midi2", plat.ToString(), configuration, "GeneratedFiles", "winrt");
    //var apiHeaderDir = System.IO.Path.Combine(apiAndServiceSolutionDir, "VSFiles\\intermediate\\Windows.Devices.Midi2",  plat.ToString(), configuration, "GeneratedFiles\\winrt");

    Information("\nCopying service and API for " + plat.ToString());

    var copyToDir = System.IO.Path.Combine(apiAndServiceStagingDir, plat.ToString());
    
    if (!DirectoryExists(copyToDir))
        CreateDirectory(copyToDir);


    // copy all the DLLs
    CopyFiles(System.IO.Path.Combine(outputDir, "*.dll"), copyToDir); 

    CopyFiles(System.IO.Path.Combine(outputDir, "*.pri"), copyToDir); 
    CopyFiles(System.IO.Path.Combine(outputDir, "*.pdb"), copyToDir); 

    CopyFiles(System.IO.Path.Combine(outputDir, "MidiSrv.exe"), copyToDir); 
    CopyFiles(System.IO.Path.Combine(outputDir, "mididmp.exe"), copyToDir); 

    CopyFiles(System.IO.Path.Combine(outputDir, "Windows.Devices.Midi2.winmd"), copyToDir); 

    CopyFiles(System.IO.Path.Combine(outputDir, "WinRTActivationEntries.txt"), copyToDir); 

    // copy the C++ header for the API
    CopyFiles(System.IO.Path.Combine(generatedFilesDir, "Windows.Devices.Midi2.h"), copyToDir); 




    // copy the API Header and the .winmd to the "API bare" folder

    var apiBareCopyToDir = System.IO.Path.Combine(releaseRootDir, "api");
    
    if (!DirectoryExists(apiBareCopyToDir))
        CreateDirectory(apiBareCopyToDir);

    if (!DirectoryExists(System.IO.Path.Combine(apiBareCopyToDir, "winrt")))
        CreateDirectory(System.IO.Path.Combine(apiBareCopyToDir, "winrt"));

    if (!DirectoryExists(System.IO.Path.Combine(apiBareCopyToDir, "winrt", "impl")))
        CreateDirectory(System.IO.Path.Combine(apiBareCopyToDir, "winrt", "impl"));


    CopyFiles(System.IO.Path.Combine(copyToDir, "Windows.Devices.Midi2.h"), apiBareCopyToDir); 

    CopyFiles(System.IO.Path.Combine(generatedFilesDir, "base.h"), System.IO.Path.Combine(apiBareCopyToDir, "winrt/")); 
    CopyFiles(System.IO.Path.Combine(generatedFilesDir, "Windows.Devices.h"), System.IO.Path.Combine(apiBareCopyToDir, "winrt/")); 
    CopyFiles(System.IO.Path.Combine(generatedFilesDir, "impl/Windows.Devices.Enumeration.2.h"), System.IO.Path.Combine(apiBareCopyToDir, "winrt/impl/")); 
    CopyFiles(System.IO.Path.Combine(generatedFilesDir, "impl/Windows.Devices.Midi.2.h"), System.IO.Path.Combine(apiBareCopyToDir, "winrt/impl/")); 
    CopyFiles(System.IO.Path.Combine(generatedFilesDir, "impl/Windows.Foundation.2.h"), System.IO.Path.Combine(apiBareCopyToDir, "winrt/impl/")); 
    CopyFiles(System.IO.Path.Combine(generatedFilesDir, "impl/Windows.Foundation.Collections.2.h"), System.IO.Path.Combine(apiBareCopyToDir, "winrt/impl/")); 
    CopyFiles(System.IO.Path.Combine(generatedFilesDir, "impl/Windows.Devices.Midi2.2.h"), System.IO.Path.Combine(apiBareCopyToDir, "winrt/impl/")); 

    CopyFiles(System.IO.Path.Combine(copyToDir, "Windows.Devices.Midi2.dll"), apiBareCopyToDir); 
    CopyFiles(System.IO.Path.Combine(copyToDir, "Windows.Devices.Midi2.winmd"), apiBareCopyToDir); 
    CopyFiles(System.IO.Path.Combine(copyToDir, "Windows.Devices.Midi2.pri"), apiBareCopyToDir); 


});





Task("BuildApiActivationRegEntriesCSharp")
    .IsDependentOn("BuildServiceAndAPI")
    .Does(() =>
{
    Information("\nBuilding WinRT Activation Entries ");

    var sourceFileName = System.IO.Path.Combine(apiAndServiceStagingDir, "x64", "WinRTActivationEntries.txt");
    var csDestinationFileName = System.IO.Path.Combine(registrySettingsStagingDir, "WinRTActivationEntries.cs");

//    const string parentHKLMRegKey = "SOFTWARE\\Microsoft\\WindowsRuntime\\ActivatableClassId\\";
//    const string wixWinrtLibFileName = "[API_INSTALLFOLDER]Windows.Devices.Midi2.dll";

    if (!System.IO.File.Exists(sourceFileName))
        throw new ArgumentException("Missing WinRT Activation entries file " + sourceFileName);

    using (StreamReader reader = System.IO.File.OpenText(sourceFileName))
    {

        if (!DirectoryExists(registrySettingsStagingDir))
            CreateDirectory(registrySettingsStagingDir);

        using (StreamWriter csWriter = System.IO.File.CreateText(csDestinationFileName))            
        {
            csWriter.WriteLine("// This file was generated by the build process");
            csWriter.WriteLine("//");
            csWriter.WriteLine("using RegistryCustomActions;");
            csWriter.WriteLine("class RegistryEntries");
            csWriter.WriteLine("{");
            csWriter.WriteLine("    public RegEntry[] ActivationEntries = new RegEntry[]");
            csWriter.WriteLine("    {");

            string line;

            while ((line = reader.ReadLine()) != null)
            {
                string trimmedLine = line.Trim();

                if (string.IsNullOrWhiteSpace(trimmedLine) || trimmedLine.StartsWith("#"))
                {
                    // comment or empty line
                    continue;
                }

                // pipe-delimited lines
                var elements = trimmedLine.Split('|');

                if (elements.Count() != 4)
                    throw new ArgumentException("Bad line:  " + trimmedLine); 

                // entries in order are
                // ClassName | ActivationType | Threading | TrustLevel

                string className = elements[0].Trim();
                string activationType = elements[1].Trim();
                string threading = elements[2].Trim();
                string trustLevel = elements[3].Trim();

                csWriter.WriteLine($"        new RegEntry{{ ClassName=\"{className}\", ActivationType={activationType}, Threading={threading}, TrustLevel={trustLevel}  }},");


            }

            csWriter.WriteLine("    };");
            csWriter.WriteLine("}");


        }
    }

});


Task("BuildApiActivationRegEntriesInternal")
    .IsDependentOn("BuildServiceAndAPI")
    .Does(() =>
{
    Information("\nBuilding WinRT Internaml XML Activation Entries");

    // read the file of dependencies
    var sourceFileName = System.IO.Path.Combine(apiAndServiceStagingDir, "x64", "WinRTActivationEntries.txt");
    var destinationFileName = System.IO.Path.Combine(registrySettingsStagingDir, "WinRTActivationEntries.xml");

    if (!System.IO.File.Exists(sourceFileName))
        throw new ArgumentException("Missing WinRT Activation entries file " + sourceFileName);

    using (StreamReader reader = System.IO.File.OpenText(sourceFileName))
    {

        if (!DirectoryExists(registrySettingsStagingDir))
            CreateDirectory(registrySettingsStagingDir);

        using (StreamWriter writer = System.IO.File.CreateText(destinationFileName))            
        {
            string line;

            while ((line = reader.ReadLine()) != null)
            {
                string trimmedLine = line.Trim();

                if (string.IsNullOrWhiteSpace(trimmedLine) || trimmedLine.StartsWith("#"))
                {
                    // comment or empty line
                    continue;
                }

                // pipe-delimited lines
                var elements = trimmedLine.Split('|');

                if (elements.Count() != 4)
                    throw new ArgumentException("Bad line:  " + trimmedLine); 

                // entries in order are
                // ClassName | ActivationType | Threading | TrustLevel

                string className = elements[0].Trim();
                var activationType = int.Parse(elements[1].Trim());
                var threading = int.Parse(elements[2].Trim());
                var trustLevel = int.Parse(elements[3].Trim());

                // TODO: Need to read the values from the file. These are hard-coded right now.
                string threadingEnum = "Both";  // STA / MTA / Both
                string trustLevelEnum = "Base";     // Base / Partial / Full

                writer.WriteLine("          <class");
                writer.WriteLine($"             activatableClassId=\"{className}\"");
                writer.WriteLine($"             threading=\"{threadingEnum}\"");
                writer.WriteLine($"             trustLevel=\"{trustLevelEnum}\"");
                writer.WriteLine($"             />");
            }

        }   
    }

});


Task("PackAPIProjection")
    .IsDependentOn("BuildServiceAndAPI")
    .Does(() =>
{
    Information("\nPacking API Projection...");

    var workingDirectory = System.IO.Path.Combine(apiAndServiceSolutionDir, "Client", "Midi2Client-Projection");

    NuGetPack(System.IO.Path.Combine(workingDirectory, "nuget", "Windows.Devices.Midi2.nuspec"), new NuGetPackSettings
    {
        WorkingDirectory = workingDirectory,
        OutputDirectory = System.IO.Path.Combine(releaseRootDir, "NuGet")  // NuGet packages cover multiple platforms
    });


});


Task("BuildConsoleApp")
    .IsDependentOn("PackAPIProjection")
    .DoesForEach(platformTargets, plat =>
{
    // TODO: Update nuget ref in console app to the new version
   

    Information("\nBuilding MIDI console app for " + plat.ToString());

    // update nuget packages for the entire solution. This is important for API/SDK NuGet in particular
    // NOTE: This doesn't work on packagereference projects. Need to consider a tool like 
    // dotnet-outdated https://github.com/dotnet-outdated/dotnet-outdated

    NuGetUpdate(consoleAppSolutionFile, new NuGetUpdateSettings
    {
        WorkingDirectory = consoleAppSolutionDir,
        Prerelease = true,
    });

    // we're specifying a rid, so we need to compile the project, not the solution
    
    string rid = "";
    if (plat == PlatformTarget.x64)
        rid = ridX64;
    else if (plat == PlatformTarget.ARM64)
        rid = ridArm64;
    else
        throw new ArgumentException("Invalid platform target " + plat.ToString());

    DotNetBuild(consoleAppProjectFile, new DotNetBuildSettings
    {
        WorkingDirectory = consoleAppProjectDir,
        Configuration = configuration, 
        Runtime = rid,        
    });


    // Note: Spectre.Console doesn't support trimming, so you get a huge exe.
    // Consider making this framework-dependent once .NET 8 releases
    DotNetPublish(consoleAppProjectFile, new DotNetPublishSettings
    {
        WorkingDirectory = consoleAppProjectDir,
        OutputDirectory = System.IO.Path.Combine(consoleAppStagingDir, plat.ToString()),
        Configuration = configuration,

        PublishSingleFile = false,
        PublishTrimmed = false,
        SelfContained = false,
        Framework = frameworkVersion,
        Runtime = rid
    });

});


Task("BuildSettingsApp")
    .IsDependentOn("PackAPIProjection")
    .DoesForEach(platformTargets, plat =>
{
    // TODO: Update nuget ref in settings app to the new version
    
    //NuGetUpdate(settingsAppSolutionFile);

    // TODO: Update nuget ref in console app to the new version
   

    Information("\nBuilding MIDI settings app for " + plat.ToString() + " " + configuration);

    // update nuget packages for the entire solution. This is important for API/SDK NuGet in particular

    //NuGetUpdate(settingsAppSolutionFile, new NuGetUpdateSettings
    //{
    //    WorkingDirectory = settingsAppSolutionDir,
    //});

    // we're specifying a rid, so we need to compile the project, not the solution
    
    string rid = "";
    if (plat == PlatformTarget.x64)
        rid = ridX64;
    else if (plat == PlatformTarget.ARM64)
        rid = ridArm64;
    else
        throw new ArgumentException("Invalid platform target " + plat.ToString());

    DotNetBuild(settingsAppProjectFile, new DotNetBuildSettings
    {
        WorkingDirectory = settingsAppSolutionDir,
        Configuration = configuration, 
        Runtime = rid,        
    });


    DotNetPublish(settingsAppProjectFile, new DotNetPublishSettings
    {
        WorkingDirectory = settingsAppSolutionDir,
        OutputDirectory = System.IO.Path.Combine(settingsAppStagingDir, plat.ToString()),
        Configuration = configuration,

        Runtime = rid,
        PublishSingleFile = false,
        PublishTrimmed = false,
        SelfContained = false,
        Framework = frameworkVersion
    });

    // WinUI makes me manually copy this over. If not for the hacked-in post-build step, would need all the .xbf files as well

});

string finalSetupNamex64 = string.Empty;
string finalSetupNameArm64 = string.Empty;

Task("BuildInstaller")
    .IsDependentOn("SetupEnvironment")
    .IsDependentOn("BuildServiceAndAPI")
    .IsDependentOn("BuildApiActivationRegEntriesCSharp")
    .IsDependentOn("BuildSettingsApp")
    .IsDependentOn("BuildConsoleApp")
    .DoesForEach(platformTargets, plat => 
{
    Information($"\nBuilding Installer for {plat.ToString()} {configuration}");

    Thread.Sleep(1000);

    var mainBundleProjectDir = System.IO.Path.Combine(setupSolutionDir, "main-bundle");
    var apiSetupProjectDir = System.IO.Path.Combine(setupSolutionDir, "api-package");
    var consoleOnlySetupProjectDir = System.IO.Path.Combine(setupSolutionDir, "console-package");
    var settingsOnlySetupProjectDir = System.IO.Path.Combine(setupSolutionDir, "settings-package");

    string rid = "";
    if (plat == PlatformTarget.x64)
        rid = ridX64;
    else if (plat == PlatformTarget.ARM64)
        rid = ridArm64;
    else
        throw new ArgumentException("Invalid platform target " + plat.ToString());

    // create the bundle info include file with the version info in it
    // this include file is referenced from the WiX Setup project and it
    // defines compile-time variables used in the project
    //<Include>
    //  <?define SetupVersionName="Dev Preview Nightly" ?>
    //  <?define SetupVersionNumber="1.0.23351.0243" ?>
    //</Include>

    string setupBuildPlatform = plat.ToString().ToLower();

    string setupBuildFullVersionString = setupBuildMajorMinor + "." + setupBuildDateNumber + "." + setupBuildTimeNumber;
    //string setupBuildFullVersionStringFile = setupBuildFullVersionString.Replace('.', '-');

    using (StreamWriter writer = System.IO.File.CreateText(setupBundleInfoIncludeFile))            
    {
        writer.WriteLine("<Include>");
        writer.WriteLine($"  <?define SetupVersionName=\"{setupVersionName} {setupBuildPlatform}\" ?>");
        writer.WriteLine($"  <?define SetupVersionNumber=\"{setupBuildFullVersionString}\" ?>");
        writer.WriteLine("</Include>");
    }

    var setupName = $"Windows MIDI Services {setupVersionName} {setupBuildFullVersionString}-{setupBuildPlatform}.exe";

    if (plat == PlatformTarget.x64)
        finalSetupNamex64 = setupName;
    else if (plat == PlatformTarget.ARM64)
        finalSetupNameArm64 = setupName;
  

    string workingDirectory = string.Empty;

    var buildSettings = new MSBuildSettings
    {
        MaxCpuCount = 1,    /* Set to 1 to avoid file locking problems */
        Configuration = configuration,
        AllowPreviewVersion = allowPreviewVersionOfBuildTools,
        PlatformTarget = plat,
        Verbosity = Verbosity.Minimal,
    };

    MSBuild(setupSolutionFile, buildSettings);

    Thread.Sleep(1000);

    string releaseStandAloneInstallerFolder = System.IO.Path.Combine(setupReleaseDir, "Component Installs", plat.ToString().ToLower());

    if (!DirectoryExists(setupReleaseDir))
        CreateDirectory(setupReleaseDir);   

    if (!DirectoryExists(releaseStandAloneInstallerFolder))
        CreateDirectory(releaseStandAloneInstallerFolder);   


    // copy and rename the main bundle installer to include version and platform

    CopyFiles(System.IO.Path.Combine(mainBundleProjectDir, "bin", plat.ToString(), "Release", "WindowsMidiServicesCompleteSetup.exe"), setupReleaseDir); 

    // rename
    MoveFile(System.IO.Path.Combine(setupReleaseDir, "WindowsMidiServicesCompleteSetup.exe"), 
            (System.IO.Path.Combine(setupReleaseDir, setupName)));

    CopyFiles(System.IO.Path.Combine(consoleOnlySetupProjectDir, "bin", plat.ToString(), "Release", "*.msi"), releaseStandAloneInstallerFolder); 
    CopyFiles(System.IO.Path.Combine(settingsOnlySetupProjectDir, "bin", plat.ToString(), "Release", "*.msi"), releaseStandAloneInstallerFolder); 
    CopyFiles(System.IO.Path.Combine(apiSetupProjectDir, "bin", plat.ToString(), "Release", "*.msi"), releaseStandAloneInstallerFolder); 

});


Task("CopyAPIArtifacts")
    .IsDependentOn("BuildInstaller")
    .IsDependentOn("PackAPIProjection")
    .DoesForEach(platformTargets, plat => 
{
    Information($"\nCopying API artifacts for {plat.ToString()} {configuration}");

    string apiStagingDir = System.IO.Path.Combine(apiAndServiceStagingDir, "bin", plat.ToString().ToLower());
    string platReleaseFolder = System.IO.Path.Combine(apiReleaseArtifactsFolder, plat.ToString().ToLower());

    if (!DirectoryExists(apiStagingDir))
        CreateDirectory(apiStagingDir);   

    if (!DirectoryExists(platReleaseFolder))
        CreateDirectory(platReleaseFolder);   

    CopyFiles(System.IO.Path.Combine(apiStagingDir, "Windows.Devices.Midi2.winmd"), platReleaseFolder); 
    CopyFiles(System.IO.Path.Combine(apiStagingDir, "Windows.Devices.Midi2.dll"), platReleaseFolder); 
    CopyFiles(System.IO.Path.Combine(apiStagingDir, "Windows.Devices.Midi2.pri"), platReleaseFolder); 
    CopyFiles(System.IO.Path.Combine(apiStagingDir, "Windows.Devices.Midi2.h"), platReleaseFolder); 
    CopyFiles(System.IO.Path.Combine(apiStagingDir, "winrt/base.h"), System.IO.Path.Combine(platReleaseFolder, "winrt/")); 
    CopyFiles(System.IO.Path.Combine(apiStagingDir, "winrt/Windows.Devices.h"), System.IO.Path.Combine(platReleaseFolder, "winrt/")); 
    CopyFiles(System.IO.Path.Combine(apiStagingDir, "winrt/impl/Windows.Devices.Enumeration.2.h"), System.IO.Path.Combine(platReleaseFolder, "winrt/impl/")); 
    CopyFiles(System.IO.Path.Combine(apiStagingDir, "winrt/impl/Windows.Devices.Midi.2.h"), System.IO.Path.Combine(platReleaseFolder, "winrt/impl/")); 
    CopyFiles(System.IO.Path.Combine(apiStagingDir, "winrt/impl/Windows.Foundation.2.h"), System.IO.Path.Combine(platReleaseFolder, "winrt/impl/")); 
    CopyFiles(System.IO.Path.Combine(apiStagingDir, "winrt/impl/Windows.Foundation.Collections.2.h"), System.IO.Path.Combine(platReleaseFolder, "winrt/impl/")); 
    CopyFiles(System.IO.Path.Combine(apiStagingDir, "winrt/impl/Windows.Devices.Midi2.2.h"), System.IO.Path.Combine(platReleaseFolder, "winrt/impl/")); 


});

//////////////////////////////////////////////////////////////////////
// TASK TARGETS
//////////////////////////////////////////////////////////////////////

Task("Default")
    .IsDependentOn("Clean")
    .IsDependentOn("BuildApiActivationRegEntriesInternal")
    .IsDependentOn("BuildInstaller")
    .IsDependentOn("CopyAPIArtifacts")
    .Does(() =>
{

    Information("\n\n");
    Information($"Installer x64   >> \"{finalSetupNamex64}\" \n");
    Information($"Installer Arm64 >> \"{finalSetupNameArm64}\"\n");
    Information("\n\n");

});




//////////////////////////////////////////////////////////////////////
// EXECUTION
//////////////////////////////////////////////////////////////////////

RunTarget(target);