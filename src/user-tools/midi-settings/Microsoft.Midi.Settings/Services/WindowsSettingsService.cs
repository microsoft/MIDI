// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Linq.Expressions;
using System.Text;
using System.Threading.Tasks;
using Windows.UI.Popups;

namespace Microsoft.Midi.Settings.Services;

internal class WindowsSettingsService : IWindowsSettingsService
{
    private void ShellExec(string uri)
    {
        try
        {
            using (var proc = new System.Diagnostics.Process())
            {
                proc.StartInfo.UseShellExecute = true;
                proc.StartInfo.FileName = uri;

                proc.Start();
            }
        }
        catch (Exception ex)
        {
            App.GetService<ILoggingService>().LogError("Error navigating to app URI", ex);
        }
    }

    public void ShowPowerSettingsPage()
    {
        ShellExec("ms-settings:powersleep");
    }

    public void ShowBluetoothSettingsPage()
    {
        ShellExec("ms-settings:bluetooth");
    }

    public void ShowDeveloperSettingsPage()
    {
        ShellExec("ms-settings:developers");
    }


    public bool RequestMidiServiceFirewallUdpAccess()
    {
        // TODO: Request midisrv be allowed through firewall
        // https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ics/windows-firewall-with-advanced-security-reference


        return false;
    }




}
