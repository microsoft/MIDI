// Extracts plain text from a specification PDF so it can be read and searched without
// a PDF viewer. Reading only; it never writes back to the source document.

using UglyToad.PdfPig;
using UglyToad.PdfPig.DocumentLayoutAnalysis.TextExtractor;

if (args.Length is < 1 or > 2)
{
    Console.Error.WriteLine("usage: specextract <input.pdf> [output.txt]");
    Console.Error.WriteLine("       when output.txt is omitted, text goes to stdout");
    return 2;
}

var inputPath = Path.GetFullPath(args[0]);

if (!File.Exists(inputPath))
{
    Console.Error.WriteLine($"error: file not found: {inputPath}");
    return 1;
}

try
{
    using var document = PdfDocument.Open(inputPath);

    var text = new StringWriter();

    foreach (var page in document.GetPages())
    {
        text.WriteLine($"===== page {page.Number} =====");
        text.WriteLine(ContentOrderTextExtractor.GetText(page));
        text.WriteLine();
    }

    if (args.Length == 2)
    {
        var outputPath = Path.GetFullPath(args[1]);
        Directory.CreateDirectory(Path.GetDirectoryName(outputPath)!);
        File.WriteAllText(outputPath, text.ToString());
        Console.Error.WriteLine($"{document.NumberOfPages} pages -> {outputPath}");
    }
    else
    {
        Console.Out.Write(text.ToString());
    }

    return 0;
}
catch (Exception ex)
{
    Console.Error.WriteLine($"error: could not read '{inputPath}': {ex.Message}");
    return 1;
}
