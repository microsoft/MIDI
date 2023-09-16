// THIS IS A DEVELOPER-ONLY WORKAROUND THAT IS NOT ENDORSED OR SUPPORTED BY MICROSOFT

using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Drawing.Text;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Security.Principal;
using WixToolset.Dtf.WindowsInstaller;

namespace RegistryCustomActions
{
    public class CustomActions
    {
        [CustomAction()]
        public static ActionResult GrantPermissions(Session session)
        {
            session.Log("REGKEYHACK: GrantPermissions");

            TakeKeyPermissions(session);

            session.Log("REGKEYHACK: GrantPermissions Leaving");

            return ActionResult.Success;
        }

        [CustomAction()]
        public static ActionResult ReturnToDefaultPermissions(Session session)
        {
            session.Log("REGKEYHACK: ReturnToDefaultPermissions");

            session.Log("REGKEYHACK: ReturnToDefaultPermissions Leaving");

            return ActionResult.Success;

        }




        //const string rootKey = "SOFTWARE\\Microsoft\\WindowsRuntime\\ActivatableClassId";
        const string rootKey = "SOFTWARE\\Microsoft\\WindowsRuntime";

        //const string administratorsGroupSID = "S-1-5-32-544";
        const string trustedInstallerSID = "S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464";    // NT SERVICE\TrustedInstaller


        // Code shamelessly taken from https://stackoverflow.com/questions/5510311/how-can-i-manipulate-token-privileges-in-net
        // which is based on this 2005 MSDN article http://msdn.microsoft.com/en-us/magazine/cc163823.aspx



        private static void TakeKeyPermissions(Session session)
        {
            //bool previousValue;

            //session.Log("REGKEYHACK: Adjusting privileges with RtlAdjustPrivilege");
            // give them permission to take ownership
            //var ownershipResult = RtlAdjustPrivilege(SeTakeOwnership, true, false, out previousValue);
            //var backupResult = RtlAdjustPrivilege(SeBackup, true, false, out previousValue);
            //var restoreResult = RtlAdjustPrivilege(SeRestore, true, false, out previousValue);
            //session.Log($"REGKEYHACK: ownershipResult={ownershipResult.ToString("X8")}, backupResult={backupResult.ToString("X8")}, restoreResult={restoreResult.ToString("X8")}");

            var privs = new[] { Privilege.TakeOwnership, Privilege.Backup, Privilege.Restore };

            session.Log("REGKEYHACK: Attempting to run with privileges");

            Privilege.RunWithPrivileges(() =>
            {
                session.Log("REGKEYHACK: Getting current user");

                // get the current running user 
                var currentUserIdentity = WindowsIdentity.GetCurrent();

                if (currentUserIdentity == null)
                {
                    session.Log($"REGKEYHACK: Current user identity is null");
                    return;
                }
                session.Log($"REGKEYHACK: Current user={currentUserIdentity.Name}");

                //string groups = string.Empty;
                //foreach (var group in currentUserIdentity.Groups)
                //    groups += group.ToString() + ", ";

                //session.Log($"REGKEYHACK: Groups={groups}");

                // in case the installer is launching us in a 32 bit process
                var baseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
                if (baseKey == null)
                {
                    session.Log($"REGKEYHACK: Base HKLM key is null");
                    return;
                }

                // Let's get to that registry entry
                session.Log($"REGKEYHACK: Attempting to open registry key {baseKey.ToString()} {rootKey}");
                var regKey = baseKey.OpenSubKey(
                    rootKey, 
                    Microsoft.Win32.RegistryKeyPermissionCheck.ReadWriteSubTree,
                    System.Security.AccessControl.RegistryRights.TakeOwnership);

                if (regKey == null)
                {
                    session.Log($"REGKEYHACK: reg key is null");
                    return;
                }
                else
                {
                    session.Log($"REGKEYHACK: key opened");
                }

                var registrySecurity = regKey.GetAccessControl(System.Security.AccessControl.AccessControlSections.All);

                if (registrySecurity == null)
                {
                    session.Log($"REGKEYHACK: GetAccessControl return null ACL");
                    return;
                }

                // and now take ownership so we can write to it. Normally, this key and all subkeys
                // are owned by TrustedInstaller. This is something we're doing ONLY for a limited 
                // developer preview. This should never be done on a production machine as it 
                // compromises WinRT / UWP security.

                session.Log("REGKEYHACK: Setting the reg key owner to the current user");
                registrySecurity.SetOwner(currentUserIdentity.User);
                regKey.SetAccessControl(registrySecurity);

                session.Log("REGKEYHACK: Setting access rule protection on that reg key");
                registrySecurity.SetAccessRuleProtection(false, false);
                regKey.SetAccessControl(registrySecurity);

                session.Log("REGKEYHACK: Creating the full access rule");
                var fullAccessRule = new RegistryAccessRule(
                    currentUserIdentity.Name,
                    RegistryRights.FullControl,
                    InheritanceFlags.ContainerInherit,
                    PropagationFlags.None,
                    AccessControlType.Allow);

                if (fullAccessRule == null)
                {
                    session.Log($"REGKEYHACK: Creating Registry Access Rule return null");
                    return;
                }
                registrySecurity.AddAccessRule(fullAccessRule);

                session.Log("REGKEYHACK: Setting ACL on the key to allow full access by the current user");
                regKey.SetAccessControl(registrySecurity);

                session.Log("REGKEYHACK: Complete");
            }, privs);
        }
    }
}
