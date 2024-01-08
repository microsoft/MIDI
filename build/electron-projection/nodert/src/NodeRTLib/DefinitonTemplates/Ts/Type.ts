@{
@:export class @(Model.Name) {

  if (Model.StaticProperties.Length > 0)
  {
    foreach(var propertyInfo in Model.StaticProperties) {
@:@TX.TsDefinitionTemplates.StaticPropertyGetter(propertyInfo)
    }
  }

  foreach(var propertyInfo in Model.MemberProperties) {
@:@TX.TsDefinitionTemplates.MemberPropertyGetter(propertyInfo)
  }

@:@TX.TsDefinitionTemplates.Constructor(Model)

  foreach(var methodInfo in Model.StaticAsyncMethods) {
@:@TX.TsDefinitionTemplates.StaticAsyncMethod(methodInfo)
  }

  foreach(var methodInfo in Model.StaticSyncMethods) {
@:@TX.TsDefinitionTemplates.StaticSyncMethod(methodInfo)
  }

  foreach(var methodInfo in Model.MemberAsyncMethods) {
@:@TX.TsDefinitionTemplates.MemberAsyncMethod(methodInfo)
  }

  foreach(var methodInfo in Model.MemberSyncMethods)
  {
    if (TX.IsMethodNotImplemented(methodInfo))
    {
@:@TX.TsDefinitionTemplates.NotImplementedMethod(methodInfo)
    }
    else
    {
      if (TX.IsIClosableClose(methodInfo.Overloads[0]))
      {
@:@TX.TsDefinitionTemplates.IClosableCloseMethod(methodInfo)
      }
      else
      {
@:@TX.TsDefinitionTemplates.MemberSyncMethod(methodInfo)
      }
    }
  }

  @if (Model.Events.Length > 0)
  {
    @TX.TsDefinitionTemplates.Event(Model)
  }

  @:}
}