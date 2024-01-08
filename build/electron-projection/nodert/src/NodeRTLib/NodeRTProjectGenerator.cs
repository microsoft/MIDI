// Copyright (c) The NodeRT Contributors
// All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the ""License""); you may
// not use this file except in compliance with the License. You may obtain a
// copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS
// OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY
// IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
//
// See the Apache Version 2.0 License for specific language governing permissions
// and limitations under the License.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Linq;

namespace NodeRTLib
{
    public enum VsVersions
    {
        Vs2012,
        Vs2013,
        Vs2015,
        Vs2017,
        Vs2019,
        Vs2022
    }

    public enum WinVersions
    {
        v8,
        v8_1,
        v10
    }

    public class NodeRTProjectGenerator
    {
        private WinVersions _winVersion;
        private VsVersions _vsVersion;
        private bool _isGenerateDef;

        public NodeRTProjectGenerator(WinVersions winVersion, VsVersions vsVersion, bool isGenerateDef)
        {
            _winVersion = winVersion;
            _vsVersion = vsVersion;
            _isGenerateDef = isGenerateDef;
        }

        public static bool TryParseWinVersion(string winVerString, out WinVersions winVer)
        {
            switch (winVerString)
            {
                case "8":
                    winVer = WinVersions.v8;
                    return true;
                case "8.1":
                    winVer = WinVersions.v8_1;
                    return true;
                case "10":
                    winVer = WinVersions.v10;
                    return true;
                default:
                    // set to some default value
                    winVer = 0;
                    return false;
            }
        }

        public static bool VerifyVsAndWinVersions(WinVersions winVersion, VsVersions vsVersion, out string errorMessage)
        {
            errorMessage = null;

            if (vsVersion == VsVersions.Vs2013 && winVersion == WinVersions.v10)
            {
                errorMessage = "VS 2013 does not support building Windows 10 modules";
                return false;
            }

            if (vsVersion == VsVersions.Vs2012 && (winVersion == WinVersions.v8_1 || winVersion == WinVersions.v10))
            {
                errorMessage = "VS 2012 only supports building Windows 8 modules";
                return false;
            }

            return true;
        }

        private string VsVersionToString(VsVersions vsVersion)
        {
            switch (vsVersion)
            {
                case VsVersions.Vs2012:
                    return "2012";
                case VsVersions.Vs2013:
                    return "2013";
                case VsVersions.Vs2015:
                    return "2015";
                case VsVersions.Vs2017:
                    return "2017";
                default:
                    return "2019";
            }
        }

        private string WinVersionToString(WinVersions winVersion)
        {
            switch (winVersion)
            {
                case WinVersions.v8:
                    return "8";
                case WinVersions.v8_1:
                    return "8.1";
                default:
                    return "10";
            }
        }

        public string GenerateProject(string winRTNamespace, string destinationFolder, string winRtFile, string npmPackageVersion, string npmScope, dynamic mainModel)
        {
            string projectName = "NodeRT_" + winRTNamespace.Replace(".", "_");

            if (!Directory.Exists(destinationFolder))
            {
                Directory.CreateDirectory(destinationFolder);
            }

            string outputFileName = "_nodert_generated.cpp";
            using (var writer = new StreamWriter(Path.Combine(destinationFolder, outputFileName)))
            {
                writer.Write(TX.CppTemplates.Wrapper(mainModel));
            }

            if (_isGenerateDef)
            {
                string libDirPath = Path.Combine(destinationFolder, "lib");

                if (!Directory.Exists(libDirPath))
                {
                    Directory.CreateDirectory(libDirPath);
                }

                using (var writer = new StreamWriter(Path.Combine(libDirPath, projectName + ".d.js")))
                {
                    writer.Write(TX.JsDefinitionTemplates.Wrapper(mainModel));
                }

                using (var writer = new StreamWriter(Path.Combine(libDirPath, projectName + ".d.ts")))
                {
                    writer.Write(TX.TsDefinitionTemplates.Wrapper(mainModel));
                }
            }


            StringBuilder bindingFileText = new StringBuilder(File.ReadAllText(Path.Combine(AppDomain.CurrentDomain.SetupInformation.ApplicationBase, @"ProjectTemplates\binding.gyp")));

            bindingFileText.Replace("{ProjectName}", projectName);

            ResolveWinrtDirsAndCompiler(bindingFileText, winRtFile);

            var bindingPath = Path.Combine(destinationFolder, "binding.gyp");
            File.WriteAllText(bindingPath, bindingFileText.ToString());
            File.Copy(Path.Combine(AppDomain.CurrentDomain.SetupInformation.ApplicationBase, @"ProjectTemplates\common.gypi"), Path.Combine(destinationFolder, "common.gypi"), true);

            CopyProjectFiles(destinationFolder);

            if (String.IsNullOrEmpty(npmPackageVersion))
            {
                npmPackageVersion = "0.1.0";
            }

            CopyAndGenerateJsPackageFiles(destinationFolder, winRTNamespace, projectName,
                npmPackageVersion, npmScope, WinVersionToString(_winVersion), VsVersionToString(_vsVersion), mainModel);

            return destinationFolder;
        }

