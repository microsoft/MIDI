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

namespace NodeRTCmd
{
    using System.IO;
    using NodeRTLib;

    class Program
    {
        static void Main(string[] args)
        {
            var argsDictionary = ParseCommandLineArgs(args);

            if (argsDictionary.ContainsKey("help"))
            {
                PrintHelp();
                return;
            }

            if (!ValidateArguments(argsDictionary))
            {
                PrintHelpAndExitWithError();
                return;
            }

            if (argsDictionary.ContainsKey("namespaces"))
            {
                PrintNamespaces(argsDictionary);
                return;
            }

            string winmd = argsDictionary["winmd"];
            string outDir = argsDictionary["outdir"];

            bool noDefGen = argsDictionary.ContainsKey("nodefgen");
            bool noBuild = argsDictionary.ContainsKey("nobuild");
            bool verbose = argsDictionary.ContainsKey("verbose");

            if (!Directory.Exists(outDir))
            {
                Directory.CreateDirectory(outDir);
            }

            string ns = ValueOrNull(argsDictionary, "namespace");
            string customWinMdDir = ValueOrNull(argsDictionary, "customwinmddir");

            VsVersions vsVersion = VsVersions.Vs2022;
            WinVersions winVersion = WinVersions.v10;

            if (argsDictionary.ContainsKey("vs"))
            {
                if (!Enum.TryParse<VsVersions>(argsDictionary["vs"], true, out vsVersion))
                {
                    Console.WriteLine("Unsupported VS version. Supported options are: VS2022, VS2019, VS2017, VS2015, VS2013, VS2012");
                    Environment.Exit(1);
                }
            }

            if (argsDictionary.ContainsKey("winver"))
            {
                if (!NodeRTProjectGenerator.TryParseWinVersion(argsDictionary["winver"], out winVersion))
                {
                    Console.WriteLine("Unssuported Windows version. Supported options are: 10, 8.1, 8");
                    Environment.Exit(1);
                }
            }

            string npmPackageVersion = null;

            if (argsDictionary.ContainsKey("npmversion"))
            {
                npmPackageVersion = argsDictionary["npmversion"];
            }

            string npmPackageScope = null;
            if (argsDictionary.ContainsKey("npmscope"))
            {
                npmPackageScope = argsDictionary["npmscope"];
            }

            String errorMessage = null;
            if (!NodeRTProjectGenerator.VerifyVsAndWinVersions(winVersion, vsVersion, out errorMessage))
            {
                Console.WriteLine("Unssuported Windows and VS versions combination.");
                Console.WriteLine(errorMessage);
                Environment.Exit(1);
            }

            // generate specific namespace
            if (!String.IsNullOrEmpty(ns))
            {
                GenerateAndBuildNamespace(ns, vsVersion, winVersion, winmd,
                    npmPackageVersion, npmPackageScope,
                    outDir, noDefGen, noBuild, verbose);
            }
            else // try to generate & build all namespaces in winmd file
            {
                List<String> failedList = new List<string>();
                Console.WriteLine("Started to generate and build all namespaces in given WinMD...\n");
                foreach (string winRtNamespace in Reflector.GetNamespaces(winmd, customWinMdDir))
                {
                    if (!GenerateAndBuildNamespace(winRtNamespace,
                          vsVersion, winVersion, winmd,
                          npmPackageVersion, npmPackageScope, outDir, noDefGen, noBuild, verbose))
                    {
                        failedList.Add(winRtNamespace);
                    }
                    Console.WriteLine();
                }

                if (failedList.Count > 0)
                {
                    Console.WriteLine("Failed to generate or build for the following namespaces:");
                    foreach (string winRtNamespace in failedList)
                    {
                        Console.WriteLine("\t{0}", winRtNamespace);
                    }
                }
                else
                {
                    Console.WriteLine("Finished!");
                }
            }
        }

        static void PrintNamespaces(Dictionary<string, string> args)
        {
            string customWinMdDir = ValueOrNull(args, "customwinmddir");

            foreach (string str in Reflector.GetNamespaces(args["winmd"], customWinMdDir))
            {
                Console.WriteLine(str);
            }
        }

        static string ValueOrNull(Dictionary<String, String> args, string key)
        {
            if (!args.ContainsKey(key))
            {
                return null;
            }

            return args[key];
        }

