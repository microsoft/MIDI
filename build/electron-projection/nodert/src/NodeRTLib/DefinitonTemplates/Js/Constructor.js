@{
@:var cls = function @(Model.Name)() {

  foreach(var propertyInfo in Model.MemberProperties) {
    @:@TX.JsDefinitionTemplates.MemberPropertyGetter(propertyInfo)
  }

  @:};
  @:

  foreach(var constructor in Model.Type.GetConstructors()) 
  { 
    if (constructor.GetParameters().Length == 0)
    {
      continue;
    }

@:var cls = function @(Model.Name)(@(TX.GetParamsFromJsMethodForDefinitions(constructor))) {
    
    foreach(var propertyInfo in Model.MemberProperties) {
      @:@TX.JsDefinitionTemplates.MemberPropertyGetter(propertyInfo)
    }

@:};
@:
  }
}