using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Security.Principal;
using WixToolset.Dtf.WindowsInstaller;
using static System.Collections.Specialized.BitVector32;

namespace custom_actions
{
    public class CustomActions
    {
        [DllImport("ntdll.dll", SetLastError = true)]
        private static extern int RtlAdjustPrivilege(
               uint Privilege,
               bool Enable,
               bool CurrentThreadOnly,
               out bool PreviousValue
            );


        private static bool RunCommand(Session session, string command, int timeoutMS = 5000)
        {
            session.Log($"RunCommand: \"{command}\"");

            Process process = new Process();

            ProcessStartInfo info = new ProcessStartInfo();
            info.FileName = "cmd.exe";
            info.UseShellExecute = false;
            info.Verb = "runas";
            info.Arguments = $"/c \"{command}\"";
            info.RedirectStandardOutput = true;
            info.RedirectStandardError = true;
            info.CreateNoWindow = true;
            //info.WindowStyle = ProcessWindowStyle.Hidden;

            process.StartInfo = info;

            var OnOutputDataReceived = new DataReceivedEventHandler((sender, e) =>
            {
                session.Log(" INFO >> " + e.Data);
            });

            var OnErrorDataReceived = new DataReceivedEventHandler((sender, e) =>
            {
                session.Log(" ERR >> " + e.Data);
            });

            process.OutputDataReceived += OnOutputDataReceived;
            process.ErrorDataReceived += OnErrorDataReceived;

            if (process.Start())
            {
                process.BeginErrorReadLine();
                process.BeginOutputReadLine();

                if (!process.WaitForExit(timeoutMS))
                {
                    session.Log($"ERROR: Process \"{command}\" did not complete within {timeoutMS} milliseconds. Killing.");

                    process.Kill();
                    return false;
                }

                process.CancelErrorRead();
                process.CancelOutputRead();

                process.OutputDataReceived -= OnOutputDataReceived;
                process.ErrorDataReceived -= OnErrorDataReceived;

                var exitCode = process.ExitCode;
                process.Close();

                if (exitCode != 0)
                {
                    return false;
                }

                return true;
            }
            else
            {
                process.OutputDataReceived -= OnOutputDataReceived;
                process.ErrorDataReceived -= OnErrorDataReceived;

                // process didn't start

                session.Log($"ERROR: Unable to start process \"{command}\"");
                return false;
            }
        }

        // the /A is for Administrators group. Otherwise, it goes to a workgroup account
        private static bool TakeOwnershipOfFile(Session session, string fullFilePath)
        {
            var takeOwnCommand = "takeown /F /A " + fullFilePath;

            return RunCommand(session, takeOwnCommand);
        }

        private static bool GrantAdministratorFullFilePermissions(Session session, string fullFilePath)
        {
            var command = "icacls " + fullFilePath + " /grant:r *S-1-5-32-544:F";

            return RunCommand(session, command);

        }

