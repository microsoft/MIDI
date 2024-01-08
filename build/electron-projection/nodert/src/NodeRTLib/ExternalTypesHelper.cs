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
using System.Reflection;

namespace NodeRTLib
{
    // a helper utility class used for fetching value types which are used by the types in the namespace
    // but are not part of the current namespace
    class ExternalTypesHelper
    {
        static List<String> ExcludedExternalNamespaces = new List<string>()
        {
            "System",
            "System.Collections",
            "Platform",
            "Windows.Foundation.Collections"
        };

        public static bool IsValueTypeNoEnumOrPrimitve(Type type)
        {
            return type.IsValueType && !type.IsEnum && !type.IsPrimitive && type.Namespace != "System";
        }

        public static void TestAndAddRecursive(Type type, String declaringTypeNamespace, List<Type> valueTypes, List<String> namespaces)
        {
            if (type.IsGenericType)
            {
                if (type.GetGenericArguments() != null)
                {
                    foreach (var genericArg in type.GetGenericArguments())
                    {
                        TestAndAddRecursive(genericArg, declaringTypeNamespace, valueTypes, namespaces);
                    }
                }
                return;
            }

            if (IsValueTypeNoEnumOrPrimitve(type) && !valueTypes.Contains(type) &&
                type.Namespace != "System" && type.Namespace != declaringTypeNamespace)
            {
                valueTypes.Add(type);
            }

            if (!IsValueTypeNoEnumOrPrimitve(type) && type.Namespace != declaringTypeNamespace)
            {
                if (!ExcludedExternalNamespaces.Contains(type.Namespace) && !namespaces.Contains(type.Namespace))
                {
                    namespaces.Add(type.Namespace);
                }
                else if (type.Namespace == "System" && type.Name == "Uri" && !namespaces.Contains("Windows.Foundation")) // Uri is special..
                {
                    namespaces.Add("Windows.Foundation");
                }
            }
        }

        public static void TestAndAddNamespaceRecursive(Type type, List<String> namespaces, String declaringTypeNamespace)
        {
            if (type.IsGenericType)
            {
                if (type.GetGenericArguments() != null)
                {
                    foreach (var genericArg in type.GetGenericArguments())
                    {
                        TestAndAddNamespaceRecursive(genericArg, namespaces, declaringTypeNamespace);
                    }
                }
            }
            else if (type.Namespace != declaringTypeNamespace && !ExcludedExternalNamespaces.Contains(type.Namespace))
            {
                namespaces.Add(type.Namespace);
            }
        }

        public static void GetExternalReferencedData(EventInfo info, List<Type> types, List<String> namespaces)
        {
            var invokeMethodInfo = info.EventHandlerType.GetMethods().Where((methodInfo) => { return (methodInfo.Name == "Invoke"); }).First();

            GetExternalReferencedData(invokeMethodInfo.GetParameters(), info.DeclaringType.Namespace, types, namespaces);
        }

        public static void GetExternalReferencedData(ParameterInfo[] info, string declaringNamespace, List<Type> types, List<String> namespaces)
        {
            foreach (var param in info)
            {
                TestAndAddRecursive(param.ParameterType, declaringNamespace, types, namespaces);
            }
        }

        public static void GetExternalReferencedData(MethodInfo info, List<Type> types, List<String> namespaces)
        {
            TestAndAddRecursive(info.ReturnType, info.DeclaringType.Namespace, types, namespaces);

            GetExternalReferencedData(info.GetParameters(), info.DeclaringType.Namespace, types, namespaces);
        }

        public static void GetExternalReferencedData(PropertyInfo info, List<Type> types, List<String> namespaces)
        {
            TestAndAddRecursive(info.PropertyType, info.DeclaringType.Namespace, types, namespaces);
        }

        public static void GetExternalReferencedDataFromMethods(dynamic[] methods, List<Type> types, List<String> namespaces)
        {
            foreach (dynamic method in methods)
            {
                foreach (MethodInfo mi in method.Overloads)
                {
                    GetExternalReferencedData(mi, types, namespaces);
                }
            }
        }

        public static void GetExternalReferencedDataForType(dynamic typeDefinition, List<Type> externalValueTypes, List<String> externalNamespaces)
        {
            foreach (ConstructorInfo ci in typeDefinition.Type.GetConstructors())
            {
                GetExternalReferencedData(ci.GetParameters(), typeDefinition.Type.Namespace, externalValueTypes, externalNamespaces);
            }

            foreach (PropertyInfo pi in typeDefinition.MemberProperties)
            {
                GetExternalReferencedData(pi, externalValueTypes, externalNamespaces);
            }

            foreach (PropertyInfo pi in typeDefinition.StaticProperties)
            {
                GetExternalReferencedData(pi, externalValueTypes, externalNamespaces);
            }

            foreach (dynamic ev in typeDefinition.Events)
            {
                EventInfo ei = ev.EventInfo;
                GetExternalReferencedData(ei, externalValueTypes, externalNamespaces);
            }

            GetExternalReferencedDataFromMethods(typeDefinition.MemberAsyncMethods, externalValueTypes, externalNamespaces);
            GetExternalReferencedDataFromMethods(typeDefinition.StaticAsyncMethods, externalValueTypes, externalNamespaces);
            GetExternalReferencedDataFromMethods(typeDefinition.MemberSyncMethods, externalValueTypes, externalNamespaces);
            GetExternalReferencedDataFromMethods(typeDefinition.StaticSyncMethods, externalValueTypes, externalNamespaces);
        }
    }
}
