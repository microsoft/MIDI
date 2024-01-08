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
using System.Linq;
using System.Text;

namespace NodeRTLib
{
    using System.Reflection;
    using Microsoft.CSharp;
    using System.CodeDom;
    using System.CodeDom.Compiler;

    using System.Dynamic;
    using System.IO;
    using System.Runtime.InteropServices.WindowsRuntime;

    using RazorTemplates.Core;

    public class Reflector
    {
        static string BaseWindMDDir = null;
        static Reflector()
        {
            AppDomain.CurrentDomain.ReflectionOnlyAssemblyResolve += (sender, eventArgs) => Assembly.ReflectionOnlyLoad(eventArgs.Name);
            WindowsRuntimeMetadata.ReflectionOnlyNamespaceResolve += (sender, eventArgs) =>
            {

                string path =
                    WindowsRuntimeMetadata.ResolveNamespace(eventArgs.NamespaceName, Enumerable.Empty<string>())
                        .FirstOrDefault();

                if (path == null) return;

                if (!String.IsNullOrEmpty(BaseWindMDDir))
                {
                    var newPath = Path.Combine(BaseWindMDDir, System.IO.Path.GetFileName(path));
                    eventArgs.ResolvedAssemblies.Add(Assembly.ReflectionOnlyLoadFrom(newPath));
                }
                else
                {
                    eventArgs.ResolvedAssemblies.Add(Assembly.ReflectionOnlyLoadFrom(path));
                }
            };
        }

        public static IEnumerable<string> GetNamespaces(string winmdFile, string baseWinMDDir)
        {
            BaseWindMDDir = baseWinMDDir;
            var assembly = Assembly.ReflectionOnlyLoadFrom(winmdFile);
            if (!assembly.FullName.Contains("WindowsRuntime"))
            {
                return new string[0];
            }

            var namespaces = from t in assembly.ExportedTypes select t.Namespace;

            return namespaces.Distinct();
        }

        public static dynamic GenerateModel(string winmdFile, string winRTNamespace, string baseWinMDDir)
        {
            BaseWindMDDir = baseWinMDDir;
            var assembly = Assembly.ReflectionOnlyLoadFrom(winmdFile);
            if (!assembly.FullName.Contains("WindowsRuntime"))
            {
                return null;
            }

            return GenerateModel(assembly, winRTNamespace);
        }

        public static IList<TypeInfo> GetHiddenTypesForNamespace(Assembly assembly, string winRTNamespace)
        {
            // namespace specific handlers:
            // Windows.Foundation.Uri is exposed in c++ while in c# System.Uri is used. Retreive the definition of Windows.Foundation.Uri specifically.
            if (winRTNamespace.Equals("Windows.Foundation"))
            {
                return (from t in assembly.DefinedTypes where t.Namespace.Equals(winRTNamespace, StringComparison.InvariantCultureIgnoreCase) && (t.Name.Equals("Uri") || t.Name.Equals("IPropertyValue")) select t).ToList();
            }

            return new List<TypeInfo>();
        }

        public static IList<Type> GetTypesForNamespace(Assembly assembly, string winRTNamespace)
        {
            IList<Type> types = (from t in assembly.ExportedTypes where t.Namespace.Equals(winRTNamespace, StringComparison.InvariantCultureIgnoreCase) && !t.IsValueType && !t.IsGenericType && t.BaseType != typeof(Delegate) && t.BaseType != typeof(MulticastDelegate) select t).ToList();

            IList<TypeInfo> hiddenTypes = GetHiddenTypesForNamespace(assembly, winRTNamespace);

            foreach (var t in hiddenTypes)
            {
                types.Add(t);
            }

            return types;
        }

        private static bool VerifyNamespaceInAssembly(Assembly assembly, string winRTNamespace)
        {
            foreach (var t in assembly.ExportedTypes)
            {
                if (t.Namespace.Equals(winRTNamespace, StringComparison.InvariantCultureIgnoreCase))
                {
                    return true;
                }
            }

            return false;
        }

