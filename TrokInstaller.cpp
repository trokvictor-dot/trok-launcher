// Trok Installer - instalador proprio do Trok Launcher, com a MESMA cara do app
// (ImGui + DX9, painel de arte a esquerda, botao no accent - estilo launcher de jogo).
// Payload: recurso RCDATA id 2 no formato TROKPAK1 (gerado pelo build-instalador.ps1).
// Arte do painel: recurso RCDATA id 3 (png). Icone: id 1.
// Modo desinstalar: "--remover" (copia-se para %TEMP% e roda "--remover2 <pasta>").
// Autoria: equipe TrokMods.

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <d3d9.h>
#include <wincodec.h>
#include <tlhelp32.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h" // degrade por vertice sem emendas
#include "imgui/backends/imgui_impl_dx9.h"
#include "imgui/backends/imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

#define JAN_W 880
#define JAN_H 540
#define VERSAO_INST "1.0"

static const ImU32 COR_ACCENT = IM_COL32(242, 97, 29, 255);
static const ImU32 COR_ACCENT_HI = IM_COL32(255, 138, 77, 255);

// regra das resolucoes (a MESMA do launcher): janela proporcional ao monitor e
// conteudo com escala amortecida = 1 + (razao-1)*0.5, presa em 0.85..1.25
static float gEsc = 1.0f;
static float S(float v) { return v * gEsc; }

static LPDIRECT3D9 gD3D;
static LPDIRECT3DDEVICE9 gDev;
static D3DPRESENT_PARAMETERS gPP;
static bool gRodando = true;
static ImFont *gFtBody, *gFtBold, *gFtMini, *gFtTitulo;
static IDirect3DTexture9* gTexArte = NULL;
static float gTexArteAR = 0.52f; // largura/altura

// etapas
enum { ET_CONFIG, ET_EMUSO, ET_INSTALANDO, ET_PRONTO, ET_REMOVER, ET_REMOVENDO, ET_REMOVIDO, ET_ERRO };
static int gEtapa = ET_CONFIG;
static char gPastaDestino[MAX_PATH];
static bool gAtalhoDesktop = true;
static bool gAtalhoIniciar = true;
static bool gApagarConfig = false;
static int  gArqAtual = 0;
static char gErro[256] = "";
static bool gPedirPasta = false;
static char gPastaRemover[MAX_PATH] = "";
static bool gModoAtt = false; // --atualizar: sem perguntas, troca os arquivos e reabre o launcher

// ---------- payload ----------
struct ArqPak { char nome[160]; const unsigned char* dados; unsigned int tam; };
#define MAX_PAK 64
static ArqPak gPak[MAX_PAK];
static int gNumPak = 0;

static bool LerPayload() {
    HRSRC r = FindResourceA(NULL, MAKEINTRESOURCEA(2), (LPCSTR)RT_RCDATA);
    if (!r) return false;
    HGLOBAL h = LoadResource(NULL, r);
    if (!h) return false;
    const unsigned char* p = (const unsigned char*)LockResource(h);
    DWORD total = SizeofResource(NULL, r);
    if (!p || total < 12 || memcmp(p, "TROKPAK1", 8) != 0) return false;
    const unsigned char* fim = p + total;
    p += 8;
    int n = *(const int*)p; p += 4;
    if (n < 0 || n > MAX_PAK) return false;
    // tabela (comparacoes pelo ESPACO RESTANTE: tamanho corrompido nao dá overflow de ponteiro)
    for (int i = 0; i < n; i++) {
        if ((int)(fim - p) < 2) return false;
        int nl = *(const unsigned short*)p; p += 2;
        if (nl <= 0 || nl > 159 || (int)(fim - p) < nl + 4) return false;
        memcpy(gPak[i].nome, p, nl); gPak[i].nome[nl] = 0; p += nl;
        gPak[i].tam = *(const unsigned int*)p; p += 4;
    }
    // dados
    for (int i = 0; i < n; i++) {
        if (gPak[i].tam > (unsigned int)(fim - p)) return false;
        gPak[i].dados = p;
        p += gPak[i].tam;
    }
    gNumPak = n;
    return true;
}

// ---------- utilidades ----------
static void CriarPastasDo(const char* caminho) { // cria as pastas intermediarias de um ARQUIVO
    char tmp[MAX_PATH];
    strncpy(tmp, caminho, MAX_PATH - 1); tmp[MAX_PATH - 1] = 0;
    for (char* c = tmp + 3; *c; c++) {
        if (*c == '\\') { *c = 0; CreateDirectoryA(tmp, NULL); *c = '\\'; }
    }
}

