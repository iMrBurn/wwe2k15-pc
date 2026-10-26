param([string]$src,[string]$dst)
Add-Type -AssemblyName System.Drawing
$im=[Drawing.Image]::FromFile($src); $b=New-Object Drawing.Bitmap 960,540; $g=[Drawing.Graphics]::FromImage($b); $g.DrawImage($im,0,0,960,540); $b.Save($dst)
