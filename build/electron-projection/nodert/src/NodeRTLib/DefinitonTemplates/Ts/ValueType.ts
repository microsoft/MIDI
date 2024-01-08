@{
@:export class @(Model.Name) {
  foreach(var field in Model.GetFields()) {
    @:@(TX.Uncap(field.Name)): @(Converter.ToJsDefinitonType(field.FieldType, TX.MainModel.Types.ContainsKey(field.FieldType)));
  }
    @:constructor();
  @:}
}