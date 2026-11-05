// wwe2k15 - custom logo import (issues #21, #29)
//
// Create-a-Logo (main menu > CREATIONS > LOGO) keeps the picture being edited in an editor object P (TU4 addresses):
// the update flag at [P+92], the canvas object O = [P+16], the image at P+96 (8-byte header, then w*h ARGB words,
// big-endian) and two backup copies at P+0x40068 and P+0x80070. sub_83277F00(P) copies the image to the canvas when the
// flag is set. The same layout as WWE 2K14's editor (found there by Jarrod, BSD-3-Clause, github.com/Jarrodfeng/wwe2k14-recomp,
// src/logo_import.h); the 2K15 function was found with a write trap on the image pages.
//
// With wwe2k15_logo_import the PNG mods\logos\import.png (next to the executable) is resized to the canvas size (128x128 or
// 256x256, exactly) and written into the three image buffers the first time the editor updates its canvas after the file
// changed; the game then shows it and saves it as a normal logo on ACCEPT. Nothing is written to disk here. A missing,
// unreadable or oversized PNG does nothing (one log line per file version).
#include "generated/default/wwe2k15_init.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#endif

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging/macros.h>

REXCVAR_DEFINE_BOOL(wwe2k15_logo_import, false, "WWE2K15",
                    "Create-a-Logo: replace the picture in the editor with mods\\logos\\import.png (resized to the canvas)");

REX_EXTERN(__imp__sub_83277F00);
REX_EXTERN(__imp__sub_831F6848);

namespace {

constexpr uint32_t kFlag = 92;       // update flag
constexpr uint32_t kCanvas = 16;     // canvas object pointer
constexpr uint32_t kImage0 = 96;     // image: 8-byte header + w*h words
constexpr uint32_t kImage1 = 0x40068;
constexpr uint32_t kImage2 = 0x80070;
constexpr uint64_t kMaxFileBytes = 32ull << 20;
constexpr uint32_t kMaxSide = 8192;

struct Picture {  // straight-alpha BGRA, top-down
  uint32_t w = 0, h = 0;
  std::vector<uint8_t> bgra;
};

std::mutex g_mutex;
std::filesystem::file_time_type g_stamp{};
uintmax_t g_size = ~0ull;
bool g_have_stamp = false;
bool g_pending = false;      // a good picture is waiting to be written into the editor
Picture g_pic;
std::chrono::steady_clock::time_point g_next_check{};

uint32_t Ld32(const uint8_t* base, uint32_t a) {
  uint32_t v;
  std::memcpy(&v, base + a, 4);
  return __builtin_bswap32(v);
}
void St32(uint8_t* base, uint32_t a, uint32_t v) {
  v = __builtin_bswap32(v);
  std::memcpy(base + a, &v, 4);
}

#if defined(_WIN32)
template <typename T>
struct Com {
  T* p = nullptr;
  ~Com() { if (p) p->Release(); }
  T** put() { return &p; }
  T* operator->() const { return p; }
};

// Decode the PNG to BGRA at its own size. Returns an empty string on success, else the reason.
std::string LoadPng(const std::filesystem::path& path, Picture& out) {
  const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool co_ok = SUCCEEDED(co);  // S_FALSE = already initialised on this thread, still balanced by CoUninitialize
  std::string err;
  {
    Com<IWICImagingFactory> factory;
    Com<IWICBitmapDecoder> decoder;
    Com<IWICBitmapFrameDecode> frame;
    Com<IWICFormatConverter> conv;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())))) {
      err = "WIC is not available";
    } else if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                        WICDecodeMetadataCacheOnDemand, decoder.put()))) {
      err = "not a readable image";
    } else if (FAILED(decoder->GetFrame(0, frame.put()))) {
      err = "no image frame";
    } else {
      UINT w = 0, h = 0;
      frame->GetSize(&w, &h);
      if (!w || !h || w > kMaxSide || h > kMaxSide) {
        err = "image size " + std::to_string(w) + "x" + std::to_string(h) + " is not supported (max 8192)";
      } else if (FAILED(factory->CreateFormatConverter(conv.put())) ||
                 FAILED(conv->Initialize(frame.p, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                         WICBitmapPaletteTypeCustom))) {
        err = "cannot convert the image to 32-bit";
      } else {
        out.w = w;
        out.h = h;
        out.bgra.assign(size_t(w) * h * 4, 0);
        if (FAILED(conv->CopyPixels(nullptr, w * 4, UINT(out.bgra.size()), out.bgra.data()))) {
          err = "cannot read the pixels";
        }
      }
    }
  }
  if (co_ok) CoUninitialize();
  return err;
}
#else
std::string LoadPng(const std::filesystem::path&, Picture&) { return "PNG import is Windows only"; }
#endif