        public static dynamic GenerateModel(Assembly assembly, string winRTNamespace)
        {
            if (!VerifyNamespaceInAssembly(assembly, winRTNamespace))
            {
                throw new Exception(String.Format("The namespace {0} is not defined in the given WinMD file.", winRTNamespace));
            }

            dynamic mainModel = new DynamicDictionary();
            TX.MainModel = mainModel;

            mainModel.Assembly = assembly;

            var types = new Dictionary<Type, dynamic>();
            mainModel.Types = types;

            var namespaces = new List<String>();
            namespaces.Add("NodeRT");
            namespaces.AddRange(winRTNamespace.Split('.'));
            mainModel.Namespaces = namespaces;
            mainModel.WinRTNamespace = winRTNamespace;

            var filteredTypes = GetTypesForNamespace(assembly, winRTNamespace);

            mainModel.Enums = (from t in assembly.ExportedTypes where t.Namespace.Equals(winRTNamespace, StringComparison.InvariantCultureIgnoreCase) && t.IsEnum select t).ToArray();
            mainModel.ValueTypes = (from t in assembly.ExportedTypes where t.Namespace.Equals(winRTNamespace, StringComparison.InvariantCultureIgnoreCase) && t.IsValueType && !t.IsEnum select t).ToArray();

            // use this container to aggregate value types which are not in the namespace
            // we will need to generate converter methods for those types
            mainModel.ExternalReferencedValueTypes = new List<Type>();

            // use this container to aggreate other namesapces which are references from this namespaces
            // we will need to create a dependancy list from these namespaces
            mainModel.ExternalReferencedNamespaces = new List<String>();

            foreach (var t in filteredTypes)
            {
                dynamic typeDefinition = new DynamicDictionary();
                types.Add(t, typeDefinition);
                typeDefinition.Name = t.Name;

                typeDefinition.MemberProperties = t.GetProperties(BindingFlags.Public | BindingFlags.Instance | BindingFlags.FlattenHierarchy);
                typeDefinition.StaticProperties = t.GetProperties(BindingFlags.Public | BindingFlags.Static | BindingFlags.FlattenHierarchy);

                var memberEvents = t.GetEvents(BindingFlags.Public | BindingFlags.Instance | BindingFlags.FlattenHierarchy).Select(eventInfo =>
                {
                    dynamic dict = new DynamicDictionary();
                    dict.EventInfo = eventInfo;
                    dict.IsStatic = false;
                    return dict;
                }).ToArray();

                var staticEvents = t.GetEvents(BindingFlags.Public | BindingFlags.Static | BindingFlags.FlattenHierarchy).Select(eventInfo =>
                {
                    dynamic dict = new DynamicDictionary();
                    dict.EventInfo = eventInfo;
                    dict.IsStatic = true;
                    return dict;
                }).ToArray();

                // one function will handle both static and member events, so put them in the same list
                typeDefinition.Events = memberEvents.Concat(staticEvents).ToArray();

                // this will be used in the InitExports method
                typeDefinition.HasStaticEvents = (staticEvents.Length > 0);
                typeDefinition.HasMemberEvents = (memberEvents.Length > 0);

                typeDefinition.Type = t;

                var publicMethods = t.GetRuntimeMethods().Where((methodInfo) =>
                {
                    return methodInfo.DeclaringType == t &&
                           (methodInfo.IsPublic || methodInfo.IsHideBySig) &&
                           !methodInfo.Name.StartsWith("add_") &&
                           !methodInfo.Name.StartsWith("remove_") &&
                           !methodInfo.Name.StartsWith("get_") &&
                           !methodInfo.Name.StartsWith("put_") &&
                           !methodInfo.IsStatic &&
                           !TX.ShouldIgnoreMethod(methodInfo);
                }).GroupBy(methodInfo => methodInfo.Name).Select(method =>
                {
                    dynamic dict = new DynamicDictionary();
                    dict.Name = method.Key;
                    dict.Overloads = method.ToArray();
                    return dict;
                }).ToArray();

                typeDefinition.MemberAsyncMethods = publicMethods.Where((methodInfo) =>
                {
                    return TX.IsAsync(methodInfo.Overloads[0]);
                }).ToArray();

                typeDefinition.MemberSyncMethods = publicMethods.Where((methodInfo) =>
                {
                    return !TX.IsAsync(methodInfo.Overloads[0]);
                }).ToArray();

                var staticMethods = t.GetRuntimeMethods().Where((methodInfo) =>
                {
                    return methodInfo.DeclaringType == t &&
                        (methodInfo.IsPublic || methodInfo.IsHideBySig) &&
                        !methodInfo.Name.StartsWith("add_") &&
                        !methodInfo.Name.StartsWith("remove_") &&
                        !methodInfo.Name.StartsWith("get_") &&
                        !methodInfo.Name.StartsWith("put_") &&
                        methodInfo.IsStatic &&
                        !TX.ShouldIgnoreMethod(methodInfo);
                }).GroupBy(methodInfo => methodInfo.Name).Select(method =>
                {
                    dynamic dict = new DynamicDictionary();
                    dict.Name = method.Key;
                    dict.Overloads = method.ToArray();
                    return dict;
                }).ToArray();

                typeDefinition.StaticAsyncMethods = staticMethods.Where((methodInfo) =>
                {
                    return TX.IsAsync(methodInfo.Overloads[0]);
                }).ToArray();

                typeDefinition.StaticSyncMethods = staticMethods.Where((methodInfo) =>
                {
                    return !TX.IsAsync(methodInfo.Overloads[0]);
                }).ToArray();

                ExternalTypesHelper.GetExternalReferencedDataForType(typeDefinition, mainModel.ExternalReferencedValueTypes, mainModel.ExternalReferencedNamespaces);
            }

            return mainModel;
        }

