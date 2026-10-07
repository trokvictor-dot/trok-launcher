// Trok Installer - instalador proprio do Trok Launcher, com a MESMA cara do app e do site
// (ImGui + DX9, painel de arte a esquerda, os tokens e a fonte do trokmods.blogspot.com).
// Payload: recurso RCDATA id 2 no formato TROKPAK1 (gerado pelo build-instalador.ps1).
// Arte do painel: recurso RCDATA id 3 (png). Icone: id 1. Montserrat: 14 (Medium) e 16 (Bold).
// Modo desinstalar: "--remover", rodando NO LUGAR (o Desinstalar.exe da propria instalacao).
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
#define VERSAO_INST "1.12"

// identidade do site (trokmods.blogspot.com): os mesmos tokens do launcher
static const ImU32 COR_ACCENT    = IM_COL32(252,  94,  58, 255); // --laranja
static const ImU32 COR_ACCENT_HI = IM_COL32(255, 128,  85, 255); // --laranja-hi
namespace Tk {
    static const ImU32 Fundo  = IM_COL32( 11,  11,  12, 255); // --fundo
    static const ImU32 Sup    = IM_COL32( 17,  17,  19, 255); // --sup (paineis, campos)
    static const ImU32 Sup2   = IM_COL32( 22,  22,  24, 255); // --sup-2
    static const ImU32 Realce = IM_COL32( 26,  26,  29, 255); // hover das linhas dos menus do site
    static const ImU32 Linha  = IM_COL32(255, 255, 255,  23); // --linha (.09)
    static const ImU32 Linha2 = IM_COL32(255, 255, 255,  38); // --linha-2 (.15)
    static const ImU32 Linha3 = IM_COL32(255, 255, 255,  61); // --linha-3 (.24)
    static const ImU32 Texto  = IM_COL32(242, 242, 243, 255); // --texto
    static const ImU32 Texto2 = IM_COL32(201, 201, 206, 255); // --texto-2
    static const ImU32 Texto3 = IM_COL32(166, 166, 172, 255); // --texto-3
    static const ImU32 Texto4 = IM_COL32(129, 129, 136, 255); // --texto-4
    static const ImU32 Vermelho = IM_COL32(240, 106, 95, 255); // --vermelho
    static const ImU32 SobreAcento = IM_COL32(23, 10, 5, 255);  // texto em cima do laranja
}

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

