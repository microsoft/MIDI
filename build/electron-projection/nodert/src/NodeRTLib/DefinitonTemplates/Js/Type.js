@{
@:@(Model.Name) = (function () {

  @:@TX.JsDefinitionTemplates.Constructor(Model)
 
  foreach(var methodInfo in Model.MemberAsyncMethods) { 
  @:@TX.JsDefinitionTemplates.MemberAsyncMethod(methodInfo)
  }

  foreach(var methodInfo in Model.MemberSyncMethods) 
  { 
    if (TX.IsMethodNotImplemented(methodInfo))
    {
  @:@TX.JsDefinitionTemplates.NotImplementedMethod(methodInfo)
    }
    else
    {
      if (TX.IsIClosableClose(methodInfo.Overloads[0]))
      {
  @:@TX.JsDefinitionTemplates.IClosableCloseMethod(methodInfo)
      }
      else
      {
  @:@TX.JsDefinitionTemplates.MemberSyncMethod(methodInfo)
      }
    }
  }

  foreach(var methodInfo in Model.StaticAsyncMethods) { 
  @:@TX.JsDefinitionTemplates.StaticAsyncMethod(methodInfo)
  }

  foreach(var methodInfo in Model.StaticSyncMethods) { 
  @:@TX.JsDefinitionTemplates.StaticSyncMethod(methodInfo)
  }

  if (Model.StaticProperties.Length > 0)
  {
    foreach(var propertyInfo in Model.StaticProperties) {
  @:@TX.JsDefinitionTemplates.StaticPropertyGetter(propertyInfo)
    }
  }

  @if(Model.Events.Length > 0)
  {
    @TX.JsDefinitionTemplates.Event(Model)
  }

  @:return cls;
@:}) ();
@:exports.@(Model.Name) = @(Model.Name);
}