        public static string GenerateString(string winmdFile, string winRTNamespace, string baseWinMDDir)
        {
            var mainModel = GenerateModel(winmdFile, winRTNamespace, baseWinMDDir);
            return TX.CppTemplates.Wrapper(mainModel);
        }

        // Used in order to retreive the namespace with the appropriate casings
        private static string ResolveNamespaceCasing(string winmdFile, string winRTNamespace, string baseWinMDDir)
        {
            foreach (string ns in GetNamespaces(winmdFile, baseWinMDDir))
            {
                if (String.Compare(winRTNamespace, ns, true) == 0)
                {
                    return ns;
                }
            }

            // not found, fallback to the given namepace
            return winRTNamespace;
        }

        public static string GenerateProject(string winmdFile, string winRTNamespace, string destinationFolder, NodeRTProjectGenerator generator,
            string npmPackageVersion, string npmScope, string baseWinMDDir)
        {
            string ns = ResolveNamespaceCasing(winmdFile, winRTNamespace, baseWinMDDir);
            var mainModel = GenerateModel(winmdFile, ns, baseWinMDDir);
            return generator.GenerateProject(ns, destinationFolder, winmdFile, npmPackageVersion, npmScope, mainModel);
        }
    }

    public static class TX
    {
        public static dynamic MainModel;
        public static dynamic CppTemplates = new DynamicTemplate("CppTemplates", ".cpp");
        public static dynamic JsDefinitionTemplates = new DynamicTemplate(@"DefinitonTemplates\Js", ".js");
        public static dynamic TsDefinitionTemplates = new DynamicTemplate(@"DefinitonTemplates\Ts", ".ts");

        public static ITemplate<dynamic> LoadTemplate(string templateName, string templateLocation, string templateExtension)
        {
            Stream stream;

            try
            {
                stream = File.OpenRead(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, string.Format(@"{0}\{1}{2}", templateLocation, templateName, templateExtension)));
            }
            catch (Exception)
            {
                return null;
            }

