# Install MinMidi:

function Install-MinMidi {

    Write-Host "Installing MinMidi driver..."
    bcdedit /set testsigning on
    certutil -addstore root .\minmidi-driver-test\testroot.cer
    certutil -addstore root .\minmidi-driver-test\testroot-sha2.cer
    .\minmidi-driver-test\devcon.exe install .\minmidi-driver-test\minmidi.inf Root\MinMidi
}

Install-MinMidi
