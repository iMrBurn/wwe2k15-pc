# usage: smallcpu.sh <tag> <affinity hex mask: F=4 cores, 3=2 cores, FF=8> <exe dir> [game args]
# Launches the game limited to the given logical cores and measures the attract-mode demo match
# (fps, max gap, CPU per thread). Needs the PC; nothing else running (tasklist).
D=$(cd "$(dirname "$0")" && pwd); T=$1; M=$2; B=$3; shift 3
cd $B; (./wwe2k15.exe "--game_data_root=C:/Dev/games/WWE2K15-TU4" --log_max_files=200 --mnk_mode=true "$@" > /dev/null 2>&1 &)
sleep 3
powershell -NoProfile -Command "(Get-Process wwe2k15).ProcessorAffinity = 0x$M; 'affinity: ' + (Get-Process wwe2k15).ProcessorAffinity.ToInt64().ToString('X')"
bash $D/demo_measure.sh $T $B
