# usage: chamber.sh <tag> <exe dir> [game args]   (EXE=wwe2k15_native.exe for the native renderer, AFF=<hex mask> to limit cores)
# Title -> Exhibition -> 6-MAN -> ELIMINATION CHAMBER with random wrestlers -> PLAY, then:
#   LOADTIME: screenshots every ~6 s after PLAY (work/dev/<tag>_ld_N.png, look for the first one that is not the loading screen)
#   CPU of the match 60 s after PLAY (<tag>_cpu.txt), pacing and perf lines from the log (<tag>.log)
# Keys only go to the game window (nav.ps1 refuses otherwise). One game at a time: check tasklist first.
# Path found 2026-10-30: S:130 moves a menu one step (the arrow keys do not), P = random wrestler,
# every wrestler needs pick + attire + entry order (3 SPACE). Check the shots (<tag>_s*.png) on the first use.
D=$(cd "$(dirname "$0")" && pwd); S=/c/Dev/wwe2k15-pc2/work/dev; mkdir -p $S; N="$(cygpath -w $D/nav.ps1)"; T=$1; B=$2; shift 2
if tasklist | grep -qiE "wwe2k15|ninja|clang-cl"; then echo "BUSY"; exit 1; fi
EXE=${EXE:-wwe2k15.exe}; PROC=$(basename $EXE .exe)
nav() { powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "$1" -shot "$2"; }
cd $B; (./$EXE "--game_data_root=C:/Dev/games/WWE2K15-TU4" --log_max_files=200 --mnk_mode=true --monitor=2 --keybind_dpad_up=Up --keybind_dpad_down=Down --keybind_dpad_left=Left --keybind_dpad_right=Right "$@" > /dev/null 2>&1 &)
[ -n "$AFF" ] && { sleep 3; powershell -NoProfile -Command "(Get-Process $PROC).ProcessorAffinity = 0x$AFF"; }
sleep 90
nav "ENTER:500,WAIT4" ${T}_s0
nav "WAIT4,ENTER:500,WAIT6,SPACE:400,WAIT20,SPACE:400,WAIT6" ${T}_s1
nav "SPACE:300,WAIT3,S:130,WAIT1,S:130,WAIT1,S:130,WAIT1,S:130,WAIT1,SPACE:300,WAIT4" ${T}_s2
nav "S:130,WAIT1,SPACE:300,WAIT3,SPACE:300,WAIT3,SPACE:300,WAIT5" ${T}_s3
P="P:300,WAIT2,SPACE:300,WAIT2,SPACE:300,WAIT2,SPACE:300,WAIT2"
nav "$P,$P,$P" ${T}_s6; nav "$P,$P,$P,WAIT3" ${T}_s7
t0=$(date +%s)
nav "SPACE:300" ${T}_ld_0
for i in 1 2 3 4 5 6 7 8; do nav "WAIT0" ${T}_ld_$i; echo "ld $i $(( $(date +%s) - t0 )) s"; done
sleep 30
powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w $D/thread_cpu.ps1)" -seconds 30 > $S/${T}_cpu.txt
nav "WAIT0" ${T}_end
cp $(ls -t $B/logs/*.log | head -1) $S/${T}.log
taskkill //IM $EXE //F > /dev/null; sleep 3
grep total $S/${T}_cpu.txt; grep "Guest pacing" $S/$T.log | tail -6 | grep -oE "\([0-9.]+ fps\), max gap [0-9.]+" | tr '\n' ';'; echo
