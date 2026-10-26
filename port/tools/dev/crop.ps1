param([string]$src,[string]$dst,[int]$x,[int]$y,[int]$w,[int]$h)
Add-Type -AssemblyName System.Drawing
$im = [Drawing.Bitmap]::FromFile($src); "size $($im.Width)x$($im.Height)"
$c = $im.Clone((New-Object Drawing.Rectangle $x,$y,$w,$h), $im.PixelFormat); $c.Save($dst)
