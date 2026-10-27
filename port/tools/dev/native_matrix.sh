# usage: native_matrix.sh [exe dir]: native vs Xenos on simulated small CPUs and on the AMD iGPU.
# Grid (about 12 runs x 3-4 min = 45 min of the PC; ask (Main)/(Rendu) first, nothing else may run):
#   renderer  : Xenos (host render-target path) and native
#   cores     : all, 4 (mask F), 2 (mask 3)
#   GPU       : the default (NVIDIA) and the AMD iGPU
# Plus one extra native run with svr_reuse_constant_blocks=true on 4 cores (A/B for the PC-off
# constant-block reuse, once that cvar is in the exe).
# Warm the shader caches first (one native and one Xenos launch) or the first rows include the one-off
# shader conversion / host pipeline builds. Results: work/dev/native_matrix_summary.txt
D=$(cd "$(dirname "$0")" && pwd); S=/c/Dev/wwe2k15-pc2/work/dev; B=${1:-/c/Dev/wwe2k15-pc2/port/out/build/win-amd64-release}
OUT=$S/native_matrix_summary.txt; : > $OUT
run() { bash $D/native_vs_xenos.sh "$@" 2>&1 | tee -a $OUT; echo >> $OUT; }
XI="--vulkan_device=1 --render_target_path_vulkan=host"          # Xenos on the iGPU
NI='--svr_gpu_device=AMD Radeon(TM) Graphics'                    # native on the iGPU
for cores in "all -" "c4 F" "c2 3"; do
  set -- $cores
  run x_dgpu_$1 xenos $B $2 --render_target_path_vulkan=host
  run n_dgpu_$1 native $B $2
  run x_igpu_$1 xenos $B $2 $XI
  run n_igpu_$1 native $B $2 "$NI"
done
run n_dgpu_c4_reuse native $B F --svr_reuse_constant_blocks=true
echo DONE >> $OUT