        private static string CreateNpmPackageName(String namepsace, String npmScope)
        {
            if (String.IsNullOrWhiteSpace(npmScope))
            {
                return namepsace.ToLowerInvariant();
            }

            return String.Format("@{0}/{1}", npmScope, namepsace.ToLowerInvariant());
        }

        protected void ResolveWinrtDirsAndCompiler(StringBuilder bindingFileText, string winrtFile)
        {
            string directoryName = Path.GetDirectoryName(winrtFile).ToLower();

            if (_winVersion == WinVersions.v8)
            {
                bindingFileText.Replace("{WinVer}", "v8.0");
            }
            else if (_winVersion == WinVersions.v8_1)
            {
                bindingFileText.Replace("{WinVer}", "v8.1");
            }
            else if (_winVersion == WinVersions.v10)
            {
                bindingFileText.Replace("{WinVer}", "v10");
            }

            // We need to find the _actual_ directory.
            if (!directoryName.EndsWith(@"windows kits\8.1\references\commonconfiguration\neutral") &&
                !directoryName.EndsWith(@"windows kits\8.0\references\commonconfiguration\neutral") &&
                !directoryName.EndsWith(@"windows kits\10\unionmetadata"))
            {
                var winmdDirectory = Path.GetDirectoryName(winrtFile);
                var additionalWinmdPath = winmdDirectory.Replace('\\', '/');

                // If the winmd file is in "%ProgramFiles%/Windows Kits/10/UnionMetadata/10.0.17134.0/,
                // we want the "10.0.17134.0" portion
                var winVer = new DirectoryInfo(winmdDirectory).Name;

                var additionalPaths = new[]
                {
                    "%ProgramFiles%/Windows Kits/10/UnionMetadata/" + winVer,
                    "%ProgramFiles%/Windows Kits/10/Include/" + winVer + "/um",
                    "%ProgramFiles(x86)%/Windows Kits/10/UnionMetadata/" + winVer,
                    "%ProgramFiles(x86)%/Windows Kits/10/Include/" + winVer + "/um",
                }.Select(p => "\"" + p + "\"").Aggregate((current, next) => current + ",\n              " + next);

                bindingFileText.Replace("{AdditionalWinmdPaths}", additionalPaths);
                bindingFileText.Replace("{UseAdditionalWinmd}", "true");
            }
            else
            {
                var additionalPaths = new[]
                {
                    "%ProgramFiles%/Windows Kits/10/UnionMetadata/",
                    "%ProgramFiles(x86)%/Windows Kits/10/UnionMetadata/",
                }.Select(p => "\"" + p + "\"").Aggregate((current, next) => current + ",\n              " + next);

                bindingFileText.Replace("{AdditionalWinmdPaths}", additionalPaths);
                bindingFileText.Replace("{UseAdditionalWinmd}", "true");
            }
        }

