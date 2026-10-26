# usage: gpu_matrix.sh: FSI vs host render-target path at 720p and 1440p (warm cache), then a cold-cache run of each.
# Needs ~30 min of the PC. Results: work/dev/matrix_summary.txt
D=$(cd "$(dirname "$0")" && pwd); S=/c/Dev/wwe2k15-pc2/work/dev; B=/c/Dev/wwe2k15-pc2/port/out/build/win-amd64-release
OUT=$S/matrix_summary.txt; : > $OUT
run() { bash $D/gpu_compare.sh "$@" 2>&1 | tee -a $OUT; echo >> $OUT; }
run host_1x $B --render_target_path_vulkan=host --resolution_scale=1   # also warms the host cache
run fsi_1x  $B --render_target_path_vulkan=fsi  --resolution_scale=1
run host_2x $B --render_target_path_vulkan=host --resolution_scale=2
run fsi_2x  $B --render_target_path_vulkan=fsi  --resolution_scale=2
C=$(bash $D/cold_exe_dir.sh coldfsi);  run cold_fsi  $C --render_target_path_vulkan=fsi  --resolution_scale=1
C=$(bash $D/cold_exe_dir.sh coldhost); run cold_host $C --render_target_path_vulkan=host --resolution_scale=1
echo DONE >> $OUT