// idioma (Rockstar-style): a primeira tela pergunta; o launcher nasce com a escolha gravada no ini
static int gLangI = 0; // 0 pt-BR, 1 en, 2 es, 3 ru, 4 id, 5 tr
struct Traducao { const char* t[6]; }; // pt, en, es, ru, id, tr (coluna vazia cai pro ingles)
static const Traducao TRADUCOES[] = {
    { { "O seu SA-MP, moderno. Não substitui nenhum arquivo do jogo.", "Your SA-MP, modern. It doesn't replace any game file.", "Tu SA-MP, moderno. No reemplaza ningún archivo del juego.", "Ваш SA-MP, современный. Не заменяет файлы игры.", "SA-MP kamu, modern. Tidak mengganti file game apa pun.", "Modern SA-MP'niz. Hiçbir oyun dosyasını değiştirmez." } },
    { { "P A S T A   D E   I N S T A L A Ç Ã O", "I N S T A L L   F O L D E R", "C A R P E T A   D E   I N S T A L A C I Ó N", "П А П К А   У С Т А Н О В К И", "F O L D E R   I N S T A L A S I", "K U R U L U M   K L A S Ö R Ü" } },
    { { "Procurar...", "Browse...", "Buscar...", "Обзор...", "Cari...", "Gözat..." } },
    { { "Criar atalho na área de trabalho", "Create a desktop shortcut", "Crear acceso directo en el escritorio", "Создать ярлык на рабочем столе", "Buat pintasan di desktop", "Masaüstüne kısayol oluştur" } },
    { { "Criar atalho no menu Iniciar", "Create a Start menu shortcut", "Crear acceso directo en el menú Inicio", "Создать ярлык в меню «Пуск»", "Buat pintasan di menu Start", "Başlat menüsüne kısayol oluştur" } },
    { { "Espaço necessário: %.1f MB", "Space needed: %.1f MB", "Espacio necesario: %.1f MB", "Требуется места: %.1f МБ", "Ruang dibutuhkan: %.1f MB", "Gereken alan: %.1f MB" } },
    { { "Instalar", "Install", "Instalar", "Установить", "Pasang", "Kur" } },
    { { "Cancelar", "Cancel", "Cancelar", "Отмена", "Batal", "İptal" } },
    { { "O Trok Launcher está aberto neste computador.", "Trok Launcher is open on this computer.", "Trok Launcher está abierto en este PC.", "Trok Launcher запущен на этом компьютере.", "Trok Launcher sedang terbuka di komputer ini.", "Trok Launcher bu bilgisayarda açık." } },
    { { "Preciso fechar ele para atualizar os arquivos (suas configurações ficam).", "It needs to be closed to update the files (your settings stay).", "Hay que cerrarlo para actualizar los archivos (tus ajustes se conservan).", "Его нужно закрыть, чтобы обновить файлы (настройки сохранятся).", "Perlu ditutup untuk memperbarui file (pengaturan kamu tetap).", "Dosyaları güncellemek için kapatılmalı (ayarlarınız kalır)." } },
    { { "Fechar e instalar", "Close and install", "Cerrar e instalar", "Закрыть и установить", "Tutup dan pasang", "Kapat ve kur" } },
    { { "Voltar", "Back", "Volver", "Назад", "Kembali", "Geri" } },
    { { "Atualizando o Trok Launcher...", "Updating Trok Launcher...", "Actualizando Trok Launcher...", "Обновление Trok Launcher...", "Memperbarui Trok Launcher...", "Trok Launcher güncelleniyor..." } },
    { { "Instalando...", "Installing...", "Instalando...", "Установка...", "Memasang...", "Kuruluyor..." } },
    { { "Pronto! O Trok Launcher está instalado.", "Done! Trok Launcher is installed.", "¡Listo! Trok Launcher está instalado.", "Готово! Trok Launcher установлен.", "Selesai! Trok Launcher terpasang.", "Hazır! Trok Launcher kuruldu." } },
    { { "Na primeira abertura ele importa seu nick e seus favoritos do SA-MP.", "On first launch it imports your nick and favorites from SA-MP.", "Al abrir por primera vez importa tu nick y tus favoritos de SA-MP.", "При первом запуске он импортирует ник и избранное из SA-MP.", "Saat pertama dibuka, nick dan favorit SA-MP kamu diimpor.", "İlk açılışta SA-MP nick'inizi ve favorilerinizi içe aktarır." } },
    { { "Abrir o launcher", "Open the launcher", "Abrir el launcher", "Открыть лаунчер", "Buka launcher", "Launcher'ı aç" } },
    { { "Fechar", "Close", "Cerrar", "Закрыть", "Tutup", "Kapat" } },
    { { "Remover o Trok Launcher deste computador?", "Remove Trok Launcher from this computer?", "¿Quitar Trok Launcher de este PC?", "Удалить Trok Launcher с этого компьютера?", "Hapus Trok Launcher dari komputer ini?", "Trok Launcher bu bilgisayardan kaldırılsın mı?" } },
    { { "Apagar também configurações, contas e imagens", "Also delete settings, accounts and images", "Borrar también ajustes, cuentas e imágenes", "Удалить также настройки, аккаунты и изображения", "Hapus juga pengaturan, akun, dan gambar", "Ayarları, hesapları ve görselleri de sil" } },
    { { "(vai para a Lixeira do Windows, dá para recuperar)", "(goes to the Windows Recycle Bin, can be recovered)", "(va a la Papelera de Windows, se puede recuperar)", "(попадёт в Корзину Windows, можно восстановить)", "(masuk Recycle Bin Windows, bisa dipulihkan)", "(Windows Geri Dönüşüm Kutusu'na gider, geri alınabilir)" } },
    { { "Remover", "Remove", "Quitar", "Удалить", "Hapus", "Kaldır" } },
    { { "Removendo...", "Removing...", "Quitando...", "Удаление...", "Menghapus...", "Kaldırılıyor..." } },
    { { "Trok Launcher removido. Valeu por ter usado!", "Trok Launcher removed. Thanks for using it!", "Trok Launcher quitado. ¡Gracias por usarlo!", "Trok Launcher удалён. Спасибо, что пользовались!", "Trok Launcher dihapus. Terima kasih sudah memakai!", "Trok Launcher kaldırıldı. Kullandığınız için teşekkürler!" } },
    { { "Algo deu errado:", "Something went wrong:", "Algo salió mal:", "Что-то пошло не так:", "Ada yang salah:", "Bir şeyler ters gitti:" } },
    { { "Tentar de novo", "Try again", "Reintentar", "Повторить", "Coba lagi", "Tekrar dene" } },
    { { "Onde instalar o Trok Launcher", "Where to install Trok Launcher", "Dónde instalar Trok Launcher", "Куда установить Trok Launcher", "Di mana memasang Trok Launcher", "Trok Launcher nereye kurulsun" } },
    { { "Pasta de instalação não encontrada.", "Install folder not found.", "Carpeta de instalación no encontrada.", "Папка установки не найдена.", "Folder instalasi tidak ditemukan.", "Kurulum klasörü bulunamadı." } },
    { { "Instalador corrompido (faltam arquivos).", "Corrupted installer (files missing).", "Instalador dañado (faltan archivos).", "Установщик повреждён (нет файлов).", "Installer rusak (file hilang).", "Kurulum dosyası bozuk (dosyalar eksik)." } },
    { { "Instalar Trok Launcher", "Install Trok Launcher", "Instalar Trok Launcher", "Установка Trok Launcher", "Pasang Trok Launcher", "Trok Launcher'ı kur" } },
    { { "Escolha o idioma do launcher", "Choose the launcher language", "Elige el idioma del launcher", "Выберите язык лаунчера", "Pilih bahasa launcher", "Launcher dilini seçin" } },
    { { "Dá para trocar depois nas Configurações.", "You can change it later in Settings.", "Puedes cambiarlo después en Ajustes.", "Потом можно поменять в настройках.", "Bisa diubah nanti di Pengaturan.", "Sonradan Ayarlar'dan değiştirebilirsiniz." } },
    { { "Remover Trok Launcher", "Remove Trok Launcher", "Quitar Trok Launcher", "Удаление Trok Launcher", "Hapus Trok Launcher", "Trok Launcher'ı kaldır" } },
    { { "Não consegui gravar: %s", "Couldn't write: %s", "No se pudo escribir: %s", "Не удалось записать: %s", "Gagal menulis: %s", "Yazılamadı: %s" } },
    { { "Só o Desinstalar.exe ficou na pasta (aberto, ele não se apaga). Pode apagar a pasta à mão.", "Only Desinstalar.exe stayed in the folder (it can't delete itself while open). You can delete the folder by hand.", "Solo quedó Desinstalar.exe en la carpeta (abierto no se borra). Puedes borrar la carpeta a mano.", "В папке остался только Desinstalar.exe (открытым он себя не удаляет). Папку можно удалить вручную.", "Hanya Desinstalar.exe yang tersisa di folder (tidak bisa terhapus saat terbuka). Folder bisa dihapus manual.", "Klasörde yalnızca Desinstalar.exe kaldı (açıkken kendini silemez). Klasörü elle silebilirsiniz." } },
    { { "I N S T A L A D O R", "I N S T A L L E R", "I N S T A L A D O R", "У С Т А Н О В К А", "I N S T A L E R", "K U R U L U M" } },
    { { "D E S I N S T A L A D O R", "U N I N S T A L L E R", "D E S I N S T A L A D O R", "У Д А Л Е Н И Е", "P E N G H A P U S", "K A L D I R M A" } },
    { { "Não deu para abrir o Direct3D 9 neste computador. Atualize o driver de vídeo e tente de novo.", "Couldn't start Direct3D 9 on this computer. Update your video driver and try again.", "No se pudo iniciar Direct3D 9 en este PC. Actualiza el driver de video e inténtalo de nuevo.", "Не удалось запустить Direct3D 9 на этом компьютере. Обновите видеодрайвер и попробуйте снова.", "Direct3D 9 tidak bisa dijalankan di komputer ini. Perbarui driver video dan coba lagi.", "Bu bilgisayarda Direct3D 9 başlatılamadı. Ekran kartı sürücüsünü güncelleyip tekrar deneyin." } },
    { { "Essa pasta tem letras que este instalador não aceita. Escolha outra.", "That folder name has characters this installer can't use. Pick another one.", "Esa carpeta tiene caracteres que este instalador no acepta. Elige otra.", "В имени этой папки есть символы, которые установщик не поддерживает. Выберите другую.", "Nama folder itu punya karakter yang tidak didukung installer ini. Pilih folder lain.", "Bu klasörün adında kurulumun desteklemediği karakterler var. Başka bir klasör seçin." } },
};
static const char* T(const char* pt) {
    if (gLangI == 0 || !pt) return pt;
    for (size_t i = 0; i < sizeof(TRADUCOES) / sizeof(TRADUCOES[0]); i++) {
        if (strcmp(TRADUCOES[i].t[0], pt) != 0) continue;
        const char* v = (gLangI >= 1 && gLangI <= 5) ? TRADUCOES[i].t[gLangI] : NULL;
        if (!v || !v[0]) v = TRADUCOES[i].t[1];
        return (v && v[0]) ? v : pt;
    }
    return pt;
}
static char gPastaDestino[MAX_PATH];
static void GravarIdiomaNoIni() { // <pasta da instalacao>\Trok Launcher.ini, [config] idioma=1..6
    // o launcher le o ini AO LADO do exe dele: antes isto ia sempre para %LOCALAPPDATA%\Trok Launcher, e
    // quem escolhia outra pasta na instalacao perdia o idioma escolhido aqui
    char ini[MAX_PATH];
    _snprintf(ini, MAX_PATH - 1, "%s\\Trok Launcher.ini", gPastaDestino); ini[MAX_PATH - 1] = 0;
    char vi[4]; _snprintf(vi, sizeof(vi) - 1, "%d", gLangI + 1); vi[sizeof(vi) - 1] = 0; // 1 pt, 2 en, 3 es, 4 ru, 5 id, 6 tr
    WritePrivateProfileStringA("config", "idioma", vi, ini);
}

// etapas
enum { ET_IDIOMA, ET_CONFIG, ET_EMUSO, ET_INSTALANDO, ET_PRONTO, ET_REMOVER, ET_REMOVENDO, ET_REMOVIDO, ET_ERRO };
static int gEtapa = ET_IDIOMA;
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

// Mesma mensagem que o launcher escuta para sair de verdade (nao vale a opcao "fechar
// para a bandeja"). Precisa bater com o WM_TROK_SAIR do TrokLauncher.cpp.
#define WM_TROK_SAIR (WM_USER + 9)

// MessageBox ANSI leria o UTF-8 do fonte pela pagina de codigo do Windows e estragaria
// os acentos (e o russo nem cabe em ANSI): converte e usa a versao wide.
static void AvisoJanela(const char* txt, UINT icone) {
    wchar_t w[512];
    MultiByteToWideChar(CP_UTF8, 0, txt ? txt : "", -1, w, 512);
    MessageBoxW(NULL, w, L"Trok Launcher", icone);
}

// Pasta do launcher que AINDA ESTA RODANDO (ele nos chamou e esta fechando). E a fonte mais
// confiavel numa atualizacao: vale mesmo pra quem ja sofreu a atualizacao errada e ficou com o
// registro apontando pro destino padrao em vez da pasta escolhida.
static BOOL CALLBACK AcharPastaDoLauncher(HWND h, LPARAM lp) {
    char cls[64];
    if (!GetClassNameA(h, cls, sizeof(cls)) || strcmp(cls, "TrokLauncher") != 0) return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (!pid) return TRUE;
    HANDLE pr = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!pr) return TRUE;
    char caminho[MAX_PATH] = "";
    DWORD tam = MAX_PATH;
    bool ok = QueryFullProcessImageNameA(pr, 0, caminho, &tam) && caminho[0]; // import direto (Vista+), como no FecharLauncherEm
    CloseHandle(pr);
    if (!ok) return TRUE;
    char* b = strrchr(caminho, '\\');
    if (!b) return TRUE;
    *b = 0;
    strncpy((char*)lp, caminho, MAX_PATH - 1);
    ((char*)lp)[MAX_PATH - 1] = 0;
    return FALSE; // achou
}

