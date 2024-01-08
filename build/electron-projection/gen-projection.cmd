@echo off

echo this will fail if you aren't on the right drive already
cd %midi_repo_root%
cd "build\release\electron-projection"

mkdir projection\windows.devices.midi2\build

rmdir /S /Q ./projection2


echo STEP 1: copy the current Windows.Devices.Midi2.winmd metadata to the electron build folder
copy /Y %midi_repo_root%\build\release\api\Windows.Devices.Midi2.winmd .\projection\windows.devices.midi2\build

echo STEP 1.2: copy Windows.winmd metadata to the same folder
copy /Y "C:\Program Files (x86)\Windows Kits\10\UnionMetadata\10.0.20348.0\Windows.winmd" .\projection\windows.devices.midi2\build

echo STEP 2: This will fail to build the project. Needs to be done from inside Visual Studio
nodert\src\NodeRTCmd\bin\Debug\NodeRTCmd.exe --winmd .\projection\windows.devices.midi2\build\Windows.Devices.Midi2.winmd --outdir .\projection2 --verbose

echo STEP 3: If it was generated, open projection2\windows.devices.midi2\build\binding.sln and recompile. If not, you need to debug. It may be the projection2 folder was in use.

echo STEP 4: need to deal with SendMessageStruct by removing it from the C++ source _nodert_generated.cpp. Remove all references to SendMessageStruct and FillMessageStruct and rebuild.

echo STEP 5: need to run node-modules\.bin\electron-rebuild

