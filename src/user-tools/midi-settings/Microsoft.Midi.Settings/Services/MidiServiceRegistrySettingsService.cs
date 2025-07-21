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
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services
{
    public class MidiServiceRegistrySettingsService : IMidiServiceRegistrySettingsService
    {
        const bool SettingsDefaultUseMMCSS = false;
        const bool SettingsDefaultDiscoveryEnabled = true;
        const bool SettingsDefaultMidi1PortNaming = true;
        const UInt32 SettingsDefaultDiscoveryTimeout = 10000;


        private const string MidiRootRegKey = @"HKEY_LOCAL_MACHINE\Software\Microsoft\Windows MIDI Services";

        private UInt32 GetRegistryNumericDWORDValue(string keyName, string valueName, UInt32 defaultValue)
        {
            try
            {
                var value = Microsoft.Win32.Registry.GetValue(keyName, valueName, defaultValue);

                if (value == null)
                {
                    // happens when the key does not exist
                    return defaultValue;
                }
                else
                {
                    return (UInt32)value;
                }
            }
            catch (Exception)
            {
                return defaultValue;
            }
        }

        private bool GetRegistryBooleanDWORDValue(string keyName, string valueName, bool defaultValue)
        {
            try
            {
                UInt32 defaultNumericValue = 0;

                if (defaultValue)
                {
                    defaultNumericValue = 1;
                }

                var value = Microsoft.Win32.Registry.GetValue(keyName, valueName, defaultNumericValue);

                if (value == null)
                {
                    // happens when the key does not exist
                    return defaultValue;
                }
                else if ((UInt32)value > 0)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
            catch (Exception)
            {
                return defaultValue;
            }

        }

        private string GetRegistryStringValue(string keyName, string valueName, string defaultValue)
        {
            try
            {
                var value = Microsoft.Win32.Registry.GetValue(keyName, valueName, defaultValue);

                if (value == null)
                {
                    // happens when the key does not exist
                    return defaultValue;
                }
                else if (value is string)
                {
                    return (string)value;
                }
                else
                {
                    return defaultValue;
                }
            }
            catch (Exception)
            {
                return defaultValue;
            }
        }

        private bool SetRegistryNumericDWORDValue(string keyName, string valueName, UInt32 newValue)
        {
            try
            {
                Microsoft.Win32.Registry.SetValue(keyName, valueName, newValue);

                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        private bool SetRegistryBooleanDWORDValue(string keyName, string valueName, bool newValue)
        {
            try
            {
                var val = newValue ? (UInt32)1 : (UInt32)0;

                Microsoft.Win32.Registry.SetValue(keyName, valueName, val);

                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        private bool SetRegistryStringValue(string keyName, string valueName, string newValue)
        {
            try
            {
                Microsoft.Win32.Registry.SetValue(keyName, valueName, newValue.Trim());

                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        //private bool DeleteRegistryValue(string regKey, string regValue)
        //{
        //    // TODO

        //    return false;
        //}

        const string ValueName_DefaultMidi1PortNaming = "DefaultMidi1PortNaming";
        const string ValueName_Midi2DiscoveryEnabled = "Midi2DiscoveryEnabled";
        const string ValueName_Midi2DiscoveryTimeoutMS = "Midi2DiscoveryTimeoutMS";
        const string ValueName_UseMMCSS = "UseMMCSS";
        const string ValueName_CurrentConfig = "CurrentConfig";


        public bool IsConfigFileSpecified()
        {
            return GetCurrentConfigFileName() != string.Empty;
        }

        public string GetCurrentConfigFileName()
        {
            return GetRegistryStringValue(MidiRootRegKey, ValueName_CurrentConfig, string.Empty);
        }

        public bool UpdateRegistryCurrentConfigFileName(string configFileName)
        {
            return SetRegistryStringValue(MidiRootRegKey, ValueName_CurrentConfig, configFileName);
        }





        public bool GetDefaultUseNewStyleMidi1PortNaming()
        {
            //return GetRegistryBooleanDWORDValue(MidiRootRegKey, ValueName_DefaultToOldMidi1PortNaming, SettingsDefaultMidi1PortNaming);

            var val = GetRegistryNumericDWORDValue(MidiRootRegKey, ValueName_DefaultMidi1PortNaming, (uint)(Midi1PortNameSelectionProperty.PortName_UseLegacyWinMM));

            if ((Midi1PortNameSelectionProperty)val == Midi1PortNameSelectionProperty.PortName_UseLegacyWinMM)
            {
                return false;
            }
            else
            {
                return true;
            }

        }
        public bool SetDefaultUseNewStyleMidi1PortNaming(Midi1PortNameSelectionProperty newValue)
        {
            return SetRegistryNumericDWORDValue(MidiRootRegKey, ValueName_DefaultMidi1PortNaming, (uint)newValue);
        }


        public bool GetMidi2DiscoveryEnabled()
        {
            return GetRegistryBooleanDWORDValue(MidiRootRegKey, ValueName_Midi2DiscoveryEnabled, SettingsDefaultDiscoveryEnabled);
        }

        public UInt32 GetMidi2DiscoveryTimeoutMS()
        {
            return GetRegistryNumericDWORDValue(MidiRootRegKey, ValueName_Midi2DiscoveryTimeoutMS, SettingsDefaultDiscoveryTimeout);
        }

        public bool GetUseMMCSS()
        {
            return GetRegistryBooleanDWORDValue(MidiRootRegKey, ValueName_UseMMCSS, SettingsDefaultUseMMCSS);
        }


        public bool SetMidi2DiscoveryEnabled(bool newValue)
        {
            // TODO: if the new value is the default, then just delete the reg entry

            return SetRegistryBooleanDWORDValue(MidiRootRegKey, ValueName_Midi2DiscoveryEnabled, newValue);
        }

        public bool SetMidi2DiscoveryTimeoutMS(UInt32 newValue)
        {
            // TODO: if the new value is the default, then just delete the reg entry

            return SetRegistryNumericDWORDValue(MidiRootRegKey, ValueName_Midi2DiscoveryTimeoutMS, newValue);
        }

        public bool SetUseMMCSS(bool newValue)
        {
            // if the new value is the default, then just delete the reg entry

            return SetRegistryBooleanDWORDValue(MidiRootRegKey, ValueName_UseMMCSS, newValue);
        }
    }
}
