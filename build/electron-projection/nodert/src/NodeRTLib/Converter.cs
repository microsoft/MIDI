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
using System.Threading.Tasks;

namespace NodeRTLib
{
    public static class Converter
    {
        /// <summary>
        /// Convert a C# dataType to its equivlant JavaScript dataType, here used to generate TypeScript and JavaScript definition files
        /// </summary>
        /// <param name="type">C# dataType</param>
        /// <param name="typeIsInNameSpace">A flag indicating if the Type is in the nameSpace</param>
        /// <returns>Equivlant JS dataType</returns>
        public static string ToJsDefinitonType(Type type, bool typeIsInNameSpace = false)
        {
            if (type.IsPrimitive)
            {
                if (type == typeof(Byte) ||
                    type == typeof(SByte) ||
                    type == typeof(Int16) ||
                    type == typeof(UInt16) ||
                    type == typeof(Int32) ||
                    type == typeof(UInt32)
                    )
                {
                    return "Number";
                }
            }

            if (type.IsByRef)
            {
                return ToJsDefinitonType(type.GetElementType(), typeIsInNameSpace);
            }

            if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(Nullable<>))
            {
                return ToJsDefinitonType(type.GetGenericArguments()[0]);
            }

            if (type.IsArray)
            {
                return string.Format("Array<{0}>", ToJsDefinitonType(type.GetElementType(), typeIsInNameSpace));
            }

            if (type.IsEnum)
            {
                if (type.Namespace.Equals(TX.MainModel.winrtnamespace))
                {
                    return type.Name;
                }
                return "Number";
            }

            if (type == typeof(void))
            {
                return "void";
            }

            if (type.IsGenericType && type.FullName != null && (type.FullName.StartsWith("Windows.Foundation.Collections") ||
               type.FullName.StartsWith("System.Collections")))
            {
                return "Object";
            }

            string[] jsType = ToJS(type, typeIsInNameSpace);

            if (jsType[0].Equals("Value"))
            {
                if (type.IsValueType)
                {
                    return type.Name;
                }
                return "Object";
            }

            return jsType[0];
        }

        public static string[] ToJS(Type type, bool typeIsInNameSpace = false)
        {
            // The primitive types are Boolean, Byte, SByte, Int16, UInt16, Int32, UInt32, Int64, UInt64, IntPtr, UIntPtr, Char, Double, and Single.
            if (type.IsPrimitive)
            {
                if (type == typeof(Byte) ||
                    type == typeof(SByte) ||
                    type == typeof(Int16) ||
                    type == typeof(UInt16) ||
                    type == typeof(Int32) ||
                    type == typeof(UInt32)
                    )
                {
                    return new[] { "Integer", "Nan::New<Integer>({0})" };
                }
                else if (type == typeof(UInt32))
                {
                    return new[] { "Integer", "Nan::New<Integer>({0})" };
                }
                else if (type == typeof(Int64) ||
                    type == typeof(UInt64) ||
                    type == typeof(Double) ||
                    type == typeof(Single) ||
                    type == typeof(IntPtr) ||
                    type == typeof(UIntPtr))
                {
                    return new[] { "Number", "Nan::New<Number>(static_cast<double>({0}))" };
                }
                else if (type == typeof(Boolean))
                {
                    return new[] { "Boolean", "Nan::New<Boolean>({0})" };
                }
                else if (type == typeof(Char))
                {
                    return new[] { "String", "NodeRT::Utils::JsStringFromChar({0})" };
                }
            }

            if (type.IsByRef)
            {
                return ToJS(type.GetElementType(), typeIsInNameSpace);
            }

            if (type == typeof(String))
            {
                return new[] { "String", "NodeRT::Utils::NewString({0}->Data())" };
            }

            if (type == typeof(DateTimeOffset) || type == typeof(DateTime))
            {
                return new[] { "Date", "NodeRT::Utils::DateTimeToJS({0})" };
            }

            if (type == typeof(Exception))
            {
                return new[] { "Number", "Nan::New<Integer>({0}.Value)" };
            }

            if (type == typeof(TimeSpan))
            {
                // convert 100 nano seconds units to millisecond
                // the conversion factor is 10000
                return new[] { "Number", "Nan::New<Number>({0}.Duration/10000.0)" };
            }

            if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(Nullable<>))
            {
                String[] conversionInfo = ToJS(type.GetGenericArguments()[0]);
                // TODO: Verify that the casting here to Local<Value> of Undefined() is actually needed..
                conversionInfo[1] = "{0} ? static_cast<Local<Value>>(" + String.Format(conversionInfo[1], "{0}->Value") + ") : Undefined()";
                return conversionInfo;
            }

