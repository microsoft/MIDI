@echo off


set sdk_bin_folder_x64=%midi_repo_root%\src\app-sdk\vsfiles\out\coalesce\x64\Release\


echo this will fail if you aren't on the right drive already
cd %midi_repo_root%
cd "build\electron-projection"

pushd

set ns_name=microsoft.windows.devices.midi2
set projection_root=projection

rmdir /S /Q %projection_root%

mkdir %projection_root%
mkdir %projection_root%\%ns_name%
mkdir %projection_root%\%ns_name%\build

echo STEP 1: copy the current %ns_name%.winmd metadata to the electron build folder
copy /Y %sdk_bin_folder_x64%\%ns_name%.winmd .\projection\%ns_name%\build

echo STEP 2: copy Windows.winmd metadata to the same folder
copy /Y "C:\Program Files (x86)\Windows Kits\10\UnionMetadata\10.0.20348.0\Windows.winmd" .\%projection_root%\%ns_name%\build

echo STEP 3: Generate the projection
nodert\src\NodeRTCmd\bin\Debug\NodeRTCmd.exe --winmd .\%projection_root%\%ns_name%\build\%ns_name%.winmd --outdir .\%projection_root% --verbose

echo STEP 3: Compile the projection. This doesn't work inside NodeRT for some reason
cd %projection_root%
cd %ns_name%
node-gyp rebuild --msvs_version=2022

echo STEP 4: need to install and run node-modules\.bin\electron-rebuild to actually use this. This is not automatic

popd