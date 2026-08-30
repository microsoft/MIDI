// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;
using System.Collections.Generic;
using System.Diagnostics;

using CommunityToolkit.Mvvm.Input;

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Models;
using Microsoft.UI.Xaml.Media.Imaging;

namespace Microsoft.Midi.Settings.Services;

public class MidiToolsService : IMidiToolsService
{
    private const string ToolsFolderName = @"Windows MIDI Services\Tools";

    private sealed record MidiToolAppDefinition(
        MidiToolApp Tool,
        string FolderName,
        string ExecutableName,
        string NameResourceKey,
        string DescriptionResourceKey,
        string IconAssetName);

    // Tools are installed side-by-side with the console, in
    // %ProgramFiles%\Windows MIDI Services\Tools\<folder>\<exe>
    private static readonly MidiToolAppDefinition[] _toolDefinitions =
    [
        new(MidiToolApp.LoopbackSetup, "LoopSetup", "midiloopbacksetup.exe",
            "ToolApp_LoopbackSetup_Name", "ToolApp_LoopbackSetup_Description", "ToolApp-LoopbackSetup.png"),

        new(MidiToolApp.NetworkSetup, "NetworkSetup", "midinetworksetup.exe",
            "ToolApp_NetworkSetup_Name", "ToolApp_NetworkSetup_Description", "ToolApp-NetworkSetup.png"),

        new(MidiToolApp.ScratchPad, "ScratchPad", "midiscratchpad.exe",
            "ToolApp_ScratchPad_Name", "ToolApp_ScratchPad_Description", "ToolApp-ScratchPad.png"),

        new(MidiToolApp.SysEx, "SysEx", "midisysextool.exe",
            "ToolApp_SysEx_Name", "ToolApp_SysEx_Description", "ToolApp-SysEx.png"),

        new(MidiToolApp.Monitor, "Monitor", "midi2monitor.exe",
            "ToolApp_Monitor_Name", "ToolApp_Monitor_Description", "ToolApp-Monitor.png"),

        new(MidiToolApp.Troubleshooter, "Troubleshooter", "miditroubleshooter.exe",
            "ToolApp_Troubleshooter_Name", "ToolApp_Troubleshooter_Description", "ToolApp-Troubleshooter.png"),
    ];

    private readonly IMidiConsoleToolsService _consoleToolsService;
    private readonly IGeneralSettingsService _generalSettingsService;
    private readonly ILoggingService _loggingService;

    public MidiToolsService(
        IMidiConsoleToolsService consoleToolsService,
        IGeneralSettingsService generalSettingsService,
        ILoggingService loggingService)
    {
        _consoleToolsService = consoleToolsService;
        _generalSettingsService = generalSettingsService;
        _loggingService = loggingService;
    }

    private static MidiToolAppDefinition GetDefinition(MidiToolApp tool)
    {
        return Array.Find(_toolDefinitions, d => d.Tool == tool)!;
    }

    private string GetToolPath(MidiToolApp tool)
    {
        var definition = GetDefinition(tool);

        return Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),
            ToolsFolderName,
            definition.FolderName,
            definition.ExecutableName);
    }

    public bool IsToolPresent(MidiToolApp tool)
    {
        try
        {
            return File.Exists(GetToolPath(tool));
        }
        catch (Exception ex)
        {
            _loggingService.LogError($"Exception checking for presence of tool {tool}", ex);

            return false;
        }
    }

    public bool LaunchTool(MidiToolApp tool, params string[] arguments)
    {
        _loggingService.LogInfo($"Enter");

        try
        {
            var toolPath = GetToolPath(tool);

            if (!File.Exists(toolPath))
            {
                _loggingService.LogError($"Tool {tool} does not exist in path '{toolPath}'");

                return false;
            }

            var startInfo = new ProcessStartInfo
            {
                FileName = toolPath,
                WorkingDirectory = Path.GetDirectoryName(toolPath),
                UseShellExecute = false,
            };

            foreach (var argument in arguments)
            {
                startInfo.ArgumentList.Add(argument);
            }

            using var process = new Process
            {
                StartInfo = startInfo
            };

            return process.Start();
        }
        catch (Exception ex)
        {
            _loggingService.LogError($"Error launching tool {tool}", ex);

            return false;
        }
    }

    public IReadOnlyList<MidiToolAppInfo> GetInstalledTools()
    {
        _loggingService.LogInfo($"Enter");

        var tools = new List<MidiToolAppInfo>();

        foreach (var definition in _toolDefinitions)
        {
            if (!IsToolPresent(definition.Tool))
            {
                continue;
            }

            var tool = definition.Tool;

            tools.Add(new MidiToolAppInfo
            {
                Name = definition.NameResourceKey.GetLocalized(),
                Description = definition.DescriptionResourceKey.GetLocalized(),
                Icon = new BitmapImage(new Uri($"ms-appx:///Assets/{definition.IconAssetName}")),
                LaunchCommand = new RelayCommand(() => LaunchTool(tool))
            });
        }

        return tools;
    }

    public bool IsEndpointMonitoringAvailable()
    {
        return IsToolPresent(MidiToolApp.Monitor) || _consoleToolsService.IsMidiConsolePresent();
    }

    public bool MonitorEndpoint(MidiEndpointDeviceInformation deviceInformation)
    {
        _loggingService.LogInfo($"Enter");

        // if the preferred tool isn't installed, fall back to the other one rather than doing nothing
        if (_generalSettingsService.GetMonitorTool() == MidiMonitorTool.MidiConsole)
        {
            if (_consoleToolsService.IsMidiConsolePresent())
            {
                return _consoleToolsService.MonitorEndpoint(deviceInformation);
            }

            return LaunchTool(MidiToolApp.Monitor, deviceInformation.EndpointDeviceId);
        }

        if (IsToolPresent(MidiToolApp.Monitor))
        {
            return LaunchTool(MidiToolApp.Monitor, deviceInformation.EndpointDeviceId);
        }

        return _consoleToolsService.MonitorEndpoint(deviceInformation);
    }
}