            if (type.FullName == "Windows.UI.Color")
            {
                return new[] { "Object", "NodeRT::Utils::ColorToJs({0})" };
            }

            if (type.FullName == "Windows.Foundation.Point")
            {
                return new[] { "Object", "NodeRT::Utils::PointToJs({0})" };
            }

            if (type.FullName == "Windows.Foundation.Rect")
            {
                return new[] { "Object", "NodeRT::Utils::RectToJs({0})" };
            }

            if (type.FullName == "Windows.Foundation.Size")
            {
                return new[] { "Object", "NodeRT::Utils::SizeToJs({0})" };
            }

            if (type.IsEnum)
            {
                return new[] { "Integer", "Nan::New<Integer>(static_cast<int>({0}))" };
            }

            if (type.IsArray)
            {
                return ToCollection("Array", type.GetElementType());
            }

            if (type == typeof(Guid))
            {
                return new[] { "String", "NodeRT::Utils::GuidToJs({0})" };
            }

            if (type.IsGenericType && type.FullName != null && (type.FullName.StartsWith("Windows.Foundation.Collections.IIterator`1") ||
                type.FullName.StartsWith("System.Collections.Generic.IEnumerator`1")))
            {
                return ToReadOnlyCollection("Iterator", type.GetGenericArguments()[0]);
            }

            if (type.IsGenericType && type.FullName != null && (type.FullName.StartsWith("Windows.Foundation.Collections.IIterable`1") ||
                type.FullName.StartsWith("System.Collections.Generic.IEnumerable`1")))
            {
                return ToReadOnlyCollection("Iterable", type.GetGenericArguments()[0]);
            }

            if (type.IsGenericType && type.FullName != null && type.FullName.StartsWith("System.Collections.Generic.IReadOnlyList`1"))
            {
                return ToCollection("VectorView", type.GetGenericArguments()[0]);
            }

            if (type.IsGenericType && type.FullName != null && type.FullName.StartsWith("System.Collections.Generic.IList`1"))
            {
                return ToCollection("Vector", type.GetGenericArguments()[0]);
            }

            if (type.IsGenericType && type.FullName != null && type.FullName.StartsWith("System.Collections.Generic.IReadOnlyDictionary`2"))
            {
                return ToReadOnlyKeyValueCollection("MapView", type.GetGenericArguments()[0], type.GetGenericArguments()[1]);
            }

            if (type.IsGenericType && type.FullName != null && type.FullName.StartsWith("System.Collections.Generic.IDictionary`2"))
            {
                return ToKeyValueCollection("Map", type.GetGenericArguments()[0], type.GetGenericArguments()[1]);
            }

            // In CS KeyValuePair is of value type so we need to make sure the check for value type is called after this
            if (type.IsGenericType && type.FullName != null && type.FullName.StartsWith("System.Collections.Generic.KeyValuePair`2"))
            {
                return ToReadOnlyKeyValue("KeyValuePair", type.GetGenericArguments()[0], type.GetGenericArguments()[1]);
            }

            if (type.IsValueType)
            {
                return new[] { "Value", type.Name + "ToJsObject({0})" };
            }

            if (typeIsInNameSpace)
            {
                return new[] { type.Name, "Wrap" + type.Name + "({0})" };
            }

            // optimization
            if (type == typeof(Object))
            {
                return new[] { "Value", "CreateOpaqueWrapper({0})" };
            }

