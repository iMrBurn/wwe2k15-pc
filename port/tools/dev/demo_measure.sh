# usage: demo_measure.sh <tag> <exe dir>: game already running from <exe dir>; waits for the demo match, measures 30 s
D=$(cd "$(dirname "$0")" && pwd); S=/c/Dev/wwe2k15-pc2/work/dev; mkdir -p $S; T=$1; B=$2
L=$B/logs/$(ls -t $B/logs | grep log$ | head -1)
t=0; until grep -q "draws/frame min [3-9][0-9][0-9][0-9]" $L || [ $t -ge 400 ]; do sleep 5; t=$((t+5)); done
sleep 25
powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w $D/thread_cpu.ps1)" -seconds 30 > $S/${T}_cpu.txt
cp $L $S/${T}.log
taskkill //IM wwe2k15.exe //F > /dev/null; sleep 3
echo "$T demo_at=$t $(grep total $S/${T}_cpu.txt)"; sed -n 4,8p $S/${T}_cpu.txt
grep "Guest pacing" $S/$T.log | tail -6 | grep -o "([0-9.]* fps), max gap [0-9.]* ms" | tr '\n' ' '; echo
