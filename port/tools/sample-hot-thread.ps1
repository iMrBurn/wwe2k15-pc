param([int]$Samples = 25, [string]$BuildDir = (Join-Path $PSScriptRoot "..\outuild\win-amd64-release"))
$b = $BuildDir
$sp = Join-Path $env:TEMP "wwe2k15-profile"; New-Item -ItemType Directory -Force $sp | Out-Null
$cdb = "$env:LOCALAPPDATA\Microsoft\WindowsApps\cdbX64.exe"
Get-Process | ? { $_.ProcessName -like "WWE 2K15 Launcher" -or $_.ProcessName -eq "wwe2k15" } | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep 2
$p = Start-Process "$b\WWE 2K15 Launcher.exe" -PassThru -WorkingDirectory $b
Start-Sleep 3
Add-Type @"
using System;using System.Runtime.InteropServices;
public class W5{[DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr h,out RECT r);[DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr h);[DllImport("user32.dll")]public static extern bool SetCursorPos(int x,int y);[DllImport("user32.dll")]public static extern void mouse_event(int f,int x,int y,int d,int e);public struct RECT{public int L,T,R,B;}}
"@
$p.Refresh(); [W5]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
$r = New-Object W5+RECT; [W5]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
[W5]::SetCursorPos($r.L + 300, $r.T + 445) | Out-Null; Start-Sleep -m 400
[W5]::mouse_event(2, 0, 0, 0, 0); [W5]::mouse_event(4, 0, 0, 0, 0)
Start-Sleep 12
$g = Get-Process wwe2k15 -ErrorAction Stop
function snap { $g.Refresh(); $h = @{}; foreach ($t in $g.Threads) { $h[$t.Id] = $t.TotalProcessorTime.TotalMilliseconds }; $h }
$a = snap; Start-Sleep 5; $c = snap
$top = $c.GetEnumerator() | % { [pscustomobject]@{Tid = $_.Key; Cpu = ($_.Value - $a[$_.Key]) / 50} } | Sort Cpu -Desc | Select -First 3
$top | % { "hot tid $($_.Tid) $([math]::Round($_.Cpu))%" }
$tid = $top[0].Tid
$out = "$sp\prof_samples.txt"; Remove-Item $out -ErrorAction SilentlyContinue
for ($i = 0; $i -lt $Samples; $i++) {
  $o = & $cdb -pv -p $g.Id -c "~~[0x$('{0:x}' -f $tid)]k 10;q" 2>&1 | Out-String
  "=== sample $i`n$o" | Add-Content $out
}
"done samples -> $out"
