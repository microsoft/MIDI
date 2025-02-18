---
layout: page
title: How to Install Development Builds
parent: Developer How-to
has_children: false
---

# How to Install Development Service builds

Windows MIDI Services has been designed from the start to be expandible with new transports, and in the future, with new transforms, by the developer community, all without having to write kernel drivers. These components are delivered as COM DLLs implementing specific interfaces, and then registered on the local computer.

## Evaluate if you need to do this

If you are a regular musician, we generally recommend you stick with what is installed in-box in Windows. If, however, you are technically astute, and safe in your computer practices, you can unlock Windows MIDI Services to enable in-development service plugins to be installed, and service settings to be changed.

> Note: The steps in this file will eventually be replaced by developer-mode settings in the MIDI Settings app.

## Enable developer mode

In Windows Settings, you must enable developer mode. System > For Developers > 

## Install PowerShell 7

The scripts require PowerShell 7 or newer. You can install PowerShell [following these instructions](https://learn.microsoft.com/powershell/scripting/install/installing-powershell-on-windows).

After installing PowerShell, you need to enable script execution in the Windows Settings app: Settings > System > For Developers > PowerShell.

![Enable PowerShell Scripts in Settings](.\enable-powershell-scripts.png)

## Take Ownership of the Registry Keys

When Windows MIDI Services is included with the in-box install of Windows, the registry keys for the transport plugins and other MIDI Services settings are all protected, and owned by Trusted Installer. This helps prevent malicious apps from adding entries and loading any arbitrary code into the service.

Developers and those using preview versions of plugins, however, need to be able to write to those keys to add new transports, or change developer settings.

Each developer release comes with a zip file with two PowerShell scripts and two .cmd files which launch them. The first lists the registry entries for for all the COM components used by the service, as well as the service itself. The second, when run as administrator, will take ownership of the relevant keys allowing the values to be replaced with development versions of components. We recommend this be done only by developers and other more technical users.

Run the two .cmd files from a normal developer command prompt (cmd, not PowerShell), as Administator as follows.

### List Registry State

First, run `list-reg.cmd` to see the current registry values. In-box services and components are all installed in `\Windows\System32`. The pre-release versions of those components are installed in `\Program Files\Windows MIDI Services`

```
c:\foo\bar> list-reg.cmd
```

![Before](.\list-reg-before.png)

You can see in this that we have the stock plugins listed as running from `System32`, and no entry for the Network MIDI 2.0 plugin. This is expected for the time when I wrote this article, as Network MIDI 2.0 is/was an in-development preview plugin, not an in-box plugin.

### Prepare for a Developer Install

After validating your current state is the in-box state (this is reverted to this state with each Windows Upgrade, including Insider Builds, as well as with any Windows MIDI Services update), you'll next want to take ownership of the keys.

Run the `dev-prep.cmd` file from an Administrator command prompt (again, a regular command shell, not PowerShell).

![Developer Prep Script](.\dev-prep-output.png)

If you see any errors in the output, stop at this point and investigate. Otherwise, the installers will appear to run fine, but if they do not have permissions to write to the correct registry keys, you won't be using the components you just installed.

## Install the Developer Packages

Now you may install all of the developer packages via their installers.

## Verify the Installation

Finally, run the `list-reg.cmd` file again to verify that everything is installed in the new locations.

![After](.\list-reg-after.png)

If everything worked, you'll see the files reported as being from `Program Files` instead of `System32`. You are now running the preview versions of the service and components.

In some cases, you may want to leave the existing service, but install only a preview plugin. In that case, the service and other components will still show `System32`, but the new component will show `Program Files` as the location.

> Note: These scripts only know about the plugins we've developed. Additional third-party components will not be shown in the output unless we know about and add them.

## wdmaud2.drv the WinMM bridge to Windows MIDI Services

The WinMM compatibility component, `wdmaud2.drv`, is a system-protected file under Windows 11 Resource Protection (WRP). It is possible to take ownership of it using the `takeown` command, but if you replace it, WRP will kick in and put the official version back in place. The mechanisms used to bypass Windows File Protection (WFP) with older versions of Windows do not necessarily apply here.

[Learn more about Windows Resource Protection](https://learn.microsoft.com/windows/win32/wfp/about-windows-file-protection)
