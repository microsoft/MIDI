@{
@:{
    @:/// <signature>
    @:/// <summary>Function summary.</summary>
    foreach (var paramInfo in Model.GetParameters()) {
    @:/// <param name="@(paramInfo.Name)" type="@(Converter.ToJsDefinitonType(paramInfo.ParameterType, TX.MainModel.Types.ContainsKey(paramInfo.ParameterType)))">A param.</param>
    }
    @:/// </signature>
  @:}
}