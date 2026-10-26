param([string]$out)
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class SS {
  [DllImport("kernel32.dll")] public static extern IntPtr OpenThread(int a, bool i, int id);
  [DllImport("kernel32.dll")] public static extern int SuspendThread(IntPtr h);
  [DllImport("kernel32.dll")] public static extern int ResumeThread(IntPtr h);
  [DllImport("kernel32.dll")] public static extern bool GetThreadContext(IntPtr h, IntPtr ctx);
  [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(int a, bool i, int id);
  [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr p, IntPtr a, byte[] b, int n, out IntPtr r);
  [DllImport("kernel32", CharSet=CharSet.Unicode)] public static extern int GetThreadDescription(IntPtr h, out IntPtr d);
}
"@
$p = Get-Process wwe2k15
$m = $p.Modules | Where-Object { $_.ModuleName -eq "wwe2k15.exe" }
$base = [uint64]$m.BaseAddress.ToInt64(); $end = $base + [uint64]$m.ModuleMemorySize
$ph = [SS]::OpenProcess(0x0010 -bor 0x0400, $false, $p.Id)
$raw = [Runtime.InteropServices.Marshal]::AllocHGlobal(1232 + 16); $buf = [IntPtr](([int64]$raw + 15) -band -16)
$lines = @()
foreach ($t in $p.Threads) {
  $h = [SS]::OpenThread(0x0002 -bor 0x0008 -bor 0x0040 -bor 0x0800, $false, $t.Id)
  $d=[IntPtr]::Zero; [void][SS]::GetThreadDescription($h,[ref]$d); $n=[Runtime.InteropServices.Marshal]::PtrToStringUni($d)
  [Runtime.InteropServices.Marshal]::WriteInt32($buf, 0x30, 0x100001)
  [void][SS]::SuspendThread($h)
  $ok = [SS]::GetThreadContext($h, $buf); $rsp = [Runtime.InteropServices.Marshal]::ReadInt64($buf, 0x98)
  $stack = New-Object byte[] 65536; $r=[IntPtr]::Zero
  [void][SS]::ReadProcessMemory($ph, [IntPtr]$rsp, $stack, 65536, [ref]$r)
  [void][SS]::ResumeThread($h)
  $hits = @()
  for ($i = 0; $i -lt 8192 -and $hits.Count -lt 6; $i++) { $v = [BitConverter]::ToUInt64($stack, $i*8); if ($v -ge $base -and $v -lt $end) { $hits += ("0x{0:X}" -f ($v - $base + 0x140000000)) } }
  if ($hits.Count) { $lines += "T $($t.Id) $n"; $lines += $hits }
}
$lines | Set-Content $out
"base 0x{0:X}" -f $base
