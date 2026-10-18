#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wrl/client.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

namespace {
ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> context;
ComPtr<IDXGISwapChain> swapchain;
ComPtr<ID3D11RenderTargetView> target;
UINT resize_w = 0, resize_h = 0;
fs::path root, user_data, game_data;
std::string page = "JOUER", status;
bool launch_requested = false;
ImFont* logo_font = nullptr;

void MakeTarget() {
  ComPtr<ID3D11Texture2D> back;
  if (SUCCEEDED(swapchain->GetBuffer(0, IID_PPV_ARGS(&back))))
    device->CreateRenderTargetView(back.Get(), nullptr, &target);
}

bool MakeDevice(HWND hwnd) {
  DXGI_SWAP_CHAIN_DESC sd{};
  sd.BufferCount = 2;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
  HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
      levels, 2, D3D11_SDK_VERSION, &sd, &swapchain, &device, nullptr, &context);
  if (FAILED(hr))
    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
        levels, 2, D3D11_SDK_VERSION, &sd, &swapchain, &device, nullptr, &context);
  if (FAILED(hr)) return false;
  MakeTarget();
  return true;
}

fs::path ExeDir() {
  wchar_t path[MAX_PATH]{};
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  return fs::path(path).parent_path();
}

void LoadSettings() {
  wchar_t local[MAX_PATH]{};
  SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, local);
  user_data = fs::path(local) / L"WWE2K15";
  fs::create_directories(user_data);
  const fs::path cfg = root / L"launcher-settings.txt";
  std::error_code ec;
  if (fs::exists(cfg, ec)) {
    std::wstring line;
    std::wifstream in(cfg);
    std::getline(in, line);
    if (!line.empty() && fs::exists(fs::path(line) / L"default.xex", ec)) game_data = line;
  }
  wchar_t desktop[MAX_PATH]{};
  SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, SHGFP_TYPE_CURRENT, desktop);
  for (const auto& p : {root / L"assets", root / L"game", root.parent_path() / L"assets",
                         fs::path(desktop) / L"WWE2K15"}) {
    if (game_data.empty() && fs::exists(p / L"default.xex", ec)) game_data = p;
  }
}

bool HasGame() {
  std::error_code ec;
  return !game_data.empty() && fs::exists(game_data / L"default.xex", ec);
}

void SaveSettings() {
  std::wofstream out(root / L"launcher-settings.txt", std::ios::trunc);
  if (out) out << game_data.wstring();
}

void OpenFolder(const fs::path& p) {
  std::error_code ec;
  fs::create_directories(p, ec);
  ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void BrowseGame() {
  IFileDialog* dialog = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&dialog)))) return;
  DWORD options = 0;
  dialog->GetOptions(&options);
  dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
  dialog->SetTitle(L"Choisis le dossier contenant default.xex");
  if (SUCCEEDED(dialog->Show(nullptr))) {
    IShellItem* item = nullptr;
    if (SUCCEEDED(dialog->GetResult(&item))) {
      PWSTR path = nullptr;
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        fs::path selected(path);
        if (fs::exists(selected / L"default.xex")) {
          game_data = selected;
          SaveSettings();
          status = "Dossier du jeu enregistré.";
        } else status = "Ce dossier ne contient pas default.xex.";
        CoTaskMemFree(path);
      }
      item->Release();
    }
  }
  dialog->Release();
}

void LaunchGame() {
  const fs::path exe = root / L"wwe2k15.exe";
  if (!fs::exists(exe)) { status = "wwe2k15.exe est introuvable : reconstruis le projet."; return; }
  if (!HasGame()) { status = "Sélectionne le dossier qui contient default.xex."; page = "RÉGLAGES"; return; }
  SaveSettings();
  const std::wstring args = L"--game_data_root=\"" + game_data.wstring() +
      L"\" --user_data_root=\"" + user_data.wstring() +
      L"\" --gpu_plugin=xenos --gpu_backend=vulkan --input_backend=sdl";
  SHELLEXECUTEINFOW info{sizeof(info)};
  info.fMask = SEE_MASK_NOCLOSEPROCESS;
  info.lpFile = exe.c_str();
  info.lpParameters = args.c_str();
  info.lpDirectory = root.c_str();
  info.nShow = SW_SHOWNORMAL;
  if (ShellExecuteExW(&info)) launch_requested = true;
  else status = "Windows n'a pas pu démarrer le jeu.";
}