            using (var reader = new StreamReader(stream))
            {
                var rawTemplate = reader.ReadToEnd();
                return Template
                    .WithBaseType<TemplateBase>()
                    .AddNamespace("NodeRTLib")
                    .Compile(rawTemplate);
            }
        }

        public static bool ShouldIgnoreMethod(MethodInfo info)
        {
            if (info.Name.Equals("Close") && info.DeclaringType.GetInterface("IDisposable") != null)
            {
                return true;
            }

            return false;
        }

        public static Func<dynamic, String> LazyTemplate(string templateName, string templateLocation, string templateExtension)
        {
            ITemplate<dynamic> template = LoadTemplate(templateName, templateLocation, templateExtension);
            if (template == null)
            {
                return null;
            }

            return (dynamic model) =>
            {
                return template.Render(model);
            };
        }


        // http://stackoverflow.com/a/1363212/1060807
        public static string TypeToString(Type type)
        {
            if (type == null) throw new ArgumentNullException("type");

            var sb = new StringBuilder();
            using (var sw = new StringWriter(sb))
            {
                var expr = new CodeTypeReferenceExpression(type);
                var prov = new CSharpCodeProvider();

                prov.GenerateCodeFromExpression(expr, sw, new CodeGeneratorOptions());
            }
            return sb.ToString();
        }

        readonly static Dictionary<String, String> CSharpToCppMethodMap = new Dictionary<string, string>()
        {
            {"Dispose", "Close"}
        };

        public static string CSharpMethodToCppMethod(string methodName)
        {
            if (CSharpToCppMethodMap.ContainsKey(methodName))
            {
                return CSharpToCppMethodMap[methodName];
            }

            return methodName;
        }

        private static string GetTypeFullName(Type type)
        {
            return (type.Namespace + "." + type.Name).Replace("`1", "");
        }

        public static string GetParamsFromJsMethodForDefinitions(dynamic method, bool isAsync = false)
        {
            StringBuilder sb = new StringBuilder();
            string commaString = string.Empty;
            foreach (var paramInfo in method.GetParameters())
            {
                sb.Append(commaString);
                sb.Append(paramInfo.Name);
                commaString = ", ";
            }

            if (isAsync)
            {
                sb.Append(commaString);
                sb.Append("callback");
            }

            return sb.ToString();
        }

        public static string GetParamsFromTsMethodForDefinitions(dynamic method, bool isAsync = false)
        {
            var sb = new StringBuilder();
            string commaString = string.Empty;
            foreach (var paramInfo in method.GetParameters())
            {
                string optionalString = string.Empty;
                if (paramInfo.IsOptional)
                {
                    optionalString = "?";
                }
                sb.Append(commaString);
                sb.Append(paramInfo.Name);
                sb.Append(": ");
                sb.Append(Converter.ToJsDefinitonType(paramInfo.ParameterType, TX.MainModel.Types.ContainsKey(paramInfo.ParameterType)));
                commaString = ", ";
            }

            if (isAsync)
            {
                System.Reflection.MethodInfo[] returnTypeMethods = method.ReturnType.GetMethods();
                Type resultType = returnTypeMethods.Where((methodInfo) => { return (methodInfo.Name == "GetResults"); }).First().ReturnType;

                string resultTypeString = Converter.ToJsDefinitonType(resultType, TX.MainModel.Types.ContainsKey(resultType));
                string errorTypeString = "Error";

                string errorParam = string.Format("error: {0}", errorTypeString);
                string resultParam = string.Format("result: {0}", resultTypeString);

                string callbackParams = errorParam;
                if (resultType != typeof(void))
                {
                    callbackParams += ", " + resultParam;
                }

                string callbackSignature = string.Format("callback: ({0}) => void", callbackParams);
                sb.Append(commaString);
                sb.Append(callbackSignature);
            }

            return sb.ToString();
        }