static BOOL CALLBACK PedirSaidaDoLauncher(HWND h, LPARAM) {
    char cls[64];
    if (GetClassNameA(h, cls, sizeof(cls)) && strcmp(cls, "TrokLauncher") == 0)
        PostMessageA(h, WM_TROK_SAIR, 0, 0);
    return TRUE;
}

static void FecharLauncherEm(const char* pasta) { // fecha SO o launcher que roda desta pasta
    char alvo[MAX_PATH];
    _snprintf(alvo, MAX_PATH - 1, "%s\\Trok Launcher.exe", pasta);
    alvo[MAX_PATH - 1] = 0;
    // Primeiro PEDE pra sair, e so depois mata. Assim o launcher chega no fim do WinMain
    // e apaga o proprio icone da bandeja (NIM_DELETE). Antes disso, o TerminateProcess
    // abaixo matava o processo sem esse passo e o Windows deixava um icone FANTASMA no
    // relogio; depois de varias atualizacoes a bandeja enchia de copias do icone.
    EnumWindows(PedirSaidaDoLauncher, 0);
    for (int i = 0; i < 30 && FindWindowA("TrokLauncher", NULL); i++) Sleep(100); // ate 3s
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
    const char* nomeApp = "Trok Launcher", *editora = "TrokMods", *site = "https://trokmods.blogspot.com/";
    RegSetValueExA(k, "DisplayName", 0, REG_SZ, (BYTE*)nomeApp, (DWORD)strlen(nomeApp) + 1);
    RegSetValueExA(k, "DisplayVersion", 0, REG_SZ, (BYTE*)VERSAO_INST, (DWORD)strlen(VERSAO_INST) + 1);
    RegSetValueExA(k, "Publisher", 0, REG_SZ, (BYTE*)editora, (DWORD)strlen(editora) + 1); // = CompanyName do exe
    RegSetValueExA(k, "URLInfoAbout", 0, REG_SZ, (BYTE*)site, (DWORD)strlen(site) + 1);
    { // tamanho em KB (o "Aplicativos" do Windows mostra): os arquivos do pacote + o desinstalador
        DWORD kb = 0;
        for (int i = 0; i < gNumPak; i++) kb += gPak[i].tam / 1024;
        char eu[MAX_PATH];
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetModuleFileNameA(NULL, eu, MAX_PATH) && GetFileAttributesExA(eu, GetFileExInfoStandard, &fa))
            kb += fa.nFileSizeLow / 1024;
        RegSetValueExA(k, "EstimatedSize", 0, REG_DWORD, (BYTE*)&kb, 4);
    }
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
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); // parte clicavel: cursor de mao
    ImGui::PopID();
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    bool hov = ImGui::IsItemHovered();
    ImDrawList* d = ImGui::GetWindowDrawList();
    // o .tk-btn-sec do site: contorno a 24%, fundo sup-2 no hover, cantos de 8
    if (hov) d->AddRectFilled(a, b, Tk::Sup2, S(8));
    d->AddRect(a, b, hov ? Cinza(255, 87) : Tk::Linha3, S(8), 0, S(1.0f));
    const char* fim = strstr(rotulo, "##");
    ImVec2 tsz = ImGui::CalcTextSize(rotulo, fim);
    d->AddText(ImVec2((a.x + b.x - tsz.x) * 0.5f, (a.y + b.y - tsz.y) * 0.5f), Tk::Texto, rotulo, fim);
    return cl;
}

// o .tk-btn-primario do site: laranja chapado, texto escuro, cantos de 8 (cor = outro fundo, ex. vermelho)
static bool BotaoPrimario(const char* rotulo, ImVec2 tam, ImU32 cor = 0) {
    ImGui::PushID(rotulo);
    bool cl = ImGui::InvisibleButton("##pri", tam);
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::PopID();
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    bool hov = ImGui::IsItemHovered();
    ImDrawList* d = ImGui::GetWindowDrawList();
    ImU32 base = cor ? cor : COR_ACCENT;
    d->AddRectFilled(a, b, hov ? LerpCor(base, IM_COL32(255, 255, 255, 255), 0.08f) : base, S(8));
    ImVec2 tsz = ImGui::CalcTextSize(rotulo);
    d->AddText(ImVec2((a.x + b.x - tsz.x) * 0.5f, (a.y + b.y - tsz.y) * 0.5f), Tk::SobreAcento, rotulo);
    return cl;
}

// fileira de botoes do rodape: primario e, ao lado, o secundario, com largura pelo texto (para caber
// "Закрыть и установить" sem cortar). Devolve 1 = primario, 2 = secundario, 0 = nada
static int BotoesRodape(float x, float y, const char* prim, const char* sec, ImU32 corPrim = 0);

static bool LinhaCheck(const char* rot, bool* v, float w) {
    ImGui::PushID(rot);
    bool cl = ImGui::InvisibleButton("##lc", ImVec2(w, S(34)));
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::PopID();
    if (cl) *v = !*v;
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    bool hov = ImGui::IsItemHovered();
    ImDrawList* d = ImGui::GetWindowDrawList();
    // o filtro do launcher: hover no #1A1A1D dos menus do site, check no laranja
    if (hov) d->AddRectFilled(a, b, Tk::Realce, S(8));
    ImVec2 ka(a.x + S(8), (a.y + b.y) * 0.5f - S(9)), kb(ka.x + S(18), ka.y + S(18));
    if (*v) {
        d->AddRectFilled(ka, kb, COR_ACCENT, S(5));
        d->PathLineTo(ImVec2(ka.x + S(4.0f), ka.y + S(9.5f))); // check em caminho unico (sem dente)
        d->PathLineTo(ImVec2(ka.x + S(7.5f), ka.y + S(13.0f)));
        d->PathLineTo(ImVec2(ka.x + S(14.0f), ka.y + S(5.0f)));
        d->PathStroke(Tk::SobreAcento, 0, S(2.2f));
    } else {
        d->AddRect(ka, kb, hov ? Cinza(255, 110) : Tk::Linha3, S(5), 0, S(1.2f));
    }
    ImVec2 tsz = ImGui::CalcTextSize(rot);
    d->AddText(ImVec2(ka.x + S(28), (a.y + b.y - tsz.y) * 0.5f), (hov || *v) ? Tk::Texto : Tk::Texto2, rot);
    return cl;
}

static int BotoesRodape(float x, float y, const char* prim, const char* sec, ImU32 corPrim) {
    int r = 0;
    ImGui::PushFont(gFtBold);
    float wP = ImGui::CalcTextSize(prim).x + S(64);
    if (wP < S(176)) wP = S(176);
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    if (BotaoPrimario(prim, ImVec2(wP, S(50)), corPrim)) r = 1;
    if (sec) {
        float wS = ImGui::CalcTextSize(sec).x + S(48);
        if (wS < S(118)) wS = S(118);
        ImGui::SetCursorScreenPos(ImVec2(x + wP + S(12), y));
        if (BotaoSec(sec, ImVec2(wS, S(50)))) r = 2;
    }
    ImGui::PopFont();
    return r;
}

// texto com quebra na largura (frases longas em russo/espanhol nao estouram o painel)
static void TextoQuebra(ImDrawList* d, ImVec2 p, ImU32 cor, const char* txt, float largura) {
    d->AddText(ImGui::GetFont(), ImGui::GetFontSize(), p, cor, txt, NULL, largura);
}