// Area-average (box) resize of straight-alpha BGRA to n x n; alpha-weighted so transparent pixels do not bleed.
std::vector<uint32_t> ToCanvas(const Picture& pic, uint32_t n) {
  std::vector<uint32_t> out(size_t(n) * n, 0);
  for (uint32_t y = 0; y < n; ++y) {
    const double y0 = double(y) * pic.h / n, y1 = double(y + 1) * pic.h / n;
    for (uint32_t x = 0; x < n; ++x) {
      const double x0 = double(x) * pic.w / n, x1 = double(x + 1) * pic.w / n;
      double sa = 0, sr = 0, sg = 0, sb = 0, sw = 0;
      for (uint32_t sy = uint32_t(y0); sy < pic.h && sy < y1; ++sy) {
        const double wy = std::min<double>(sy + 1, y1) - std::max<double>(sy, y0);
        for (uint32_t sx = uint32_t(x0); sx < pic.w && sx < x1; ++sx) {
          const double wx = std::min<double>(sx + 1, x1) - std::max<double>(sx, x0);
          const double wgt = wx * wy;
          const uint8_t* p = &pic.bgra[(size_t(sy) * pic.w + sx) * 4];
          const double a = p[3] / 255.0;
          sa += a * wgt;
          sr += p[2] * a * wgt;
          sg += p[1] * a * wgt;
          sb += p[0] * a * wgt;
          sw += wgt;
        }
      }
      if (sw <= 0 || sa <= 0) continue;  // fully transparent
      const auto c = [&](double v) { return uint32_t(std::min(255.0, std::max(0.0, v / sa + 0.5))); };
      const uint32_t a8 = uint32_t(std::min(255.0, sa / sw * 255.0 + 0.5));
      out[size_t(y) * n + x] = (a8 << 24) | (c(sr) << 16) | (c(sg) << 8) | c(sb);
    }
  }
  return out;
}

// Look at the file at most once a second; load it again only when its date or size changed.
void PollFile() {
  const auto now = std::chrono::steady_clock::now();
  if (now < g_next_check) return;
  g_next_check = now + std::chrono::seconds(1);
  const auto path = rex::filesystem::GetExecutableFolder() / "mods" / "logos" / "import.png";
  std::error_code ec;
  const auto t = std::filesystem::last_write_time(path, ec);
  const auto size = ec ? 0 : std::filesystem::file_size(path, ec);
  if (ec) {
    g_have_stamp = false;  // missing file: forget it, so putting it back (even unchanged) imports again
    g_pending = false;
    return;
  }
  if (g_have_stamp && t == g_stamp && size == g_size) return;
  g_have_stamp = true;
  g_stamp = t;
  g_size = size;
  g_pending = false;
  if (size > kMaxFileBytes) {
    REXLOG_WARN("logo import: {} is too big ({} bytes, max {}), ignored", path.string(), size, kMaxFileBytes);
    return;
  }
  Picture pic;
  const std::string err = LoadPng(path, pic);
  if (!err.empty()) {
    REXLOG_WARN("logo import: cannot use {}: {}", path.string(), err);
    return;
  }
  g_pic = std::move(pic);
  g_pending = true;
  REXLOG_INFO("logo import: {} loaded ({}x{}), it will replace the picture the next time Create-a-Logo updates its canvas",
              path.string(), g_pic.w, g_pic.h);
}

}  // namespace

extern "C" REX_FUNC(sub_83277F00) {
  if (REXCVAR_GET(wwe2k15_logo_import)) {
    const uint32_t editor = ctx.r3.u32;
    std::lock_guard<std::mutex> lock(g_mutex);
    PollFile();
    // Write when an update is already going to copy the image (flag set: the slot's own pixels were just loaded, or the user
    // drew), so what the slot loaded is not put back over the import afterwards.
    if (g_pending && editor && Ld32(base, editor + kFlag) != 0) {
      const uint32_t canvas = Ld32(base, editor + kCanvas);
      uint32_t size = 0;
      if (canvas) {
        PPCContext c = ctx;
        c.lr = 0;
        c.r3.u64 = canvas;
        __imp__sub_831F6848(c, base);
        size = c.r3.u32;
      }
      if (size != 128 && size != 256) {
        REXLOG_WARN("logo import: unexpected canvas size {}, nothing written", size);
        g_pending = false;
      } else {
        const std::vector<uint32_t> px = ToCanvas(g_pic, size);
        for (const uint32_t offset : {kImage0, kImage1, kImage2}) {
          uint8_t* dst = base + editor + offset + 8;  // after the 8-byte header
          for (size_t i = 0; i < px.size(); ++i) {
            const uint32_t v = __builtin_bswap32(px[i]);
            std::memcpy(dst + i * 4, &v, 4);
          }
        }
        St32(base, editor + kFlag, 1);
        g_pending = false;
        REXLOG_INFO("logo import: picture written into the editor ({}x{} canvas, editor {:08X})", size, size, editor);
      }
    }
  }
  __imp__sub_83277F00(ctx, base);
}