        [CustomAction]
        public static ActionResult TakeWdmaud2Ownership(Session session)
        {
            session.Log("TakeWdmaud2Ownership: Started");

            string systemDriverPath = Environment.ExpandEnvironmentVariables(@"%systemroot%\System32\wdmaud2.drv");
            string systemDriverBakPath = Environment.ExpandEnvironmentVariables(@"%systemroot%\System32\wdmaud2.drv.bak");

            try
            {
                // check for current wdmaud2.drv. We don't do any backup
                // if there's not a current wdmaud2.drv
                if (File.Exists(systemDriverPath))
                {
                    session.Log($"TakeWdmaud2Ownership: {systemDriverPath} exists. Attempting to take ownership.");

                    // take ownership of wdmaud2.drv
                    if (!TakeOwnershipOfFile(session, systemDriverPath))
                    {
                        session.Log($"ERROR: TakeWdmaud2Ownership: Unable to take ownership of {systemDriverPath}");

                        // todo: set an error that the UI can show
                        return ActionResult.Failure;
                    }

                    session.Log($"TakeWdmaud2Ownership: Took ownership of {systemDriverPath}.");

                    // give permissions to admin group
                    if (!GrantAdministratorFullFilePermissions(session, systemDriverPath))
                    {
                        session.Log($"ERROR: TakeWdmaud2Ownership: Unable to grant administrator permissions to {systemDriverPath}");

                        // todo: set an error that the UI can show
                        return ActionResult.Failure;
                    }

                    session.Log($"TakeWdmaud2Ownership: Granted Administrator rights to replace {systemDriverPath}.");

                    // see if the backup is already there, and take ownership and delete if it is
                    if (File.Exists(systemDriverBakPath))
                    {
                        if (!TakeOwnershipOfFile(session, systemDriverBakPath))
                        {
                            session.Log($"ERROR: TakeWdmaud2Ownership: Unable to take ownership of {systemDriverBakPath}");

                            // todo: set an error that the UI can show
                            return ActionResult.Failure;
                        }
                        else
                        {
                            session.Log($"TakeWdmaud2Ownership: Took ownership of backup file {systemDriverBakPath}.");
                        }

                        if (!GrantAdministratorFullFilePermissions(session, systemDriverBakPath))
                        {
                            session.Log($"ERROR: TakeWdmaud2Ownership: Unable to grant administrator permissions to backup file {systemDriverBakPath}");

                            // todo: set an error that the UI can show
                            return ActionResult.Failure;
                        }
                        else
                        {
                            session.Log($"TakeWdmaud2Ownership: Granted Administrator rights to replace backup file {systemDriverBakPath}.");
                        }

                        session.Log($"TakeWdmaud2Ownership: Deleting older backup file {systemDriverBakPath}.");
                        File.Delete(systemDriverBakPath);
                    }


                    // rename current version to .bak
                    session.Log($"TakeWdmaud2Ownership: Backing up {systemDriverPath} to {systemDriverBakPath}.");
                    File.Move(systemDriverPath, systemDriverBakPath);
                }

                session.Log("TakeWdmaud2Ownership: Completed");

                // we don't do the actual file copy here, as that will happen in the regular installer
                return ActionResult.Success;
            }
            catch (Exception ex)
            {
                session.Log("ERROR: TakeWdmaud2Ownership: Exception trying to take ownership of wdmaud2.drv " + ex.ToString());

                // todo: set an error the UI can show
                return ActionResult.Failure;
            }
        }










        private enum RegLocation
        {
            HKCR,
            HKLM
        }

        // S-1-5-32-545 is the local users group (all users)
        //private static System.Security.Principal.SecurityIdentifier sid = new System.Security.Principal.SecurityIdentifier("S-1-5-32-545");

        // S-1-5-32-544 is local Administrators group
        private static System.Security.Principal.SecurityIdentifier sid = new System.Security.Principal.SecurityIdentifier("S-1-5-32-544");

