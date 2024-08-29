# Electron sample

Before you can use this, you must take a few extra steps to get to the right Electron ABI vs the Node one.

1. From this folder, execute

```
npm install --save-dev @electron/rebuild
```

2. Copy all the SDK projections (`microsoft.windows.devices.midi2*`) into the `node_modules` folder

3. From this folder, execute

```
.\node_modules\.bin\electron-rebuild
```

The following steps are only required until we get WinRT Type activation proxies functioning

1. Copy the SDK implementation DLLs into the electron project folder. Specifically the .dll, .winmd, and .pri files

2. Copy the electron.exe.manifest into the project folder.