        public static string ToWinRT(Type type, bool generateHat = true)
        {
            if (type == typeof(void))
            {
                return "void";
            }

            string fullName = GetTypeFullName(type);
            var sb = new StringBuilder();
            bool typeConverted = false;

            if (CSharpToCppClassMap.ContainsKey(fullName))
            {
                sb.Append(CSharpToCppClassMap[fullName]);
                typeConverted = true;
            }

            if (!type.IsGenericType && !typeConverted && !type.IsArray)
            {
                if (type.IsPrimitive)
                {
                    // in case this is a primitive type
                    return Converter.ToWinRT(type)[0];
                }
                else
                {
                    sb.Append(type.FullName);
                }
            }
            else if (type.IsArray)
            {
                sb.Append("System.Array");
                sb.Append('<');
                sb.AppendFormat(ToWinRT(type.GetElementType()));
                sb.Append('>');
            }
            else if (type.IsGenericType)
            {
                if (!typeConverted)
                {
                    sb.Append(type.Namespace);
                    sb.Append(".");
                    sb.Append(type.Name);
                    sb.Remove(sb.Length - 2, 2);
                }

                sb.Append('<');

                foreach (var arg in type.GenericTypeArguments)
                {
                    var argString = ToWinRT(arg);
                    sb.AppendFormat("{0}, ", argString);
                }
                sb.Remove(sb.Length - 2, 2);
                sb.Append('>');
            }

            if ((!type.IsValueType || RefTypesInCppValueInCS.Contains(fullName)) && generateHat)
            {
                sb.Append('^');
            }

            sb.Replace(".", "::");
            if (sb.ToString().StartsWith("System::"))
            {
                sb.Replace("System::", "Platform::");
            }
            sb.Insert(0, "::");

            return sb.ToString();
        }

        readonly static IList<String> RefTypesInCppValueInCS = new List<String>()
        {
            "System.Collections.Generic.KeyValuePair",
            "System.Collections.Generic.KeyValuePair`2"
        };

        // some of the basic types are translated to Platform:: and some are to Windows::Foundation...
        // so we use this ugly map for now
        readonly static Dictionary<String, String> CSharpToCppClassMap = new Dictionary<string, string>()
        {
            {"System.Uri", "Windows::Foundation::Uri"},
            {"System.Collections.Generic.IEnumerable", "Windows::Foundation::Collections::IIterable"},
            {"System.Collections.Generic.IReadOnlyList", "Windows::Foundation::Collections::IVectorView"},
            {"System.Collections.Generic.IList", "Windows::Foundation::Collections::IVector"},
            {"System.Collections.Generic.IEnumerable`1", "Windows::Foundation::Collections::IIterable"},
            {"System.Collections.Generic.IReadOnlyList`1", "Windows::Foundation::Collections::IVectorView"},
            {"System.Collections.Generic.IList`1", "Windows::Foundation::Collections::IVector"},
            {"System.Collections.Generic.IDictionary`2", "Windows::Foundation::Collections::IMap"},
            {"System.Collections.Generic.IDictionary", "Windows::Foundation::Collections::IMap"},
            {"System.Collections.Generic.IReadOnlyDictionary", "Windows::Foundation::Collections::IMapView"},
            {"System.Collections.Generic.IReadOnlyDictionary`2", "Windows::Foundation::Collections::IMapView"},
            {"System.Collections.Generic.KeyValuePair", "Windows::Foundation::Collections::IKeyValuePair"},
            {"System.Collections.Generic.KeyValuePair`2", "Windows::Foundation::Collections::IKeyValuePair"},
            {"System.DateTimeOffset", "Windows::Foundation::DateTime"},
            {"System.DateTime", "Windows::Foundation::DateTime"},
            {"System.TimeSpan", "Windows::Foundation::TimeSpan"},
            {"System.EventHandler", "Windows::Foundation::EventHandler"},
            {"System.EventArgs", "Windows::Foundation::EventArgs"},
        };