static bool GravarArquivo(const char* caminho, const unsigned char* dados, unsigned int tam) {
    CriarPastasDo(caminho);
    HANDLE f = CreateFileA(caminho, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE && GetLastError() == ERROR_SHARING_VIOLATION) {
        // exe em uso: o Windows deixa RENOMEAR um exe rodando - tira o velho do caminho e grava o novo
        char velho[MAX_PATH + 8];
        _snprintf(velho, sizeof(velho) - 1, "%s.velho", caminho);
        velho[sizeof(velho) - 1] = 0;
        DeleteFileA(velho);
        if (MoveFileExA(caminho, velho, MOVEFILE_REPLACE_EXISTING)) {
            f = CreateFileA(caminho, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (f == INVALID_HANDLE_VALUE) // nao gravou: DESFAZ o rename (instalacao nunca fica sem exe)
                MoveFileExA(velho, caminho, MOVEFILE_REPLACE_EXISTING);
        }
    }
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD esc = 0;
    BOOL ok = WriteFile(f, dados, tam, &esc, NULL);
    CloseHandle(f);
    return ok && esc == tam;
}

static bool LauncherRodandoEm(const char* pasta) {
    char alvo[MAX_PATH];
    _snprintf(alvo, MAX_PATH - 1, "%s\\Trok Launcher.exe", pasta);
    alvo[MAX_PATH - 1] = 0;
    if (GetFileAttributesA(alvo) == INVALID_FILE_ATTRIBUTES) return false;
    HANDLE f = CreateFileA(alvo, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_SHARING_VIOLATION;
    CloseHandle(f);
    return false;
}

static void FecharLauncherEm(const char* pasta) { // fecha SO o launcher que roda desta pasta
    char alvo[MAX_PATH];
    _snprintf(alvo, MAX_PATH - 1, "%s\\Trok Launcher.exe", pasta);
    alvo[MAX_PATH - 1] = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    if (Process32First(snap, &pe)) do {
        if (_stricmp(pe.szExeFile, "Trok Launcher.exe") != 0) continue;
        HANDLE h = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!h) continue;
        char caminho[MAX_PATH];
        DWORD n = MAX_PATH;
        if (QueryFullProcessImageNameA(h, 0, caminho, &n) && _stricmp(caminho, alvo) == 0) {
            TerminateProcess(h, 0);
            WaitForSingleObject(h, 3000);
        }
        CloseHandle(h);
    } while (Process32Next(snap, &pe));
    CloseHandle(snap);
    Sleep(250); // solta os locks de arquivo
}

static void CriarAtalho(const char* alvo, int pastaCSIDL, const char* nome) {
    char pasta[MAX_PATH];
    if (FAILED(SHGetFolderPathA(NULL, pastaCSIDL, NULL, 0, pasta))) return;
    char lnk[MAX_PATH];
    _snprintf(lnk, MAX_PATH - 1, "%s\\%s.lnk", pasta, nome); lnk[MAX_PATH - 1] = 0;
    IShellLinkA* sl = NULL;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkA, (void**)&sl))) return;
    sl->SetPath(alvo);
    char dirAlvo[MAX_PATH];
    strncpy(dirAlvo, alvo, MAX_PATH - 1); dirAlvo[MAX_PATH - 1] = 0;
    char* b = strrchr(dirAlvo, '\\'); if (b) *b = 0;
    sl->SetWorkingDirectory(dirAlvo);
    IPersistFile* pf = NULL;
    if (SUCCEEDED(sl->QueryInterface(IID_IPersistFile, (void**)&pf))) {
        wchar_t w[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, lnk, -1, w, MAX_PATH);
        pf->Save(w, TRUE);
        pf->Release();
    }
    sl->Release();
}

static void RemoverAtalho(int pastaCSIDL, const char* nome) {
    char pasta[MAX_PATH];
    if (FAILED(SHGetFolderPathA(NULL, pastaCSIDL, NULL, 0, pasta))) return;
    char lnk[MAX_PATH];
    _snprintf(lnk, MAX_PATH - 1, "%s\\%s.lnk", pasta, nome); lnk[MAX_PATH - 1] = 0;
    DeleteFileA(lnk);
}

static void RegistrarDesinstalador() {
    HKEY k;
    if (RegCreateKeyExA(HKEY_CURRENT_USER,
            "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TrokLauncher",
            0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) != ERROR_SUCCESS) return;
    char exe[MAX_PATH], des[MAX_PATH + 24], ico[MAX_PATH + 8];
    _snprintf(exe, MAX_PATH - 1, "%s\\Trok Launcher.exe", gPastaDestino);
    exe[MAX_PATH - 1] = 0;
    _snprintf(des, sizeof(des) - 1, "\"%s\\Desinstalar.exe\" --remover", gPastaDestino);
    des[sizeof(des) - 1] = 0;
    _snprintf(ico, sizeof(ico) - 1, "%s\\Trok Launcher.exe", gPastaDestino);
    ico[sizeof(ico) - 1] = 0;
    RegSetValueExA(k, "DisplayName", 0, REG_SZ, (BYTE*)"Trok Launcher", 14);
    RegSetValueExA(k, "DisplayVersion", 0, REG_SZ, (BYTE*)VERSAO_INST, (DWORD)strlen(VERSAO_INST) + 1);
    RegSetValueExA(k, "Publisher", 0, REG_SZ, (BYTE*)"Equipe TrokMods", 16);
    RegSetValueExA(k, "InstallLocation", 0, REG_SZ, (BYTE*)gPastaDestino, (DWORD)strlen(gPastaDestino) + 1);
    RegSetValueExA(k, "DisplayIcon", 0, REG_SZ, (BYTE*)ico, (DWORD)strlen(ico) + 1);
    RegSetValueExA(k, "UninstallString", 0, REG_SZ, (BYTE*)des, (DWORD)strlen(des) + 1);
    DWORD um = 1;
    RegSetValueExA(k, "NoModify", 0, REG_DWORD, (BYTE*)&um, 4);
    RegSetValueExA(k, "NoRepair", 0, REG_DWORD, (BYTE*)&um, 4);
    RegCloseKey(k);
}