        static bool GenerateAndBuildNamespace(string ns,
            VsVersions vsVersion, WinVersions winVersion, string winmd,
            string npmPackageVersion, string npmScope, string outDir, bool noDefGen, bool noBuild, bool verbose)
        {
            string moduleOutDir = Path.Combine(outDir, ns.ToLower());

            if (!Directory.Exists(moduleOutDir))
            {
                Directory.CreateDirectory(moduleOutDir);
            }

            var generator = new NodeRTProjectGenerator(winVersion, vsVersion, !noDefGen);

            Console.WriteLine("Generating code for: {0}...", ns);

            try
            {
                Reflector.GenerateProject(winmd, ns, moduleOutDir, generator, npmPackageVersion, npmScope, null);
            }
            catch (Exception e)
            {
                Console.WriteLine("Failed to generate code, error: {0}", e.Message);
                return false;
            }

            if (!noBuild)
            {
                Console.WriteLine("Building {0}...", ns);

                try
                {
                    NodeRTProjectBuildUtils.BuildWithNodeGyp(moduleOutDir, vsVersion, verbose);
                }
                catch (IOException e)
                {
                    Console.WriteLine("IO Error occured after building the project, error: {0}", e.Message);
                    Console.WriteLine("Generated project files are located at: {0}", moduleOutDir);
                    return false;
                }
                catch (Exception e)
                {
                    Console.WriteLine("Failed to build the generated project, please try to build the project manually");
                    Console.WriteLine(e.ToString());
                    Console.WriteLine("Generated project files are located at: {0}", moduleOutDir);
                    return false;
                }
                Console.WriteLine("NodeRT module generated and built successfully at: {0}", moduleOutDir);
            }
            return true;
        }

        static bool ValidateArguments(Dictionary<String, String> args)
        {
            if (!args.ContainsKey("winmd") || String.IsNullOrEmpty(args["winmd"]))
                return false;

            if (args.ContainsKey("namespaces"))
                return true;

            if (!args.ContainsKey("outdir") || String.IsNullOrEmpty(args["outdir"]))
                return false;

            return true;
        }

        static void PrintHelp()
        {
            Console.WriteLine();
            Console.WriteLine("  --winmd [path]               File path to winmd file from which the module");
            Console.WriteLine("                               will be generated");
            Console.WriteLine();
            Console.WriteLine("  --namespaces                 Lists all of the namespaces in the winmd file");
            Console.WriteLine("                               (only needs --winmd)");
            Console.WriteLine();
            Console.WriteLine("  --namespace [namespace]      The namespace to generate from the winmd when");
            Console.WriteLine("                               not specified , all namespaces will be generated");
            Console.WriteLine();
            Console.WriteLine();
            Console.WriteLine("  --outdir [path]              The output dir in which the NodeRT module");
            Console.WriteLine("                               will be created in");
            Console.WriteLine();
            Console.WriteLine("  --vs [Vs2022|Vs2019|Vs2017|Vs2015|Vs2013|Vs2012]  Optional, VS version to use, default is Vs2022");
            Console.WriteLine();
            Console.WriteLine("  --winver [10|8.1|8]          Optional, Windows SDK version to use, default is 10");
            Console.WriteLine();
            Console.WriteLine("  --npmscope                   Optional, the scope that will be specified for the generated");
            Console.WriteLine("                               npm package");
            Console.WriteLine();
            Console.WriteLine("  --npmversion                 Optional, the version that will be specified for the generated");
            Console.WriteLine("                               npm package");
            Console.WriteLine();
            Console.WriteLine("  --nodefgen                   Optional, specifying this option will reult in");
            Console.WriteLine("                               skipping the generation of TypeScript and");
            Console.WriteLine("                               JavaScript definition files");
            Console.WriteLine();
            Console.WriteLine("  --nobuild                    Optional, specifying this option will result in");
            Console.WriteLine("                               skipping the build process for the NodeRT module");
            Console.WriteLine();
            Console.WriteLine("  --verbose                    Optional, specifying this option will result in");
            Console.WriteLine("                               verbose output for the module build operation");
            Console.WriteLine();
            Console.WriteLine("  --help                       Print this help screen");
            Console.WriteLine();
        }

        static void PrintHelpAndExitWithError()
        {
            Console.WriteLine("Bad Usage, please see the command line options below:");
            PrintHelp();
            Environment.Exit(1);
        }

        static Dictionary<String, String> ParseCommandLineArgs(string[] args)
        {
            Dictionary<string, string> argsDic = new Dictionary<string, string>();
            for (int i = 0; i < args.Length; i++)
            {
                if (args[i].StartsWith("--"))
                {
                    string argName = args[i].Substring(2);
                    string value = "";
                    if (args.Length > (i + 1) && !args[i + 1].StartsWith("--"))
                    {
                        value = args[i + 1];
                    }
                    argsDic[argName] = value;
                }
            }

            return argsDic;
        }
    }
}