        private static bool GrantAdministratorFullRegKeyPermissions(Session session, RegLocation location, string key, uint recurseLevel = 0)
        {
            // call RtlAdjustPrivilege to grant administrator rights
            // SeTakeOwnership, SeBackup, and SeRestore
            // recursive

            session.Log($"GrantAdministratorFullRegKeyPermissions: {recurseLevel} Granting reg key permissions to {location.ToString()}:{key}.");

            Microsoft.Win32.RegistryKey rootKey;

            switch (location)
            {
                case RegLocation.HKCR:
                    rootKey = RegistryKey.OpenBaseKey(RegistryHive.ClassesRoot, RegistryView.Registry64);
                    break;
                case RegLocation.HKLM:
                    rootKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
                    break;
                default:
                    return false;
            }

            session.Log($"GrantAdministratorFullRegKeyPermissions: {recurseLevel} Opening sub key {location.ToString()}:{key} to take ownership.");

            var regKey = rootKey.OpenSubKey(
                key,
                Microsoft.Win32.RegistryKeyPermissionCheck.ReadWriteSubTree,
                System.Security.AccessControl.RegistryRights.TakeOwnership);

            if (regKey != null)
            {
                session.Log($"GrantAdministratorFullRegKeyPermissions: {recurseLevel} Setting owner of {location.ToString()}:{key}.");
                var acl = new System.Security.AccessControl.RegistrySecurity();
                acl.SetOwner(sid);
                regKey.SetAccessControl(acl);

                session.Log($"GrantAdministratorFullRegKeyPermissions: {recurseLevel} Setting access rule protection for {location.ToString()}:{key}.");
                acl.SetAccessRuleProtection(false, false);
                regKey.SetAccessControl(acl);

                // only for top level keys, change permission for key and prop to subkeys
                if (recurseLevel == 0)
                {
                    session.Log($"GrantAdministratorFullRegKeyPermissions: {recurseLevel} opening null subkey for {location.ToString()}:{key}.");

                    regKey = regKey.OpenSubKey(
                        "",
                        Microsoft.Win32.RegistryKeyPermissionCheck.ReadWriteSubTree,
                        System.Security.AccessControl.RegistryRights.ChangePermissions);

                    session.Log($"GrantAdministratorFullRegKeyPermissions: {recurseLevel} setting registry access rule for {location.ToString()}:{key}.");

                    var rule = new System.Security.AccessControl.RegistryAccessRule(
                        sid,
                        System.Security.AccessControl.RegistryRights.FullControl,
                        System.Security.AccessControl.InheritanceFlags.ContainerInherit,
                        System.Security.AccessControl.PropagationFlags.None,
                        System.Security.AccessControl.AccessControlType.Allow);

                    acl.ResetAccessRule(rule);
                    regKey.SetAccessControl(acl);
                }


                // recurse
                foreach (var subKey in regKey.OpenSubKey("").GetSubKeyNames())
                {
                    GrantAdministratorFullRegKeyPermissions(session, location, key + "\\" + subKey, recurseLevel + 1);
                }

            }
            else
            {
                session.Log($"WARNING: Unable to open registry key {location.ToString()}:{key}. This is fine if a first install.");

                // we don't bail, because it just may mean the key is not there

                //return false;
            }

            return true;
        }