            if (type.Namespace == "System" && type.Name == "Uri")
            {
                return new[] { "Value", "NodeRT::Utils::CreateExternalWinRTObject(\"Windows.Foundation\", " +
                "\"" + type.Name + "\", "+
                "{0})" };
            }

            return new[] { "Value", "NodeRT::Utils::CreateExternalWinRTObject(" +
                "\"" + type.Namespace + "\", "+
                "\"" + type.Name + "\", "+
                "{0})" };
        }

        public static string[] ToCollection(string collectionName, Type elementType)
        {
            string elementRtType = TX.ToWinRT(elementType);
            string[] elementTypeToJs = ToJS(elementType, TX.MainModel.Types.ContainsKey(elementType));
            string checkType = TypeCheck(elementType, TX.MainModel.Types.ContainsKey(elementType));
            string[] jsToElementType = ToWinRT(elementType, TX.MainModel.Types.ContainsKey(elementType));
            // note that double curl braces here are used because String.Format will 
            string creatorFunction = "NodeRT::Collections::" + collectionName + "Wrapper<" + elementRtType + ">::Create" + collectionName + "Wrapper({0}, \r\n" +
"            [](" + elementRtType + " val) -> Local<Value> {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(elementTypeToJs[1], "val")) + ";\r\n" +
"            }},\r\n" +
"            [](Local<Value> value) -> bool {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(checkType, "value")) + ";\r\n" +
"            }},\r\n" +
"            [](Local<Value> value) -> " + elementRtType + " {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(jsToElementType[1], "value")) + ";\r\n" +
"            }}\r\n" +
"          )";

            return new[] { "NodeRT::Collections::Create" + collectionName + "<" + elementRtType + ">", creatorFunction };
        }

        public static string[] ToReadOnlyCollection(string collectionName, Type elementType)
        {
            string elementRtType = TX.ToWinRT(elementType);
            string[] elementTypeToJs = ToJS(elementType, TX.MainModel.Types.ContainsKey(elementType));

            // note that double curl braces here are used because String.Format will 
            string creatorFunction = "NodeRT::Collections::" + collectionName + "Wrapper<" + elementRtType + ">::Create" + collectionName + "Wrapper({0}, \r\n" +
"            [](" + elementRtType + " val) -> Local<Value> {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(elementTypeToJs[1], "val")) + ";\r\n" +
"            }}\r\n" +
"          )";

            return new[] { "NodeRT::Collections::Create" + collectionName + "<" + elementRtType + ">", creatorFunction };
        }

        public static string[] ToReadOnlyKeyValue(string collectionName, Type keyType, Type valueType)
        {
            string keyRtType = TX.ToWinRT(keyType);
            string[] keyTypeToJs = ToJS(keyType, TX.MainModel.Types.ContainsKey(keyType));

            string valueRtType = TX.ToWinRT(valueType);
            string[] valueTypeToJs = ToJS(valueType, TX.MainModel.Types.ContainsKey(valueType));

            // note that double curl braces here are used because String.Format will 
            string creatorFunction = "NodeRT::Collections::" + collectionName + "Wrapper<" + keyRtType + "," + valueRtType + ">::Create" + collectionName + "Wrapper({0}, \r\n" +
"            [](" + keyRtType + " val) -> Local<Value> {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(keyTypeToJs[1], "val")) + ";\r\n" +
"            }},\r\n" +
"            [](" + valueRtType + " val) -> Local<Value> {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(valueTypeToJs[1], "val")) + ";\r\n" +
"            }}\r\n" +
"          )";

            return new[] { "NodeRT::Collections::Create" + collectionName + "<" + keyRtType + "," + valueRtType + ">", creatorFunction };
        }

        public static string[] ToReadOnlyKeyValueCollection(string collectionName, Type keyType, Type valueType)
        {
            string keyRtType = TX.ToWinRT(keyType);
            string[] keyTypeToJs = ToJS(keyType, TX.MainModel.Types.ContainsKey(keyType));
            string keyCheckType = TypeCheck(keyType, TX.MainModel.Types.ContainsKey(keyType));
            string[] jsToKeyElementType = ToWinRT(keyType, TX.MainModel.Types.ContainsKey(keyType));

            string valueRtType = TX.ToWinRT(valueType);
            string[] valueTypeToJs = ToJS(valueType, TX.MainModel.Types.ContainsKey(valueType));

            // note that double curl braces here are used because String.Format will 
            string creatorFunction = "NodeRT::Collections::" + collectionName + "Wrapper<" + keyRtType + "," + valueRtType + ">::Create" + collectionName + "Wrapper({0}, \r\n" +
"            [](" + keyRtType + " val) -> Local<Value> {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(keyTypeToJs[1], "val")) + ";\r\n" +
"            }},\r\n" +
"            [](Local<Value> value) -> bool {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(keyCheckType, "value")) + ";\r\n" +
"            }},\r\n" +
"            [](Local<Value> value) -> " + keyRtType + " {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(jsToKeyElementType[1], "value")) + ";\r\n" +
"            }},\r\n" +
"            [](" + valueRtType + " val) -> Local<Value> {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(valueTypeToJs[1], "val")) + ";\r\n" +
"            }}\r\n" +
"          )";

            return new[] { "NodeRT::Collections::Create" + collectionName + "<" + keyRtType + "," + valueRtType + ">", creatorFunction };
        }

        public static string[] ToKeyValueCollection(string collectionName, Type keyType, Type valueType)
        {
            string keyRtType = TX.ToWinRT(keyType);
            string[] keyTypeToJs = ToJS(keyType, TX.MainModel.Types.ContainsKey(keyType));
            string keyCheckType = TypeCheck(keyType, TX.MainModel.Types.ContainsKey(keyType));
            string[] jsToKeyElementType = ToWinRT(keyType, TX.MainModel.Types.ContainsKey(keyType));

            string valueRtType = TX.ToWinRT(valueType);
            string[] valueTypeToJs = ToJS(valueType, TX.MainModel.Types.ContainsKey(valueType));
            string valueCheckType = TypeCheck(valueType, TX.MainModel.Types.ContainsKey(valueType));
            string[] jsToValueElementType = ToWinRT(valueType, TX.MainModel.Types.ContainsKey(valueType));

            // note that double curl braces here are used because String.Format will 
            string creatorFunction = "NodeRT::Collections::" + collectionName + "Wrapper<" + keyRtType + "," + valueRtType + ">::Create" + collectionName + "Wrapper({0}, \r\n" +
"            [](" + keyRtType + " val) -> Local<Value> {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(keyTypeToJs[1], "val")) + ";\r\n" +
"            }},\r\n" +
"            [](Local<Value> value) -> bool {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(keyCheckType, "value")) + ";\r\n" +
"            }},\r\n" +
"            [](Local<Value> value) -> " + keyRtType + " {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(jsToKeyElementType[1], "value")) + ";\r\n" +
"            }},\r\n" +
"            [](" + valueRtType + " val) -> Local<Value> {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(valueTypeToJs[1], "val")) + ";\r\n" +
"            }},\r\n" +
"            [](Local<Value> value) -> bool {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(valueCheckType, "value")) + ";\r\n" +
"            }},\r\n" +
"            [](Local<Value> value) -> " + valueRtType + " {{\r\n" +
"              return " + ReplaceBracketsWithDoubleBrackets(String.Format(jsToValueElementType[1], "value")) + ";\r\n" +
"            }}\r\n" +
"          )";

            return new[] { "NodeRT::Collections::Create" + collectionName + "<" + keyRtType + "," + valueRtType + ">", creatorFunction };
        }


        private static string GetTypeCheckerAndConverterLambdas(Type type)
        {
            string rtType = TX.ToWinRT(type);
            string[] jsToElementType = ToWinRT(type, TX.MainModel.Types.ContainsKey(type));

            string checkType;

            if (type == typeof(String))
            {
                // if the expected winRT type is String, allow non winrtWrapped object to be passed in and converted to Platform::String^
                checkType = "(!NodeRT::Utils::IsWinRtWrapper({0}))";
            }
            else
            {
                checkType = TypeCheck(type, TX.MainModel.Types.ContainsKey(type));
            }

            return
"                 [](Local<Value> value) -> bool {{\r\n" +
"                   return " + ReplaceBracketsWithDoubleBrackets(String.Format(checkType, "value")) + ";\r\n" +
"                 }},\r\n" +
"                 [](Local<Value> value) -> " + rtType + " {{\r\n" +
"                   return " + ReplaceBracketsWithDoubleBrackets(String.Format(jsToElementType[1], "value")) + ";\r\n" +
"                 }}";
        }



        private static string[] GetKeyValueTypeAndLambdas(Type keyType, Type valueType)
        {
            string keyRtType = TX.ToWinRT(keyType);
            string valueRtType = TX.ToWinRT(valueType);

            string templateVals = (keyType == typeof(String)) ? "<" + valueRtType + ">" : "<" + keyRtType + ", " + valueRtType + ">";

            return new[] { templateVals,
                GetTypeCheckerAndConverterLambdas(keyType) + ",\r\n" + GetTypeCheckerAndConverterLambdas(valueType) + "\r\n"};
        }

        public static string[] GetValueTypeAndLambdas(Type valueType)
        {
            return new[] { "<" + TX.ToWinRT(valueType) + ">", GetTypeCheckerAndConverterLambdas(valueType) + "\r\n" };
        }

        public static string JsToWinrtCollection(Type type, string jsType, string winrtFullType, bool typeIsInNameSpace)
        {
            string methodNameSuffix = null;
            string[] typeAndLambdas = null;
            string castingType = null;

            Type keyType = null;
            Type valueType = null;

            if (type.IsGenericType && (type.FullName != null))
            {
                if (type.FullName.StartsWith("Windows.Foundation.Collections.IIterable`1") ||
                type.FullName.StartsWith("System.Collections.Generic.IEnumerable`1"))
                {
                    Type innerTemplateType = type.GetGenericArguments()[0];
                    if (innerTemplateType.FullName.StartsWith("System.Collections.Generic.KeyValuePair`2"))
                    {
                        methodNameSuffix = "ToWinrtMap";
                        keyType = innerTemplateType.GetGenericArguments()[0];
                        valueType = innerTemplateType.GetGenericArguments()[1];
                    }
                    else
                    {
                        methodNameSuffix = "ToWinrtVector";
                        valueType = innerTemplateType;
                    }
                }

                if (type.FullName.StartsWith("System.Collections.Generic.IReadOnlyList`1"))
                {
                    methodNameSuffix = "ToWinrtVectorView";
                    valueType = type.GetGenericArguments()[0];
                }
                else if (type.FullName.StartsWith("System.Collections.Generic.IList`1"))
                {
                    methodNameSuffix = "ToWinrtVector";
                    valueType = type.GetGenericArguments()[0];
                }
                else if (type.FullName.StartsWith("System.Collections.Generic.IReadOnlyDictionary`2"))
                {
                    methodNameSuffix = "ToWinrtMapView";
                    keyType = type.GetGenericArguments()[0];
                    valueType = type.GetGenericArguments()[1];
                }
                else if (type.FullName.StartsWith("System.Collections.Generic.IDictionary`2"))
                {
                    methodNameSuffix = "ToWinrtMap";
                    keyType = type.GetGenericArguments()[0];
                    valueType = type.GetGenericArguments()[1];
                }
            }
            else if (type.IsArray)
            {
                methodNameSuffix = "ToWinrtArray";
                valueType = type.GetElementType();
            }

            if (string.IsNullOrEmpty(methodNameSuffix) || valueType == null)
            {
                throw new Exception("Failed to handle given collection");
            }
            if ((keyType == null) || (keyType == typeof(String)))
            {
                typeAndLambdas = GetValueTypeAndLambdas(valueType);
            }
            else
            {
                typeAndLambdas = GetKeyValueTypeAndLambdas(keyType, valueType);
            }

            castingType = (keyType == typeof(String)) ? "Object" : "Array";

            string funcStr = "\r\n" +
"            [] (v8::Local<v8::Value> value) -> " + winrtFullType + "\r\n" +
"            {{\r\n" +
"              if (value->Is" + jsType + "())\r\n" +
"              {{\r\n" +
"                return NodeRT::Collections::Js" + castingType + methodNameSuffix + typeAndLambdas[0] + "(value.As<" + castingType + ">(), \r\n" + typeAndLambdas[1] +
"                );\r\n" +
"              }}\r\n" +
"              else\r\n" +
"              {{\r\n" +
"                return " + (typeIsInNameSpace ? ("Unwrap" + type.Name + "(value)") : ("dynamic_cast<" + winrtFullType + ">(NodeRT::Utils::GetObjectInstance(value))")) + ";\r\n" +
"              }}\r\n" +
"            }} ({0})";

            return funcStr;
        }


        public static string ReplaceBracketsWithDoubleBrackets(string str)
        {
            return str.Replace("{", "{{").Replace("}", "}}");
        }

        public static string[] ToWinRT(Type type, bool typeIsInNameSpace = false)
        {
            // The primitive types are Boolean, Byte, SByte, Int16, UInt16, Int32, UInt32, Int64, UInt64, IntPtr, UIntPtr, Char, Double, and Single.
            if (type.IsPrimitive)
            {
                if (type == typeof(Byte))
                {
                    return new[] { "unsigned char", "static_cast<unsigned char>(Nan::To<int32_t>({0}).FromMaybe(0))" };
                }
                else if (type == typeof(SByte))
                {
                    return new[] { "char", "static_cast<char>(Nan::To<int32_t>({0}).FromMaybe(0))" };
                }
                else if (type == typeof(Int16))
                {
                    return new[] { "short", "static_cast<short>(Nan::To<int32_t>({0}).FromMaybe(0))" };
                }
                else if (type == typeof(UInt16))
                {
                    return new[] { "unsigned short", "static_cast<unsigned short>(Nan::To<int32_t>({0}).FromMaybe(0))" };
                }
                else if (type == typeof(Int32))
                {
                    return new[] { "int", "static_cast<int>(Nan::To<int32_t>({0}).FromMaybe(0))" };
                }
                else if (type == typeof(UInt32))
                {
                    return new[] { "unsigned int", "static_cast<unsigned int>(Nan::To<uint32_t>({0}).FromMaybe(0))" };
                }
                else if (type == typeof(Int64))
                {
                    return new[] { "__int64", "Nan::To<int64_t>({0}).FromMaybe(0)" };
                }
                else if (type == typeof(UInt64))
                {
                    return new[] { "unsigned __int64", "static_cast<unsigned __int64>(Nan::To<int64_t>({0}).FromMaybe(0))" };
                }
                else if (type == typeof(IntPtr))
                {
                    return new[] { "__int64", "Nan::To<int64_t>({0}).FromMaybe(0)" };
                }
                else if (type == typeof(UIntPtr))
                {
                    return new[] { "unsigned __int64", "static_cast<unsigned __int64>(Nan::To<int64_t>({0}).FromMaybe(0))" };
                }
                else if (type == typeof(Single))
                {
                    return new[] { "float", "static_cast<float>(Nan::To<double>({0}).FromMaybe(0.0))" };
                }
                else if (type == typeof(Double))
                {
                    return new[] { "double", "Nan::To<double>({0}).FromMaybe(0.0)" };
                }
                else if (type == typeof(Boolean))
                {
                    return new[] { "bool", "Nan::To<bool>({0}).FromMaybe(false)" };
                }
                else if (type == typeof(Char))
                {
                    return new[] { "wchar_t", "NodeRT::Utils::GetFirstChar({0})" };
                }
            }

            if (type == typeof(String))
            {
                return new[] { "Platform::String^", "ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), {0})))" };
            }

            if (type == typeof(DateTime) || type == typeof(DateTimeOffset))
            {
                return new[] { "::Windows::Foundation::DateTime", "NodeRT::Utils::DateTimeFromJSDate({0})" };
            }

            if (type == typeof(Guid))
            {
                return new[] { "::Platform::Guid", "NodeRT::Utils::GuidFromJs({0})" };
            }

            if (type == typeof(Exception))
            {
                return new[] { "::Windows::Foundation::HResult", "NodeRT::Utils::HResultFromJsInt32(Nan::To<int32_t>({0}).FromMaybe(0))" };
            }

            if (type == typeof(TimeSpan))
            {
                return new[] { "::Windows::Foundation::TimeSpan", "NodeRT::Utils::TimeSpanFromMilli(Nan::To<int64_t>({0}).FromMaybe(0))" };
            }

            if (type.FullName == "Windows.UI.Color")
            {
                return new[] { "::Windows::UI::Color", "NodeRT::Utils::ColorFromJs({0})" };
            }

            if (type.FullName == "Windows.Foundation.Point")
            {
                return new[] { "::Windows::Foundation::Point", "NodeRT::Utils::PointFromJs({0})" };
            }

            if (type.FullName == "Windows.Foundation.Rect")
            {
                return new[] { "::Windows::Foundation::Rect", "NodeRT::Utils::RectFromJs({0})" };
            }

            if (type.FullName == "Windows.Foundation.Size")
            {
                return new[] { "::Windows::Foundation::Size", "NodeRT::Utils::SizeFromJs({0})" };
            }

            if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(Nullable<>))
            {
                var genericArgConversionInfo = ToWinRT(type.GetGenericArguments()[0]);
                return new[] { "::Platform::IBox<" + genericArgConversionInfo[0] + ">^", "ref new ::Platform::Box<" + genericArgConversionInfo[0] + ">(" + genericArgConversionInfo[1] + ")" };
            }

            var winrtFullType = TX.ToWinRT(type);

            if (type.IsEnum)
            {
                return new[] { winrtFullType, "static_cast<" + winrtFullType + ">(Nan::To<int32_t>({0}).FromMaybe(0))" };
            }

            // this if clause should come after IsEnum, since an enum will also return true for IsValueType
            if (type.IsValueType && !(type.IsGenericType && type.FullName.StartsWith("System.Collections.Generic.KeyValuePair`2")))
            {
                return new[] { winrtFullType, type.Name + "FromJsObject({0})" };
            }


            string jsType;
            if (IsWinrtCollection(type, out jsType))
            {
                return new[] { winrtFullType, JsToWinrtCollection(type, jsType, winrtFullType, typeIsInNameSpace) };
            }

            if (typeIsInNameSpace)
            {
                return new[] { winrtFullType, "Unwrap" + type.Name + "({0})" };
            }

            return new[] { winrtFullType, "dynamic_cast<" + winrtFullType + ">(NodeRT::Utils::GetObjectInstance({0}))" };
        }


        public static string TypeCheck(Type type, bool typeIsInNameSpace = false)
        {
            // The primitive types are Boolean, Byte, SByte, Int16, UInt16, Int32, UInt32, Int64, UInt64, IntPtr, UIntPtr, Char, Double, and Single.
            if (type.IsPrimitive)
            {
                if (type == typeof(Byte) ||
                    type == typeof(SByte) ||
                    type == typeof(Int16) ||
                    type == typeof(UInt16) ||
                    type == typeof(Int32)
                    )
                {
                    return "{0}->IsInt32()";
                }
                else if (type == typeof(UInt32))
                {
                    return "{0}->IsUint32()";
                }
                else if (type == typeof(Int64) ||
                    type == typeof(UInt64))
                {
                    return "{0}->IsNumber()";
                }
                else if (type == typeof(Double) ||
                    type == typeof(Single) ||
                    type == typeof(IntPtr) ||
                    type == typeof(UIntPtr))
                {
                    return "{0}->IsNumber()";
                }
                else if (type == typeof(Boolean))
                {
                    return "{0}->IsBoolean()";
                }
                else if (type == typeof(Char))
                {
                    return "{0}->IsString()";
                }
            }

            if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(Nullable<>))
            {
                return TypeCheck(type.GetGenericArguments()[0], TX.MainModel.Types.ContainsKey(type.GetGenericArguments()[0]));
            }

            if (type == typeof(Guid))
            {
                return "NodeRT::Utils::IsGuid({0})";
            }

            if (type.FullName == "Windows.UI.Color")
            {
                return "NodeRT::Utils::IsColor({0})";
            }

            if (type.FullName == "Windows.Foundation.Point")
            {
                return "NodeRT::Utils::IsPoint({0})";
            }

            if (type.FullName == "Windows.Foundation.Size")
            {
                return "NodeRT::Utils::IsSize({0})";
            }

            if (type.FullName == "Windows.Foundation.Rect")
            {
                return "NodeRT::Utils::IsRect({0})";
            }

            if (type.IsEnum)
            {
                return "{0}->IsInt32()";
            }

            if (type == typeof(String))
            {
                return "{0}->IsString()";
            }

            if (type == typeof(DateTimeOffset) || type == typeof(DateTime))
            {
                return "{0}->IsDate()";
            }

            if (type == typeof(TimeSpan))
            {
                return "{0}->IsNumber()";
            }

            // this if clause should come after IsEnum, since an enum will also return true for IsValueType
            if (type.IsValueType && !(type.IsGenericType && type.FullName.StartsWith("System.Collections.Generic.KeyValuePair`2")))
            {
                return "Is" + type.Name + "JsObject({0})";
            }

            string wrapperTest = "NodeRT::Utils::IsWinRtWrapperOf<" + TX.ToWinRT(type, true) + ">({0})";

            string jsType;
            if (IsWinrtCollection(type, out jsType))
            {
                // in case the type is a winrt collection, we allow to pass a JS Object/Array instead of a wrapped collection
                return "(" + wrapperTest + " || " + "{0}->Is" + jsType + "())";
            }

            return wrapperTest;
        }

        public static string ToOutParameterName(Type type)
        {
            // The primitive types are Boolean, Byte, SByte, Int16, UInt16, Int32, UInt32, Int64, UInt64, IntPtr, UIntPtr, Char, Double, and Single.
            if (type.IsPrimitive)
            {
                if (type == typeof(Byte) || type == typeof(SByte) || type == typeof(Int16) ||
                    type == typeof(UInt16) || type == typeof(Int32) || type == typeof(UInt32) ||
                    type == typeof(Int64) || type == typeof(IntPtr) || type == typeof(UIntPtr) ||
                    type == typeof(Single) || type == typeof(Double))
                {
                    return "number";
                }
                else if (type == typeof(Boolean))
                {
                    return "boolean";
                }
                else if (type == typeof(Char))
                {
                    return "string";
                }
            }

            if (type == typeof(String))
            {
                return "string";
            }

            if (type == typeof(DateTimeOffset) || type == typeof(DateTime))
            {
                return "date";

            }

            return TX.Uncap(type.Name);
        }

        public static bool IsWinrtCollection(Type type, out string jsType)
        {
            if (type.IsGenericType && type.FullName != null && (type.FullName.StartsWith("Windows.Foundation.Collections.IIterable")
                || type.FullName.StartsWith("System.Collections.Generic.IEnumerable`1")))
            {
                Type innerTemplateType = type.GetGenericArguments()[0];
                if (innerTemplateType.FullName.StartsWith("System.Collections.Generic.KeyValuePair`2"))
                {
                    jsType = "Object";
                }
                else
                {
                    jsType = "Array";
                }
                return true;
            }

            if (type.IsGenericType && type.FullName != null && type.FullName.StartsWith("System.Collections.Generic.IReadOnlyDictionary`2")
                || type.FullName.StartsWith("System.Collections.Generic.IDictionary`2"))
            {
                jsType = "Object";
                return true;
            }

            if ((type.IsGenericType && type.FullName != null && (type.FullName.StartsWith("System.Collections.Generic.IReadOnlyList`1")
                || type.FullName.StartsWith("System.Collections.Generic.IList`1"))) || type.IsArray)
            {
                jsType = "Array";
                return true;
            }

            jsType = null;
            return false;
        }
    }
}
