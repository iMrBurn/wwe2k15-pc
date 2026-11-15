# Sends a key sequence to the running game, then saves a full-screen capture in work\dev\<shot>.png.
#   -keys "ENTER:300,WAIT6,S:150,WAIT1,SPACE:200"   KEY[:hold ms], WAIT<seconds>, DUP/DDOWN = Shift+arrow
# Keys go out only while the game window is in front; otherwise nothing is sent, "NOFOCUS" is
# printed and the script exits with code 2 (callers must stop their loop). The window is asked to
# come forward once, with no key injected (no Alt trick: it opened menus in the app the user was
# using), and every key checks again that the foreground window is the game's.
# On an AZERTY keyboard the game's "W" (stick up) is the key Z.
param([string]$keys, [string]$shot="nav")
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
$S="C:\Dev\wwe2k15-pc2\work\dev"; New-Item -ItemType Directory -Force $S | Out-Null
if(-not ("K" -as [type])){ Add-Type @"
using System; using System.Runtime.InteropServices;
public class K { [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, uint fl, UIntPtr ex);
 [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint c, uint t);
 public static void Tap(byte vk, int ms){ byte sc=(byte)MapVirtualKey(vk,0); keybd_event(vk,sc,0,UIntPtr.Zero); System.Threading.Thread.Sleep(ms); keybd_event(vk,sc,2,UIntPtr.Zero);} }
"@ }
if(-not ("FG" -as [type])){ Add-Type @"
using System; using System.Runtime.InteropServices;
public class FG { [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder s, int n); }
"@ }
$gp = Get-Process wwe2k15,wwe2k15_native -ErrorAction SilentlyContinue | Select-Object -First 1
function GameFocused {
  if (-not $gp -or $gp.HasExited) { return $false }
  $fw = [FG]::GetForegroundWindow(); $fp = 0; [void][FG]::GetWindowThreadProcessId($fw, [ref]$fp)
  $title = New-Object System.Text.StringBuilder 256; [void][FG]::GetWindowText($fw, $title, 256)
  return ($fp -eq $gp.Id -and $title.ToString() -like "wwe2k15*")
}
$sendKeys = $true
if ($keys -and $keys -notmatch '^(WAIT\d+,?)*$') {
  if ($gp -and -not (GameFocused)) {
    [void][FG]::ShowWindow($gp.MainWindowHandle, 9); [void][FG]::SetForegroundWindow($gp.MainWindowHandle)
    Start-Sleep -Milliseconds 400
  }
  if (-not (GameFocused)) { Write-Output "NOFOCUS: game window not in front, keys not sent"; exit 2 }
}
Start-Sleep -Milliseconds 400
$map=@{ "ENTER"=0x0D; "SPACE"=0x20; "UP"=0x26; "DOWN"=0x28; "LEFT"=0x25; "RIGHT"=0x27; "W"=0x57; "S"=0x53; "A"=0x41; "D"=0x44; "BACK"=0x08; "TAB"=0x09; "P"=0x50; "L"=0x4C; "X"=0x58; "Z"=0x5A; "Q"=0x51; "1"=0x31; "3"=0x33; "I"=0x49; "K"=0x4B; "J"=0x4A; "F9"=0x78 }
foreach($k in $(if ($sendKeys) { $keys.Split(",") } else { @() })){ if (-not ($k -match "^WAIT") -and -not (GameFocused)) { Write-Output "NOFOCUS mid-sequence: stopped"; exit 2 }; if($k -match '^TYPE:(.*)$'){ [Windows.Forms.SendKeys]::SendWait($Matches[1].Replace("~e",[string][char]0xE9).Replace("~a",[string][char]0xE0).Replace("~c",[string][char]0xE7)); Start-Sleep -Milliseconds 700; continue }; $k=$k.Trim().ToUpper(); if($k -match '^WAIT(\d+)$'){ Start-Sleep -Seconds ([int]$Matches[1]) } elseif($k -match "^D(LEFT|RIGHT|UP|DOWN)$"){ [K]::keybd_event(0x10,0x2A,0,[UIntPtr]::Zero); [K]::Tap([byte]$map[$Matches[1]],250); [K]::keybd_event(0x10,0x2A,2,[UIntPtr]::Zero); Start-Sleep -Milliseconds 700 } elseif($k -match "^(\w+):(\d+)$"){ [K]::Tap([byte]$map[$Matches[1]],[int]$Matches[2]); Start-Sleep -Milliseconds 700 } else { [K]::Tap([byte]$map[$k],200); Start-Sleep -Milliseconds 700 } }
Start-Sleep 2
$b=[Windows.Forms.SystemInformation]::VirtualScreen; $bmp=New-Object Drawing.Bitmap $b.Width,$b.Height; $g=[Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Location,[Drawing.Point]::Empty,$b.Size); $bmp.Save("$S\$shot.png")