// caminho do Windows (pagina ANSI) -> UTF-8 para o ImGui: "C:\Users\João" aparecia "Jo?o" no campo
static void AnsiParaUtf8(const char* in, char* out, int outSz) {
    wchar_t w[MAX_PATH * 2];
    if (!MultiByteToWideChar(CP_ACP, 0, in, -1, w, MAX_PATH * 2) ||
        !WideCharToMultiByte(CP_UTF8, 0, w, -1, out, outSz, NULL, NULL)) {
        strncpy(out, in, outSz - 1); out[outSz - 1] = 0;
    }
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
    // so caminho relativo limpo: nada de "..", unidade ou raiz (o pacote e nosso, mas a trava e barata)
    if (strstr(a.nome, "..") || strchr(a.nome, ':') || a.nome[0] == '\\' || a.nome[0] == '/') {
        _snprintf(gErro, sizeof(gErro) - 1, T("Não consegui gravar: %s"), a.nome);
        return false;
    }
    char destino[MAX_PATH];
    _snprintf(destino, MAX_PATH - 1, "%s\\%s", gPastaDestino, a.nome);
    destino[MAX_PATH - 1] = 0;
    if (!GravarArquivo(destino, a.dados, a.tam)) {
        _snprintf(gErro, sizeof(gErro) - 1, T("Não consegui gravar: %s"), a.nome);
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

// "Iniciar com o Windows" do launcher: sai junto se apontava para esta instalacao (antes ficava uma
// entrada orfa, e o Gerenciador de Tarefas listava um programa que nao existe mais)
static void RemoverInicioComWindows(const char* pasta) {
    HKEY k;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                      KEY_QUERY_VALUE | KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    char v[MAX_PATH * 2] = "";
    DWORD tam = sizeof(v) - 1, tipo = 0;
    if (RegQueryValueExA(k, "Trok Launcher", NULL, &tipo, (BYTE*)v, &tam) == ERROR_SUCCESS && tipo == REG_SZ) {
        v[tam < sizeof(v) ? tam : sizeof(v) - 1] = 0;
        const char* p = (v[0] == '"') ? v + 1 : v;
        size_t L = strlen(pasta);
        if (L && _strnicmp(p, pasta, L) == 0 && p[L] == '\\') RegDeleteValueA(k, "Trok Launcher");
    }
    RegCloseKey(k);
}

// Desinstalacao NO LUGAR. Antes o "--remover" se copiava para o %TEMP% e rodava a copia de la (para
// conseguir apagar a propria pasta) - o padrao classico de dropper que antivirus marca. Agora: o
// Windows nao deixa apagar um exe aberto, mas deixa MOVE-LO no mesmo disco; entao o desinstalador sai
// da pasta (so renomeia, nada e copiado nem executado), e o resto e apagado daqui mesmo. So os itens
// conhecidos do launcher saem: arquivo de outra pessoa dentro da pasta fica (e a pasta com ele).
static bool gSobrouDesinstalador = false;
static void ExecutarRemocao(const char* pasta, bool apagarConfig) {
    char sistema[MAX_PATH];
    if (GetSystemDirectoryA(sistema, MAX_PATH)) SetCurrentDirectoryA(sistema); // a pasta de trabalho nao segura a pasta
    FecharLauncherEm(pasta); // se o launcher estiver aberto, fecha antes de apagar
    RemoverAtalho(CSIDL_DESKTOPDIRECTORY, "Trok Launcher");
    RemoverAtalho(CSIDL_PROGRAMS, "Trok Launcher");
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TrokLauncher");
    RemoverInicioComWindows(pasta);

    // 1) o desinstalador sai da pasta (renomear um exe aberto e permitido no mesmo disco)
    {
        char eu[MAX_PATH], tmp[MAX_PATH], destino[MAX_PATH + 48];
        bool saiu = false;
        DWORD n = GetTempPathA(MAX_PATH, tmp);
        if (GetModuleFileNameA(NULL, eu, MAX_PATH) && n && n < MAX_PATH - 48) {
            _snprintf(destino, sizeof(destino) - 1, "%sTrokLauncher-desinstalador.exe", tmp);
            destino[sizeof(destino) - 1] = 0;
            saiu = MoveFileExA(eu, destino, MOVEFILE_REPLACE_EXISTING) != 0;
            if (!saiu) { // o de uma remocao anterior ainda aberto: outro nome
                _snprintf(destino, sizeof(destino) - 1, "%sTrokLauncher-desinstalador-%lu.exe", tmp, GetCurrentProcessId());
                destino[sizeof(destino) - 1] = 0;
                saiu = MoveFileExA(eu, destino, 0) != 0;
            }
        }
        gSobrouDesinstalador = !saiu; // disco diferente do %TEMP%: ele fica, o resto sai
    }

    // 2) o programa (sempre)
    static const char* PROGRAMA[] = { "Trok Launcher.exe", "Trok Launcher.exe.velho", "LEIA-ME.txt", "LICENCAS.txt" };
    for (int i = 0; i < (int)(sizeof(PROGRAMA) / sizeof(PROGRAMA[0])); i++) {
        char c[MAX_PATH + 32];
        _snprintf(c, sizeof(c) - 1, "%s\\%s", pasta, PROGRAMA[i]);
        c[sizeof(c) - 1] = 0;
        for (int t = 0; t < 10 && GetFileAttributesA(c) != INVALID_FILE_ATTRIBUTES && !DeleteFileA(c); t++)
            Sleep(200); // o launcher que acabou de fechar ainda pode estar soltando o exe
    }

    // 3) configuracoes, contas e imagens (se pedido): para a LIXEIRA, recuperavel
    if (apagarConfig) {
        static const char* DADOS[] = { "Trok Launcher.ini", "Trok Launcher.ini.bak", "Trok Launcher.ini.antes-import",
                                       "crash.log", "avatars", "capas", "imagens", "cache" };
        const int ND = (int)(sizeof(DADOS) / sizeof(DADOS[0]));
        char lista[(MAX_PATH + 32) * 8 + 2];
        int pos = 0;
        for (int i = 0; i < ND; i++) {
            char c[MAX_PATH + 32];
            _snprintf(c, sizeof(c) - 1, "%s\\%s", pasta, DADOS[i]);
            c[sizeof(c) - 1] = 0;
            if (GetFileAttributesA(c) == INVALID_FILE_ATTRIBUTES) continue;
            int L = (int)strlen(c);
            if (pos + L + 2 > (int)sizeof(lista)) break;
            memcpy(lista + pos, c, L + 1);
            pos += L + 1;
        }
        lista[pos] = 0; // fim da lista: dois zeros seguidos
        if (pos > 0) {
            SHFILEOPSTRUCTA op = { 0 };
            op.wFunc = FO_DELETE;
            op.pFrom = lista;
            op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
            SHFileOperationA(&op);
        }
    }

    // 4) a pasta so sai se ficou vazia
    RemoveDirectoryA(pasta);

    // 5) o setup que o launcher baixou para se atualizar (%LOCALAPPDATA%\Trok Launcher\update)
    char base[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, base))) {
        char c[MAX_PATH + 64];
        _snprintf(c, sizeof(c) - 1, "%s\\Trok Launcher\\update\\TrokLauncher-Setup.exe", base); c[sizeof(c) - 1] = 0;
        DeleteFileA(c);
        _snprintf(c, sizeof(c) - 1, "%s\\Trok Launcher\\update", base); c[sizeof(c) - 1] = 0;
        RemoveDirectoryA(c);
        _snprintf(c, sizeof(c) - 1, "%s\\Trok Launcher", base); c[sizeof(c) - 1] = 0;
        if (_stricmp(c, pasta) != 0) RemoveDirectoryA(c); // so se vazia
    }
}

// a pasta do desinstalador e a que o Windows tem registrada para o launcher?
static bool PastaRegistrada(const char* pasta) {
    HKEY k;
    bool ok = false;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TrokLauncher",
                      0, KEY_QUERY_VALUE, &k) == ERROR_SUCCESS) {
        char v[MAX_PATH] = "";
        DWORD tam = MAX_PATH - 1, tipo = 0;
        if (RegQueryValueExA(k, "InstallLocation", NULL, &tipo, (BYTE*)v, &tam) == ERROR_SUCCESS && tipo == REG_SZ) {
            v[tam < MAX_PATH ? tam : MAX_PATH - 1] = 0;
            ok = _stricmp(v, pasta) == 0;
        }
        RegCloseKey(k);
    }
    return ok;
}

