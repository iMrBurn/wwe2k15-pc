param([int]$seconds = 20, [int]$topThreads = 4, [string]$out = "samples.txt")
# Statistical sampler: periodically suspends the busiest wwe2k15 threads and records RIP.
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class Smp {
  [DllImport("kernel32.dll")] public static extern IntPtr OpenThread(int a, bool i, int id);
  [DllImport("kernel32.dll")] public static extern int SuspendThread(IntPtr h);
  [DllImport("kernel32.dll")] public static extern int ResumeThread(IntPtr h);
  [DllImport("kernel32.dll")] public static extern bool GetThreadContext(IntPtr h, IntPtr ctx);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
  public static ulong Rip(IntPtr h, IntPtr buf) {
    Marshal.WriteInt32(buf, 0x30, 0x100001); // CONTEXT_CONTROL
    if (SuspendThread(h) < 0) return 0;
    ulong rip = 0;
    if (GetThreadContext(h, buf)) rip = (ulong)Marshal.ReadInt64(buf, 0xF8);
    ResumeThread(h);
    return rip;
  }
}
"@
$p = Get-Process wwe2k15 -ErrorAction Stop
$mods = $p.Modules | ForEach-Object { [pscustomobject]@{ Name = $_.ModuleName; Base = [uint64]$_.BaseAddress.ToInt64(); End = [uint64]$_.BaseAddress.ToInt64() + [uint64]$_.ModuleMemorySize } }
# Pick the busiest threads over one second.
$t0 = @{}; foreach ($t in $p.Threads) { try { $t0[$t.Id] = $t.TotalProcessorTime.TotalMilliseconds } catch {} }
Start-Sleep 1; $p.Refresh()
$busy = $p.Threads | ForEach-Object { try { [pscustomobject]@{ Id = $_.Id; Ms = $_.TotalProcessorTime.TotalMilliseconds - $t0[$_.Id] } } catch {} } |
  Sort-Object Ms -Descending | Select-Object -First $topThreads
$busy | ForEach-Object { "thread {0}: {1:N0} ms/s" -f $_.Id, $_.Ms }
$raw = [Runtime.InteropServices.Marshal]::AllocHGlobal(1232 + 16)
$buf = [IntPtr](([int64]$raw + 15) -band -16)
$handles = @{}; foreach ($b in $busy) { $handles[$b.Id] = [Smp]::OpenThread(0x0002 -bor 0x0008 -bor 0x0040, $false, $b.Id) }
$counts = @{}
$deadline = (Get-Date).AddSeconds($seconds)
$n = 0
while ((Get-Date) -lt $deadline) {
  foreach ($b in $busy) {
    $rip = [Smp]::Rip($handles[$b.Id], $buf)
    if (-not $rip) { continue }
    $key = "{0} {1}" -f $b.Id, $rip
    $counts[$key] = 1 + [int]$counts[$key]
    $n++
  }
  Start-Sleep -Milliseconds 2
}
foreach ($h in $handles.Values) { [void][Smp]::CloseHandle($h) }
$counts.GetEnumerator() | Sort-Object Value -Descending | ForEach-Object {
  $id, $r = $_.Key.Split(" "); $rip = [uint64]$r
  $m = $mods | Where-Object { $rip -ge $_.Base -and $rip -lt $_.End } | Select-Object -First 1
  if ($m) { "{0} {1} {2} 0x{3:X}" -f $_.Value, $id, $m.Name, ($rip - $m.Base) } else { "{0} {1} ? 0x{2:X}" -f $_.Value, $id, $rip }
} | Set-Content $out
"samples: $n -> $out"
