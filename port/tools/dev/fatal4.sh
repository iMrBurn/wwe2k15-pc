# usage: fatal4.sh <tag> <exe dir> [game args]   (EXE=wwe2k15_native.exe, AFF=<hex mask>)
# Title -> Exhibition -> 4-MAN (Fatal 4-Way) with Cena + random wrestlers -> entrances (4 x 70-100 s) -> match.
# Waits for the 4th "[fps] back to 60" in the log, then measures 40 s of the match: per-thread CPU (<tag>_cpu.txt), pacing and [perf] lines (<tag>.log).
# Keys only go to the game window (nav.ps1 refuses otherwise): keep the window in front. One game at a time.
D=$(cd "$(dirname "$0")" && pwd); S=/c/Dev/wwe2k15-pc2/work/dev; N="$(cygpath -w $D/nav.ps1)"; T=$1; B=$2; shift 2
if tasklist | grep -qiE "wwe2k15|ninja|clang-cl"; then echo "BUSY"; exit 1; fi
EXE=${EXE:-wwe2k15_native.exe}; PROC=$(basename $EXE .exe)
nav() { powershell -NoProfile -ExecutionPolicy Bypass -File "$N" -keys "$1" -shot "$2"; }
cd $B; (./$EXE "--game_data_root=C:/Dev/games/WWE2K15-TU4" --log_max_files=200 --mnk_mode=true --keybind_dpad_up=Up --keybind_dpad_down=Down --keybind_dpad_left=Left --keybind_dpad_right=Right "$@" > /dev/null 2>&1 &)
[ -n "$AFF" ] && { sleep 3; powershell -NoProfile -Command "(Get-Process $PROC).ProcessorAffinity = 0x$AFF"; }
sleep 90
nav "ENTER:500,WAIT4" ${T}_s0
nav "WAIT4,ENTER:500,WAIT6,SPACE:400,WAIT20,SPACE:400,WAIT6" ${T}_s1
nav "SPACE:300,WAIT3,S:130,WAIT1,S:130,WAIT1,S:130,WAIT1,SPACE:300,WAIT4" ${T}_s2
nav "SPACE:300,WAIT4,SPACE:300,WAIT4,SPACE:300,WAIT2,SPACE:300,WAIT2" ${T}_s3
P="P:300,WAIT2,SPACE:300,WAIT2,SPACE:300,WAIT2,SPACE:300,WAIT2"
nav "$P,$P,$P,WAIT3" ${T}_s4
L=$(ls -t $B/logs/*.log | head -1)
for i in $(seq 1 90); do n=$(grep -c "back to 60" $L); [ $n -ge 4 ] && break; sleep 10; done
sleep 25
nav "WAIT0" ${T}_m0
powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w $D/thread_cpu.ps1)" -seconds 30 > $S/${T}_cpu.txt
nav "WAIT0" ${T}_m1
cp $L $S/${T}.log
taskkill //IM $EXE //F > /dev/null; sleep 3
grep total $S/${T}_cpu.txt; grep "Guest pacing" $S/$T.log | tail -6 | grep -oE "\([0-9.]+ fps\), max gap [0-9.]+" | tr '\n' ';'; echo