// desinstaladores de remocoes anteriores que ficaram no %TEMP% (e o das versoes ate a 1.11)
static void LimparSobrasDoTemp() {
    char tmp[MAX_PATH];
    DWORD n = GetTempPathA(MAX_PATH, tmp);
    if (!n || n >= MAX_PATH - 48) return;
    char c[MAX_PATH + 64];
    _snprintf(c, sizeof(c) - 1, "%sTrokDesinstalar.exe", tmp); c[sizeof(c) - 1] = 0;
    DeleteFileA(c);
    _snprintf(c, sizeof(c) - 1, "%sTrokLauncher-desinstalador*.exe", tmp); c[sizeof(c) - 1] = 0;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(c, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        char arq[MAX_PATH + 64];
        _snprintf(arq, sizeof(arq) - 1, "%s%s", tmp, fd.cFileName); arq[sizeof(arq) - 1] = 0;
        DeleteFileA(arq); // em uso agora: falha quieto e fica para a proxima
    } while (FindNextFileA(h, &fd));
    FindClose(h);
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
    dl->AddRectFilled(ImVec2(0, 0), ds, Tk::Fundo);
    const bool emRemocao = (gEtapa == ET_REMOVER || gEtapa == ET_REMOVENDO || gEtapa == ET_REMOVIDO);
    const bool ocupado = (gEtapa == ET_INSTALANDO || gEtapa == ET_REMOVENDO);

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
            IM_COL32(11, 11, 12, 0), IM_COL32(11, 11, 12, 0), IM_COL32(11, 11, 12, 200), IM_COL32(11, 11, 12, 200));
    } else {
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(wArte, ds.y), Tk::Sup);
        DesenhaMira(dl, ImVec2(wArte * 0.5f, ds.y * 0.42f), wArte * 0.44f, COR_ACCENT);
    }
    dl->AddRectFilled(ImVec2(wArte, 0), ImVec2(wArte + 1, ds.y), Tk::Linha);

    // fechar (X): o botao de janela do launcher (quadrado de 38, fundo branco a 6% no hover)
    ImGui::SetCursorScreenPos(ImVec2(ds.x - S(50), S(12)));
    if (ImGui::InvisibleButton("##fx", ImVec2(S(38), S(38))) && !ocupado)
        gRodando = false;
    {
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        bool hX = ImGui::IsItemHovered() && !ocupado;
        if (hX) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); dl->AddRectFilled(a, b, Cinza(255, 16), S(8)); }
        ImU32 cX = ocupado ? Tk::Texto4 : (hX ? Tk::Texto : Tk::Texto2);
        ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
        float r = S(5.5f);
        dl->AddLine(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), cX, S(1.6f));
        dl->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x + r, c.y - r), cX, S(1.6f));
    }

    float px = wArte + S(44); // margem do painel direito
    float wCampo = ds.x - px - S(44);
    const float yBot = ds.y - S(96); // a fileira de botoes

    // cabecalho: o rotulo espacado no destaque (o selo da Home do launcher) e o nome
    ImGui::PushFont(gFtMini);
    dl->AddText(ImVec2(px, S(50)), COR_ACCENT, emRemocao ? T("D E S I N S T A L A D O R") : T("I N S T A L A D O R"));
    ImGui::PopFont();
    ImGui::PushFont(gFtTitulo);
    dl->AddText(ImVec2(px - S(1), S(66)), Tk::Texto, "Trok Launcher");
    ImGui::PopFont();

    if (gEtapa == ET_IDIOMA) {
        // primeira tela: idioma (ja vem marcado o do Windows); os dois rotulos aparecem nas duas linguas
        ImGui::PushFont(gFtBody);
        dl->AddText(ImVec2(px, S(120)), Tk::Texto2, "Escolha o idioma do launcher");
        dl->AddText(ImVec2(px, S(144)), Tk::Texto4, "Choose the launcher language");
        ImGui::PopFont();
        ImGui::PushFont(gFtBold);
        const char* OPI[6] = { "Portugu\u00eas (Brasil)", "English", "Espa\u00f1ol", "\u0420\u0443\u0441\u0441\u043a\u0438\u0439", "Bahasa Indonesia", "T\u00fcrk\u00e7e" };
        float wOp = (wCampo - S(10)) * 0.5f; // duas colunas de tres
        for (int k = 0; k < 6; k++) {
            ImGui::SetCursorScreenPos(ImVec2(px + (k % 2) * (wOp + S(10)), S(182) + (k / 2) * S(54)));
            char idi[12]; sprintf(idi, "##idi%d", k);
            bool cli = ImGui::InvisibleButton(idi, ImVec2(wOp, S(46)));
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
            bool sel = (gLangI == k), hov = ImGui::IsItemHovered();
            // os cartoes do site: sup com linha a 9%; o escolhido ganha a borda no laranja
            dl->AddRectFilled(a, b, sel ? Tk::Sup2 : (hov ? Tk::Realce : Tk::Sup), S(12));
            dl->AddRect(a, b, sel ? COR_ACCENT : (hov ? Tk::Linha2 : Tk::Linha), S(12), 0, sel ? S(1.5f) : S(1.0f));
            ImVec2 rc(a.x + S(22), (a.y + b.y) * 0.5f);
            dl->AddCircle(rc, S(7), sel ? COR_ACCENT : Tk::Linha3, 24, S(1.6f));
            if (sel) dl->AddCircleFilled(rc, S(3.5f), COR_ACCENT, 24);
            ImVec2 tsz = ImGui::CalcTextSize(OPI[k]);
            dl->AddText(ImVec2(a.x + S(40), (a.y + b.y - tsz.y) * 0.5f), sel ? Tk::Texto : Tk::Texto2, OPI[k]);
            if (cli) gLangI = k;
        }
        ImGui::PopFont();
        ImGui::PushFont(gFtMini);
        dl->AddText(ImVec2(px, S(350)), Tk::Texto4, T("Dá para trocar depois nas Configurações."));
        ImGui::PopFont();
        static const char* CONT[6] = { "Continuar", "Continue", "Continuar", "\u041f\u0440\u043e\u0434\u043e\u043b\u0436\u0438\u0442\u044c", "Lanjut", "Devam" };
        int rI = BotoesRodape(px, yBot, CONT[gLangI], T("Cancelar"));
        if (rI == 1) gEtapa = ET_CONFIG;
        else if (rI == 2) gRodando = false;
    }
    else if (gEtapa == ET_CONFIG) {
        ImGui::PushFont(gFtBody);
        TextoQuebra(dl, ImVec2(px, S(120)), Tk::Texto2, T("O seu SA-MP, moderno. Não substitui nenhum arquivo do jogo."), wCampo);
        ImGui::PopFont();

        ImGui::PushFont(gFtMini);
        dl->AddText(ImVec2(px, S(168)), Tk::Texto4, T("P A S T A   D E   I N S T A L A Ç Ã O"));
        ImGui::PopFont();
        // campo da pasta (o campo do site: sup, contorno a 15%) + procurar
        ImGui::PushFont(gFtBody);
        ImVec2 ca(px, S(188)), cb(px + wCampo - S(118), S(188) + S(38));
        dl->AddRectFilled(ca, cb, Tk::Sup, S(8));
        dl->AddRect(ca, cb, Tk::Linha2, S(8), 0, S(1.0f));
        char pastaU8[MAX_PATH * 3];
        AnsiParaUtf8(gPastaDestino, pastaU8, (int)sizeof(pastaU8));
        ImGui::PushClipRect(ca, ImVec2(cb.x - S(8), cb.y), true);
        ImVec2 psz = ImGui::CalcTextSize(pastaU8);
        dl->AddText(ImVec2(ca.x + S(12), (ca.y + cb.y - psz.y) * 0.5f), Tk::Texto2, pastaU8);
        ImGui::PopClipRect();
        ImGui::SetCursorScreenPos(ImVec2(cb.x + S(10), S(188)));
        if (BotaoSec(T("Procurar..."), ImVec2(S(108), S(38)))) gPedirPasta = true;
        ImGui::PopFont();

        ImGui::PushFont(gFtBody);
        ImGui::SetCursorScreenPos(ImVec2(px - S(8), S(244)));
        LinhaCheck(T("Criar atalho na área de trabalho"), &gAtalhoDesktop, wCampo + S(8));
        ImGui::SetCursorScreenPos(ImVec2(px - S(8), S(280)));
        LinhaCheck(T("Criar atalho no menu Iniciar"), &gAtalhoIniciar, wCampo + S(8));
        ImGui::PopFont();

        // requisito de espaco (payload e minusculo, mas informa)
        unsigned int total = 0;
        for (int i = 0; i < gNumPak; i++) total += gPak[i].tam;
        char inf[96];
        _snprintf(inf, sizeof(inf) - 1, T("Espaço necessário: %.1f MB"), total / 1048576.0f);
        inf[sizeof(inf) - 1] = 0;
        ImGui::PushFont(gFtMini);
        dl->AddText(ImVec2(px, S(330)), Tk::Texto4, inf);
        ImGui::PopFont();

        int rC = BotoesRodape(px, yBot, T("Instalar"), T("Cancelar"));
        if (rC == 1) {
            gArqAtual = 0;
            gErro[0] = 0;
            gEtapa = LauncherRodandoEm(gPastaDestino) ? ET_EMUSO : ET_INSTALANDO;
        }
        else if (rC == 2) gRodando = false;
    }
    else if (gEtapa == ET_EMUSO) {
        ImGui::PushFont(gFtBody);
        TextoQuebra(dl, ImVec2(px, S(124)), Tk::Texto, T("O Trok Launcher está aberto neste computador."), wCampo);
        TextoQuebra(dl, ImVec2(px, S(152)), Tk::Texto3, T("Preciso fechar ele para atualizar os arquivos (suas configurações ficam)."), wCampo);
        ImGui::PopFont();
        int rE = BotoesRodape(px, yBot, T("Fechar e instalar"), T("Voltar"));
        if (rE == 1) {
            FecharLauncherEm(gPastaDestino);
            gArqAtual = 0;
            gEtapa = ET_INSTALANDO;
        }
        else if (rE == 2) gEtapa = ET_CONFIG;
    }
    else if (gEtapa == ET_INSTALANDO) {
        // 1 arquivo por frame
        if (!InstalarProximo()) gEtapa = ET_ERRO;
        else if (gArqAtual >= gNumPak) {
            FinalizarInstalacao();
            if (!gModoAtt) GravarIdiomaNoIni(); // atualizacao nao mexe na escolha do usuario
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
        dl->AddText(ImVec2(px, S(140)), Tk::Texto2, gModoAtt ? T("Atualizando o Trok Launcher...") : T("Instalando..."));
        ImGui::PopFont();
        { // porcentagem na ponta direita, em cima da barra
            char pct[16];
            _snprintf(pct, sizeof(pct) - 1, "%d%%", (int)(frac * 100.0f + 0.5f)); pct[sizeof(pct) - 1] = 0;
            ImGui::PushFont(gFtMini);
            ImVec2 pcs = ImGui::CalcTextSize(pct);
            dl->AddText(ImVec2(px + wCampo - pcs.x, S(146)), Tk::Texto3, pct);
            ImGui::PopFont();
        }
        ImVec2 ba(px, S(176)), bb(px + wCampo, S(182));
        dl->AddRectFilled(ba, bb, Tk::Sup2, S(3));
        if (frac > 0)
            dl->AddRectFilled(ba, ImVec2(ba.x + (bb.x - ba.x) * frac, bb.y), COR_ACCENT, S(3));
        if (gArqAtual < gNumPak) {
            ImGui::PushFont(gFtMini);
            dl->AddText(ImVec2(px, S(196)), Tk::Texto4, gPak[gArqAtual].nome);
            ImGui::PopFont();
        }
    }
    else if (gEtapa == ET_PRONTO) {
        ImGui::PushFont(gFtBody);
        TextoQuebra(dl, ImVec2(px, S(124)), Tk::Texto, T("Pronto! O Trok Launcher está instalado."), wCampo);
        TextoQuebra(dl, ImVec2(px, S(152)), Tk::Texto3, T("Na primeira abertura ele importa seu nick e seus favoritos do SA-MP."), wCampo);
        ImGui::PopFont();
        int rP = BotoesRodape(px, yBot, T("Abrir o launcher"), T("Fechar"));
        if (rP == 1) {
            char alvo[MAX_PATH];
            _snprintf(alvo, MAX_PATH - 1, "%s\\Trok Launcher.exe", gPastaDestino);
            alvo[MAX_PATH - 1] = 0;
            ShellExecuteA(NULL, "open", alvo, NULL, gPastaDestino, SW_SHOWNORMAL);
            gRodando = false;
        }
        else if (rP == 2) gRodando = false;
    }
    else if (gEtapa == ET_REMOVER) {
        ImGui::PushFont(gFtBody);
        TextoQuebra(dl, ImVec2(px, S(124)), Tk::Texto, T("Remover o Trok Launcher deste computador?"), wCampo);
        ImGui::SetCursorScreenPos(ImVec2(px - S(8), S(166)));
        LinhaCheck(T("Apagar também configurações, contas e imagens"), &gApagarConfig, wCampo + S(8));
        ImGui::PopFont();
        ImGui::PushFont(gFtMini);
        dl->AddText(ImVec2(px + S(28), S(204)), Tk::Texto4, T("(vai para a Lixeira do Windows, dá para recuperar)"));
        ImGui::PopFont();
        int rR = BotoesRodape(px, yBot, T("Remover"), T("Cancelar"), Tk::Vermelho); // o --vermelho do site
        if (rR == 1) gEtapa = ET_REMOVENDO;
        else if (rR == 2) gRodando = false;
    }
    else if (gEtapa == ET_REMOVENDO) {
        ImGui::PushFont(gFtBody);
        dl->AddText(ImVec2(px, S(140)), Tk::Texto2, T("Removendo..."));
        ImGui::PopFont();
        static int frameRem = 0; // o "Removendo..." aparece ANTES do trabalho pesado
        if (++frameRem >= 2) {
            ExecutarRemocao(gPastaRemover, gApagarConfig);
            gEtapa = ET_REMOVIDO;
        }
    }
    else if (gEtapa == ET_REMOVIDO) {
        ImGui::PushFont(gFtBody);
        TextoQuebra(dl, ImVec2(px, S(124)), Tk::Texto, T("Trok Launcher removido. Valeu por ter usado!"), wCampo);
        if (gSobrouDesinstalador) // disco diferente do %TEMP%: o desinstalador aberto nao saiu da pasta
            TextoQuebra(dl, ImVec2(px, S(152)), Tk::Texto3, T("Só o Desinstalar.exe ficou na pasta (aberto, ele não se apaga). Pode apagar a pasta à mão."), wCampo);
        ImGui::PopFont();
        if (BotoesRodape(px, yBot, T("Fechar"), NULL) == 1) gRodando = false;
    }
    else if (gEtapa == ET_ERRO) {
        ImGui::PushFont(gFtBody);
        dl->AddText(ImVec2(px, S(124)), Tk::Vermelho, T("Algo deu errado:"));
        TextoQuebra(dl, ImVec2(px, S(150)), Tk::Texto2, gErro, wCampo);
        ImGui::PopFont();
        int rX = BotoesRodape(px, yBot, T("Tentar de novo"), T("Fechar"));
        if (rX == 1) gEtapa = ET_CONFIG;
        else if (rX == 2) gRodando = false;
    }

    // rodape
    ImGui::PushFont(gFtMini);
    dl->AddText(ImVec2(px, ds.y - S(30)), Tk::Texto4, gLangI ? "Trok Launcher v" VERSAO_INST "  \xC2\xB7  TrokMods team"
                                                             : "Trok Launcher v" VERSAO_INST "  \xC2\xB7  equipe TrokMods");
    ImGui::PopFont();

    // arrastar a janela pela faixa do topo
    if (ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered() && io.MousePos.y < S(40) && io.MousePos.x > wArte) {
        ReleaseCapture();
        SendMessageA(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }
    dl->AddRect(ImVec2(0.5f, 0.5f), ImVec2(ds.x - 0.5f, ds.y - 0.5f), Cinza(255, 26), 0, 0, 1);
    ImGui::End();
    ImGui::PopStyleVar(2);
}

// ---------- main ----------
static LRESULT WINAPI WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 1;
    if (m == WM_DESTROY) { gRodando = false; PostQuitMessage(0); return 0; }
    return DefWindowProcW(h, m, w, l); // janela Unicode: o titulo em russo/turco aparece certo na barra de tarefas
}

