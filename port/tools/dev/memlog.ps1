param([string]$proc = "wwe2k15_native", [string]$out = "mem.csv", [int]$seconds = 600)
# Samples working set / private bytes / peak of a process every 5 s into a CSV (stops when it exits).
"time,ws_mb,private_mb,peak_ws_mb" | Out-File -Encoding ascii $out
$end = (Get-Date).AddSeconds($seconds)
while ((Get-Date) -lt $end) {
  $p = Get-Process $proc -ErrorAction SilentlyContinue
  if ($p) { "{0},{1},{2},{3}" -f (Get-Date -Format HH:mm:ss), [int]($p.WorkingSet64/1MB), [int]($p.PrivateMemorySize64/1MB), [int]($p.PeakWorkingSet64/1MB) | Out-File -Append -Encoding ascii $out }
  Start-Sleep -Seconds 5
}