void SlantedButton(const char* label, ImVec2 size, ImU32 color, bool selected, bool* pressed) {
  ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const float skew = 16.0f;
  ImVec2 pts[] = {{p.x + skew,p.y},{p.x + size.x,p.y},{p.x + size.x-skew,p.y+size.y},{p.x,p.y+size.y}};
  draw->AddConvexPolyFilled(pts, 4, color);
  if (selected) draw->AddLine(pts[3], pts[2], IM_COL32(255,255,255,100), 2.0f);
  ImGui::InvisibleButton(label, size);
  const ImVec2 t = ImGui::CalcTextSize(label);
  draw->AddText(ImVec2(p.x + (size.x-t.x)*0.5f, p.y + (size.y-t.y)*0.5f),
                selected ? IM_COL32(20,20,22,255) : IM_COL32(238,238,240,255), label);
  *pressed = ImGui::IsItemClicked();
}

void DrawUi() {
  ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos({0,0});
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::Begin("WWE 2K15 Launcher", nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 size = ImGui::GetWindowSize();
  draw->AddRectFilled(ImVec2(0,0), ImVec2(280,size.y), IM_COL32(8,8,10,255));
  draw->AddRectFilled(ImVec2(0,size.y-35), ImVec2(280,size.y-23), IM_COL32(176,20,37,255));
  draw->AddRectFilled(ImVec2(0,size.y-20), ImVec2(240,size.y-14), IM_COL32(207,166,58,255));
  draw->AddText(logo_font, 36, {30,38}, IM_COL32(235,235,237,255), "WWE");
  draw->AddText(logo_font, 64, {30,77}, IM_COL32(190,25,44,255), "2K15");
  draw->AddText({31,148}, IM_COL32(156,156,160,255), "PC  |  RECOMPILATION NATIVE");

  const char* nav[] = {"JOUER", "RÉGLAGES", "CONTRÔLES", "MODS", "SAUVEGARDES", "À PROPOS"};
  ImGui::SetCursorPos({16,206});
  float y = 206;
  for (const char* item : nav) {
    bool clicked = false;
    SlantedButton(item, {248,53}, page == item ? IM_COL32(195,26,45,255) : IM_COL32(31,31,35,255), page == item, &clicked);
    if (clicked) page = item;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
    y += 63;
  }
  ImGui::SetCursorPos({320,32});
  ImGui::BeginChild("content", {size.x-352,size.y-64}, false);
  ImGui::TextColored({0.88f,0.84f,0.72f,1}, "WWE 2K15 PC");
  ImGui::GetWindowDrawList()->AddLine(ImVec2(ImGui::GetWindowPos().x, ImGui::GetCursorScreenPos().y+6),
      ImVec2(ImGui::GetWindowPos().x+ImGui::GetWindowWidth(), ImGui::GetCursorScreenPos().y+6), IM_COL32(176,20,37,255), 3);
  ImGui::Dummy({0,16});
  if (page == "JOUER") {
    ImGui::TextDisabled("Version beta de développement");
    ImGui::Dummy({0,8});
    ImGui::TextColored(HasGame() ? ImVec4(.5f,.85f,.35f,1) : ImVec4(1,.72f,.25f,1),
        HasGame() ? "Fichiers du jeu trouvés." : "Fichiers du jeu à sélectionner dans Réglages.");
    ImGui::TextDisabled("GPU : Vulkan  |  Manette : SDL");
    ImGui::Dummy({0,36});
    bool play = false;
    SlantedButton("JOUER", {340,92}, IM_COL32(210,38,54,255), true, &play);
    if (play) LaunchGame();
    ImGui::SameLine();
    if (ImGui::Button("JOURNAUX", {156,54})) OpenFolder(root / L"logs");
    ImGui::Dummy({0,24});
    if (!status.empty()) ImGui::TextWrapped("%s", status.c_str());
    ImGui::SetCursorPosY(ImGui::GetWindowHeight()-78);
    ImGui::Separator();
    ImGui::TextDisabled("Aucune donnée du jeu n'est incluse. Une copie légale est requise.");
  } else if (page == "RÉGLAGES") {
    ImGui::Text("Dossier du jeu Xbox 360");
    ImGui::TextDisabled("Le dossier sélectionné doit contenir default.xex.");
    ImGui::Dummy({0,8});
    ImGui::TextWrapped("%s", game_data.empty() ? "Aucun dossier sélectionné" : game_data.string().c_str());
    if (ImGui::Button("Parcourir…", {150,38})) BrowseGame();
    ImGui::SameLine();
    if (ImGui::Button("Enregistrer", {150,38})) SaveSettings();
    ImGui::Dummy({0,22});
    ImGui::Text("Rendu : Vulkan (RTX / GPU haute performance)");
    ImGui::Text("Manette : SDL, dont DualSense");
    ImGui::TextDisabled("Les options de rendu avancées restent dans wwe2k15.toml.");
  } else if (page == "CONTRÔLES") {
    ImGui::Text("Manettes"); ImGui::Separator();
    ImGui::TextWrapped("Branche ta manette avant de lancer le jeu. SDL gère les manettes Xbox et PlayStation, dont la DualSense.");
    ImGui::Dummy({0,14}); ImGui::TextDisabled("La configuration des boutons se fait dans le jeu.");
  } else if (page == "MODS") {
    ImGui::TextWrapped("Espace de préparation des mods. Le remplacement de fichiers n'est pas encore activé dans cette beta.");
    if (ImGui::Button("Ouvrir le dossier mods", {210,38})) OpenFolder(root / L"mods");
  } else if (page == "SAUVEGARDES") {
    ImGui::TextWrapped("Sauvegardes et données utilisateur stockées dans LocalAppData\\WWE2K15.");
    if (ImGui::Button("Ouvrir le dossier", {190,38})) OpenFolder(user_data);
  } else {
    ImGui::TextWrapped("Portage PC non officiel de WWE 2K15, basé sur ReXGlue. Aucune donnée du jeu n'est fournie.");
    ImGui::Dummy({0,8});
    if (ImGui::Button("Ouvrir le dépôt GitHub", {220,38}))
      ShellExecuteW(nullptr, L"open", L"https://github.com/iMrBurn/wwe2k15-pc", nullptr, nullptr, SW_SHOWNORMAL);
    ImGui::Dummy({0,20}); ImGui::TextDisabled("Sans affiliation avec WWE, 2K ou Yuke's.");
  }
  ImGui::EndChild();
  ImGui::End();
}

LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (ImGui_ImplWin32_WndProcHandler(hwnd,msg,wp,lp)) return 1;
  if (msg == WM_SIZE && wp != SIZE_MINIMIZED) { resize_w = LOWORD(lp); resize_h = HIWORD(lp); return 0; }
  if (msg == WM_SYSCOMMAND && (wp & 0xfff0) == SC_KEYMENU) return 0;
  if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcW(hwnd,msg,wp,lp);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  root = ExeDir(); LoadSettings();
  WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, WndProc, 0, 0, instance, nullptr, nullptr, nullptr, nullptr,
                 L"WWE2K15Launcher", nullptr};
  RegisterClassExW(&wc);
  HWND hwnd = CreateWindowW(wc.lpszClassName, L"WWE 2K15 PC", WS_OVERLAPPEDWINDOW,
      100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);
  if (!MakeDevice(hwnd)) { MessageBoxW(hwnd,L"Impossible d'initialiser l'interface graphique.",L"WWE 2K15 PC",MB_ICONERROR); return 1; }
  ShowWindow(hwnd,SW_SHOWDEFAULT); UpdateWindow(hwnd);
  IMGUI_CHECKVERSION(); ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle(); style.WindowRounding=0; style.FrameRounding=2; style.ChildRounding=0;
  style.Colors[ImGuiCol_WindowBg] = ImVec4(.055f,.055f,.06f,1);
  style.Colors[ImGuiCol_Text] = ImVec4(.9f,.9f,.91f,1);
  style.Colors[ImGuiCol_Button] = ImVec4(.15f,.15f,.17f,1);
  style.Colors[ImGuiCol_ButtonHovered] = ImVec4(.65f,.12f,.17f,1);
  ImGuiIO& io=ImGui::GetIO(); logo_font = io.Fonts->AddFontDefault();
  if (fs::exists(L"C:/Windows/Fonts/impact.ttf"))
    logo_font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/impact.ttf", 52.0f);
  ImGui_ImplWin32_Init(hwnd); ImGui_ImplDX11_Init(device.Get(),context.Get());
  bool done=false;
  while (!done && !launch_requested) {
    MSG msg; while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); if(msg.message==WM_QUIT) done=true; }
    if(done || launch_requested) break;
    if(resize_w && resize_h) { target.Reset(); swapchain->ResizeBuffers(0,resize_w,resize_h,DXGI_FORMAT_UNKNOWN,0); resize_w=resize_h=0; MakeTarget(); }
    ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame(); DrawUi(); ImGui::Render();
    const float bg[]={.035f,.035f,.04f,1}; context->OMSetRenderTargets(1,target.GetAddressOf(),nullptr); context->ClearRenderTargetView(target.Get(),bg);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); swapchain->Present(1,0);
  }
  ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
  target.Reset(); swapchain.Reset(); context.Reset(); device.Reset(); DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName,wc.hInstance);
  CoUninitialize();
  return launch_requested ? 0 : 0;
}
