# usage: entrance.sh <tag> <exe dir> [game args]: title -> 1v1 match, then a screenshot every ~5 s for 150 s (entrances/victory at 30 vs 60 fps); EXE=wwe2k15_native.exe
D=$(cd "$(dirname "$0")" && pwd); S=/c/Dev/wwe2k15-pc2/work/dev; mkdir -p $S; N="$(cygpath -w $D/nav.ps1)"; T=$1; B=$2; shift 2
# AFF=<hex mask> limits the game to some logical cores (F = 4, 3 = 2)
LAUNCH=./${EXE:-wwe2k15.exe}
cd $B; ($LAUNCH "--game_data_root=C:/Dev/games/WWE2K15-TU4" --log_max_files=200 --mnk_mode=true --keybind_dpad_up=Up --keybind_dpad_down=Down --keybind_dpad_left=Left --keybind_dpad_right=Right --keybind_rstick_up=I --keybind_rstick_down=K --keybind_rstick_left=J --keybind_rstick_right=L "$@" > /dev/null 2>&1 &)
[ -n "$AFF" ] && { sleep 3; powershell -NoProfile -Command "(Get-Process $(basename ${EXE:-wwe2k15.exe} .exe)).ProcessorAffinity = 0x$AFF"; }
sleep 90
powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "ENTER:500" -shot ${T}_s0
for i in 1 2 3; do powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "WAIT0" -shot ${T}_s$i; done
powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "WAIT4" -shot ${T}_menu0
powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "WAIT4,ENTER:500,WAIT6,SPACE:400,WAIT20,SPACE:400,WAIT6" -shot ${T}_menu
powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "SPACE:300,WAIT3,SPACE:300,WAIT3,SPACE:300,WAIT6,P:300,WAIT3,SPACE:300,WAIT3,SPACE:300,WAIT3,P:300,WAIT3,SPACE:300,WAIT3,SPACE:300,WAIT3,SPACE:300,WAIT4" -shot ${T}_sel
powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "SPACE:300,WAIT3,SPACE:300,WAIT4,SPACE:300,WAIT1" -shot ${T}_start
t0=$(date +%s)
for i in $(seq 1 30); do powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "WAIT2" -shot ${T}_e$(printf %02d $i); echo "e$i $(( $(date +%s) - t0 ))s $(date +%T)"; done
taskkill //IM ${EXE:-wwe2k15.exe} //F > /dev/null; sleep 3
cp $(ls -t $B/logs/*.log | head -1) $S/${T}.log
