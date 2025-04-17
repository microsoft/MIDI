---
layout: doc
title: How to Install Development Builds
parent: Developer How-to
has_children: false
---

# How to Install Development Service builds

Windows MIDI Services has been designed from the start to be expandible with new transports, and in the future, with new transforms, by the developer community, all without having to write kernel drivers. These components are delivered as COM DLLs implementing specific interfaces, and then registered on the local computer.

## Evaluate if you need to do this

If you are a regular musician, we generally recommend you stick with what is installed in-box in Windows. If, however, you are technically astute, and safe in your computer practices, you can unlock Windows MIDI Services to enable in-development service plugins to be installed, and service settings to be changed.

## Required: Uninstall any previous developer packages

In Settings > Apps > Installed Apps uninstall any previously installed developer packages for Windows MIDI Services.

## Required: Turn on Developer Mode in Windows

Normally, the service will checked for signed binaries before loading them. If you install the service and then run an application, you will not see any endpoints because no transports have been loaded.

If Developer Mode is turned on in settings, the service skips the check for signing and loads the binaries.

Settings > System > For developers > Developer Mode

![Developer Mode](.\settings-developer-mode.png)

# Easiest Way: Use the MIDI Settings App

The easiest way to set up your PC to use developer builds is to use the MIDI Settings app. This is a new feature added on February 28, 2025 and is in testing.

## Install the SDK Runtime and Tools

First, install the SDK and Tools installer, without installing the service installer. This will install the MIDI Settings app.

## Run the MIDI Settings app as Administrator

Next, run the MIDI Settings app **as Administrator**. Depending upon what is already on your system, it may tell you there's no service present. That is fine.

Because you previously enabled developer mode, you will see a **For Developers** page in the app. You will also see "Developer Mode Enabled".

Because you have run this as Administrator, you will see "Administrator" in the title bar, and the buttons under "Install Developer Preview Releases" will be enabled.

![Developer Prep Settings](.\developer-prep-settings.png)

## Prepare for Developer install

Click the button that says "Prepare for Developer Install"

## Run the Service and components installer

When that succeeds, you may run the installer for the Service and components.

## Replace wdmaud2

After the Service and components installer completes, unzip the `wdmaud2` zip file for your processor architecture (Arm64 or x64) to a known location on your PC.

Then click the "Replace wdmaud2.drv ... " button on the developer page.

## Reboot

After the wdmaud2.drv replacement has completed, reboot your PC.


# Command-line Way: Use PowerShell Scripts

## Required: Install PowerShell 7+

The scripts require PowerShell 7 or newer. You can install PowerShell [following these instructions](https://learn.microsoft.com/powershell/scripting/install/installing-powershell-on-windows).

After installing PowerShell, you need to enable script execution in the Windows Settings app: Settings > System > For Developers > PowerShell.

![Enable PowerShell Scripts in Settings](.\enable-powershell-scripts.png)


## Optional: List Registry State

First, run `list-reg.cmd` to see the current registry values. In-box services and components are all installed in `\Windows\System32`. The pre-release versions of those components are installed in `\Program Files\Windows MIDI Services`

```
c:\path\to\utilities\in\zip> list-reg.cmd
```

![Before](.\list-reg-before.png)

You can see in this that we have the stock plugins listed as running from `System32`, and no entry for the Network MIDI 2.0 plugin. This is expected for the time when I wrote this article, as Network MIDI 2.0 is/was an in-development preview plugin, not an in-box plugin.

## Required: Prepare for a Developer Install

After validating your current state is the in-box state (this is reverted to this state with each Windows Upgrade, including Insider Builds, as well as with any Windows MIDI Services update), you'll next want to take ownership of the keys.

Run the `dev-prep.cmd` file from an Administrator command prompt (again, a regular command shell, not PowerShell).

![Developer Prep Script](.\dev-prep-output.png)

If you see any errors in the output, stop at this point and investigate. Otherwise, the installers will appear to run fine, but if they do not have permissions to write to the correct registry keys, you won't be using the components you just installed.

## Required: Install the Developer Packages

Now you may install all of the developer packages from the release via their installers.

## Recommended: Verify the Installation

Finally, run the `list-reg.cmd` file again to verify that everything is installed in the new locations.

![After](.\list-reg-after.png)

If everything worked, you'll see the files reported as being from `Program Files` instead of `System32`. You are now running the preview versions of the service and components.

In some cases, you may want to leave the existing service, but install only a preview plugin. In that case, the service and other components will still show `System32`, but the new component will show `Program Files` as the location.

> Note: These scripts only know about the plugins we've developed. Additional third-party components will not be shown in the output unless we know about and add them.

## wdmaud2.drv the WinMM bridge to Windows MIDI Services

Current releases include separate Arm64 and x64 wdmaudi2.drv zip files, with the driver itself as well as a PowerShell script to replace the existing in-box version.

Installing this requires that you are running a Windows Insider Canary build that includes wdmaud2.drv already, and that you are a local Administrator on the PC. You can go through this process at any point, but it's typically run *after* you have already installed the rest of the packages for the release.

1. Unzip the wdmaud2 zip file for your architecture. Then open an Administrator command prompt in the directory which includes the `wdmaud2.drv` file from the zip.
2. Run `dev-replace-wdmaud2.cmd` from that command prompt
3. Reboot, and then WinMM support will be in place

## When to do these steps

Each time you have an OS upgrade (not update, but upgrade) from one version of Canary to another, you will need to repeat the above steps.