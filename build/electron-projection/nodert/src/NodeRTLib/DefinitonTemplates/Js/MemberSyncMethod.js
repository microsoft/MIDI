@{
  foreach(var overload in Model.Overloads) {
@:cls.prototype.@(TX.Uncap(Model.Name)) = function @(TX.Uncap(Model.Name))(@(TX.GetParamsFromJsMethodForDefinitions(overload))) @TX.JsDefinitionTemplates.SyncMethodBody(overload)
  }
}