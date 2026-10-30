# Rebuilds pdnet\src\rebirth_word.h from rebirth-art.png (the lettering with its own transparency): crops it to what
# is visible and writes it at full size. Writes preview.png (the result over black, 3x) next to this script.
$d = $PSScriptRoot
Add-Type -AssemblyName System.Drawing
Add-Type -Path "$d\Key.cs" -ReferencedAssemblies System.Drawing
$s = [Drawing.Bitmap]::FromFile("$d\rebirth-art.png")
$x0 = $s.Width; $y0 = $s.Height; $x1 = -1; $y1 = -1
for ($y = 0; $y -lt $s.Height; $y++) { for ($x = 0; $x -lt $s.Width; $x++) { if ($s.GetPixel($x, $y).A -gt 0) {
    if ($x -lt $x0) { $x0 = $x }; if ($x -gt $x1) { $x1 = $x }; if ($y -lt $y0) { $y0 = $y }; if ($y -gt $y1) { $y1 = $y } } } }
$word = $s.Clone([Drawing.Rectangle]::FromLTRB($x0, $y0, $x1 + 1, $y1 + 1), [Drawing.Imaging.PixelFormat]::Format32bppArgb)
[Key]::Preview("$d\preview.png", $word, [Drawing.Color]::Black, 3)
[Key]::Header("$d\..\..\src\rebirth_word.h", $word)
$s.Dispose()
"rebirth_word.h: {0}x{1}" -f $word.Width, $word.Height