// ---------- desenho (mini-kit do launcher) ----------
static ImU32 Cinza(int v, int a = 255) { return IM_COL32(v, v, v, a); }
static ImU32 LerpCor(ImU32 c1, ImU32 c2, float t) {
    int r1 = c1 & 255, g1 = (c1 >> 8) & 255, b1 = (c1 >> 16) & 255;
    int r2 = c2 & 255, g2 = (c2 >> 8) & 255, b2 = (c2 >> 16) & 255;
    return IM_COL32((int)(r1 + (r2 - r1) * t), (int)(g1 + (g2 - g1) * t), (int)(b1 + (b2 - b1) * t), 255);
}

static void RectGradV(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 topo, ImU32 base, float r) {
    // uma primitiva so, vertices tingidos: degrade limpo sem emendas de AA
    int v0 = dl->VtxBuffer.Size;
    dl->AddRectFilled(a, b, IM_COL32_WHITE, r);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, dl->VtxBuffer.Size,
                                                  a, ImVec2(a.x, b.y), topo, base);
}

static bool BotaoSec(const char* rotulo, ImVec2 tam) {
    ImGui::PushID(rotulo);
    bool cl = ImGui::InvisibleButton("##sec", tam);
    ImGui::PopID();
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    bool hov = ImGui::IsItemHovered();
    ImDrawList* d = ImGui::GetWindowDrawList();
    if (hov) d->AddRectFilled(a, b, Cinza(255, 12), S(9));
    d->AddRect(a, b, Cinza(hov ? 200 : 120), S(9), 0, S(1.5f));
    const char* fim = strstr(rotulo, "##");
    ImVec2 tsz = ImGui::CalcTextSize(rotulo, fim);
    d->AddText(ImVec2((a.x + b.x - tsz.x) * 0.5f, (a.y + b.y - tsz.y) * 0.5f), Cinza(hov ? 230 : 170), rotulo, fim);
    return cl;
}

static bool BotaoPrimario(const char* rotulo, ImVec2 tam) {
    ImGui::PushID(rotulo);
    bool cl = ImGui::InvisibleButton("##pri", tam);
    ImGui::PopID();
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    bool hov = ImGui::IsItemHovered();
    ImDrawList* d = ImGui::GetWindowDrawList();
    ImU32 topo = hov ? LerpCor(COR_ACCENT, IM_COL32(255, 255, 255, 255), 0.08f) : COR_ACCENT;
    RectGradV(d, a, b, topo, LerpCor(topo, COR_ACCENT_HI, 0.55f), S(12));
    ImVec2 tsz = ImGui::CalcTextSize(rotulo);
    d->AddText(ImVec2((a.x + b.x - tsz.x) * 0.5f, (a.y + b.y - tsz.y) * 0.5f), Cinza(12), rotulo);
    return cl;
}

static bool LinhaCheck(const char* rot, bool* v, float w) {
    ImGui::PushID(rot);
    bool cl = ImGui::InvisibleButton("##lc", ImVec2(w, S(32)));
    ImGui::PopID();
    if (cl) *v = !*v;
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    bool hov = ImGui::IsItemHovered();
    ImDrawList* d = ImGui::GetWindowDrawList();
    if (hov) d->AddRectFilled(a, b, Cinza(255, 12), S(7));
    ImVec2 ka(a.x + S(4), (a.y + b.y) * 0.5f - S(9)), kb(ka.x + S(18), ka.y + S(18));
    if (*v) {
        d->AddRectFilled(ka, kb, COR_ACCENT, S(5));
        d->AddLine(ImVec2(ka.x + S(4), ka.y + S(9.5f)), ImVec2(ka.x + S(7.5f), ka.y + S(13)), Cinza(12), S(2.2f));
        d->AddLine(ImVec2(ka.x + S(7.5f), ka.y + S(13)), ImVec2(ka.x + S(14), ka.y + S(5)), Cinza(12), S(2.2f));
    } else {
        d->AddRect(ka, kb, Cinza(hov ? 180 : 110), S(5), 0, S(1.5f));
    }
    ImVec2 tsz = ImGui::CalcTextSize(rot);
    d->AddText(ImVec2(ka.x + S(28), (a.y + b.y - tsz.y) * 0.5f), Cinza(hov ? 235 : 195), rot);
    return cl;
}

static void DesenhaMira(ImDrawList* dl, ImVec2 c, float tam, ImU32 cor) {
    float esp = tam * 0.11f, r = tam * 0.40f, tick = tam * 0.25f;
    dl->AddCircle(c, r, cor, 48, esp);
    for (int q = 0; q < 4; q++) {
        float ang = q * 1.5708f - 1.5708f;
        float dx = cosf(ang), dy = sinf(ang);
        ImVec2 p1(c.x + dx * (r + esp * 0.5f), c.y + dy * (r + esp * 0.5f));
        ImVec2 p2(c.x + dx * (r + esp * 0.5f - tick), c.y + dy * (r + esp * 0.5f - tick));
        dl->AddLine(p1, p2, cor, esp);
        dl->AddCircleFilled(p1, esp * 0.5f, cor);
        dl->AddCircleFilled(p2, esp * 0.5f, cor);
    }
}

