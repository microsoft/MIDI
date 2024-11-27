
    
    function Show-MidiRegInproc32([String] $key, [String] $comment)    
    {
        Write-Host $comment.PadRight(22) -NoNewline -ForegroundColor DarkCyan
        #Write-Host " : " -NoNewline -ForegroundColor White

        $info = Get-ItemProperty -Path $key  -Name "(default)" -ErrorAction SilentlyContinue

        if ($info -eq $null)
        {
             Write-Host "Registration not found" -ForegroundColor Red
        }
        else
        {
            Write-Host $info."(default)"  -ForegroundColor Cyan
        }
    }
    

    Write-Host
    Write-Host "Windows MIDI Services Developer Component Registration List" -ForegroundColor DarkCyan
    Write-Host "".PadRight(75,'=') -ForegroundColor DarkGray
    Write-Host "This queries the registry for current Windows MIDI Services registrations." -ForegroundColor DarkCyan
    Write-Host "If the paths include System32, you are using in-box components and not the." -ForegroundColor DarkCyan
    Write-Host "developer preview versions from GitHub. To install the bits from GitHub, " -ForegroundColor DarkCyan
    Write-Host "you will need to run the dev-prep script before running the installer." -ForegroundColor DarkCyan
    Write-Host
    

    # display info about the MIDI Service that is installed

    Write-Host
    Write-Host "Windows Service " -ForegroundColor DarkGray

    $service = Get-Service -Name "midisrv" -ErrorAction SilentlyContinue
    
    if ($service -eq $null)
    {
        Write-Host "Midisrv service is not present. " -ForegroundColor DarkGray
    }
    else
    {
        Write-Host $service.DisplayName.PadRight(22) -NoNewline  -ForegroundColor DarkCyan
        Write-Host $service.BinaryPathName -ForegroundColor Cyan
    }

    # Now all the registry entries for COM objects used by the service. This list must be
    # Manually maintained

    Write-Host
    Write-Host "API Interface " -ForegroundColor DarkGray
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{2BA15E4E-5417-4A66-85B8-2B2260EFBC84}\InprocServer32" "Midisrv"

    Write-Host
    Write-Host "V1.0 In-Box Transports " -ForegroundColor DarkGray
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{0f273b18-e372-4d95-87ac-c31c3d22e937}\InprocServer32" "KS Aggregate (MIDI 1)"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}\InprocServer32" "KS (UMP)"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{942BF02D-93C0-4EA8-B03E-D51156CA75E1}\InprocServer32" "Loopback"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{8FEAAD91-70E1-4A19-997A-377720A719C1}\InprocServer32" "Virtual MIDI"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{ac9b5417-3fe0-4e62-960f-034ee4235a1a}\InprocServer32" "Diagnostics"

    Write-Host
    Write-Host "Preview Transports " -ForegroundColor DarkGray
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{c95dcd1f-cde3-4c2d-913c-528cb8a4cbe6}\InprocServer32" "Network MIDI 2.0"

    Write-Host
    Write-Host "V1.0 In-Box Transforms " -ForegroundColor DarkGray
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{A8798C54-6066-45F0-9ADB-648BC0641ABF}\InprocServer32" "Bytestream 2 UMP"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{96121591-8D68-479F-9B48-2BF0B90113F7}\InprocServer32" "UMP 2 Bytestream"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{a42cde44-7fa9-4597-a8ee-b40b96bcddb1}\InprocServer32" "Message Scheduler"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{dc638b31-cf31-48ed-9e79-02740bf5d013}\InprocServer32" "Protocol Downscaler"


