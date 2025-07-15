// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Helpers
{
    class WindowsDeveloperModeHelper
    {
        private static bool _isDeveloperModeEnabled = false;

        public static bool IsDeveloperModeEnabled
        {
            get => _isDeveloperModeEnabled;
            private set => _isDeveloperModeEnabled = value;
        }

        public static void Refresh()
        {
            CheckDeveloperModeRegistryKey();
        }

        private static void CheckDeveloperModeRegistryKey()
        {
            const string keyName = @"HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock";
            const string valueName = "AllowDevelopmentWithoutDevLicense";

            var value = Microsoft.Win32.Registry.GetValue(keyName, valueName, 0);

            if (value == null || (int)value != 1)
            {
                IsDeveloperModeEnabled = false;
            }
            else
            {
                IsDeveloperModeEnabled = true;
            }
        }

        // TODO: implement reg watcher for the dev mode value so user doesn't need to close app to refresh
        // http://www.pinvoke.net/default.aspx/advapi32.regnotifychangekeyvalue
        // https://stackoverflow.com/questions/41231586/how-to-detect-if-developer-mode-is-active-on-windows-10




    }
}
