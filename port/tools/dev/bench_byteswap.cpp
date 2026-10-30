#include <immintrin.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
typedef uint32_t u32; typedef uint8_t u8;
template <bool F> __attribute__((noinline)) void old_(u8* dst, const u32* src, u32 size) {
  u32 count = size / 4; auto* out = (u32*)dst; u32 i = 0;
  const __m128i abs_mask = _mm_set1_epi32(0x7FFFFFFF), inf_bits = _mm_set1_epi32(0x7F800000);
  for (; i + 4 <= count; i += 4) {
    __m128i v = _mm_loadu_si128((const __m128i*)(src + i));
    v = _mm_or_si128(_mm_slli_epi16(v, 8), _mm_srli_epi16(v, 8));
    v = _mm_shufflelo_epi16(v, _MM_SHUFFLE(2, 3, 0, 1));
    v = _mm_shufflehi_epi16(v, _MM_SHUFFLE(2, 3, 0, 1));
    if (F) { __m128i n = _mm_cmpgt_epi32(_mm_and_si128(v, abs_mask), inf_bits); v = _mm_andnot_si128(n, v); }
    _mm_storeu_si128((__m128i*)(out + i), v);
  }
  for (; i < count; ++i) { u32 v = __builtin_bswap32(src[i]); out[i] = F ? ((v & 0x7FFFFFFFu) > 0x7F800000u ? 0u : v) : v; }
}
template <bool F> __attribute__((noinline)) void new_(u8* dst, const u32* src, u32 size) {
  u32 count = size / 4; auto* out = (u32*)dst; u32 i = 0;
  const __m128i abs_mask = _mm_set1_epi32(0x7FFFFFFF), inf_bits = _mm_set1_epi32(0x7F800000);
  const __m128i sh = _mm_set_epi8(12,13,14,15, 8,9,10,11, 4,5,6,7, 0,1,2,3);
  for (; i + 4 <= count; i += 4) {
    __m128i v = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)(src + i)), sh);
    if (F) { __m128i n = _mm_cmpgt_epi32(_mm_and_si128(v, abs_mask), inf_bits); v = _mm_andnot_si128(n, v); }
    _mm_storeu_si128((__m128i*)(out + i), v);
  }
  for (; i < count; ++i) { u32 v = __builtin_bswap32(src[i]); out[i] = F ? ((v & 0x7FFFFFFFu) > 0x7F800000u ? 0u : v) : v; }
}
int main() {
  srand(1); std::vector<u32> src(4096 + 8); for (auto& s : src) s = (u32)rand() * 2654435761u ^ rand();
  // bit exactness incl. unaligned tails and odd sizes
  int bad = 0;
  for (u32 size = 0; size <= 4096; size += 4) for (int off = 0; off < 4; ++off) {
    std::vector<u8> a(4100), b(4100); old_<true>(a.data(), src.data() + off, size); new_<true>(b.data(), src.data() + off, size); if (memcmp(a.data(), b.data(), size)) ++bad;
    old_<false>(a.data(), src.data() + off, size); new_<false>(b.data(), src.data() + off, size); if (memcmp(a.data(), b.data(), size)) ++bad;
  }
  printf("mismatches %d\n", bad);
  std::vector<u8> dst(8192);
  for (u32 size : {256u, 1024u, 4096u}) for (int variant = 0; variant < 2; ++variant) {
    auto t0 = std::chrono::steady_clock::now(); const int N = 2000000 * 1024 / size;
    for (int k = 0; k < N; ++k) { if (variant) new_<true>(dst.data(), src.data(), size); else old_<true>(dst.data(), src.data(), size); asm volatile("" ::: "memory"); }
    double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / N;
    printf("size %5u %s: %.1f ns/call (%.2f B/ns)\n", size, variant ? "pshufb" : "old   ", ns, size / ns);
  }
}