// dll do sistema pelo caminho completo do System32 (nunca uma com o mesmo nome na pasta do setup)
static HMODULE CarregarDllDoSistema(const char* nome) {
    char cam[MAX_PATH];
    UINT n = GetSystemDirectoryA(cam, MAX_PATH);
    if (!n || n + strlen(nome) + 2 > MAX_PATH) return NULL;
    strcat(cam, "\\");
    strcat(cam, nome);
    return LoadLibraryA(cam);
}

// Montserrat embutida (a fonte do site e do launcher); a memoria e do recurso, o atlas nao libera
static ImFont* FonteEmbutida(ImGuiIO& io, int id, float tam, const ImWchar* rango) {
    HRSRC r = FindResourceA(NULL, MAKEINTRESOURCEA(id), (LPCSTR)RT_RCDATA);
    if (!r) return NULL;
    HGLOBAL h = LoadResource(NULL, r);
    void* p = h ? LockResource(h) : NULL;
    DWORD n = SizeofResource(NULL, r);
    if (!p || n < 1024) return NULL;
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    return io.Fonts->AddFontFromMemoryTTF(p, (int)n, tam, &cfg, rango);
}

// reserva: a Segoe do proprio Windows, pela pasta real (antes era "C:\Windows\Fonts" fixo)
static ImFont* FonteDoWindows(ImGuiIO& io, const char* arq, float tam, const ImWchar* rango) {
    char cam[MAX_PATH];
    UINT n = GetWindowsDirectoryA(cam, MAX_PATH);
    if (!n || n + strlen(arq) + 9 > MAX_PATH) return NULL;
    strcat(cam, "\\Fonts\\");
    strcat(cam, arq);
    if (GetFileAttributesA(cam) == INVALID_FILE_ATTRIBUTES) return NULL;
    return io.Fonts->AddFontFromFileTTF(cam, tam, NULL, rango);
}

