# usage: d3d12_matrix.sh: D3D12 vs Vulkan backend in the demo match, on the NVIDIA GPU (720p, 1440p)
# and on the AMD iGPU (720p). Needs ~25 min of the PC. Results: work/dev/d3d12_summary.txt
D=$(cd "$(dirname "$0")" && pwd); S=/c/Dev/wwe2k15-pc2/work/dev; B=/c/Dev/wwe2k15-pc2/port/out/build/win-amd64-release
OUT=$S/d3d12_summary.txt; : > $OUT
run() { bash $D/gpu_compare.sh "$@" 2>&1 | tee -a $OUT; grep -h "GPU time" $S/$1.log | tail -3 | cut -c40- >> $OUT; grep -ch "DEVICE_HUNG\|device removed\|DXGI_ERROR" $S/$1.log >> $OUT; echo >> $OUT; }
run d3d12_1x $B --gpu_backend=d3d12 --resolution_scale=1
run vk_1x    $B --gpu_backend=vulkan --resolution_scale=1
run d3d12_2x $B --gpu_backend=d3d12 --resolution_scale=2
run vk_2x    $B --gpu_backend=vulkan --resolution_scale=2
run d3d12_amd $B --gpu_backend=d3d12 --d3d12_adapter=1 --resolution_scale=1
run vk_amd    $B --gpu_backend=vulkan --vulkan_device=1 --resolution_scale=1
echo DONE >> $OUT
