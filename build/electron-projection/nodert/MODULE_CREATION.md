# :wrench: Creation of WinRT Modules

NodeRT is a tool that automatically generates node.js Native add-on wrappers for <a href="http://en.wikipedia.org/wiki/Windows_Runtime">UWP/WinRT APIs</a>.

NodeRT automatically exposes Microsoft’s UWP/WinRT APIs to the Node.js environment by generating Node modules. This enables Node.js developers to write code that consumes native Windows capabilities. The generated modules' APIs are (almost) the same as the <a href="http://msdn.microsoft.com/en-us/library/windows/apps/br211377.aspx">UWP/WinRT APIs listed in MSDN</a>.
NodeRT can be used to generate Node modules both from command line (NodeRTCmd) and from its UI tool (NodeRTUI). NodeRT is developed and released by a group of Node.js enthusiasts at Microsoft.

Here is an example of using NodeRT <a href="http://msdn.microsoft.com/library/windows/apps/br225603">windows.devices.geolocation</a> module to retrieve the current location:

```js
const { Geolocator } = require('windows.devices.geolocation')
const locator = new Geolocator()

locator.getGeopositionAsync((error, result) => {
  if (error) {
    console.error(error)
    return
  }

  const { coordinate } = result
  const { longitude, latitude } = coordinate

  console.info(longitude, latitude)
})
```

## Table of Contents

 * [NodeRT Prerequisites](#prerequisites)
 * [Generating a NodeRT module using the UI](#generating-a-nodert-module-using-the-ui)
 * [Generating a NodeRT module using the CLI](#generating-a-nodert-module-using-the-cli)

## Prerequisites

<a name="Prerequisites"></a>
<H3>NodeRT Prerequisites</H3>
First, in order to use WinRT you must be running on a Windows environment that supports WinRT - meaning Windows 10, Windows 8.1, Windows 8, or Windows Server 2012.

In order to use NodeRT, make sure you have the following installed:<br>
* Visual Studio 2019, 2017, or 2015 for generating Windows 10 compatible modules<br>
* Visual Studio 2013 or 2012 for generating Windows 8.1/8 compatible modules respectively.
* Windows SDK for the version of Windows your are using:
	- [Windows 10 SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk)
	- [Windows 8.1 SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-8-1-sdk)
	- [Windows 8](https://developer.microsoft.com/en-us/windows/downloads/windows-8-sdk)
* node.js (version > 8.*) - from <a href="https://nodejs.org/en/">nodejs.org</a><br>
* node-gyp - make sure to get the latest version from npm by running:
```
npm install -g node-gyp
```

Next, download the latest NodeRT release from <a href="https://github.com/NodeRT/NodeRT/releases">here</a>, or clone this repository to your machine and build the NodeRT solution using Visual Studio.

## Generating a NodeRT module using the UI

First, launch the UI tool by running `NodeRTUI.exe`:

![Windows.Devices.Geolocation NodeRT module contents](/doc/images/nodert_screenshot.png)

Then, follow this short list of steps in order to create a NodeRT module:

* Choose a WinMD file: <br>
    - For Windows 10 SDK: <br>
    ```
    c:\Program Files (x86)\Windows Kits\10\UnionMetadata\Windows.winmd
    ```
    - For Windows 8.1 SDK: <br>
    ```
    c:\Program Files (x86)\Windows Kits\8.1\References\CommonConfiguration\Neutral\Windows.winmd
    ```
    - For Windows 8.0 SDK: <br>
    ```
    c:\Program Files (x86)\Windows Kits\8.0\References\CommonConfiguration\Neutral\Windows.winmd
    ```
* Choose a namespace to generate from the list of namespaces.<br>
* Select whether you are generating a Windows 10 compatible module using VS 015, Windows 8.1 compatible module using VS2013 or a Windows 8.0 compatible module using VS2012.<br>
* Choose the output directory in which the module will be created, or just stick with the default ones.
* You're good to go, hit the Generate & Build button! A message box with (hopefully) a success message should appear shortly.

## Generating a NodeRT module using the CLI

If you prefer a command line interface, modules can be created with the `NodeRTCmd` tool.

An example of generating the Windows.Devices.Geolocation namespace from the Windows 10 Windows.winmd:
```
NodeRTCmd.exe --winmd "c:\Program Files (x86)\Windows Kits\10\UnionMetadata\Windows.winmd" --outdir c:\NodeRT\output --namespace Windows.Devices.Geolocation
```
Note that omitting the --namespace option will generate all of the namespaces in the Winmd file.

The following is the list of options that the tool supports:

```sh
 --winmd [path]              File path to winmd file from which the module
                             will be generated

 --namespaces                Lists all of the namespaces in the winmd file
                             (only needs --winmd)

 --namespace [namespace]     The namespace to generate from the winmd when
                             not specified , all namespaces will be generated

 --outdir [path]             The output dir in which the compiled NodeRT module
                             will be created in

 --vs [Vs2019|Vs2017|Vs2015|Vs2013|Vs2012] Optional, VS version to use, default is Vs2019

 --winver [10|8.1|8]         Optional, Windows SDK version to use, default is 10

 --npmscope                  Optional, the scope that will be specified for the generated
                             npm package

 --npmversion                Optional, the version that will be specified for the generated
                             npm package

 --nodefgen                  Optional, specifying this option will reult in
                             skipping the generation of TypeScript and
                             JavaScript definition files

 --nobuild                   Optional, specifying this option will result in
                             skipping the build process for the NodeRT module

 --verbose                   Optional, specifying this option will result in
                             verbose output for the module build operation

 --help                      Print this help screen

```