        [CustomAction]
        public static ActionResult TakeRegistryOwnership(Session session)
        {
            session.Log("TakeRegistryOwnership: Started");

            WindowsIdentity identity = WindowsIdentity.GetCurrent();
            WindowsPrincipal principal = new WindowsPrincipal(identity);

            session.Log($"TakeRegistryOwnership: Identity: \"{identity.Name}\" Is Administrator: \"{principal.IsInRole(WindowsBuiltInRole.Administrator)}\".");

            string[] hklmRegKeysToOwn =
            {
                @"SOFTWARE\Microsoft\Windows MIDI Services\Transport Plugins",
                @"SOFTWARE\Microsoft\Windows MIDI Services\Endpoint Processing Plugins",
            };

            string[] hkcrRegKeysToOwn =
            {
                @"CLSID\{0f273b18-e372-4d95-87ac-c31c3d22e937}",  // KS Aggregate Transport
                @"CLSID\{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}",  // KS Transport
                @"CLSID\{942BF02D-93C0-4EA8-B03E-D51156CA75E1}",  // Loopback Transport
                @"CLSID\{8FEAAD91-70E1-4A19-997A-377720A719C1}",  // Virtual MIDI Transport
                @"CLSID\{ac9b5417-3fe0-4e62-960f-034ee4235a1a}",  // Diagnostics Transport
                @"CLSID\{5dc87270-f318-4838-a4f9-6aadc63e925f}",  // Bluetooth MIDI Transport
                @"CLSID\{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}",  // Network MIDI 2.0 Transport
                @"CLSID\{10088473-9478-4E62-850B-3D2315E135B8}",  // Basic Loopback MIDI Transport

                @"CLSID\{2BA15E4E-5417-4A66-85B8-2B2260EFBC84}",  // Main Midisrv Transport

                @"CLSID\{A8798C54-6066-45F0-9ADB-648BC0641ABF}",  // Bytestream 2 UMP Transform
                @"CLSID\{96121591-8D68-479F-9B48-2BF0B90113F7}",  // UMP 2 Bytestream Transform
                @"CLSID\{a42cde44-7fa9-4597-a8ee-b40b96bcddb1}",  // Message Scheduler
                @"CLSID\{dc638b31-cf31-48ed-9e79-02740bf5d013}",  // Protocol Downscaler
            };

            const uint SeTakeOwnership = 9;
            const uint SeBackup = 17;
            const uint SeRestore = 18;

            int privSucceeded;

            bool enabledSeTakeOwnership = false;
            privSucceeded = RtlAdjustPrivilege(SeTakeOwnership, true, false, out enabledSeTakeOwnership);
            if (privSucceeded < 0)
            {
                session.Log($"ERROR: TakeRegistryOwnership: unable to RtlAdjustPrivilege SeTakeOwnership");
                return ActionResult.Failure;
            }

            bool enabledSeBackup = false;
            privSucceeded = RtlAdjustPrivilege(SeBackup, true, false, out enabledSeBackup);
            if (privSucceeded < 0)
            {
                session.Log($"ERROR: TakeRegistryOwnership: unable to RtlAdjustPrivilege SeBackup");
                return ActionResult.Failure;
            }

            bool enabledSeRestore = false;
            privSucceeded = RtlAdjustPrivilege(SeRestore, true, false, out enabledSeRestore);
            if (privSucceeded < 0)
            {
                session.Log($"ERROR: TakeRegistryOwnership: unable to RtlAdjustPrivilege SeRestore");
                return ActionResult.Failure;
            }


            foreach (var regkey in hklmRegKeysToOwn)
            {
                bool result = GrantAdministratorFullRegKeyPermissions(session, RegLocation.HKLM, regkey);

                if (!result)
                {
                    session.Log($"ERROR: TakeRegistryOwnership: unable to grant administrator permissions to HKLM {regkey}");
                    return ActionResult.Failure;
                }

            }

            foreach (var regkey in hkcrRegKeysToOwn)
            {
                bool result = GrantAdministratorFullRegKeyPermissions(session, RegLocation.HKCR, regkey);

                if (!result)
                {
                    session.Log($"ERROR: TakeRegistryOwnership: unable to grant administrator permissions to HKCR {regkey}");
                    return ActionResult.Failure;
                }
            }

            // restore to original values

            session.Log("TakeRegistryOwnership: Restoring original process privileges");

            privSucceeded = RtlAdjustPrivilege(SeTakeOwnership, enabledSeTakeOwnership, false, out enabledSeTakeOwnership);
            privSucceeded = RtlAdjustPrivilege(SeBackup, enabledSeBackup, false, out enabledSeBackup);
            privSucceeded = RtlAdjustPrivilege(SeRestore, enabledSeRestore, false, out enabledSeRestore);

            session.Log("TakeRegistryOwnership: Completed");

            return ActionResult.Success;
        }


        [CustomAction]
        public static ActionResult RemoveInBoxService(Session session)
        {
            session.Log("RemoveInBoxService: Started");

            session.Log("RemoveInBoxService: (Service removal not yet implemented)");

            // we ignore return codes here because the service may not have previously existed
            RunCommand(session, "sc stop midisrv", 20000);
            RunCommand(session, "sc delete midisrv", 20000);

            session.Log("RemoveInBoxService: Completed");

            return ActionResult.Success;
        }


        // ------------------------------------------------------------------------------------
        // Transport COM registration.
        //
        // A preview transport and the in-box one share a CLSID. Uninstalling the preview after the
        // in-box version has shipped used to delete that CLSID and the transport plugin key
        // outright, which left the in-box transport unable to load. Self-registration cannot avoid
        // that, because DllUnregisterServer removes the key whoever happens to own it, so
        // registration is done here instead and removal only happens for a registration this
        // installer still owns.
        // ------------------------------------------------------------------------------------

        private const string TransportPluginsKeyPath = @"SOFTWARE\Microsoft\Windows MIDI Services\Transport Plugins";

