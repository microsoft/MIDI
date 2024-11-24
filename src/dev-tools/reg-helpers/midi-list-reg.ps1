
    
    function Show-MidiRegInproc32([String] $key, [String] $comment)    
    {
        Write-Host $comment.PadRight(30) -NoNewline -ForegroundColor DarkGray
        #Write-Host " : " -NoNewline -ForegroundColor White

        $info = Get-ItemProperty -Path $key  -Name "(default)" -ErrorAction SilentlyContinue

        if ($info -eq $null)
        {
             Write-Host "Registration not found" -ForegroundColor Red
        }
        else
        {
            Write-Host $info."(default)"  -ForegroundColor DarkCyan
        }
    }
    
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{0f273b18-e372-4d95-87ac-c31c3d22e937}\InprocServer32" "KS Aggregate Transport"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}\InprocServer32" "KS Transport"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{942BF02D-93C0-4EA8-B03E-D51156CA75E1}\InprocServer32" "Loopback Transport"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{8FEAAD91-70E1-4A19-997A-377720A719C1}\InprocServer32" "Virtual MIDI Transport"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{ac9b5417-3fe0-4e62-960f-034ee4235a1a}\InprocServer32" "Diagnostics Transport"

    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{2BA15E4E-5417-4A66-85B8-2B2260EFBC84}\InprocServer32" "Main Midisrv Transport"

    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{A8798C54-6066-45F0-9ADB-648BC0641ABF}\InprocServer32" "Bytestream 2 UMP Transform"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{96121591-8D68-479F-9B48-2BF0B90113F7}\InprocServer32" "UMP 2 Bytestream Transform"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{a42cde44-7fa9-4597-a8ee-b40b96bcddb1}\InprocServer32" "Message Scheduler"
    Show-MidiRegInproc32 "Registry::HKEY_CLASSES_ROOT\CLSID\{dc638b31-cf31-48ed-9e79-02740bf5d013}\InprocServer32" "Protocol Downscaler"
