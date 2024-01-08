@{
@:_@(Model.Name) = function () {
  var counter = 0;
  foreach(var field in Model.DeclaredFields)
  {
    if (counter != 0)
    {
  @:this.@(TX.Uncap(field.Name)) = @(counter-1);
    }
  counter++;
  }
@:}
@:exports.@(Model.Name) = new _@(Model.Name)();
}