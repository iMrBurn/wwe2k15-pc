param([int]$seconds=20)
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class TD { [DllImport("kernel32")] public static extern IntPtr OpenThread(int a, bool i, int id);
 [DllImport("kernel32", CharSet=CharSet.Unicode)] public static extern int GetThreadDescription(IntPtr h, out IntPtr d); }
"@
function Snap { $p = Get-Process wwe2k15; $r=@{}; foreach($t in $p.Threads){ try { $r[$t.Id]=$t.TotalProcessorTime.TotalMilliseconds } catch {} }; $r }
$a = Snap; $t0=[DateTime]::Now; Start-Sleep $seconds; $b = Snap; $el=([DateTime]::Now-$t0).TotalMilliseconds
$p = Get-Process wwe2k15; $tot=0
$rows = foreach($id in $b.Keys){ if($a.ContainsKey($id)){ $pct=($b[$id]-$a[$id])/$el*100; $tot+=$pct
  $h=[TD]::OpenThread(0x0800,$false,$id); $d=[IntPtr]::Zero; [void][TD]::GetThreadDescription($h,[ref]$d); $n=[Runtime.InteropServices.Marshal]::PtrToStringUni($d)
  [pscustomobject]@{id=$id; name=$n; pct=[math]::Round($pct,1)} } }
$rows | Sort-Object pct -Descending | Select-Object -First 12 | Format-Table -AutoSize | Out-String -Width 200
"total {0:N0} % of one core" -f $tot