        public static string Uncap(string s)
        {
            return s[0].ToString().ToLower() + s.Substring(1);
        }

        public static string ForEachEvent(IEnumerable<dynamic> events, string format, int lenToRemove)
        {
            var sb = new System.Text.StringBuilder();
            foreach (var e in events)
            {
                sb.AppendFormat(format, e.EventInfo.Name, Uncap(e.EventInfo.Name), TX.ToWinRT(e.EventInfo.EventHandlerType));
            }
            if (sb.Length > 0) sb.Remove(sb.Length - lenToRemove, lenToRemove);
            return sb.ToString();
        }

        public static string ForEachParameter(IEnumerable<ParameterInfo> parameters, string format, int lenToRemove)
        {
            var sb = new System.Text.StringBuilder();
            foreach (var param in parameters)
            {
                sb.AppendFormat(format, param.Name, TX.ToWinRT(param.ParameterType));
            }
            if (sb.Length > 0) sb.Remove(sb.Length - lenToRemove, lenToRemove);
            return sb.ToString();
        }

        public static string ForEachType(IEnumerable<Type> types, string format, int lenToRemove)
        {
            var sb = new System.Text.StringBuilder();
            var counter = 0;
            foreach (var type in types)
            {
                sb.AppendFormat(format, type.Name, TX.ToWinRT(type), counter);
                counter++;
            }
            if (sb.Length > 0) sb.Remove(sb.Length - lenToRemove, lenToRemove);
            return sb.ToString();
        }

        public static string ToWinRTType(Type type)
        {
            var CSharpType = TypeToString(type);
            var typeName = CSharpType
                            .Replace(".", "::")
                            .Replace("System::", "Platform::");
            string refType = (type.IsValueType && !RefTypesInCppValueInCS.Contains(GetTypeFullName(type))) ? null : "^";

            typeName += refType;

            return typeName.Replace("&^", "^&");
        }

        // We are interseted in methods which return type IAsyncOperation/IAsyncAction
        public static bool IsAsync(MethodInfo info)
        {
            foreach (Type t in info.ReturnType.GetInterfaces())
            {
                if (t.FullName == null)
                    continue;

                if (t.FullName.StartsWith("Windows.Foundation.IAsyncOperation`1") ||
                   t.FullName.StartsWith("Windows.Foundation.IAsyncOperationWithProgress`2") ||
                   t.FullName.StartsWith("Windows.Foundation.IAsyncAction") ||
                   t.FullName.StartsWith("Windows.Foundation.IAsyncActionWithProgress`1"))
                {
                    return true;
                }
            }

            if (info.ReturnType.FullName == null)
                return false;

            return info.ReturnType.FullName.StartsWith("Windows.Foundation.IAsyncOperation`1") ||
               info.ReturnType.FullName.StartsWith("Windows.Foundation.IAsyncOperationWithProgress`2") ||
               info.ReturnType.FullName.StartsWith("Windows.Foundation.IAsyncAction") ||
               info.ReturnType.FullName.StartsWith("Windows.Foundation.IAsyncActionWithProgress`1");
        }

        public static bool IsArrayAsOut(MethodInfo info, out string sizeStr)
        {
            ParameterInfo[] parameters = info.GetParameters();

            if (info.Name == "GetMany" &&
                parameters.Length == 2 &&
                parameters[0].ParameterType == typeof(UInt32) &&
                info.DeclaringType.GetProperties().Where(pi => pi.Name == "Size").Count() > 0)
            {
                sizeStr = "wrapper->_instance->Size - arg0";
                return true;
            }
            else if (info.Name == "GetMany" &&
                parameters.Length == 2 &&
                parameters[0].ParameterType == typeof(UInt32) &&
                info.DeclaringType.GetProperties().Where(pi => pi.Name == "Length").Count() > 0)
            {
                sizeStr = "wrapper->_instance->Length - arg0";
                return true;
            }
            else if (info.Name == "GetMany" &&
                parameters.Length == 1 &&
                info.DeclaringType.GetProperties().Where(pi => pi.Name == "Size").Count() > 0)
            {
                sizeStr = "wrapper->_instance->Size";
                return true;
            }

            sizeStr = String.Empty;
            return false;
        }

