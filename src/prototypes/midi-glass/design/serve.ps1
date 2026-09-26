# Minimal unelevated static file server for reviewing the MIDI Glass mockups.
# TcpListener rather than HttpListener: HttpListener needs a URL ACL reservation (admin).
param(
    [int]$Port = 8742,
    [string]$Root = $PSScriptRoot
)

$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $Port)
$listener.Start()
Write-Host "serving $Root on http://127.0.0.1:$Port/"

$types = @{
    '.html' = 'text/html; charset=utf-8'
    '.css'  = 'text/css; charset=utf-8'
    '.js'   = 'text/javascript; charset=utf-8'
    '.png'  = 'image/png'
    '.svg'  = 'image/svg+xml'
}

while ($true)
{
    $client = $listener.AcceptTcpClient()
    try
    {
        $stream = $client.GetStream()
        $buffer = [byte[]]::new(8192)
        $read = $stream.Read($buffer, 0, $buffer.Length)
        if ($read -le 0) { continue }

        $request = [System.Text.Encoding]::ASCII.GetString($buffer, 0, $read)
        $line = ($request -split "`r`n")[0]
        $path = ($line -split ' ')[1]
        if (-not $path) { $path = '/' }
        $path = ($path -split '\?')[0]
        if ($path -eq '/') { $path = '/index.html' }

        $leaf = [System.IO.Path]::GetFileName($path)
        $full = Join-Path $Root $leaf

        if ((Test-Path -LiteralPath $full -PathType Leaf) -and
            ([System.IO.Path]::GetFullPath($full)).StartsWith(([System.IO.Path]::GetFullPath($Root)), [StringComparison]::OrdinalIgnoreCase))
        {
            $bytes = [System.IO.File]::ReadAllBytes($full)
            $ext = [System.IO.Path]::GetExtension($full).ToLowerInvariant()
            $type = $types[$ext]
            if (-not $type) { $type = 'application/octet-stream' }

            $head = "HTTP/1.1 200 OK`r`nContent-Type: $type`r`nContent-Length: $($bytes.Length)`r`nCache-Control: no-store`r`nConnection: close`r`n`r`n"
        }
        else
        {
            $bytes = [System.Text.Encoding]::UTF8.GetBytes("not found: $leaf")
            $head = "HTTP/1.1 404 Not Found`r`nContent-Type: text/plain`r`nContent-Length: $($bytes.Length)`r`nConnection: close`r`n`r`n"
        }

        $headBytes = [System.Text.Encoding]::ASCII.GetBytes($head)
        $stream.Write($headBytes, 0, $headBytes.Length)
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush()
    }
    catch { }
    finally { $client.Close() }
}
