#tool nuget:?package=NuGet.CommandLine&version=5.10


var target = Argument("target", "Default");


var ridX64 = "win10-x64";
var ridArm64 = "win10-arm64";
var frameworkVersion = "net8.0-windows10.0.20348.0";


//var platformTargets = new[]{PlatformTarget.x64, PlatformTarget.ARM64};
var platformTargets = new[]{PlatformTarget.x64};



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

var apiAndServiceSolutionDir = System.IO.Path.Combine(srcDir, "api");
var apiAndServiceSolutionFile = System.IO.Path.Combine(apiAndServiceSolutionDir, "Midi2.sln");
var apiAndServiceStagingDir = System.IO.Path.Combine(stagingRootDir, "api");

var sdkSolutionDir = System.IO.Path.Combine(srcDir, "app-dev-sdk");
var sdkSolutionFile = System.IO.Path.Combine(sdkSolutionDir, "midi-sdk.sln");
var sdkStagingDir = System.IO.Path.Combine(stagingRootDir, "app-dev-sdk");

var vsFilesDir = System.IO.Path.Combine(apiAndServiceSolutionDir, "VSFiles");

var consoleAppSolutionDir = System.IO.Path.Combine(srcDir, "user-tools", "midi-console", "Midi");
var consoleAppSolutionFile = System.IO.Path.Combine(consoleAppSolutionDir, "Midi.csproj");
var consoleAppStagingDir = System.IO.Path.Combine(stagingRootDir, "midi-console");

var settingsAppSolutionDir = System.IO.Path.Combine(srcDir, "user-tools", "midi-settings");
var settingsAppSolutionFile = System.IO.Path.Combine(settingsAppSolutionDir, "midi-settings.sln");
var settingsAppStagingDir = System.IO.Path.Combine(stagingRootDir, "midi-settings");


var setupSolutionDir = System.IO.Path.Combine(srcDir, "oob-setup");
var setupSolutionFile = System.IO.Path.Combine(setupSolutionDir, "midi-services-setup.sln");

var setupReleaseDir = releaseRootDir;



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

    //https://aka.ms/vs/17/release/vc_redist.x64.exe




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

    Information("\nCopying service and API for " + plat.ToString());

    var copyToDir = System.IO.Path.Combine(apiAndServiceStagingDir, plat.ToString());
    
    if (!DirectoryExists(copyToDir))
        CreateDirectory(copyToDir);

    // copy all the DLLs
    CopyFiles(System.IO.Path.Combine(outputDir, "*.dll"), copyToDir); 
    CopyFiles(System.IO.Path.Combine(outputDir, "MidiSrv.exe"), copyToDir); 

    CopyFiles(System.IO.Path.Combine(outputDir, "Windows.Devices.Midi2.winmd"), copyToDir); 
    CopyFiles(System.IO.Path.Combine(outputDir, "Windows.Devices.Midi2.pri"), copyToDir); 

    CopyFiles(System.IO.Path.Combine(outputDir, "WinRTActivationEntries.txt"), copyToDir); 
});