// ---------- arte (recurso 3, png) ----------
static void CarregarArte(LPDIRECT3DDEVICE9 dev) {
    HRSRC r = FindResourceA(NULL, MAKEINTRESOURCEA(3), (LPCSTR)RT_RCDATA);
    if (!r) return;
    HGLOBAL h = LoadResource(NULL, r);
    const unsigned char* p = (const unsigned char*)LockResource(h);
    DWORD tam = SizeofResource(NULL, r);
    if (!p || !tam) return;
    IStream* st = SHCreateMemStream(p, tam);
    if (!st) return;
    IWICImagingFactory* fab = NULL;
    IWICBitmapDecoder* dec = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICBitmapSource* src = NULL;
    do {
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                    IID_IWICImagingFactory, (void**)&fab))) break;
        if (FAILED(fab->CreateDecoderFromStream(st, NULL, WICDecodeMetadataCacheOnDemand, &dec))) break;
        if (FAILED(dec->GetFrame(0, &frame))) break;
        if (FAILED(WICConvertBitmapSource(GUID_WICPixelFormat32bppBGRA, frame, &src))) break;
        UINT w = 0, hh = 0;
        src->GetSize(&w, &hh);
        if (!w || !hh || w > 4096 || hh > 4096) break;
        if (FAILED(dev->CreateTexture(w, hh, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &gTexArte, NULL))) { gTexArte = NULL; break; }
        D3DLOCKED_RECT lr;
        if (FAILED(gTexArte->LockRect(0, &lr, NULL, 0))) { gTexArte->Release(); gTexArte = NULL; break; }
        src->CopyPixels(NULL, lr.Pitch, lr.Pitch * hh, (BYTE*)lr.pBits);
        gTexArte->UnlockRect(0);
        gTexArteAR = (float)w / (float)hh;
    } while (0);
    if (src) src->Release();
    if (frame) frame->Release();
    if (dec) dec->Release();
    if (fab) fab->Release();
    st->Release();
}

// ---------- instalacao passo a passo (1 arquivo por frame = barra animada) ----------
static bool InstalarProximo() {
    if (gArqAtual >= gNumPak) return true;
    ArqPak& a = gPak[gArqAtual];
    char destino[MAX_PATH];
    _snprintf(destino, MAX_PATH - 1, "%s\\%s", gPastaDestino, a.nome);
    destino[MAX_PATH - 1] = 0;
    if (!GravarArquivo(destino, a.dados, a.tam)) {
        _snprintf(gErro, sizeof(gErro) - 1, "Nao consegui gravar: %s", a.nome);
        return false;
    }
    gArqAtual++;
    return true;
}

static void FinalizarInstalacao() {
    { // limpa o exe antigo renomeado numa atualizacao anterior (se sobrou)
        char velho[MAX_PATH + 8];
        _snprintf(velho, sizeof(velho) - 1, "%s\\Trok Launcher.exe.velho", gPastaDestino);
        velho[sizeof(velho) - 1] = 0;
        DeleteFileA(velho);
    }
    // desinstalador = copia deste exe
    char eu[MAX_PATH], des[MAX_PATH];
    GetModuleFileNameA(NULL, eu, MAX_PATH);
    _snprintf(des, MAX_PATH - 1, "%s\\Desinstalar.exe", gPastaDestino);
    des[MAX_PATH - 1] = 0;
    CopyFileA(eu, des, FALSE);
    char alvo[MAX_PATH];
    _snprintf(alvo, MAX_PATH - 1, "%s\\Trok Launcher.exe", gPastaDestino);
    if (!gModoAtt) { // atualizacao nao recria atalho que o usuario tenha apagado
        if (gAtalhoDesktop) CriarAtalho(alvo, CSIDL_DESKTOPDIRECTORY, "Trok Launcher");
        if (gAtalhoIniciar) CriarAtalho(alvo, CSIDL_PROGRAMS, "Trok Launcher");
    }
    RegistrarDesinstalador();
}

static void ExecutarRemocao(const char* pasta, bool apagarConfig) {
    Sleep(600); // da tempo do processo original fechar
    FecharLauncherEm(pasta); // se o launcher estiver aberto, fecha antes de apagar
    RemoverAtalho(CSIDL_DESKTOPDIRECTORY, "Trok Launcher");
    RemoverAtalho(CSIDL_PROGRAMS, "Trok Launcher");
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TrokLauncher");
    if (apagarConfig) {
        // pasta inteira para a LIXEIRA (recuperavel)
        char duplo[MAX_PATH + 2];
        memset(duplo, 0, sizeof(duplo));
        strncpy(duplo, pasta, MAX_PATH - 1);
        SHFILEOPSTRUCTA op = { 0 };
        op.wFunc = FO_DELETE;
        op.pFrom = duplo;
        op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
        SHFileOperationA(&op);
    } else {
        // so o programa; config/imagens/avatars ficam
        const char* nomes[4] = { "Trok Launcher.exe", "Trok Launcher.exe.velho", "Desinstalar.exe", "LEIA-ME.txt" };
        for (int i = 0; i < 4; i++) {
            char c[MAX_PATH + 8];
            _snprintf(c, sizeof(c) - 1, "%s\\%s", pasta, nomes[i]);
            c[sizeof(c) - 1] = 0;
            DeleteFileA(c);
        }
    }
}

