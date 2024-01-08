declare module "@(TX.MainModel.winrtnamespace.ToLower())" {
  @{
    foreach(var vt in Model.ExternalReferencedValueTypes) {
  @:@TX.TsDefinitionTemplates.ValueType(vt)
    }
    foreach(var vt in Model.ValueTypes) {
  @:@TX.TsDefinitionTemplates.ValueType(vt)
    }
    foreach(var en in Model.Enums) {
  @:@TX.TsDefinitionTemplates.Enum(en)
    }
    foreach(var t in Model.Types.Values) {
  @:@TX.TsDefinitionTemplates.Type(t)
    }
@:}
  }