// atualizacao sem Direct3D (driver quebrado): troca os arquivos sem janela e reabre o launcher, em vez
// de deixar a pessoa sem launcher (o antigo ja foi fechado a essa altura)
static int AtualizarSemJanela() {
    bool ok = true;
    while (ok && gArqAtual < gNumPak) ok = InstalarProximo();
    if (!ok) { AvisoJanela(gErro, MB_ICONERROR); return 1; }
    FinalizarInstalacao();
    char alvo[MAX_PATH];
    _snprintf(alvo, MAX_PATH - 1, "%s\\Trok Launcher.exe", gPastaDestino);
    alvo[MAX_PATH - 1] = 0;
    ShellExecuteA(NULL, "open", alvo, NULL, gPastaDestino, SW_SHOWNORMAL);
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR linha, int) {
    { // DLLs do sistema SO do System32. O setup costuma rodar da pasta Downloads: sem isso, uma dll com
      // nome de dll do sistema deixada la seria carregada no lugar da verdadeira (DLL planting). O d3d9 e o
      // WindowsCodecs entram por delay-load (build-instalador.ps1), entao ja chegam depois desta linha
        typedef BOOL(WINAPI* FnSDD)(DWORD);
        FnSDD SetDDD = (FnSDD)GetProcAddress(GetModuleHandleA("kernel32.dll"), "SetDefaultDllDirectories");
        if (SetDDD) SetDDD(0x00000800); // LOAD_LIBRARY_SEARCH_SYSTEM32
    }
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    { // nitido em qualquer escala do Windows (mesma regra do launcher; senao o DWM borra)
        typedef BOOL(WINAPI* FnCtx)(HANDLE);
        HMODULE u32 = GetModuleHandleA("user32.dll");
        FnCtx SetCtx = u32 ? (FnCtx)GetProcAddress(u32, "SetProcessDpiAwarenessContext") : NULL;
        if (!SetCtx || !SetCtx((HANDLE)-4)) SetProcessDPIAware(); // -4 = per-monitor v2
    }

    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) { // idioma do Windows ANTES de qualquer aviso
        case LANG_PORTUGUESE: gLangI = 0; break;
        case LANG_SPANISH:    gLangI = 2; break;
        case LANG_RUSSIAN:    gLangI = 3; break;
        case LANG_INDONESIAN: gLangI = 4; break;
        case LANG_TURKISH:    gLangI = 5; break;
        default:              gLangI = 1; break;
    }

    // modo remocao: "--remover" roda NO LUGAR (o Desinstalar.exe da propria instalacao, que e o que o
    // "Aplicativos" do Windows chama). A pasta vem do caminho do proprio exe - nunca da linha de comando -
    // e so vale se o exe e o Desinstalar.exe de uma instalacao nossa
    if (strstr(linha, "--remover")) {
        bool pastaOk = false;
        char eu[MAX_PATH] = "";
        GetModuleFileNameA(NULL, eu, MAX_PATH);
        char* b = strrchr(eu, '\\');
        if (b && _stricmp(b + 1, "Desinstalar.exe") == 0) {
            *b = 0;
            strncpy(gPastaRemover, eu, MAX_PATH - 1);
            gPastaRemover[MAX_PATH - 1] = 0;
            char chk[MAX_PATH + 24];
            _snprintf(chk, sizeof(chk) - 1, "%s\\Trok Launcher.exe", gPastaRemover);
            chk[sizeof(chk) - 1] = 0;
            pastaOk = GetFileAttributesA(chk) != INVALID_FILE_ATTRIBUTES || PastaRegistrada(gPastaRemover);
        }
        if (!pastaOk) {
            AvisoJanela(T("Pasta de instalação não encontrada."), MB_ICONERROR);
            CoUninitialize();
            return 1;
        }
        gEtapa = ET_REMOVER;
    }

    if (gEtapa != ET_REMOVER) {
        if (!LerPayload()) {
            AvisoJanela(T("Instalador corrompido (faltam arquivos)."), MB_ICONERROR);
            CoUninitialize();
            return 1;
        }
        LimparSobrasDoTemp(); // desinstaladores de remocoes anteriores que ficaram no %TEMP%
    }
    {
        char base[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, base)))
            _snprintf(gPastaDestino, MAX_PATH - 1, "%s\\Trok Launcher", base);
        else strcpy(gPastaDestino, "C:\\Trok Launcher");
    }
    if (gEtapa == ET_IDIOMA && strstr(linha, "--atualizar")) { // atualizacao silenciosa
        gModoAtt = true;
        // A atualizacao tem que cair NA PASTA ONDE O LAUNCHER ESTA, nao no destino padrao: quem
        // escolheu outra pasta na instalacao recebia a versao nova em %LOCALAPPDATA%, continuava
        // abrindo a antiga pelo atalho e o aviso de atualizacao voltava pra sempre.
        // 1a fonte: a pasta que o proprio launcher mandou (--atualizar "<pasta>").
        {
            char pasta[MAX_PATH] = "";
            const char* asp = strchr(strstr(linha, "--atualizar"), '"');
            if (asp) {
                const char* fim = strchr(asp + 1, '"');
                if (fim && fim - asp - 1 < MAX_PATH) { memcpy(pasta, asp + 1, fim - asp - 1); pasta[fim - asp - 1] = 0; }
            }
            // 2a fonte: o proprio launcher que esta fechando agora (cura quem ja ficou com o
            // registro errado por causa da atualizacao que instalava sempre no destino padrao)
            if (!pasta[0]) EnumWindows(AcharPastaDoLauncher, (LPARAM)pasta);
            if (!pasta[0]) { // 3a fonte: onde o desinstalador diz que a instalacao mora
                HKEY k;
                if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TrokLauncher",
                                  0, KEY_QUERY_VALUE, &k) == ERROR_SUCCESS) {
                    DWORD tam = MAX_PATH - 1, tipo = 0;
                    if (RegQueryValueExA(k, "InstallLocation", NULL, &tipo, (BYTE*)pasta, &tam) != ERROR_SUCCESS || tipo != REG_SZ)
                        pasta[0] = 0;
                    else pasta[tam < MAX_PATH ? tam : MAX_PATH - 1] = 0;
                    RegCloseKey(k);
                }
            }
            if (pasta[0]) { // so aceita se o launcher realmente estiver la
                size_t L = strlen(pasta);
                while (L > 3 && (pasta[L - 1] == '\\' || pasta[L - 1] == '/')) pasta[--L] = 0;
                char exe[MAX_PATH];
                _snprintf(exe, MAX_PATH - 1, "%s\\Trok Launcher.exe", pasta); exe[MAX_PATH - 1] = 0;
                if (GetFileAttributesA(exe) != INVALID_FILE_ATTRIBUTES) {
                    strncpy(gPastaDestino, pasta, MAX_PATH - 1);
                    gPastaDestino[MAX_PATH - 1] = 0;
                }
            }
        }
        Sleep(500); // o launcher que nos chamou esta fechando
        FecharLauncherEm(gPastaDestino);
        gArqAtual = 0;
        gEtapa = ET_INSTALANDO;
    }

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0, 0, hInst,
        LoadIconW(hInst, MAKEINTRESOURCEW(1)), LoadCursorW(NULL, (LPCWSTR)IDC_ARROW), NULL, NULL,
        L"TrokInstaller", LoadIconW(hInst, MAKEINTRESOURCEW(1)) };
    RegisterClassExW(&wc);
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
    wchar_t tituloW[128];
    MultiByteToWideChar(CP_UTF8, 0, gEtapa == ET_REMOVER ? T("Remover Trok Launcher") : T("Instalar Trok Launcher"), -1, tituloW, 128);
    HWND hwnd = CreateWindowW(L"TrokInstaller", tituloW, WS_POPUP,
        (sw - janW) / 2, (sh - janH) / 2, janW, janH, NULL, NULL, hInst, NULL);
    HMODULE dwm = CarregarDllDoSistema("dwmapi.dll");
    if (dwm) {
        typedef HRESULT(WINAPI* FnAttr)(HWND, DWORD, LPCVOID, DWORD);
        FnAttr SetAttr = (FnAttr)GetProcAddress(dwm, "DwmSetWindowAttribute");
        // cantos arredondados + borda padrao do Windows 11 (DWMWCP_ROUND), igual ao launcher
        if (SetAttr) { DWORD pref = 2; SetAttr(hwnd, 33, &pref, sizeof(pref)); }
    }

    gD3D = Direct3DCreate9(D3D_SDK_VERSION);
    ZeroMemory(&gPP, sizeof(gPP));
    gPP.Windowed = TRUE; gPP.SwapEffect = D3DSWAPEFFECT_DISCARD;
    gPP.BackBufferFormat = D3DFMT_UNKNOWN;
    gPP.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    gDev = NULL;
    if (gD3D && FAILED(gD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
            D3DCREATE_HARDWARE_VERTEXPROCESSING, &gPP, &gDev))) {
        gDev = NULL;
        if (FAILED(gD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING, &gPP, &gDev))) gDev = NULL;
    }
    if (!gD3D || !gDev) { // sem Direct3D 9: antes caia num ponteiro nulo; agora avisa (ou atualiza sem janela)
        int cod = 1;
        if (gModoAtt) cod = AtualizarSemJanela();
        else AvisoJanela(T("Não deu para abrir o Direct3D 9 neste computador. Atualize o driver de vídeo e tente de novo."), MB_ICONERROR);
        if (gD3D) gD3D->Release();
        DestroyWindow(hwnd);
        UnregisterClassW(L"TrokInstaller", hInst);
        CoUninitialize();
        return cod;
    }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(gDev);
    static const ImWchar RANGO_I[] = { 0x0020, 0x024F, 0x0400, 0x04FF, 0 }; // latim estendido + cirilico
    gFtBody   = FonteEmbutida(io, 14, S(15.0f), RANGO_I); // Montserrat Medium (a fonte do site)
    gFtBold   = FonteEmbutida(io, 16, S(15.5f), RANGO_I); // Montserrat Bold
    gFtMini   = FonteEmbutida(io, 16, S(12.0f), RANGO_I);
    gFtTitulo = FonteEmbutida(io, 16, S(30.0f), RANGO_I);
    if (!gFtBody || !gFtBold || !gFtMini || !gFtTitulo) { // sem os recursos: a Segoe do proprio Windows
        io.Fonts->Clear();
        gFtBody   = FonteDoWindows(io, "segoeui.ttf",  S(16.0f), RANGO_I);
        gFtBold   = FonteDoWindows(io, "segoeuib.ttf", S(16.5f), RANGO_I);
        gFtMini   = FonteDoWindows(io, "segoeuib.ttf", S(13.0f), RANGO_I);
        gFtTitulo = FonteDoWindows(io, "segoeuib.ttf", S(32.0f), RANGO_I);
    }
    if (!gFtBody)   gFtBody   = io.Fonts->AddFontDefault();
    if (!gFtBold)   gFtBold   = gFtBody;
    if (!gFtMini)   gFtMini   = gFtBody;
    if (!gFtTitulo) gFtTitulo = gFtBold;
    CarregarArte(gDev);

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    while (gRodando) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) gRodando = false;
        }
        if (!gRodando) break;
        { // device perdido (tela bloqueada, troca de resolucao): espera e reseta, em vez de congelar a janela
            HRESULT coop = gDev->TestCooperativeLevel();
            if (coop == D3DERR_DEVICELOST) { Sleep(100); continue; }
            if (coop == D3DERR_DEVICENOTRESET) {
                ImGui_ImplDX9_InvalidateDeviceObjects();
                if (FAILED(gDev->Reset(&gPP))) { Sleep(200); continue; }
                ImGui_ImplDX9_CreateDeviceObjects();
            }
        }
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        DesenhaUI(hwnd);
        ImGui::EndFrame();
        gDev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_RGBA(11, 11, 12, 255), 1.0f, 0); // o #0B0B0C do site
        if (gDev->BeginScene() >= 0) {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            gDev->EndScene();
        }
        gDev->Present(NULL, NULL, NULL, NULL);
        if (gPedirPasta) { // dialogo nativo FORA do frame
            gPedirPasta = false;
            wchar_t tituloD[128];
            MultiByteToWideChar(CP_UTF8, 0, T("Onde instalar o Trok Launcher"), -1, tituloD, 128);
            BROWSEINFOW bi = { 0 };
            bi.hwndOwner = hwnd;
            bi.lpszTitle = tituloD; // em Unicode: o titulo em russo nao vira "????"
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t pastaW[MAX_PATH];
                char pasta[MAX_PATH];
                BOOL perdeu = FALSE;
                if (SHGetPathFromIDListW(pidl, pastaW)) {
                    if (WideCharToMultiByte(CP_ACP, 0, pastaW, -1, pasta, MAX_PATH, NULL, &perdeu) && !perdeu) {
                        size_t L = strlen(pasta);
                        while (L > 3 && pasta[L - 1] == '\\') pasta[--L] = 0; // "D:\" fica; "D:\Jogos\" perde a barra
                        const char* ult = strrchr(pasta, '\\');
                        if (ult && _stricmp(ult + 1, "Trok Launcher") == 0) // escolheu a pasta do launcher: nao dobra
                            strncpy(gPastaDestino, pasta, MAX_PATH - 1);
                        else
                            _snprintf(gPastaDestino, MAX_PATH - 1, (L == 3) ? "%sTrok Launcher" : "%s\\Trok Launcher", pasta);
                        gPastaDestino[MAX_PATH - 1] = 0;
                    } else AvisoJanela(T("Essa pasta tem letras que este instalador não aceita. Escolha outra."), MB_ICONWARNING);
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
    UnregisterClassW(L"TrokInstaller", hInst);
    CoUninitialize();
    return 0;
}
