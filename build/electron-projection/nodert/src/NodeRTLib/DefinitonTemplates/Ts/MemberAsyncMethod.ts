@{
  foreach(var overload in Model.Overloads) {
    @:@(TX.Uncap(Model.Name))(@(TX.GetParamsFromTsMethodForDefinitions(overload, isAsync: true))): void ;
  }
}