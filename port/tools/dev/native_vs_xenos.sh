# usage: native_vs_xenos.sh <tag> <xenos|native> <exe dir> <affinity hex mask or -> [game args]
# One run of the attract-mode demo match on either renderer, optionally limited to some logical
# cores (F = 4, 3 = 2, FF = 8), and prints one summary block: time to the title screen, time to the
# demo match, fps / worst frame gap, CPU per thread, and (native) the [perf] lines.
# Needs the PC to itself: nothing else running (it refuses if a game or build is up).
# GPU choice goes in the game args: Xenos --vulkan_device=1, native --svr_gpu_device="AMD Radeon(TM) Graphics".
# native_matrix.sh runs the whole grid. Results: work/dev/<tag>.log, <tag>_cpu.txt, native_matrix_summary.txt.
D=$(cd "$(dirname "$0")" && pwd); S=/c/Dev/wwe2k15-pc2/work/dev; mkdir -p $S
T=$1; R=$2; B=$3; M=$4; shift 4
if tasklist | grep -qiE "wwe2k15|ninja|clang-cl"; then echo "BUSY: another game or build is running"; exit 1; fi
if [ "$R" = native ]; then EXE=wwe2k15_native.exe; PROC=wwe2k15_native; else EXE=wwe2k15.exe; PROC=wwe2k15; fi
OLD=$(ls -t $B/logs/*.log 2>/dev/null | head -1)   # the newest log is this run's once it differs from this
T0=$(date +%s)
cd $B; (./$EXE "--game_data_root=C:/Dev/games/WWE2K15-TU4" --log_max_files=200 --mnk_mode=true "$@" > /dev/null 2>&1 &)
if [ "$M" != "-" ]; then
  sleep 3
  powershell -NoProfile -Command "(Get-Process $PROC).ProcessorAffinity = 0x$M"
fi
# log file: written a few seconds after launch
until [ "$(ls -t $B/logs/*.log 2>/dev/null | head -1)" != "$OLD" ]; do sleep 1; done
L=$(ls -t $B/logs/*.log | head -1)
# title screen reached: the first "Guest pacing" line (both renderers print it every 5 s once frames flow)
t_title=0; until grep -q "Guest pacing" $L || [ $t_title -ge 300 ]; do sleep 2; t_title=$((t_title+2)); done
# demo match: Xenos logs "draws/frame min 3000+"; native has no such line, so wait for a [perf] interval whose
# render-thread work is >= 6 ms (menus are ~1-2 ms, a match 8-12 ms on the dev PC). UNVALIDATED trigger:
# check the first run's log and adjust the awk threshold if it fires early/late.
t=0
if [ "$R" = native ]; then
  until awk '/\[perf\] [0-9]+ frames/ { for(i=1;i<=NF;i++) if($i=="work"){ if($(i+1)+0>=6) f=1 } } END{exit !f}' $L || [ $t -ge 400 ]; do sleep 5; t=$((t+5)); done
else
  until grep -q "draws/frame min [3-9][0-9][0-9][0-9]" $L || [ $t -ge 400 ]; do sleep 5; t=$((t+5)); done
fi
T1=$(date +%s)
sleep 25
powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w $D/thread_cpu.ps1)" -seconds 30 > $S/${T}_cpu.txt
cp $L $S/${T}.log
taskkill //IM $EXE //F > /dev/null; sleep 3
echo "== $T ($R, affinity $M, args: $*)"
echo "title after ~${t_title}s of log time, demo match detected ${t}s later (wall $((T1-T0)) s from launch)"
grep total $S/${T}_cpu.txt; sed -n 4,8p $S/${T}_cpu.txt
echo -n "pacing (last 6): "; grep "Guest pacing" $S/$T.log | tail -6 | grep -o "[0-9.]* fps), max gap [0-9.]* ms" | tr '\n' ';'; echo
if [ "$R" = native ]; then
  grep "\[perf\]" $S/$T.log | tail -4
else
  grep -o "Vulkan.*swaps.*" $S/$T.log | tail -2
fi