        // The registry view is stated rather than inherited from the process, so that a 32-bit
        // custom action host could not quietly read and write the WOW6432Node copy of a
        // registration the 64-bit service will never see.
        private static RegistryKey OpenClassesRoot64()
        {
            return RegistryKey.OpenBaseKey(RegistryHive.ClassesRoot, RegistryView.Registry64);
        }

        private static RegistryKey OpenLocalMachine64()
        {
            return RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
        }

        private static string ReadInprocServer32(string clsid)
        {
            using (var root = OpenClassesRoot64())
            using (var key = root.OpenSubKey($@"CLSID\{clsid}\InprocServer32"))
            {
                return key?.GetValue(null) as string;
            }
        }

        // Older installers stamped an explicit ACL on these keys with a WiX Permission element,
        // which REPLACES the ACL rather than adding to it. That granted Administrators and removed
        // read access for everybody else, so midisrv running as LOCAL SERVICE could not see the
        // plugin and silently loaded no transport. MSI never repairs the security descriptor of a
        // key that already exists, so machines that took that ACL stay broken through every
        // upgrade unless something explicitly turns inheritance back on.
        private static void EnsureInheritedRegistryAcl(Session session, string subKeyPath)
        {
            try
            {
                using (var hklm = OpenLocalMachine64())
                // A handle from CreateSubKey carries neither READ_CONTROL nor WRITE_DAC, so the
                // key is reopened with exactly the rights the ACL work needs.
                using (var key = hklm.OpenSubKey(
                    subKeyPath,
                    RegistryKeyPermissionCheck.ReadWriteSubTree,
                    System.Security.AccessControl.RegistryRights.ReadPermissions |
                    System.Security.AccessControl.RegistryRights.ChangePermissions))
                {
                    if (key == null)
                    {
                        session.Log($"EnsureInheritedRegistryAcl: {subKeyPath} could not be opened for ACL inspection.");
                        return;
                    }

                    var acl = key.GetAccessControl(System.Security.AccessControl.AccessControlSections.Access);

                    if (!acl.AreAccessRulesProtected)
                    {
                        session.Log($"EnsureInheritedRegistryAcl: {subKeyPath} already inherits its ACL.");
                        return;
                    }

                    session.Log($"WARNING: EnsureInheritedRegistryAcl: {subKeyPath} has a protected ACL, most likely stamped by an older installer. Re-enabling inheritance.");

                    acl.SetAccessRuleProtection(false, false);
                    key.SetAccessControl(acl);

                    session.Log($"EnsureInheritedRegistryAcl: {subKeyPath} now inherits from its parent.");
                }
            }
            catch (Exception ex)
            {
                // Not fatal. A wrong ACL stops the service seeing the plugin, but failing the
                // install leaves the machine in a worse state than logging it does.
                session.Log($"ERROR: EnsureInheritedRegistryAcl: could not repair the ACL on {subKeyPath}. {ex.Message}");
            }
        }

        [CustomAction]
        public static ActionResult RegisterTransportComServer(Session session)
        {
            // Set by the SetProperty immediately before this action in the wxs
            var data = session.CustomActionData;

            string dllPath = data["DllPath"];
            string clsid = data["Clsid"];
            string pluginKeyName = data["PluginKeyName"];

            session.Log($"RegisterTransportComServer: dll=\"{dllPath}\" clsid=\"{clsid}\" plugin=\"{pluginKeyName}\"");

            if (!File.Exists(dllPath))
            {
                session.Log($"ERROR: RegisterTransportComServer: \"{dllPath}\" does not exist.");
                return ActionResult.Failure;
            }

            if (!RunCommand(session, $"regsvr32.exe /s \"{dllPath}\"", 30000))
            {
                session.Log("ERROR: RegisterTransportComServer: regsvr32 failed.");
                return ActionResult.Failure;
            }

            // RunCommand does not surface a non-zero exit code, so the registration is confirmed by
            // reading it back rather than by trusting regsvr32.
            string registered = ReadInprocServer32(clsid);

            if (!string.Equals(registered?.Trim('"'), dllPath, StringComparison.OrdinalIgnoreCase))
            {
                session.Log($"ERROR: RegisterTransportComServer: after registering, the CLSID reads back as \"{registered}\".");
                return ActionResult.Failure;
            }

            try
            {
                // No explicit ACL is written here, deliberately. Inheriting from the parent gives
                // Administrators and SYSTEM full control and Users read, and LOCAL SERVICE is a
                // member of Users, which is how midisrv gets to see the plugin at all.
                using (var hklm = OpenLocalMachine64())
                using (var plugins = hklm.CreateSubKey(TransportPluginsKeyPath))
                using (var plugin = plugins.CreateSubKey(pluginKeyName))
                {
                    plugin.SetValue("CLSID", clsid, RegistryValueKind.String);
                    plugin.SetValue("Enabled", 1, RegistryValueKind.DWord);
                }

                EnsureInheritedRegistryAcl(session, TransportPluginsKeyPath);
                EnsureInheritedRegistryAcl(session, $@"{TransportPluginsKeyPath}\{pluginKeyName}");
            }
            catch (Exception ex)
            {
                session.Log($"ERROR: RegisterTransportComServer: writing the plugin key failed. {ex.Message}");
                return ActionResult.Failure;
            }

            session.Log("RegisterTransportComServer: Completed");

            return ActionResult.Success;
        }

