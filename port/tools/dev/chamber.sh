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
if [ "$LOADPROF" = dump ]; then echo 0 > $S/${T}_dump.txt; export WWE2K15_DUMP_FILE=$(cygpath -w $S/${T}_dump.txt); fi
if [ "$LOADPROF" = ts ]; then echo 100 > $S/${T}_ts.txt; export WWE2K15_TIMESCALE_FILE=$(cygpath -w $S/${T}_ts.txt); fi
cd $B; (./$EXE "--game_data_root=C:/Dev/games/WWE2K15-TU4" --log_max_files=200 --mnk_mode=true --monitor=2 --keybind_dpad_up=Up --keybind_dpad_down=Down --keybind_dpad_left=Left --keybind_dpad_right=Right "$@" > /dev/null 2>&1 &)
[ -n "$AFF" ] && { sleep 3; powershell -NoProfile -Command "(Get-Process $PROC).ProcessorAffinity = 0x$AFF"; }
sleep 90
nav "ENTER:500,WAIT4" ${T}_s0
nav "WAIT4,ENTER:500,WAIT6,SPACE:400,WAIT20,SPACE:400,WAIT6" ${T}_s1; [ -n "$F9ALL" ] && nav "F9:100,WAIT2" ${T}_f9s1
nav "SPACE:300,WAIT3,S:130,WAIT1,S:130,WAIT1,S:130,WAIT1,S:130,WAIT1,SPACE:300,WAIT4" ${T}_s2; [ -n "$F9ALL" ] && nav "F9:100,WAIT2" ${T}_f9s2
nav "S:130,WAIT1,SPACE:300,WAIT3,SPACE:300,WAIT3,SPACE:300,WAIT5" ${T}_s3; [ -n "$F9ALL" ] && nav "F9:100,WAIT2" ${T}_f9s3
P="P:300,WAIT2,SPACE:300,WAIT2,SPACE:300,WAIT2,SPACE:300,WAIT2"
nav "$P,$P,$P" ${T}_s6; nav "$P,$P,$P,WAIT3" ${T}_s7; [ -n "$F9ALL" ] && nav "F9:100,WAIT2" ${T}_f9s7
t0=$(date +%s)
if [ "$LOADPROF" = draw ]; then nav "SPACE:300" ${T}_pl; sleep 3; nav "F9:100" ${T}_f9a; sleep 2; nav "F9:100" ${T}_f9b; sleep 20; nav "F9:100" ${T}_f9m; sleep 2; cp $(ls -t $B/logs/*.log | head -1) $S/${T}.log; taskkill //IM $EXE //F > /dev/null; exit 0; fi
if [ "$LOADPROF" = dump ]; then echo 1 > $S/${T}_dump.txt; sleep 2; nav "SPACE:300" ${T}_pl; sleep 3; echo 2 > $S/${T}_dump.txt; sleep 4; echo 3 > $S/${T}_dump.txt; sleep 18; echo 4 > $S/${T}_dump.txt; sleep 3; echo 5 > $S/${T}_dump.txt; sleep 3; nav "WAIT0" ${T}_end; taskkill //IM $EXE //F > /dev/null; exit 0; fi
if [ "$LOADPROF" = stackm ]; then nav "SPACE:300" ${T}_pl; sleep 35; for k in 1 2; do SCAN_HITS=40 SCAN_PROC=$PROC powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w $D/stackscan.ps1)" -out "$(cygpath -w $S/${T}_stack$k.txt)"; sleep 2; done; taskkill //IM $EXE //F > /dev/null; exit 0; fi
if [ "$LOADPROF" = ts ]; then TSF=$(cygpath -w $S/${T}_ts.txt); echo 100 > $S/${T}_ts.txt; nav "SPACE:300" ${T}_ld_0; echo ${TSVAL:-400} > $S/${T}_ts.txt; for i in 1 2 3 4 5 6 7 8; do nav "WAIT0" ${T}_ld_$i; echo "ld $i $(( $(date +%s) - t0 )) s"; [ $i = 4 ] && echo 100 > $S/${T}_ts.txt; done; echo 100 > $S/${T}_ts.txt; sleep 20; nav "WAIT0" ${T}_end; cp $(ls -t $B/logs/*.log | head -1) $S/${T}.log; taskkill //IM $EXE //F > /dev/null; exit 0; fi
if [ -n "$LOADPROF" ]; then nav "SPACE:300" ${T}_pl; if [ "$LOADPROF" = stack ]; then sleep 4; SCAN_PROC=$PROC powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w $D/stackscan.ps1)" -out "$(cygpath -w $S/${T}_stack.txt)"; sleep 3; SCAN_PROC=$PROC powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w $D/stackscan.ps1)" -out "$(cygpath -w $S/${T}_stack2.txt)"; elif [ "$LOADPROF" = samp ]; then powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w $D/sampler.ps1)" -seconds 14 -topThreads 6 -proc $PROC -out "$(cygpath -w $S/${T}_samp.txt)"; else powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w $D/thread_cpu.ps1)" -seconds 14 > $S/${T}_loadcpu.txt; fi; nav "WAIT0" ${T}_ld_8; cp $(ls -t $B/logs/*.log | head -1) $S/${T}.log; taskkill //IM $EXE //F > /dev/null; exit 0; fi
nav "SPACE:300" ${T}_ld_0
for i in 1 2 3 4 5 6 7 8; do nav "WAIT0" ${T}_ld_$i; echo "ld $i $(( $(date +%s) - t0 )) s"; done
sleep 30
powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w $D/thread_cpu.ps1)" -seconds 30 > $S/${T}_cpu.txt
nav "WAIT0" ${T}_end
cp $(ls -t $B/logs/*.log | head -1) $S/${T}.log
taskkill //IM $EXE //F > /dev/null; sleep 3
grep total $S/${T}_cpu.txt; grep "Guest pacing" $S/$T.log | tail -6 | grep -oE "\([0-9.]+ fps\), max gap [0-9.]+" | tr '\n' ';'; echo