Task("BuildApiActivationRegEntries")
    .IsDependentOn("BuildServiceAndAPI")
    .DoesForEach(platformTargets, plat =>
{
    Information("\nBuilding WinRT Activation Entries for " + plat.ToString());

    // read the file of dependencies
    var sourceFileName = System.IO.Path.Combine(apiAndServiceStagingDir, plat.ToString(), "WinRTActivationEntries.txt");
    var wxiDestinationFileName = System.IO.Path.Combine(apiAndServiceStagingDir, plat.ToString(), "WinRTActivationEntries.wxi");

    const string parentHKLMRegKey = "SOFTWARE\\Microsoft\\WindowsRuntime\\ActivatableClassId\\";
    const string wixWinrtLibFileName = "[API_INSTALLFOLDER]\\Windows.Devices.Midi2.dll";

    if (!System.IO.File.Exists(sourceFileName))
        throw new ArgumentException("Missing WinRT Activation entries file " + sourceFileName);

    using (StreamReader reader = System.IO.File.OpenText(sourceFileName))
    {
        using (StreamWriter wxiWriter = System.IO.File.CreateText(wxiDestinationFileName))            
        {
            wxiWriter.WriteLine("<Include xmlns=\"http://wixtoolset.org/schemas/v4/wxs\">");

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

                wxiWriter.WriteLine($"<RegistryKey Root=\"HKLM\" Key=\"{parentHKLMRegKey}{className}\">");
                
                wxiWriter.WriteLine($"    <RegistryValue Name=\"DllPath\" Type=\"string\" Value=\"{wixWinrtLibFileName}\" />");
                wxiWriter.WriteLine($"    <RegistryValue Name=\"ActivationType\" Type=\"integer\" Value=\"{activationType}\" />");
                wxiWriter.WriteLine($"    <RegistryValue Name=\"Threading\" Type=\"integer\" Value=\"{threading}\" />");
                wxiWriter.WriteLine($"    <RegistryValue Name=\"TrustLevel\" Type=\"integer\" Value=\"{trustLevel}\" />");
               
                wxiWriter.WriteLine("</RegistryKey>");

            }

            wxiWriter.WriteLine("</Include>");

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



Task("BuildSDK")
    .IsDependentOn("BuildServiceAndAPI")
    .IsDependentOn("PackAPIProjection")
    .DoesForEach(platformTargets, plat => 
{
    Information("\nBuilding SDK for " + plat.ToString());

    // TODO


});


Task("PackSDKProjection")
    .IsDependentOn("BuildSDK")
    .Does(() => 
{
    Information("\nPacking SDK...");

    // TODO


});



Task("BuildConsoleApp")
    .IsDependentOn("PackAPIProjection")
    .IsDependentOn("PackSDKProjection")
    .DoesForEach(platformTargets, plat =>
{
    // TODO: Update nuget ref in console app to the new version
   

    Information("\nBuilding MIDI console app for " + plat.ToString());

    DotNetBuild(consoleAppSolutionFile, new DotNetBuildSettings
    {
        WorkingDirectory = consoleAppSolutionDir,
        Configuration = configuration
    });

    string rid = "";
    if (plat == PlatformTarget.x64)
        rid = ridX64;
    else if (plat == PlatformTarget.ARM64)
        rid = ridArm64;
    else
        throw new ArgumentException("Invalid platform target " + plat.ToString());


    // Note: Spectre.Console doesn't support trimming, so you get a huge exe.
    // Consider making this framework-dependent once .NET 8 releasesmi
    DotNetPublish(consoleAppSolutionFile, new DotNetPublishSettings
    {
        WorkingDirectory = consoleAppSolutionDir,
        OutputDirectory = System.IO.Path.Combine(consoleAppStagingDir, plat.ToString()),
        Configuration = configuration,

        PublishSingleFile = true,
        PublishTrimmed = false,
        SelfContained = true,
        Framework = frameworkVersion,
        Runtime = rid
    });

});


Task("BuildSettingsApp")
    .IsDependentOn("PackAPIProjection")
    .IsDependentOn("PackSDKProjection")
    .DoesForEach(platformTargets, plat =>
{
    // TODO: Update nuget ref in settings app to the new version

});



Task("BuildInstaller")
    .IsDependentOn("BuildServiceAndAPI")
    .IsDependentOn("BuildApiActivationRegEntries")
    .IsDependentOn("BuildSDK")
    .IsDependentOn("BuildSettingsApp")
    .IsDependentOn("BuildConsoleApp")
    .DoesForEach(platformTargets, plat => 
{
    var buildSettings = new MSBuildSettings
    {
        MaxCpuCount = 0,
        Configuration = configuration,
        AllowPreviewVersion = allowPreviewVersionOfBuildTools,
        PlatformTarget = plat,
        Verbosity = Verbosity.Minimal,       
    };

    MSBuild(setupSolutionFile, buildSettings);

    if (!DirectoryExists(setupReleaseDir))
        CreateDirectory(setupReleaseDir);

    CopyFiles(System.IO.Path.Combine(setupSolutionDir, "main-bundle", "bin", plat.ToString(), configuration, "*.exe"), setupReleaseDir); 
});




//////////////////////////////////////////////////////////////////////
// TASK TARGETS
//////////////////////////////////////////////////////////////////////

Task("Default")
    .IsDependentOn("Clean")
    .IsDependentOn("BuildInstaller");




//////////////////////////////////////////////////////////////////////
// EXECUTION
//////////////////////////////////////////////////////////////////////

RunTarget(target);