        [CustomAction]
        public static ActionResult UnregisterTransportComServerIfOwned(Session session)
        {
            var data = session.CustomActionData;

            string dllPath = data["DllPath"];
            string clsid = data["Clsid"];
            string pluginKeyName = data["PluginKeyName"];

            session.Log($"UnregisterTransportComServerIfOwned: dll=\"{dllPath}\" clsid=\"{clsid}\"");

            string registered = null;

            try
            {
                registered = ReadInprocServer32(clsid);
            }
            catch (Exception ex)
            {
                session.Log($"UnregisterTransportComServerIfOwned: could not read the registration. {ex.Message}");
            }

            if (string.IsNullOrEmpty(registered))
            {
                session.Log("UnregisterTransportComServerIfOwned: nothing is registered. Leaving everything alone.");
                return ActionResult.Success;
            }

            // Ownership is decided by the path, not by whether it looks like an in-box location.
            // Anything other than the file this installer placed belongs to somebody else, and
            // removing it is what broke the in-box transport before.
            if (!string.Equals(registered.Trim('"'), dllPath, StringComparison.OrdinalIgnoreCase))
            {
                session.Log(
                    $"UnregisterTransportComServerIfOwned: the CLSID points at \"{registered}\", " +
                    $"not at \"{dllPath}\". Another copy of this transport owns it, most likely an " +
                    "in-box one, so the COM registration and the transport plugin key are being left " +
                    "in place. Removing them here is what previously left the in-box transport unable to load.");

                return ActionResult.Success;
            }

            if (!File.Exists(dllPath))
            {
                session.Log($"UnregisterTransportComServerIfOwned: \"{dllPath}\" is already gone, so it cannot self-unregister. Removing the keys directly.");

                try
                {
                    using (var root = OpenClassesRoot64())
                    {
                        root.DeleteSubKeyTree($@"CLSID\{clsid}", false);
                    }
                }
                catch (Exception ex)
                {
                    session.Log($"UnregisterTransportComServerIfOwned: could not remove the CLSID. {ex.Message}");
                }
            }
            else if (!RunCommand(session, $"regsvr32.exe /s /u \"{dllPath}\"", 30000))
            {
                // Not fatal. Leaving a stale CLSID behind is better than failing the uninstall.
                session.Log("UnregisterTransportComServerIfOwned: regsvr32 /u reported a failure.");
            }

            try
            {
                using (var hklm = OpenLocalMachine64())
                using (var plugins = hklm.OpenSubKey(TransportPluginsKeyPath, true))
                {
                    plugins?.DeleteSubKeyTree(pluginKeyName, false);
                }
            }
            catch (Exception ex)
            {
                session.Log($"UnregisterTransportComServerIfOwned: could not remove the plugin key. {ex.Message}");
            }

            session.Log("UnregisterTransportComServerIfOwned: Completed");

            return ActionResult.Success;
        }

    }
}
