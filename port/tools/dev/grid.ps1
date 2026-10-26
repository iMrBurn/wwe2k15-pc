param([string]$dir,[string]$names,[string]$out)
Add-Type -AssemblyName System.Drawing
$list = $names.Split(","); $bmp = New-Object Drawing.Bitmap (480*3), (270*[math]::Ceiling($list.Count/3)); $g=[Drawing.Graphics]::FromImage($bmp); $i=0
foreach($n in $list){ $im=[Drawing.Image]::FromFile("$dir\$n.png"); $g.DrawImage($im, ($i%3)*480, [math]::Floor($i/3)*270, 480, 270); $im.Dispose(); $i++ }
$bmp.Save($out)
