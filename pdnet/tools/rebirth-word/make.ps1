# Rebuilds pdnet\src\rebirth_word.h from rebirth-art.png (lettering on a painted checkerboard): keys the checker out,
# trims, shrinks to -Height rows. Writes preview.png (the result over black, 3x) next to this script.
param([int]$Height = 80)
$d = $PSScriptRoot
Add-Type -AssemblyName System.Drawing
Add-Type -Path "$d\Key.cs" -ReferencedAssemblies System.Drawing
$s = [Drawing.Bitmap]::FromFile("$d\rebirth-art.png")
$tile = [Key]::Tile($s, 1450, 0, 1670, 110) # a clean corner of the checkerboard
$w = 0; $h = 0
$o = [Key]::Lift($s, $tile, [ref]$w, [ref]$h)
$small = [Key]::Shrink($o, $w, [Key]::Bounds($o, $w, $h, 0.25), $Height)
[Key]::Preview("$d\preview.png", $small, [Drawing.Color]::Black, 3)
[Key]::Header("$d\..\..\src\rebirth_word.h", $small)
$s.Dispose()
"rebirth_word.h: {0}x{1}" -f $small.Width, $small.Height