        private void CopyAndGenerateJsPackageFiles(string destinationFolder, string winRTNamespace, string projectName, string npmPackageVersion, string npmScope, string winVersion, string vsVersion, dynamic mainModel)
        {
            string npmPackageName = CreateNpmPackageName(winRTNamespace, npmScope);
            string libDirPath = Path.Combine(destinationFolder, "lib");
            if (!Directory.Exists(libDirPath))
            {
                Directory.CreateDirectory(libDirPath);
            }

            // write the main.js file:
            StringBuilder mainJsFileText = new StringBuilder(File.ReadAllText(Path.Combine(AppDomain.CurrentDomain.SetupInformation.ApplicationBase, @"JsPackageFiles\main.js")));
            mainJsFileText.Replace("{ProjectName}", projectName);
            if (npmScope == null)
            {
                npmScope = "";
            }
            mainJsFileText.Replace("{NpmScope}", npmScope);
            StringBuilder referencedNamespacesListBuilder = new StringBuilder();
            foreach (string ns in mainModel.ExternalReferencedNamespaces)
            {
                if (referencedNamespacesListBuilder.Length > 0)
                    referencedNamespacesListBuilder.Append(", ");
                referencedNamespacesListBuilder.Append("'" + ns + "'");
            }

            mainJsFileText.Replace("{ExternalReferencedNamespaces}", referencedNamespacesListBuilder.ToString());

            File.WriteAllText(Path.Combine(libDirPath, "main.js"), mainJsFileText.ToString());

            // write the README.md file
            string readmeTemplatePath;
            if (_vsVersion == VsVersions.Vs2019)
                readmeTemplatePath = Path.Combine(AppDomain.CurrentDomain.SetupInformation.ApplicationBase, @"JsPackageFiles\README.vs2019.md");
            else
                readmeTemplatePath = Path.Combine(AppDomain.CurrentDomain.SetupInformation.ApplicationBase, @"JsPackageFiles\README.md");

            StringBuilder readmeFileText = new StringBuilder(File.ReadAllText(readmeTemplatePath));
            readmeFileText.Replace("{Namespace}", winRTNamespace);
            readmeFileText.Replace("{ModuleName}", winRTNamespace.ToLowerInvariant());
            readmeFileText.Replace("{PackageName}", npmPackageName);
            readmeFileText.Replace("{WinVersion}", winVersion);
            readmeFileText.Replace("{VSVersion}", vsVersion);

            File.WriteAllText(Path.Combine(destinationFolder, "README.md"), readmeFileText.ToString());

            // write the package.json file:
            string packageJsonTemplatePath;
            if (_vsVersion == VsVersions.Vs2019 || _vsVersion == VsVersions.Vs2022)
                packageJsonTemplatePath = Path.Combine(AppDomain.CurrentDomain.SetupInformation.ApplicationBase, @"JsPackageFiles\package.vs2019.json");
            else
                packageJsonTemplatePath = Path.Combine(AppDomain.CurrentDomain.SetupInformation.ApplicationBase, @"JsPackageFiles\package.json");

            StringBuilder packageJsonFileText = new StringBuilder(File.ReadAllText(packageJsonTemplatePath));
            packageJsonFileText.Replace("{Namespace}", winRTNamespace);
            packageJsonFileText.Replace("{PackageName}", npmPackageName);
            packageJsonFileText.Replace("{PackageVersion}", npmPackageVersion);
            packageJsonFileText.Replace("{Keywords}", GeneratePackageKeywords(mainModel, winRTNamespace));
            packageJsonFileText.Replace("{Dependencies}", GeneratePackageDependencies(mainModel.ExternalReferencedNamespaces, npmScope, npmPackageVersion));

            if (_vsVersion == VsVersions.Vs2012)
                packageJsonFileText.Replace("{VSVersion}", "2012");
            else if (_vsVersion == VsVersions.Vs2013)
                packageJsonFileText.Replace("{VSVersion}", "2013");
            else if (_vsVersion == VsVersions.Vs2015)
                packageJsonFileText.Replace("{VSVersion}", "2015");
            else if (_vsVersion == VsVersions.Vs2017)
                packageJsonFileText.Replace("{VSVersion}", "2017");
            else if (_vsVersion == VsVersions.Vs2019)
                packageJsonFileText.Replace("{VSVersion}", "2019");
            else
                packageJsonFileText.Replace("{VSVersion}", "2022");

            File.WriteAllText(Path.Combine(destinationFolder, "package.json"), packageJsonFileText.ToString());

            // copy the .npmignore and license files
            File.Copy(Path.Combine(AppDomain.CurrentDomain.SetupInformation.ApplicationBase, @"JsPackageFiles\.npmignore"),
                Path.Combine(destinationFolder, ".npmignore"), true);
            File.Copy(Path.Combine(AppDomain.CurrentDomain.SetupInformation.ApplicationBase, @"JsPackageFiles\LICENSE"),
                Path.Combine(destinationFolder, "LICENSE"), true);
        }

        private string GeneratePackageKeywords(dynamic mainModel, string winRtNamespace)
        {
            StringBuilder keywordsBuilder = new StringBuilder();
            keywordsBuilder.Append("\"" + winRtNamespace + "\"");

            foreach (string str in winRtNamespace.Split('.'))
            {
                keywordsBuilder.Append(",\r\n    \"" + str + "\"");
            }

            foreach (var c in mainModel.Types)
            {
                keywordsBuilder.Append(",\r\n    \"" + c.Key.Name + "\"");
            }

            foreach (var e in mainModel.Enums)
            {
                keywordsBuilder.Append(",\r\n    \"" + e.Name + "\"");
            }

            foreach (var v in mainModel.ValueTypes)
            {
                keywordsBuilder.Append(",\r\n    \"" + v.Name + "\"");
            }

            return keywordsBuilder.ToString();
        }

        private string GeneratePackageDependencies(List<String> externalReferencedNamespaces, string npmScope, string npmVersion)
        {
            if (npmVersion == null)
                npmVersion = "*";
            StringBuilder depsBuilder = new StringBuilder();

            foreach (string ns in externalReferencedNamespaces)
            {
                if (depsBuilder.Length > 0)
                    depsBuilder.Append(",\r\n    ");
                depsBuilder.Append("\"" + CreateNpmPackageName(ns, npmScope) + "\" : \"" + npmVersion + "\"");
            }

            return depsBuilder.ToString();
        }

        private void CopyProjectFiles(string destinationFolder)
        {
            string dirPath = Path.Combine(AppDomain.CurrentDomain.SetupInformation.ApplicationBase, "ProjectFiles");
            CopyDirRecurse(dirPath, destinationFolder);

        }

        private void CopyDirRecurse(string srcDir, string destDir)
        {
            string[] files = Directory.GetFiles(srcDir);

            foreach (string file in files)
            {
                try
                {
                    File.Copy(file, Path.Combine(destDir, Path.GetFileName(file)), true);
                }
                catch (Exception e)
                {
                    throw e;
                }
            }

            string[] dirs = Directory.GetDirectories(srcDir);
            foreach (string dir in dirs)
            {
                string newDirPath = Path.Combine(destDir, Path.GetFileName(dir));
                try
                {
                    Directory.CreateDirectory(newDirPath);
                    CopyDirRecurse(srcDir, newDirPath);
                }
                catch (Exception e)
                {
                    throw e;
                }
            }
        }
    }
}
