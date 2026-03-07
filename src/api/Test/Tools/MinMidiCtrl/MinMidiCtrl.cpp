#include "pch.h"
#include "MinMidiControl.h"

enum class Command
{
    None,
    AddPort,
    RemovePort,
    EnableSurpriseRemovalSimulation,
    DisableSurpriseRemovalSimulation
};

void PrintUsage()
{
    std::wcout <<
        L"Usage:\n"
        L"  MinMidiCtrl.exe /AddPort <UMP | ByteStream>\n"
        L"  MinMidiCtrl.exe /RemovePort <UMP | ByteStream>\n"
        L"  MinMidiCtrl.exe /EnableSRSim\n"
        L"  MinMidiCtrl.exe /DisableSRSim\n";
}

Command ParseCommand(int argc, WCHAR** argv, MidiDataFormats &formats)
{
    if (argc < 2)
    {
        return Command::None;
    }

    if (_wcsicmp(argv[1], L"/AddPort") == 0)
    {
        if (argc != 3)
        {
            return Command::None;
        }

        if (_wcsicmp(argv[2], L"UMP") == 0)
        {
            std::wcout << L"Adding UMP port.\n";
            formats = MidiDataFormats_UMP;
        }
        else if (_wcsicmp(argv[2], L"ByteStream") == 0)
        {
            std::wcout << L"Adding ByteStream port.\n";
            formats = MidiDataFormats_ByteStream;
        }
        else
        {
            return Command::None;
        }

        return Command::AddPort;

    }

    if (_wcsicmp(argv[1], L"/RemovePort") == 0)
    {
        if (argc != 3)
        {
            return Command::None;
        }

        if (_wcsicmp(argv[2], L"UMP") == 0)
        {
            std::wcout << L"Removing UMP port.\n";
            formats = MidiDataFormats_UMP;
        }
        else if (_wcsicmp(argv[2], L"ByteStream") == 0)
        {
            std::wcout << L"Removing ByteStream port.\n";
            formats = MidiDataFormats_ByteStream;
        }
        else
        {
            return Command::None;
        }

        return Command::RemovePort;
    }

    if (_wcsicmp(argv[1], L"/EnableSRSim") == 0)
    {
        if (argc != 2)
        {
            return Command::None;
        }

        std::wcout << L"Enabling Surprise Removal Simulation.\n";
        return Command::EnableSurpriseRemovalSimulation;
    }

    if (_wcsicmp(argv[1], L"/DisableSRSim") == 0)
    {
        if (argc != 2)
        {
            return Command::None;
        }

        std::wcout << L"Disabling Surprise Removal Simulation.\n";
        return Command::DisableSurpriseRemovalSimulation;
    }

    return Command::None;
}

int __cdecl wmain(int argc, WCHAR ** argv)
{
    winrt::init_apartment();
    MidiDataFormats formats {MidiDataFormats_Invalid};

    Command cmd = ParseCommand(argc, argv, formats);

    switch (cmd)
    {
        case Command::AddPort:
            if (FAILED(SendDriverCommand(KSPROPERTY_MINMIDICONTROL_ADDPORT, (DWORD) formats)))
            {
                std::wcout << L"KSPROPERTY_MINMIDICONTROL_ADDPORT failed...\n";
            }
            break;
        case Command::RemovePort:
            if (FAILED(SendDriverCommand(KSPROPERTY_MINMIDICONTROL_REMOVEPORT, (DWORD) formats)))
            {
                std::wcout << L"KSPROPERTY_MINMIDICONTROL_REMOVEPORT failed...\n";
            }
            break;
        case Command::EnableSurpriseRemovalSimulation:
            if (FAILED(SendDriverCommand(KSPROPERTY_MINMIDICONTROL_SURPRISEREMOVESIMULATION, 1)))
            {
                std::wcout << L"KSPROPERTY_MINMIDICONTROL_REMOVEPORT failed...\n";
            }
            break;
        case Command::DisableSurpriseRemovalSimulation:
            if (FAILED(SendDriverCommand(KSPROPERTY_MINMIDICONTROL_SURPRISEREMOVESIMULATION, 0)))
            {
                std::wcout << L"KSPROPERTY_MINMIDICONTROL_REMOVEPORT failed...\n";
            }
            break;

        default:
            PrintUsage();
            return 1;
    }

    return 0;
}

