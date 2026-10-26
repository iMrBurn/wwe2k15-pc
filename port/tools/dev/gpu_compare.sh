# usage: gpu_compare.sh <tag> <exe dir> [game args]
# Starts the game from <exe dir>, waits for the attract-mode demo match, then measures 30 s of it:
# GPU load/clock (nvidia-smi, every 0.5 s), per-thread CPU, and the "Guest pacing" / presentation lines.
# Example: gpu_compare.sh fsi_1x $EXE --render_target_path_vulkan=fsi --resolution_scale=1
D=$(cd "$(dirname "$0")" && pwd); S=/c/Dev/wwe2k15-pc2/work/dev; mkdir -p $S; T=$1; B=$2; shift 2
if tasklist | grep -qiE "wwe2k15|ninja|clang-cl"; then echo "BUSY: another game or build is running"; exit 1; fi
cd $B; (./wwe2k15.exe "--game_data_root=C:/Dev/games/WWE2K15-TU4" --log_max_files=200 "$@" > /dev/null 2>&1 &)
sleep 20; L=$B/logs/$(ls -t $B/logs | grep log$ | head -1)
t=0; until grep -q "draws/frame min [3-9][0-9][0-9][0-9]" $L || [ $t -ge 500 ]; do sleep 5; t=$((t+5)); done
sleep 20
nvidia-smi --query-gpu=utilization.gpu,clocks.gr,power.draw --format=csv,noheader,nounits -lms 500 > $S/${T}_gpu.csv &
SM=$!
powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w $D/thread_cpu.ps1)" -seconds 30 > $S/${T}_cpu.txt
kill $SM 2>/dev/null
cp $L $S/${T}.log
taskkill //IM wwe2k15.exe //F > /dev/null; sleep 3
echo "== $T demo_at=${t}s args: $*"
awk -F, 'NR>2{u+=$1;c+=$2;p+=$3;n++; if($1>m)m=$1} END{printf "GPU util avg %.0f%% max %d%%, clock avg %.0f MHz, power avg %.0f W (%d samples)\n",u/n,m,c/n,p/n,n}' $S/${T}_gpu.csv
grep total $S/${T}_cpu.txt
echo "pacing (last 6):"; grep "Guest pacing" $S/$T.log | tail -6 | grep -o "[0-9.]* fps), max gap [0-9.]* ms" | tr '\n' ';'; echo
echo "pipelines created in game after storage load: $(grep -c 'Creating graphics pipeline state' $S/$T.log) total, $(grep -o 'Created [0-9]* graphics pipelines from Vulkan storage' $S/$T.log | head -1)"
grep "skipped for pending shaders" $S/$T.log | tail -2 | grep -o "[0-9]* swap requests, [0-9]* skipped for pending shaders"
