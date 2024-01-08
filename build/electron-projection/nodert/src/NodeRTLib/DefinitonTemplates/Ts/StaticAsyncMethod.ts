@{
  foreach(var overload in Model.Overloads) {
    @:static @(TX.Uncap(Model.Name))(@(TX.GetParamsFromTsMethodForDefinitions(overload, isAsync: true))): void ;
  }
}
