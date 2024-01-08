    constructor();
@{
  foreach(var constructor in Model.Type.GetConstructors()) 
  {
    if (constructor.GetParameters().Length > 0)
    {
    @:constructor(@(TX.GetParamsFromTsMethodForDefinitions(constructor)));
    }
  }
}