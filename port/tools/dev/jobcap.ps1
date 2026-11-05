param([string]$proc = "wwe2k15_native", [double]$core = 0.33, [int]$wait = 120, [int]$seconds = 600)
# Dev: caps a process's CPU with a Windows job object (hard cap) to <core> of one core, to stand in for a slower CPU.
# Waits up to <wait> s for the process to exist, then keeps the job alive for <seconds>.
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class Job {
  [DllImport("kernel32.dll")] public static extern IntPtr CreateJobObject(IntPtr a, string n);
  [DllImport("kernel32.dll")] public static extern bool SetInformationJobObject(IntPtr j, int c, IntPtr p, int l);
  [DllImport("kernel32.dll")] public static extern bool AssignProcessToJobObject(IntPtr j, IntPtr p);
  [StructLayout(LayoutKind.Sequential)] public struct CPU { public uint Flags; public uint CpuRate; }
  public static bool Cap(IntPtr job, uint rate) {
    CPU c; c.Flags = 0x1 | 0x4; c.CpuRate = rate; // ENABLE | HARD_CAP
    IntPtr p = Marshal.AllocHGlobal(8); Marshal.StructureToPtr(c, p, false);
    return SetInformationJobObject(job, 15, p, 8);
  }
}
"@
$logical = [Environment]::ProcessorCount
$rate = [uint32][math]::Max(1, [math]::Round($core / $logical * 10000))
$job = [Job]::CreateJobObject([IntPtr]::Zero, $null)
if (-not [Job]::Cap($job, $rate)) { Write-Output "cap failed"; exit 1 }
$end = (Get-Date).AddSeconds($wait)
while (-not ($p = Get-Process $proc -ErrorAction SilentlyContinue) -and (Get-Date) -lt $end) { Start-Sleep -Milliseconds 200 }
if (-not $p) { Write-Output "process not found"; exit 1 }
[void][Job]::AssignProcessToJobObject($job, $p.Handle)
Write-Output ("capped {0} to {1} of one core (rate {2}/10000 of {3} logical)" -f $proc, $core, $rate, $logical)
Start-Sleep -Seconds $seconds