// ---------- UI ----------
static void DesenhaUI(HWND hwnd) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 ds = io.DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ds);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##inst", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ds, Cinza(13));

    // ===== painel de ARTE a esquerda (proporcional a janela) =====
    float wArte = ds.x * (340.0f / 880.0f);
    if (gTexArte) {
        // cover crop
        float arPainel = wArte / ds.y;
        ImVec2 uv0(0, 0), uv1(1, 1);
        if (gTexArteAR > arPainel) { float f = arPainel / gTexArteAR; uv0.x = 0.5f - f * 0.5f; uv1.x = 0.5f + f * 0.5f; }
        else { float f = gTexArteAR / arPainel; uv0.y = 0.5f - f * 0.5f; uv1.y = 0.5f + f * 0.5f; }
        dl->AddImage((ImTextureID)gTexArte, ImVec2(0, 0), ImVec2(wArte, ds.y), uv0, uv1);
        dl->AddRectFilledMultiColor(ImVec2(0, ds.y - S(150)), ImVec2(wArte, ds.y),
            Cinza(8, 0), Cinza(8, 0), Cinza(8, 200), Cinza(8, 200));
    } else {
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(wArte, ds.y), Cinza(9));
        DesenhaMira(dl, ImVec2(wArte * 0.5f, ds.y * 0.42f), wArte * 0.44f, COR_ACCENT);
    }
    dl->AddRectFilled(ImVec2(wArte, 0), ImVec2(wArte + 1, ds.y), Cinza(34));

    // fechar (X)
    ImGui::SetCursorScreenPos(ImVec2(ds.x - S(44), S(12)));
    if (ImGui::InvisibleButton("##fx", ImVec2(S(30), S(26))) && gEtapa != ET_INSTALANDO && gEtapa != ET_REMOVENDO)
        gRodando = false;
    {
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        if (ImGui::IsItemHovered()) dl->AddRectFilled(a, b, IM_COL32(200, 60, 54, 230), S(6));
        dl->AddLine(ImVec2(a.x + S(10), a.y + S(8)), ImVec2(b.x - S(10), b.y - S(8)), Cinza(220), S(1.6f));
        dl->AddLine(ImVec2(a.x + S(10), b.y - S(8)), ImVec2(b.x - S(10), a.y + S(8)), Cinza(220), S(1.6f));
    }

    float px = wArte + S(44); // margem do painel direito
    float wCampo = ds.x - px - S(44);

    ImGui::PushFont(gFtTitulo);
    dl->AddText(ImVec2(px - S(2), S(54)), Cinza(240), "TROK LAUNCHER");
    ImGui::PopFont();

    if (gEtapa == ET_CONFIG) {
        ImGui::PushFont(gFtBody);
        dl->AddText(ImVec2(px, S(120)), Cinza(160), "O seu SA-MP, moderno. Nao substitui nenhum arquivo do jogo.");
        ImGui::PopFont();

        ImGui::PushFont(gFtMini);
        dl->AddText(ImVec2(px, S(168)), Cinza(140), "P A S T A   D E   I N S T A L A C A O");
        ImGui::PopFont();
        // campo da pasta + procurar
        ImGui::SetCursorScreenPos(ImVec2(px, S(188)));
        ImGui::PushFont(gFtBody);
        ImVec2 ca(px, S(188)), cb(px + wCampo - S(118), S(188) + S(36));
        dl->AddRectFilled(ca, cb, Cinza(20), S(9));
        dl->AddRect(ca, cb, Cinza(46), S(9), 0, 1);
        ImGui::PushClipRect(ca, ImVec2(cb.x - S(8), cb.y), true);
        dl->AddText(ImVec2(ca.x + S(12), ca.y + S(9)), Cinza(190), gPastaDestino);
        ImGui::PopClipRect();
        ImGui::SetCursorScreenPos(ImVec2(cb.x + S(10), S(188)));
        if (BotaoSec("Procurar...", ImVec2(S(108), S(36)))) gPedirPasta = true;
        ImGui::PopFont();

        ImGui::PushFont(gFtBody);
        ImGui::SetCursorScreenPos(ImVec2(px - S(4), S(248)));
        LinhaCheck("Criar atalho na area de trabalho", &gAtalhoDesktop, wCampo);
        ImGui::SetCursorScreenPos(ImVec2(px - S(4), S(284)));
        LinhaCheck("Criar atalho no menu Iniciar", &gAtalhoIniciar, wCampo);
        ImGui::PopFont();

        // requisito de espaco (payload e minusculo, mas informa)
        unsigned int total = 0;
        for (int i = 0; i < gNumPak; i++) total += gPak[i].tam;
        char inf[96];
        sprintf(inf, "Espaco necessario: %.1f MB", total / 1048576.0f);
        ImGui::PushFont(gFtMini);
        dl->AddText(ImVec2(px, S(334)), Cinza(110), inf);
        ImGui::PopFont();

        ImGui::PushFont(gFtBold);
        ImGui::SetCursorScreenPos(ImVec2(px, ds.y - S(96)));
        if (BotaoPrimario("INSTALAR", ImVec2(S(216), S(52)))) {
            gArqAtual = 0;
            gErro[0] = 0;
            gEtapa = LauncherRodandoEm(gPastaDestino) ? ET_EMUSO : ET_INSTALANDO;
        }
        ImGui::SetCursorScreenPos(ImVec2(px + S(228), ds.y - S(96)));
        if (BotaoSec("Cancelar", ImVec2(S(120), S(52)))) gRodando = false;
        ImGui::PopFont();
    }
    else if (gEtapa == ET_EMUSO) {
        ImGui::PushFont(gFtBody);
        dl->AddText(ImVec2(px, S(140)), Cinza(200), "O Trok Launcher esta aberto neste computador.");
        dl->AddText(ImVec2(px, S(168)), Cinza(140), "Preciso fechar ele para atualizar os arquivos (suas configuracoes ficam).");
        ImGui::PopFont();
        ImGui::PushFont(gFtBold);
        ImGui::SetCursorScreenPos(ImVec2(px, ds.y - S(96)));
        if (BotaoPrimario("FECHAR E INSTALAR", ImVec2(S(236), S(52)))) {
            FecharLauncherEm(gPastaDestino);
            gArqAtual = 0;
            gEtapa = ET_INSTALANDO;
        }
        ImGui::SetCursorScreenPos(ImVec2(px + S(248), ds.y - S(96)));
        if (BotaoSec("Voltar", ImVec2(S(110), S(52)))) gEtapa = ET_CONFIG;
        ImGui::PopFont();
    }
    else if (gEtapa == ET_INSTALANDO) {
        // 1 arquivo por frame
        if (!InstalarProximo()) gEtapa = ET_ERRO;
        else if (gArqAtual >= gNumPak) {
            FinalizarInstalacao();
            if (gModoAtt) { // atualizacao: reabre o launcher novo e some
                char alvoA[MAX_PATH];
                _snprintf(alvoA, MAX_PATH - 1, "%s\\Trok Launcher.exe", gPastaDestino);
                ShellExecuteA(NULL, "open", alvoA, NULL, gPastaDestino, SW_SHOWNORMAL);
                gRodando = false;
            }
            gEtapa = ET_PRONTO;
        }
        float frac = gNumPak ? (float)gArqAtual / gNumPak : 1.0f;
        ImGui::PushFont(gFtBody);
        dl->AddText(ImVec2(px, S(150)), Cinza(180), gModoAtt ? "Atualizando o Trok Launcher..." : "Instalando...");
        ImGui::PopFont();
        ImVec2 ba(px, S(190)), bb(px + wCampo, S(198));
        dl->AddRectFilled(ba, bb, Cinza(34), S(4));
        if (frac > 0)
            dl->AddRectFilled(ba, ImVec2(ba.x + (bb.x - ba.x) * frac, bb.y), COR_ACCENT, S(4));
        if (gArqAtual < gNumPak) {
            ImGui::PushFont(gFtMini);
            dl->AddText(ImVec2(px, S(210)), Cinza(120), gPak[gArqAtual].nome);
            ImGui::PopFont();
        }
    }
    else if (gEtapa == ET_PRONTO) {
        ImGui::PushFont(gFtBody);
        dl->AddText(ImVec2(px, S(140)), Cinza(200), "Pronto! O Trok Launcher esta instalado.");
        dl->AddText(ImVec2(px, S(168)), Cinza(140), "Na primeira abertura ele importa seu nick e seus favoritos do SA-MP.");
        ImGui::PopFont();
        ImGui::PushFont(gFtBold);
        ImGui::SetCursorScreenPos(ImVec2(px, ds.y - S(96)));
        if (BotaoPrimario("ABRIR O LAUNCHER", ImVec2(S(238), S(52)))) {
            char alvo[MAX_PATH];
            _snprintf(alvo, MAX_PATH - 1, "%s\\Trok Launcher.exe", gPastaDestino);
            ShellExecuteA(NULL, "open", alvo, NULL, gPastaDestino, SW_SHOWNORMAL);
            gRodando = false;
        }
        ImGui::SetCursorScreenPos(ImVec2(px + S(250), ds.y - S(96)));
        if (BotaoSec("Fechar", ImVec2(S(110), S(52)))) gRodando = false;
        ImGui::PopFont();
    }
    else if (gEtapa == ET_REMOVER) {
        ImGui::PushFont(gFtBody);
        dl->AddText(ImVec2(px, S(140)), Cinza(200), "Remover o Trok Launcher deste computador?");
        ImGui::SetCursorScreenPos(ImVec2(px - S(4), S(186)));
        LinhaCheck("Apagar tambem configuracoes, contas e imagens", &gApagarConfig, wCampo);
        ImGui::PopFont();
        ImGui::PushFont(gFtMini);
        dl->AddText(ImVec2(px + S(24), S(224)), Cinza(110), "(vai para a Lixeira do Windows, da para recuperar)");
        ImGui::PopFont();
        ImGui::PushFont(gFtBold);
        ImGui::SetCursorScreenPos(ImVec2(px, ds.y - S(96)));
        ImGui::PushID("rm");
        bool rm = ImGui::InvisibleButton("##rm", ImVec2(S(180), S(52)));
        ImGui::PopID();
        {
            ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
            bool hov = ImGui::IsItemHovered();
            ImU32 verm = IM_COL32(214, 74, 68, 255);
            RectGradV(dl, a, b, hov ? LerpCor(verm, IM_COL32(255,255,255,255), 0.08f) : verm, IM_COL32(238, 112, 104, 255), S(12));
            ImVec2 tsz = ImGui::CalcTextSize("REMOVER");
            dl->AddText(ImVec2((a.x + b.x - tsz.x) * 0.5f, (a.y + b.y - tsz.y) * 0.5f), Cinza(250), "REMOVER");
        }
        if (rm) gEtapa = ET_REMOVENDO;
        ImGui::SetCursorScreenPos(ImVec2(px + S(192), ds.y - S(96)));
        if (BotaoSec("Cancelar", ImVec2(S(120), S(52)))) gRodando = false;
        ImGui::PopFont();
    }
    else if (gEtapa == ET_REMOVENDO) {
        ImGui::PushFont(gFtBody);
        dl->AddText(ImVec2(px, S(150)), Cinza(180), "Removendo...");
        ImGui::PopFont();
        static int frameRem = 0; // o "Removendo..." aparece ANTES do trabalho pesado
        if (++frameRem >= 2) {
            ExecutarRemocao(gPastaRemover, gApagarConfig);
            gEtapa = ET_REMOVIDO;
        }
    }
    else if (gEtapa == ET_REMOVIDO) {
        ImGui::PushFont(gFtBody);
        dl->AddText(ImVec2(px, S(140)), Cinza(200), "Trok Launcher removido. Valeu por ter usado!");
        ImGui::PopFont();
        ImGui::PushFont(gFtBold);
        ImGui::SetCursorScreenPos(ImVec2(px, ds.y - S(96)));
        if (BotaoPrimario("FECHAR", ImVec2(S(150), S(52)))) gRodando = false;
        ImGui::PopFont();
    }
    else if (gEtapa == ET_ERRO) {
        ImGui::PushFont(gFtBody);
        dl->AddText(ImVec2(px, S(140)), IM_COL32(240, 120, 116, 255), "Algo deu errado:");
        dl->AddText(ImVec2(px, S(166)), Cinza(180), gErro);
        ImGui::PopFont();
        ImGui::PushFont(gFtBold);
        ImGui::SetCursorScreenPos(ImVec2(px, ds.y - S(96)));
        if (BotaoPrimario("TENTAR DE NOVO", ImVec2(S(206), S(52)))) gEtapa = ET_CONFIG;
        ImGui::SetCursorScreenPos(ImVec2(px + S(218), ds.y - S(96)));
        if (BotaoSec("Fechar", ImVec2(S(110), S(52)))) gRodando = false;
        ImGui::PopFont();
    }

    // rodape
    ImGui::PushFont(gFtMini);
    dl->AddText(ImVec2(px, ds.y - S(30)), Cinza(85), "Trok Launcher v" VERSAO_INST "  -  equipe TrokMods");
    ImGui::PopFont();

    // arrastar a janela pela faixa do topo
    if (ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered() && io.MousePos.y < S(40) && io.MousePos.x > wArte) {
        ReleaseCapture();
        SendMessageA(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }
    dl->AddRect(ImVec2(0.5f, 0.5f), ImVec2(ds.x - 0.5f, ds.y - 0.5f), Cinza(50), 0, 0, 1);
    ImGui::End();
    ImGui::PopStyleVar(2);
}

