@{
  foreach(var vt in Model.ExternalReferencedValueTypes) {
@:@TX.JsDefinitionTemplates.ValueType(vt)
  }

  foreach(var vt in Model.ValueTypes) {
@:@TX.JsDefinitionTemplates.ValueType(vt)
  }

  foreach(var en in Model.Enums) {
@:@TX.JsDefinitionTemplates.Enum(en)
  }

  foreach(var t in Model.Types.Values) {
@:@TX.JsDefinitionTemplates.Type(t)
  }
}