# usage: cold_exe_dir.sh <name> [toml]: copies the game exe + DLLs + config into work/dev/exe_<name> with an EMPTY cache (cold-cache / fresh-install tests)
B=/c/Dev/wwe2k15-pc2/port/out/build/win-amd64-release; O=/c/Dev/wwe2k15-pc2/work/dev/exe_$1
rm -rf "$O"; mkdir -p "$O/logs"; cp $B/wwe2k15.exe $B/*.dll $B/gamecontrollerdb.txt "$O/"; cp ${2:-/c/Dev/wwe2k15-pc2/port/wwe2k15_runtime.toml} "$O/wwe2k15.toml"; echo "$O"