// ---------- main ----------
static LRESULT WINAPI WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 1;
    if (m == WM_DESTROY) { gRodando = false; PostQuitMessage(0); return 0; }
    return DefWindowProcA(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR linha, int) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    { // nitido em qualquer escala do Windows (mesma regra do launcher; senao o DWM borra)
        typedef BOOL(WINAPI* FnCtx)(HANDLE);
        HMODULE u32 = GetModuleHandleA("user32.dll");
        FnCtx SetCtx = u32 ? (FnCtx)GetProcAddress(u32, "SetProcessDpiAwarenessContext") : NULL;
        if (!SetCtx || !SetCtx((HANDLE)-4)) SetProcessDPIAware(); // -4 = per-monitor v2
    }

    // modo remocao: --remover (copia p/ temp e relanca) / --remover2 <pasta>
    if (strstr(linha, "--remover2")) {
        const char* p = strstr(linha, "--remover2") + 10;
        while (*p == ' ' || *p == '"') p++;
        strncpy(gPastaRemover, p, MAX_PATH - 1);
        char* fimq = strchr(gPastaRemover, '"');
        if (fimq) *fimq = 0;
        int fim2 = (int)strlen(gPastaRemover);
        while (fim2 > 0 && (gPastaRemover[fim2 - 1] == ' ')) gPastaRemover[--fim2] = 0;
        // seguranca: so remove uma pasta que EXISTE e tem cara de instalacao nossa
        bool pastaOk = false;
        if (gPastaRemover[0]) {
            DWORD atr = GetFileAttributesA(gPastaRemover);
            if (atr != INVALID_FILE_ATTRIBUTES && (atr & FILE_ATTRIBUTE_DIRECTORY)) {
                char chk[MAX_PATH + 24];
                _snprintf(chk, sizeof(chk) - 1, "%s\\Trok Launcher.exe", gPastaRemover);
                chk[sizeof(chk) - 1] = 0;
                if (GetFileAttributesA(chk) != INVALID_FILE_ATTRIBUTES) pastaOk = true;
                _snprintf(chk, sizeof(chk) - 1, "%s\\Desinstalar.exe", gPastaRemover);
                chk[sizeof(chk) - 1] = 0;
                if (GetFileAttributesA(chk) != INVALID_FILE_ATTRIBUTES) pastaOk = true;
            }
        }
        if (!pastaOk) {
            MessageBoxA(NULL, "Pasta de instalacao nao encontrada.", "Trok Launcher", MB_ICONERROR);
            CoUninitialize();
            return 1;
        }
        gEtapa = ET_REMOVER;
    } else if (strstr(linha, "--remover")) {
        char eu[MAX_PATH], tmp[MAX_PATH], cmd[MAX_PATH * 2];
        GetModuleFileNameA(NULL, eu, MAX_PATH);
        char pasta[MAX_PATH];
        strncpy(pasta, eu, MAX_PATH - 1);
        char* b = strrchr(pasta, '\\'); if (b) *b = 0;
        GetTempPathA(MAX_PATH, tmp);
        strcat(tmp, "TrokDesinstalar.exe");
        CopyFileA(eu, tmp, FALSE);
        _snprintf(cmd, sizeof(cmd) - 1, "--remover2 \"%s\"", pasta);
        ShellExecuteA(NULL, "open", tmp, cmd, NULL, SW_SHOWNORMAL);
        CoUninitialize();
        return 0;
    }

    if (gEtapa != ET_REMOVER && !LerPayload()) {
        MessageBoxA(NULL, "Instalador corrompido (payload ausente).", "Trok Launcher", MB_ICONERROR);
        return 1;
    }
    {
        char base[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, base)))
            _snprintf(gPastaDestino, MAX_PATH - 1, "%s\\Trok Launcher", base);
        else strcpy(gPastaDestino, "C:\\Trok Launcher");
    }
    if (gEtapa == ET_CONFIG && strstr(linha, "--atualizar")) { // atualizacao silenciosa
        gModoAtt = true;
        Sleep(500); // o launcher que nos chamou esta fechando
        FecharLauncherEm(gPastaDestino);
        gArqAtual = 0;
        gEtapa = ET_INSTALANDO;
    }

    WNDCLASSEXA wc = { sizeof(wc), CS_CLASSDC, WndProc, 0, 0, hInst,
        LoadIconA(hInst, MAKEINTRESOURCEA(1)), LoadCursorA(NULL, (LPCSTR)IDC_ARROW), NULL, NULL,
        "TrokInstaller", LoadIconA(hInst, MAKEINTRESOURCEA(1)) };
    RegisterClassExA(&wc);
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    // regra das resolucoes: janela ~46% da largura do monitor (880 no 1080p, ~1180 no 2k),
    // presa em 880..1280; conteudo com escala amortecida (metade da razao), presa em 0.85..1.25
    int janW = (int)(sw * 0.46f);
    if (janW < JAN_W) janW = JAN_W;
    if (janW > 1280) janW = 1280;
    int janH = janW * JAN_H / JAN_W;
    if (janH > sh - 120) { janH = sh - 120; janW = janH * JAN_W / JAN_H; } // monitor baixo
    float razao = (float)janW / JAN_W;
    gEsc = 1.0f + (razao - 1.0f) * 0.5f;
    if (gEsc < 0.85f) gEsc = 0.85f;
    if (gEsc > 1.25f) gEsc = 1.25f;
    HWND hwnd = CreateWindowA("TrokInstaller", "Instalar Trok Launcher", WS_POPUP,
        (sw - janW) / 2, (sh - janH) / 2, janW, janH, NULL, NULL, hInst, NULL);
    HMODULE dwm = LoadLibraryA("dwmapi.dll");
    if (dwm) {
        typedef HRESULT(WINAPI* FnAttr)(HWND, DWORD, LPCVOID, DWORD);
        FnAttr SetAttr = (FnAttr)GetProcAddress(dwm, "DwmSetWindowAttribute");
        if (SetAttr) { DWORD pref = 2; SetAttr(hwnd, 33, &pref, sizeof(pref)); }
    }

    gD3D = Direct3DCreate9(D3D_SDK_VERSION);
    ZeroMemory(&gPP, sizeof(gPP));
    gPP.Windowed = TRUE; gPP.SwapEffect = D3DSWAPEFFECT_DISCARD;
    gPP.BackBufferFormat = D3DFMT_UNKNOWN;
    gPP.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    if (FAILED(gD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING, &gPP, &gDev)))
        gD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &gPP, &gDev);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(gDev);
    gFtBody   = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", S(16.0f));
    gFtBold   = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", S(17.0f));
    gFtMini   = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", S(13.5f)); // piso ~13.5
    gFtTitulo = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguibl.ttf", S(40.0f));
    if (!gFtBody) { gFtBody = gFtBold = gFtMini = gFtTitulo = io.Fonts->AddFontDefault(); }
    CarregarArte(gDev);

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    while (gRodando) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            if (msg.message == WM_QUIT) gRodando = false;
        }
        if (!gRodando) break;
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        DesenhaUI(hwnd);
        ImGui::EndFrame();
        gDev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_RGBA(13, 13, 15, 255), 1.0f, 0);
        if (gDev->BeginScene() >= 0) {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            gDev->EndScene();
        }
        gDev->Present(NULL, NULL, NULL, NULL);
        if (gPedirPasta) { // dialogo nativo FORA do frame
            gPedirPasta = false;
            BROWSEINFOA bi = { 0 };
            bi.lpszTitle = "Onde instalar o Trok Launcher";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
            if (pidl) {
                char pasta[MAX_PATH];
                if (SHGetPathFromIDListA(pidl, pasta)) {
                    _snprintf(gPastaDestino, MAX_PATH - 1, "%s\\Trok Launcher", pasta);
                    gPastaDestino[MAX_PATH - 1] = 0;
                }
                CoTaskMemFree(pidl);
            }
        }
    }

    if (gTexArte) gTexArte->Release();
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (gDev) gDev->Release();
    if (gD3D) gD3D->Release();
    DestroyWindow(hwnd);
    UnregisterClassA("TrokInstaller", hInst);
    CoUninitialize();
    return 0;
}