        public static bool IsMethodNotImplemented(MethodInfo info)
        {
            // check for array out parameters, as we currnetly do not support this other then some specific cases
            string dummy;
            if (IsArrayAsOut(info, out dummy))
                return false;

            if (info.GetParameters().Where(pi => pi.IsOut && pi.ParameterType.IsArray).Count() > 0)
            {
                return true;
            }

            return false;
        }

        public static bool IsMethodNotImplemented(dynamic overloadsContainer)
        {
            foreach (MethodInfo info in overloadsContainer.Overloads)
            {
                if (IsMethodNotImplemented(info))
                {
                    return true;
                }
            }

            return false;
        }

        // checks whether the given method is of type 
        public static bool IsIClosableClose(MethodInfo info)
        {
            return info.Name.Equals("Dispose") && info.DeclaringType.GetInterface("System.IDisposable") != null;
        }
    }

    // The class derived from DynamicObject. 
    public class DynamicDictionary : DynamicObject
    {
        // The inner dictionary.
        Dictionary<string, object> dictionary
            = new Dictionary<string, object>();

        // This property returns the number of elements 
        // in the inner dictionary. 
        public int Count
        {
            get
            {
                return dictionary.Count;
            }
        }

        // If you try to get a value of a property  
        // not defined in the class, this method is called. 
        public override bool TryGetMember(
            GetMemberBinder binder, out object result)
        {
            // Converting the property name to lowercase 
            // so that property names become case-insensitive. 
            string name = binder.Name.ToLower();

            // If the property name is found in a dictionary, 
            // set the result parameter to the property value and return true. 
            // Otherwise, return false. 
            return dictionary.TryGetValue(name, out result);
        }

        // If you try to set a value of a property that is 
        // not defined in the class, this method is called. 
        public override bool TrySetMember(
            SetMemberBinder binder, object value)
        {
            // Converting the property name to lowercase 
            // so that property names become case-insensitive.

            dictionary[binder.Name.ToLower()] = value;

            // You can always add a value to a dictionary, 
            // so this method always returns true. 
            return true;
        }
    }

    // The class derived from DynamicObject. 
    public class DynamicTemplate : DynamicObject
    {
        // The inner dictionary.
        Dictionary<string, object> dictionary
            = new Dictionary<string, object>();

        private string templateLocation;

        public string TemplateLocation
        {
            get
            {
                return templateLocation;
            }
        }

        private string templateExtension;

        public string TemplateExtension
        {
            get
            {
                return templateExtension;
            }
        }

        public DynamicTemplate(string templateLocation, string templateExtension)
        {
            this.templateLocation = templateLocation;
            this.templateExtension = templateExtension;
        }

        // This property returns the number of elements
        // in the inner dictionary. 
        public int Count
        {
            get
            {
                return dictionary.Count;
            }
        }

        // If you try to get a value of a property  
        // not defined in the class, this method is called. 
        public override bool TryGetMember(
            GetMemberBinder binder, out object result)
        {
            string name = binder.Name;

            // If the property name is found in a dictionary, 
            // set the result parameter to the property value and return true. 
            // Otherwise, try to load the template. 
            if (!dictionary.TryGetValue(name, out result))
            {
                Func<dynamic, String> template = TX.LazyTemplate(name, this.templateLocation, this.templateExtension);
                if (template != null)
                {
                    dictionary[name] = template;
                    result = template;
                    return true;
                }

                return false;
            }

            return true;
        }

        // If you try to set a value of a property that is 
        // not defined in the class, this method is called. 
        public override bool TrySetMember(
            SetMemberBinder binder, object value)
        {
            return false;
        }
    }
}
