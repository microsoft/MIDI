@{
  foreach(var overload in Model.Overloads) {
@:cls.@(TX.Uncap(Model.Name)) = function @(TX.Uncap(Model.Name))(@(TX.GetParamsFromJsMethodForDefinitions(overload, isAsync: true))) @TX.JsDefinitionTemplates.AsyncMethodBody(overload)
  }
}
