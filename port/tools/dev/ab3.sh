# usage: ab3.sh <tag> <exe dir> [game args]: title -> save-load shots -> 1v1 match; CPU at +150 s, then RIP samples
D=$(cd "$(dirname "$0")" && pwd); S=/c/Dev/wwe2k15-pc2/work/dev; mkdir -p $S; N="$(cygpath -w $D/nav.ps1)"; T=$1; B=$2; shift 2
# AFF=<hex mask> limits the game to some logical cores (F = 4, 3 = 2)
LAUNCH=./wwe2k15.exe
cd $B; ($LAUNCH "--game_data_root=C:/Dev/games/WWE2K15-TU4" --log_max_files=200 --mnk_mode=true --keybind_dpad_up=Up --keybind_dpad_down=Down --keybind_dpad_left=Left --keybind_dpad_right=Right --keybind_rstick_up=I --keybind_rstick_down=K --keybind_rstick_left=J --keybind_rstick_right=L "$@" > /dev/null 2>&1 &)
[ -n "$AFF" ] && { sleep 3; powershell -NoProfile -Command "(Get-Process wwe2k15).ProcessorAffinity = 0x$AFF"; }
sleep 90
powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "ENTER:500" -shot ${T}_s0
for i in 1 2 3; do powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "WAIT0" -shot ${T}_s$i; done
powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "WAIT4" -shot ${T}_menu0
powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "WAIT4,ENTER:500,WAIT6,SPACE:400,WAIT20,SPACE:400,WAIT6" -shot ${T}_menu
powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "SPACE:300,WAIT3,SPACE:300,WAIT3,SPACE:300,WAIT6,P:300,WAIT3,SPACE:300,WAIT3,SPACE:300,WAIT3,P:300,WAIT3,SPACE:300,WAIT3,SPACE:300,WAIT3,SPACE:300,WAIT4" -shot ${T}_sel
powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "SPACE:300,WAIT3,SPACE:300,WAIT4,SPACE:300,WAIT2" -shot ${T}_start
sleep 150; powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w $D/thread_cpu.ps1)" -seconds 30 > $S/${T}_cpu.txt
powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "WAIT0" -shot ${T}_mid
[ -n "$SAMPLE" ] && powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w $D/sampler.ps1)" -seconds 40 -topThreads 3 -out "$(cygpath -w $S/${T}_samples.txt)"
powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "WAIT0" -shot ${T}_end
taskkill //IM wwe2k15.exe //F > /dev/null; sleep 3
cp $(ls -t $B/logs/*.log | head -1) $S/${T}.log
