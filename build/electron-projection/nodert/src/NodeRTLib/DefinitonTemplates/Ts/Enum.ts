@{
@:export enum @(Model.Name) {
  var counter = 0;
  foreach(var field in Model.DeclaredFields)
  {
    if (counter != 0)
    {
    @:@(TX.Uncap(field.Name)),
    }
    counter++;
  }
  @:}
}