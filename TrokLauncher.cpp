// Trok Launcher - launcher moderno de SA-MP (estilo janela do Riot Client, sem fullscreen).
// Design aprovado no mockup: base preto e branco + cor de destaque a escolha do usuario,
// ceu de San Andreas em silhueta P&B que muda por servidor, sidebar, 3 telas, JOGAR gigante.
// Backend real: query UDP dos servidores SA-MP, nick no registro, lanca samp.exe ip:porta.
// Autoria: equipe TrokMods. Alvo: Windows x86, zero dependencias (CRT estatico).

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wininet.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <tlhelp32.h>
#include <d3d9.h>
#include <wincodec.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h" // ShadeVertsLinearColorGradientKeepAlpha (degrade sem emenda)
#include "imgui/backends/imgui_impl_dx9.h"
#include "imgui/backends/imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shlwapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ===================== config =====================

#define JANELA_W 1420
#define JANELA_H 800
#define MAX_SERVIDORES 16
#ifdef TROK_TESTE_UPDATE
#define VERSAO "v0.9" // exe de AMOSTRA: se acha antigo p/ demonstrar o fluxo de atualizacao
#else
#define VERSAO "v1.0"
#endif

// atualizacoes: arquivo de texto hospedado (GitHub raw e gratis). Formato:
//   linha 1 = versao (ex: v1.1) | linha 2 = link do download | resto = novidades (1 por linha)
#ifdef TROK_TESTE_UPDATE // amostra: versao.txt LOCAL com link direto (demo 100% offline)
#define URL_VERSAO "file:///C:/Users/victo/OneDrive/Documentos/__GRAND%20THEFT%20AUTOS/SAMP%20Graphics%20Trok%202%20-%20Copia/MoonLoader/_dev/TrokLauncher/versão-teste.txt"
#else
#define URL_VERSAO "https://raw.githubusercontent.com/trokvictor-dot/trok-launcher/main/versao.txt"
#endif

// Discord Rich Presence: crie um Application em discord.com/developers (gratis),
// cole o Application ID aqui. "0" = desligado.
#define DISCORD_APP_ID "0"

struct Servidor {
    char nome[96];      // do .ini; substituido pelo hostname da query quando responder
    char ip[64];        // "host:porta"
    char modo[64];
    int  sky;           // legado do formato do ini (sem uso visual)
    char apelido[96];   // nome de exibicao definido pelo usuario (nao muda com a query)
    char img[MAX_PATH]; // imagem de exibicao do servidor (fundo da home)
    char logo[MAX_PATH];// logo: substitui o NOME no card de favorito
    char sites[4][160]; // links oficiais: 0 discord, 1 site, 2 forum, 3 youtube
    char contaPref[32]; // nick da conta pre-selecionada ao jogar ("" = conta atual)
    IDirect3DTexture9* tex;
    IDirect3DTexture9* texLogo;
    // resultado da query (thread) --
    volatile int  online, maxp, ping; // ping -1 = sem resposta ainda
    volatile int  senha;              // 1 = servidor com senha
    char hostnameQ[96];
    char modoQ[64];
};

struct Accent { const char* nome; ImU32 cor, hi; };
static const Accent ACCENTS[] = { // paleta escolhida por ele (referencia de imagem): tons editoriais
    { "Branco",    IM_COL32(222,224,228,255), IM_COL32(198,201,207,255) }, // off-white (branco puro estourava)
    { "Tijolo",    IM_COL32(191, 49, 38,255), IM_COL32(215, 90, 80,255) },
    { "Laranja",   IM_COL32(252, 94, 58,255), IM_COL32(255,128, 85,255) }, // = laranja do blog (#FC5E3A / #FF8055)
    { "Mostarda",  IM_COL32(236,201, 78,255), IM_COL32(245,220,133,255) },
    { "Oliva",     IM_COL32( 97,128, 79,255), IM_COL32(138,163,120,255) },
    { "Azul",      IM_COL32( 33,111,237,255), IM_COL32( 92,156,247,255) }, // saturado (o marinho era apagado)
    { "Framboesa", IM_COL32(199, 78, 94,255), IM_COL32(218,118,132,255) },
};
#define N_ACCENTS 7

// destaque PERSONALIZADO (seletor tipo Photoshop nas Configuracoes); gAccent == N_ACCENTS usa ele
static Accent gAccentCustom = { "Custom", IM_COL32(252, 94, 58, 255), IM_COL32(255, 128, 85, 255) };

// "datas" = instalacoes diferentes do GTA/SA-MP (costume da comunidade)
#define MAX_DATAS 8
struct DataGta {
    char nome[64];
    char caminho[MAX_PATH];
    char desc[160];
    char img[MAX_PATH];             // imagem 16:9 (png/jpg); vazio = card com inicial
    char userfiles[MAX_PATH];       // User Files desta data; vazio = Documentos\GTA San Andreas User Files
    IDirect3DTexture9* tex;
};
static DataGta gDatas[MAX_DATAS];
static int gNumDatas = 0;
static int gDataSel = 0;

static Servidor gSrv[MAX_SERVIDORES];
static int  gNumSrv = 0;
static int  gSel = 0;
static int  gTela = 0;              // 0 home, 1 servidores, 2 datas, 3 opcoes
static int  gAccent = 2;            // 0..N_ACCENTS-1 = paleta; N_ACCENTS = personalizada (2 = LARANJA oficial)
static const Accent& AccentAtual() { return (gAccent >= N_ACCENTS) ? gAccentCustom : ACCENTS[gAccent]; }
static char gNick[32] = "Nick_Sobrenome";
static char gPastaGta[MAX_PATH] = "C:\\Jogos\\GTA San Andreas";
static bool gFecharAoJogar = true;
static bool gFecharBandeja = true;   // X esconde para a bandeja em vez de sair
static bool gIniciarMin = false;     // abre ja escondido na bandeja
static bool gLembrarUlt = true;      // abre no ultimo servidor selecionado
static bool gUltimoPrimeiro = true;  // ao jogar, o servidor vai pro inicio dos favoritos
static char gIniPath[MAX_PATH];
static bool gPulaSalvarSaida = false; // depois de importar, sair NAO pode regravar a config antiga
static char gBusca[64] = "";

// ceu com crossfade
static int   gSkyAtual = 0;
static int   gSkyAlvo = 0;
static float gSkyFade = 1.0f;

// conexao (overlay)
static bool  gConectando = false;
static float gConnT = 0;
static char  gConnNome[96], gConnIp[64];
static char  gConnSenha[32] = "";    // senha digitada p/ servidor trancado (vai na linha de comando)
static bool  gPedirSenha = false;    // modal pedindo a senha antes de conectar
static char  gPendNome[96], gPendIp[64];

// galeria (screenshots do User Files da data em uso)
#define MAX_FOTOS 1000  // guarda as 1000 MAIS RECENTES; texturas so das visiveis (streaming)
struct Foto { char caminho[MAX_PATH]; FILETIME quando; IDirect3DTexture9* tex; bool falhou; }; // falhou = cache negativo (arquivo corrompido nao re-decodifica em loop)
static Foto gFotos[MAX_FOTOS];
static int  gNumFotos = 0;
static char gFotosDir[MAX_PATH] = "";
static int  gFotoVista = -1;         // visualizador aberto na foto N
static int  gGalData = -1;           // qual DATA a galeria mostra (-1 = comeca na data em uso)
#define N_FG 3 // full-res em memoria: a foto aberta + as 2 vizinhas (setas ficam instantaneas)
struct FGSlot { char caminho[MAX_PATH]; IDirect3DTexture9* tex; bool pedida; char falhas; };
static FGSlot gFG[N_FG];
static float gFGFade = 0;            // cross-fade thumb -> full-res (mata o "pop" da troca)
static int   gFGFadeIdx = -1;
static DWORD gUltimaAtividade = 0;   // ultimo input do usuario (render preguicoso)

// checagem de atualizacao (thread; so mostra o botao se houver versao nova)
static char gAttVersao[32] = "", gAttUrl[300] = "", gAttNotas[1600] = "";
static volatile int gAttEstado = 0;  // 0 = nada, 1 = tem atualizacao
static bool gAttPopup = false;
// download DENTRO do launcher: baixa o setup e roda em modo --atualizar (troca e reabre)
static volatile int gAttBaixa = 0;   // 0 parado, 1 baixando, 2 falhou, 3 baixado (instalar+sair)
static volatile int gAttPct = 0;     // 0..100 (-1 = servidor nao informou o tamanho)
static char gAttArquivo[MAX_PATH] = "";

// aba MODS: posts do blog TrokMods (feed RSS do Blogger); post novo = bolinha na sidebar
#define URL_BLOG "https://trokmods.blogspot.com"
#define URL_DISCORD "https://discord.gg/2uHzexhg6J"   // comunidade TrokMods
#define URL_CAFE "https://livepix.gg/trokmods"                 // "me pague um café" (placeholder)
// GitHub so por baixo dos panos (versao.txt e download do update): nada visivel aponta pra la
#define URL_POST_LAUNCHER URL_BLOG "/2026/09/trok-launcher-v10-o-seu-sa-mp-moderno.html"
#define MAX_MODS 30
struct ModPost {
    char titulo[160];
    char url[300];
    char data[48];
    char resumo[200];        // comeco do texto do post, sem html
    char imgCache[MAX_PATH]; // primeira imagem do post, baixada pra pasta de cache
};
static ModPost gMods[MAX_MODS];
static IDirect3DTexture9* gModsTex[MAX_MODS]; // capas dos cards (carregadas na UI)
static volatile int gNumMods = 0;
static volatile int gModsEstado = 0; // 0 nunca buscou, 1 buscando, 2 ok, 3 sem posts/falhou
static bool gModsNovo = false;       // tem post que o usuario ainda nao viu
static char gModsFeed[300] = URL_BLOG "/feeds/posts/default?alt=rss"; // troca pelo ini se quiser
static char gModsUltimo[300] = "";   // url do post mais recente ja visto (persistido no ini)
static char gModsVistoAte[300] = ""; // marco da visita ATUAL: cards acima dele ganham bolinha

// discord rich presence
static bool gDiscordRP = true;
static volatile bool gRPJogando = false;
static char gRPServidor[96] = "";
static long long gRPDesde = 0;
static HWND gHwnd = NULL;
static float gEscala = 1.0f; // janela fisica / 1420 logico: a UI inteira escala junto

static bool gFocarBusca = false;   // Ctrl+F: foca a busca da aba Servidores
static bool gBoasVindas = false;   // 1a execucao sem SA-MP detectado: guia a pessoa
static bool gSampOk = true;        // a data em uso tem samp.exe? (reavaliado a cada ~1s; esconde os botoes Jogar)
static float gHomeSlide = 1.0f;    // slide estilo Rockstar: conteudo da Home entra da direita
static bool  gSideAberta = false;  // menu lateral expandido (nomes das abas, estilo Rockstar)
static float gSideAnim = 0.0f;     // 0 = recolhido (76px), 1 = expandido (~230px)
static bool gSalvarSenhaServ = false; // opcoes do SA-MP original (HKCU\Software\SAMP: SaveServPasses/SaveRconPasses)
static bool gSalvarSenhaRcon = false;

// aviso na tela (toast): unico feedback de erro que o usuario ve (ex: samp.exe sumiu)
static char gAviso[200] = "";
static float gAvisoT = 0;
static void Avisar(const char* msg) {
    strncpy(gAviso, msg, sizeof(gAviso) - 1);
    gAviso[sizeof(gAviso) - 1] = 0;
    gAvisoT = 4.5f;
}

// lista publica de servidores (masterlist da open.mp - a lists.sa-mp.com original morreu)
#define MAX_PUB 500
struct SrvPub { char nome[96]; char ip[64]; char modo[64]; int on, maxp, pw; };
static SrvPub gPub[MAX_PUB];
static volatile int gNumPub = 0;
static volatile int gPubEstado = 0;  // 0 nunca pedido, 1 baixando, 2 ok, -1 falhou
static int gSubAba = 0;              // tela servidores: 0 favoritos, 1 internet
static bool gOcCheios = false, gOcSenha = false, gOcVazios = false; // filtros (internet + favoritos)
static bool gOcOff = false;                     // ocultar sem resposta (so faz sentido nos favoritos)
static bool gFiltrosAberto = false;             // dropdown de filtros (funil ao lado da busca)
static ImVec2 gFiltroBtnA, gFiltroBtnB;         // retangulo do botao funil

// edicao de datas (menu ... / adicionar); pickers rodam FORA do frame imgui
static int  gPickImagem = -1;        // indice da data que quer trocar a imagem
static int  gPickCaminho = -1;       // indice da data que quer trocar o caminho
static int  gPickUserFiles = -1;     // indice da data que quer trocar o User Files
static bool gAddData = false;        // pediu para adicionar uma data
static int  gRenomear = -1;          // modal de renomear/descricao aberto para a data N
static int  gDataMenuAlvo = -1;      // qual data o menu ... esta mirando
static char gEditNome[64], gEditDesc[160];

// edicao de servidor favorito (apelido, imagem de exibicao, remover)
static int  gEditSrv = -1;           // modal aberto para o favorito N
static int  gPickImgSrv = -1;        // favorito que quer trocar a imagem (picker fora do frame)
static int  gPickLogoSrv = -1;       // favorito que quer trocar a LOGO
static char gEditApelido[96];
static bool gAddLink = false;        // formulario "+ adicionar link" aberto no Editar servidor
static char gNovoRot[24], gNovoUrl[150];
static bool gEscolherAvatar = false; // painel de CONTAS (nick + avatar por conta)
static int  gAvatarCor = 0;          // avatar da conta ativa (cor; imagens depois)

// contas: cada uma tem nick proprio (util p/ quem usa nick diferente por servidor)
#define MAX_PERFIS 8
struct Perfil { char nick[32]; int cor; char avatar[64]; }; // avatar = arquivo em avatars\ ("" = cor)
static Perfil gPerfis[MAX_PERFIS];
static int  gNumPerfis = 0;
static int  gPerfilSel = 0;
static int  gContaEdit = -1;         // -1 = lista de contas; >=0 = editando a conta N
static char gEditNick[32];
static int  gEditCor = 0;
static char gEditAvatar[64] = "";

// avatares por IMAGEM: pasta avatars\ ao lado do exe (png/jpg 1:1)
#define MAX_AVATARES 32
struct AvatarImg { char arquivo[64]; IDirect3DTexture9* tex; };
static AvatarImg gAvatares[MAX_AVATARES];
static int gNumAvatares = 0;
static float gContasAnim = 0;        // 0 fechado .. 1 aberto (dropdown colapsavel de contas)
static ImVec2 gCtTopA, gCtTopB;      // retangulo do controle nick+avatar do topo
static ImVec2 gContasA, gContasB;    // retangulo do dropdown (do frame anterior)
static bool gMouseNoDrop = false;    // mouse sobre o dropdown: bloqueia zonas IsMouseHoveringRect por baixo
static float gHovCard[MAX_SERVIDORES] = { 0 }; // hover animado dos cards do rail (0..1)

// texturas removidas DURANTE um frame nao podem ser liberadas na hora (a drawlist do frame
// ainda referencia); ficam aqui e sao liberadas fora do frame, no loop principal
static IDirect3DTexture9* gLixoTex[160]; // 160: a galeria descarta muitas texturas de uma vez
static int gNumLixo = 0;

// crossfade do fundo da Home ao trocar de favorito
static IDirect3DTexture9* gFundoAtual = NULL;
static IDirect3DTexture9* gFundoAnt = NULL;   // a anterior, saindo de cena
static float gFundoFade = 1.0f;

static void AdiarRelease(IDirect3DTexture9* t) {
    if (!t) return;
    if (t == gFundoAtual) gFundoAtual = NULL; // nunca desenhar textura ja liberada
    if (t == gFundoAnt) gFundoAnt = NULL;
    if (gNumLixo < 160) gLixoTex[gNumLixo++] = t;
}

static LPDIRECT3DDEVICE9 gDev;       // usado tambem para recarregar imagem de data

static CRITICAL_SECTION gLock;      // protege os campos Q dos servidores
static volatile bool gRodando = true;

static ImFont *gFtBody, *gFtBold, *gFtMono, *gFtDisplay, *gFtBotao, *gFtMini, *gFtMonoS;
static ImFont *gFtCardNome, *gFtCardDesc; // nome/descricao das miniaturas de favorito (um tico maiores)
static ImFont *gFtPostTit;                // titulo do post na aba Mods (mesmo peso do blog)
static ImFont *gFtMiniLeve;              // Segoe UI Regular pequena (texto discreto, tipo a url do link)
static ImFont *gFtBotaoPost;              // rotulo do botao ABRIR O POST (peso 800 do blog)
static ImFont *gFtIco = NULL, *gFtIcoG = NULL; // fonte de icones LUCIDE (embutida no exe)

// icones LUCIDE (codepoints da release 1.39.0) - adeus PathLineTo fragmentado
#define I_CASA         u8"\uE0F5" // house
#define I_SERVIDORES   u8"\uE153" // server
#define I_PASTA        u8"\uE0D7" // folder
#define I_PASTA_ABRIR  u8"\uE247" // folder-open
#define I_ENGRENAGEM   u8"\uE154" // settings
#define I_GALERIA      u8"\uE5C4" // images
#define I_PACOTE       u8"\uE129" // package
#define I_INFO         u8"\uE0F9" // info
#define I_ATUALIZAR    u8"\uE145" // refresh-cw
#define I_FUNIL        u8"\uE0DC" // funnel
#define I_BAIXAR       u8"\uE0B2" // download
#define I_SUBIR        u8"\uE19E" // upload
#define I_LIXEIRA      u8"\uE18E" // trash-2
#define I_COPIAR       u8"\uE09E" // copy
#define I_CHECK        u8"\uE06C" // check
#define I_SETA_ESQ     u8"\uE06E" // chevron-left
#define I_SETA_DIR     u8"\uE06F" // chevron-right
#define I_FECHAR       u8"\uE1B2" // x
#define I_MENOS        u8"\uE11C" // minus
#define I_FAVORITO     u8"\uE060" // bookmark
#define I_PAINEL_FECHA u8"\uE21C" // panel-left-close
#define I_PAINEL_ABRE  u8"\uE21D" // panel-left-open
#define I_COMUNIDADE   u8"\uE40D" // messages-square (lucide nao tem logo do Discord)
#define I_GLOBO        u8"\uE0E8" // globe
static void Icone(ImDrawList* d, ImVec2 centro, const char* gl, ImU32 cor, float tam = 19.0f);

// ===================== util =====================

static ImU32 ComAlpha(ImU32 c, float a) {
    return (c & 0x00FFFFFF) | ((ImU32)(a * 255.0f) << 24);
}
static ImU32 Cinza(int v, int a = 255) { return IM_COL32(v, v, v, a); }

// texto sobre a cor de destaque: escuro em cor clara, branco em cor escura (contraste sempre)
static ImU32 TextoSobreAccent(ImU32 c) {
    int r = c & 255, g = (c >> 8) & 255, b = (c >> 16) & 255;
    int lum = (r * 299 + g * 587 + b * 114) / 1000;
    return lum > 150 ? Cinza(12) : Cinza(255);
}

// brilho suave na cor de destaque ao redor de um retangulo (halo do botao Jogar)
static void BrilhoSuave(ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 cor, float esp, int forca) {
    const int N = 9;
    float alCamada = (forca / 9.0f + 1.0f) / 255.0f;
    for (int i = N; i >= 1; i--) {
        float e = esp * 1.5f * i / N;
        d->AddRectFilled(ImVec2(a.x - e, a.y - e), ImVec2(b.x + e, b.y + e),
                         ComAlpha(cor, alCamada), e + (b.y - a.y) * 0.5f);
    }
}

// sombra macia atras de conteudo sobre imagem: MUITAS camadas de alpha baixissimo,
// para o decaimento ser continuo (3 camadas fortes viravam degraus visiveis - feio)
static void SombraSuave(ImDrawList* d, ImVec2 a, ImVec2 b, float esp, int forca) {
    const int N = 9;
    int al = forca / 9 + 1; // alpha por camada; empilhado o centro fica ~7-9% (quase nada)
    for (int i = N; i >= 1; i--) {
        float e = esp * 1.5f * i / N;
        d->AddRectFilled(ImVec2(a.x - e, a.y - e), ImVec2(b.x + e, b.y + e), IM_COL32(0, 0, 0, al), e + 8.0f);
    }
}

// "..." padrao do app: sem fundo chapado - sombra espalhada e sutil sob os pontos p/ leitura
static void DesenhaReticencias(ImDrawList* d, ImVec2 ma, ImVec2 mb, bool hov) {
    if (hov) d->AddRectFilled(ma, mb, Cinza(255, 22), 7);
    float cx = (ma.x + mb.x) * 0.5f, cy = (ma.y + mb.y) * 0.5f;
    for (int pt = -1; pt <= 1; pt++) {
        ImVec2 c(cx + pt * 6.5f, cy);
        d->AddCircleFilled(c, 7.5f, IM_COL32(0, 0, 0, 20), 16); // halo largo
        d->AddCircleFilled(c, 4.5f, IM_COL32(0, 0, 0, 28), 16); // miolo da sombra
        d->AddCircleFilled(c, 1.6f, Cinza(hov ? 255 : 232), 12);
    }
}

// dica (tooltip) para o item recem-desenhado; e o rotulo textual dos botoes de icone
static void Dica(const char* t) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("%s", t);
}

// bolinha de status: verde online, amarela com senha, vermelha sem resposta
// (com texto no hover - cor sozinha nao comunica p/ daltonicos)
static void BolaStatus(ImDrawList* d, ImVec2 c, int ping, int senha) {
    ImU32 cor = (ping < 0) ? IM_COL32(226, 82, 79, 235)
              : (senha ? IM_COL32(238, 200, 80, 235) : IM_COL32(96, 214, 116, 235));
    d->AddCircleFilled(c, 3.0f, cor, 20);
    if (ImGui::IsMouseHoveringRect(ImVec2(c.x - 7, c.y - 7), ImVec2(c.x + 7, c.y + 7)))
        ImGui::SetTooltip("%s", ping < 0 ? "sem resposta" : (senha ? "online, com senha" : "online"));
}

// compara nomes como humano le: 2 vem antes de 10
static int CmpNatural(const char* a, const char* b) {
    while (*a && *b) {
        if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
            long va = strtol(a, (char**)&a, 10);
            long vb = strtol(b, (char**)&b, 10);
            if (va != vb) return va < vb ? -1 : 1;
        } else {
            int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
            if (ca != cb) return ca - cb;
            a++; b++;
        }
    }
    return (int)strlen(a) - (int)strlen(b);
}

static ImU32 CorPing(int ping) {
    if (ping < 0)   return Cinza(90);
    if (ping < 70)  return Cinza(255);
    if (ping < 110) return Cinza(171);
    return Cinza(110);
}

static void LerNickRegistro();
static void GravarNickRegistro();
static void LerOpcoesSampRegistro();
static void GravarOpcaoSampRegistro(const char* nome, bool val);
static bool SampValido();

static void SalvarPerfis() {
    char chave[16], linha[140], n[16];
    for (int i = 0; i < MAX_PERFIS; i++) {
        sprintf(chave, "perfil%d", i);
        if (i < gNumPerfis) {
            sprintf(linha, "%s|%d|%s", gPerfis[i].nick, gPerfis[i].cor, gPerfis[i].avatar);
            WritePrivateProfileStringA("perfis", chave, linha, gIniPath);
        } else {
            WritePrivateProfileStringA("perfis", chave, NULL, gIniPath);
        }
    }
    sprintf(n, "%d", gPerfilSel);
    WritePrivateProfileStringA("config", "perfil_sel", n, gIniPath);
}

static bool gTrocouManual = false; // trocou de conta NA MAO nesta sessao: a escolha vence a pre-selecao do servidor

static void AplicarPerfil(int i, bool manual = false) {
    if (i < 0 || i >= gNumPerfis) return;
    if (manual) gTrocouManual = true;
    gPerfilSel = i;
    strncpy(gNick, gPerfis[i].nick, sizeof(gNick) - 1);
    gAvatarCor = gPerfis[i].cor % N_ACCENTS;
    GravarNickRegistro(); // o SA-MP passa a usar este nick
    SalvarPerfis();
}

// Primeira execucao: importa o que o launcher ORIGINAL do SA-MP ja guardava -
// nick e pasta do jogo do registro (HKCU\Software\SAMP), favoritos do USERDATA.DAT.
static void CriarIniInicial() {
    char nick[32] = "Nick_Sobrenome", pasta[MAX_PATH] = "C:\\Jogos\\GTA San Andreas";
    HKEY k;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\SAMP", 0, KEY_QUERY_VALUE, &k) == ERROR_SUCCESS) {
        char exe[MAX_PATH] = "";
        DWORD tam = sizeof(nick), tipo;
        RegQueryValueExA(k, "PlayerName", 0, &tipo, (BYTE*)nick, &tam);
        tam = sizeof(exe);
        if (RegQueryValueExA(k, "gta_sa_exe", 0, &tipo, (BYTE*)exe, &tam) == ERROR_SUCCESS && exe[0]) {
            char* b = strrchr(exe, '\\');
            if (b) { *b = 0; strncpy(pasta, exe, sizeof(pasta) - 1); }
        }
        RegCloseKey(k);
    }
    FILE* f = fopen(gIniPath, "wb");
    if (!f) return;
    fputs("; ============== Trok Launcher ==============\r\n"
          "; nick: seu nome no SA-MP (sem espacos)\r\n"
          "; accent: 0 branco, 1 tijolo, 2 laranja (oficial), 3 mostarda, 4 oliva, 5 azul, 6 framboesa\r\n"
          "; servidorN: nome|ip:porta|modo|ceu (ceu: 0 por-do-sol, 1 noite, 2 amanhecer, 3 tempestade)\r\n"
          "; dataN: Nome|pasta do jogo|descrição|imagem 16:9 (png/jpg, opcional)\r\n"
          "; (nick, pasta e favoritos importados do seu SA-MP original)\r\n", f);
    fprintf(f, "[config]\r\nnick = %s\r\naccent = 2\r\nfechar_ao_jogar = 1\r\n", nick); // 2 = LARANJA oficial
    // favoritos do USERDATA.DAT: SAMP + u32 versao + u32 qtde + [ipLen+ip u32porta nomeLen+nome passLen+pass rconLen+rcon]
    fputs("[servidores]\r\n", f);
    int nImp = 0;
    char ud[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, ud))) {
        strcat(ud, "\\GTA San Andreas User Files\\SAMP\\USERDATA.DAT");
        FILE* d = fopen(ud, "rb");
        if (d) {
            static unsigned char buf[65536];
            int n = (int)fread(buf, 1, sizeof(buf), d);
            fclose(d);
            if (n > 12 && memcmp(buf, "SAMP", 4) == 0) {
                int qt = *(int*)(buf + 8), p = 12;
                for (int i = 0; i < qt && nImp < MAX_SERVIDORES && p + 8 < n; i++) {
                    char ip[64] = "", nome[96] = "";
                    int il = *(int*)(buf + p); p += 4;
                    if (il < 0 || p + il > n) break;
                    memcpy(ip, buf + p, il < 63 ? il : 63); p += il;
                    int porta = *(int*)(buf + p); p += 4;
                    int nl = *(int*)(buf + p); p += 4;
                    if (nl < 0 || p + nl > n) break;
                    memcpy(nome, buf + p, nl < 95 ? nl : 95); p += nl;
                    int pl = *(int*)(buf + p); p += 4;
                    if (pl < 0 || p + pl > n) break;
                    p += pl;
                    int rl = *(int*)(buf + p); p += 4;
                    if (rl < 0 || p + rl > n) break;
                    p += rl;
                    for (char* c = nome; *c; c++) if (*c == '|') *c = '/'; // | e o separador do ini
                    fprintf(f, "servidor%d = %s|%s:%d||%d\r\n", nImp, nome[0] ? nome : ip, ip, porta, nImp % 4);
                    nImp++;
                }
            }
        }
    }
    if (nImp == 0)
        fputs("servidor0 = Adicione seus servidores aqui|127.0.0.1:7777|-|0\r\n", f);
    fprintf(f, "[datas]\r\ndata0 = Principal|%s||\r\n", pasta);
    fclose(f);
}

static void LerConfig() {
    if (GetFileAttributesA(gIniPath) == INVALID_FILE_ATTRIBUTES) CriarIniInicial();
    GetPrivateProfileStringA("config", "nick", gNick, gNick, sizeof(gNick), gIniPath);
    GetPrivateProfileStringA("config", "pasta_gta", gPastaGta, gPastaGta, sizeof(gPastaGta), gIniPath);
    gAccent = GetPrivateProfileIntA("config", "accent", 2, gIniPath); // padrao: Laranja
    if (gAccent < 0 || gAccent > N_ACCENTS) gAccent = 2; // N_ACCENTS = personalizada, e valido
    GetPrivateProfileStringA("config", "mods_feed", gModsFeed, gModsFeed, sizeof(gModsFeed), gIniPath);
    GetPrivateProfileStringA("config", "mods_ultimo", gModsUltimo, gModsUltimo, sizeof(gModsUltimo), gIniPath);
    {
        int cc = GetPrivateProfileIntA("config", "accent_custom", 0x3A5EFC, gIniPath); // 0xBBGGRR (#FC5E3A)
        int r = cc & 255, g = (cc >> 8) & 255, b = (cc >> 16) & 255;
        gAccentCustom.cor = IM_COL32(r, g, b, 255);
        gAccentCustom.hi = IM_COL32(r + (255 - r) * 28 / 100, g + (255 - g) * 28 / 100, b + (255 - b) * 28 / 100, 255);
    }
    gFecharAoJogar = GetPrivateProfileIntA("config", "fechar_ao_jogar", 1, gIniPath) != 0;
    gFecharBandeja = GetPrivateProfileIntA("config", "fechar_bandeja", 1, gIniPath) != 0;
    gIniciarMin    = GetPrivateProfileIntA("config", "iniciar_min", 0, gIniPath) != 0;
    gOcCheios = GetPrivateProfileIntA("config", "ocultar_cheios", 0, gIniPath) != 0;
    gOcSenha  = GetPrivateProfileIntA("config", "ocultar_senha", 0, gIniPath) != 0;
    gOcVazios = GetPrivateProfileIntA("config", "ocultar_vazios", 0, gIniPath) != 0;
    gOcOff    = GetPrivateProfileIntA("config", "ocultar_off", 0, gIniPath) != 0;
    gDiscordRP = GetPrivateProfileIntA("config", "discord_rp", 1, gIniPath) != 0;
    gNumSrv = 0;
    for (int i = 0; i < MAX_SERVIDORES; i++) {
        char chave[24], linha[2048] = ""; // 1024 truncava (7 campos + 4 links + conta passam disso)
        sprintf(chave, "servidor%d", i);
        GetPrivateProfileStringA("servidores", chave, "", linha, sizeof(linha), gIniPath);
        if (!linha[0]) continue;
        Servidor& s = gSrv[gNumSrv];
        memset(&s, 0, sizeof(s));
        s.ping = -1;
        char* p1 = strchr(linha, '|');
        if (!p1) continue;
        *p1++ = 0;
        char* p2 = strchr(p1, '|');
        if (p2) { *p2++ = 0; }
        char* p3 = p2 ? strchr(p2, '|') : NULL;
        if (p3) { *p3++ = 0; }
        strncpy(s.nome, linha, sizeof(s.nome) - 1);
        strncpy(s.ip, p1, sizeof(s.ip) - 1);
        if (p2) strncpy(s.modo, p2, sizeof(s.modo) - 1);
        s.sky = p3 ? atoi(p3) % 4 : (gNumSrv % 4);
        // campos opcionais na ordem: apelido|imagem|logo|discord|site|forum|youtube|conta
        char* resto = p3 ? strchr(p3, '|') : NULL;
        char* tok[8] = { 0 };
        int nt = 0;
        if (resto) {
            *resto++ = 0;
            while (resto && nt < 8) { tok[nt++] = resto; char* nx = strchr(resto, '|'); if (nx) *nx++ = 0; resto = nx; }
        }
        if (nt > 0) strncpy(s.apelido, tok[0], sizeof(s.apelido) - 1);
        if (nt > 1) strncpy(s.img, tok[1], sizeof(s.img) - 1);
        if (nt > 2) strncpy(s.logo, tok[2], sizeof(s.logo) - 1);
        for (int q = 0; q < 4; q++) if (nt > 3 + q) strncpy(s.sites[q], tok[3 + q], sizeof(s.sites[0]) - 1);
        if (nt > 7) strncpy(s.contaPref, tok[7], sizeof(s.contaPref) - 1);
        gNumSrv++;
    }
    if (gNumSrv == 0) { // ini sem servidores: um placeholder para a UI nao ficar vazia
        Servidor& s = gSrv[0];
        memset(&s, 0, sizeof(s));
        strcpy(s.nome, "Adicione servidores no .ini");
        strcpy(s.ip, "127.0.0.1:7777");
        strcpy(s.modo, "-");
        s.ping = -1;
        gNumSrv = 1;
    }
    // datas (instalacoes)
    gNumDatas = 0;
    for (int i = 0; i < MAX_DATAS; i++) {
        char chave[16], linha[512] = "";
        sprintf(chave, "data%d", i);
        GetPrivateProfileStringA("datas", chave, "", linha, sizeof(linha), gIniPath);
        if (!linha[0]) continue;
        DataGta& d = gDatas[gNumDatas];
        memset(&d, 0, sizeof(d));
        char* p1 = strchr(linha, '|');
        if (!p1) continue;
        *p1++ = 0;
        char* p2 = strchr(p1, '|');
        if (p2) *p2++ = 0;
        char* p3 = p2 ? strchr(p2, '|') : NULL;
        if (p3) *p3++ = 0;
        char* p4 = p3 ? strchr(p3, '|') : NULL;   // userfiles (opcional)
        if (p4) *p4++ = 0;
        strncpy(d.nome, linha, sizeof(d.nome) - 1);
        strncpy(d.caminho, p1, sizeof(d.caminho) - 1);
        if (p2) strncpy(d.desc, p2, sizeof(d.desc) - 1);
        if (p3) strncpy(d.img, p3, sizeof(d.img) - 1);
        if (p4) strncpy(d.userfiles, p4, sizeof(d.userfiles) - 1);
        gNumDatas++;
    }
    if (gNumDatas == 0) { // garante ao menos uma, com a pasta atual
        DataGta& d = gDatas[0];
        memset(&d, 0, sizeof(d));
        strcpy(d.nome, "Principal");
        strncpy(d.caminho, gPastaGta, sizeof(d.caminho) - 1);
        d.desc[0] = 0;
        gNumDatas = 1;
    }
    gAvatarCor = GetPrivateProfileIntA("config", "avatar_cor", 0, gIniPath) % N_ACCENTS;
    gDataSel = GetPrivateProfileIntA("config", "data_sel", 0, gIniPath);
    if (gDataSel < 0 || gDataSel >= gNumDatas) gDataSel = 0;
    strncpy(gPastaGta, gDatas[gDataSel].caminho, sizeof(gPastaGta) - 1); // a data manda na pasta
    LerNickRegistro(); // o registro do SA-MP vence o ini: nick unico com o launcher original
    LerOpcoesSampRegistro(); // SaveServPasses/SaveRconPasses (opcoes do samp.exe original)
    // contas
    gNumPerfis = 0;
    for (int i = 0; i < MAX_PERFIS; i++) {
        char chave[16], linha[140] = "";
        sprintf(chave, "perfil%d", i);
        GetPrivateProfileStringA("perfis", chave, "", linha, sizeof(linha), gIniPath);
        if (!linha[0]) continue;
        Perfil& pf = gPerfis[gNumPerfis];
        memset(&pf, 0, sizeof(pf));
        char* pp = strchr(linha, '|');
        if (pp) {
            *pp++ = 0;
            char* pav = strchr(pp, '|'); // 3o campo: arquivo do avatar
            if (pav) { *pav++ = 0; strncpy(pf.avatar, pav, sizeof(pf.avatar) - 1); }
            pf.cor = atoi(pp) % N_ACCENTS;
        }
        strncpy(pf.nick, linha, sizeof(pf.nick) - 1);
        gNumPerfis++;
    }
    if (gNumPerfis == 0) { // migracao: vira a conta 0
        strncpy(gPerfis[0].nick, gNick, sizeof(gPerfis[0].nick) - 1);
        gPerfis[0].cor = gAvatarCor;
        gNumPerfis = 1;
    }
    gPerfilSel = GetPrivateProfileIntA("config", "perfil_sel", 0, gIniPath);
    if (gPerfilSel < 0 || gPerfilSel >= gNumPerfis) gPerfilSel = 0;
    gLembrarUlt = GetPrivateProfileIntA("config", "lembrar_ultimo", 1, gIniPath) != 0;
    gUltimoPrimeiro = GetPrivateProfileIntA("config", "ultimo_primeiro", 1, gIniPath) != 0;
    if (gLembrarUlt) { // abre no ultimo servidor selecionado (opcional)
        gSel = GetPrivateProfileIntA("config", "last_sel", 0, gIniPath);
        if (gSel < 0 || gSel >= gNumSrv) gSel = 0;
    }
    // sync: o nick do registro atualiza a conta ativa; o avatar vem da conta
    strncpy(gPerfis[gPerfilSel].nick, gNick, sizeof(gPerfis[0].nick) - 1);
    gAvatarCor = gPerfis[gPerfilSel].cor % N_ACCENTS;
}

static void SalvarConfig() {
    char n[16];
    WritePrivateProfileStringA("config", "nick", gNick, gIniPath);
    WritePrivateProfileStringA("config", "pasta_gta", gPastaGta, gIniPath);
    sprintf(n, "%d", gAccent);
    WritePrivateProfileStringA("config", "accent", n, gIniPath);
    sprintf(n, "%d", (int)(gAccentCustom.cor & 0xFFFFFF));
    WritePrivateProfileStringA("config", "accent_custom", n, gIniPath);
    sprintf(n, "%d", gDataSel);
    WritePrivateProfileStringA("config", "data_sel", n, gIniPath);
    sprintf(n, "%d", gAvatarCor);
    WritePrivateProfileStringA("config", "avatar_cor", n, gIniPath);
    WritePrivateProfileStringA("config", "fechar_ao_jogar", gFecharAoJogar ? "1" : "0", gIniPath);
    WritePrivateProfileStringA("config", "fechar_bandeja", gFecharBandeja ? "1" : "0", gIniPath);
    WritePrivateProfileStringA("config", "iniciar_min", gIniciarMin ? "1" : "0", gIniPath);
    sprintf(n, "%d", gSel);
    WritePrivateProfileStringA("config", "last_sel", n, gIniPath);
    WritePrivateProfileStringA("config", "ocultar_cheios", gOcCheios ? "1" : "0", gIniPath);
    WritePrivateProfileStringA("config", "ocultar_senha", gOcSenha ? "1" : "0", gIniPath);
    WritePrivateProfileStringA("config", "ocultar_vazios", gOcVazios ? "1" : "0", gIniPath);
    WritePrivateProfileStringA("config", "ocultar_off", gOcOff ? "1" : "0", gIniPath);
    WritePrivateProfileStringA("config", "discord_rp", gDiscordRP ? "1" : "0", gIniPath);
    WritePrivateProfileStringA("config", "lembrar_ultimo", gLembrarUlt ? "1" : "0", gIniPath);
    WritePrivateProfileStringA("config", "ultimo_primeiro", gUltimoPrimeiro ? "1" : "0", gIniPath);
    WritePrivateProfileStringA("config", "mods_feed", gModsFeed, gIniPath);
    WritePrivateProfileStringA("config", "mods_ultimo", gModsUltimo, gIniPath);
}

// ===================== imagens das datas (WIC - decodificador nativo do Windows) =====================

// maxLado > 0: reduz a imagem no WIC antes de virar textura (thumb barato; full-res pesa ~8 MB cada)
static IDirect3DTexture9* CarregarImagemMax(LPDIRECT3DDEVICE9 dev, const char* caminho, int maxLado) {
    if (!caminho[0]) return NULL;
    wchar_t w[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, caminho, -1, w, MAX_PATH);
    IWICImagingFactory* fab = NULL;
    IWICBitmapDecoder* dec = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICBitmapScaler* esc = NULL;
    IWICBitmapSource* src = NULL;
    IDirect3DTexture9* tex = NULL;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                IID_IWICImagingFactory, (void**)&fab))) return NULL;
    do {
        if (FAILED(fab->CreateDecoderFromFilename(w, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec))) break;
        if (FAILED(dec->GetFrame(0, &frame))) break;
        IWICBitmapSource* fonte = frame;
        UINT ow = 0, oh = 0;
        frame->GetSize(&ow, &oh);
        if (maxLado > 0 && ow && oh && ((int)ow > maxLado || (int)oh > maxLado)) {
            UINT nw, nh;
            if (ow >= oh) { nw = maxLado; nh = (UINT)((float)oh * maxLado / ow); }
            else          { nh = maxLado; nw = (UINT)((float)ow * maxLado / oh); }
            if (nw < 1) nw = 1;
            if (nh < 1) nh = 1;
            // thumbs (ate 512): Linear e varias vezes mais rapido que Fant e a diferenca
            // visual em miniatura e imperceptivel; imagens grandes seguem no Fant
            WICBitmapInterpolationMode modo = (maxLado <= 512)
                ? WICBitmapInterpolationModeLinear : WICBitmapInterpolationModeFant;
            if (SUCCEEDED(fab->CreateBitmapScaler(&esc)) &&
                SUCCEEDED(esc->Initialize(frame, nw, nh, modo)))
                fonte = esc;
        }
        if (FAILED(WICConvertBitmapSource(GUID_WICPixelFormat32bppBGRA, fonte, &src))) break;
        UINT wpx = 0, hpx = 0;
        src->GetSize(&wpx, &hpx);
        if (!wpx || !hpx || wpx > 4096 || hpx > 4096) break;
        if (FAILED(dev->CreateTexture(wpx, hpx, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL))) { tex = NULL; break; }
        D3DLOCKED_RECT lr;
        if (FAILED(tex->LockRect(0, &lr, NULL, 0))) { tex->Release(); tex = NULL; break; }
        src->CopyPixels(NULL, lr.Pitch, lr.Pitch * hpx, (BYTE*)lr.pBits); // BGRA = ARGB do DX9
        tex->UnlockRect(0);
    } while (0);
    if (src) src->Release();
    if (esc) esc->Release();
    if (frame) frame->Release();
    if (dec) dec->Release();
    if (fab) fab->Release();
    return tex;
}

static IDirect3DTexture9* CarregarImagem(LPDIRECT3DDEVICE9 dev, const char* caminho) {
    return CarregarImagemMax(dev, caminho, 0);
}

// logo do TrokMods embutida no exe (recurso RCDATA) - o simbolo da sidebar
static IDirect3DTexture9* gLogoTrok = NULL;
static IDirect3DTexture9* gLogoBlogger = NULL; // marca do Blogger em silhueta (da pra tingir)
static IDirect3DTexture9* CarregarImagemRecurso(LPDIRECT3DDEVICE9 dev, int id, int maxLado) {
    HRSRC r = FindResourceA(NULL, MAKEINTRESOURCEA(id), (LPCSTR)RT_RCDATA);
    if (!r) return NULL;
    HGLOBAL hg = LoadResource(NULL, r);
    const unsigned char* p = (const unsigned char*)LockResource(hg);
    DWORD tam = SizeofResource(NULL, r);
    if (!p || !tam) return NULL;
    IStream* st = SHCreateMemStream(p, tam);
    if (!st) return NULL;
    IWICImagingFactory* fab = NULL;
    IWICBitmapDecoder* dec = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICBitmapSource* src = NULL;
    IDirect3DTexture9* tex = NULL;
    do {
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                    IID_IWICImagingFactory, (void**)&fab))) break;
        if (FAILED(fab->CreateDecoderFromStream(st, NULL, WICDecodeMetadataCacheOnDemand, &dec))) break;
        if (FAILED(dec->GetFrame(0, &frame))) break;
        IWICBitmapSource* fonte = frame;
        IWICBitmapScaler* escR = NULL;
        UINT ow = 0, oh = 0;
        frame->GetSize(&ow, &oh);
        if (maxLado > 0 && ow && oh && ((int)ow > maxLado || (int)oh > maxLado)) {
            // pre-escala com Fant: desenhar 512px num quadradinho de 32 sem isso = serrilhado
            UINT nw, nh;
            if (ow >= oh) { nw = maxLado; nh = (UINT)((float)oh * maxLado / ow); }
            else          { nh = maxLado; nw = (UINT)((float)ow * maxLado / oh); }
            if (nw < 1) nw = 1;
            if (nh < 1) nh = 1;
            if (SUCCEEDED(fab->CreateBitmapScaler(&escR)) &&
                SUCCEEDED(escR->Initialize(frame, nw, nh, WICBitmapInterpolationModeFant)))
                fonte = escR;
        }
        HRESULT hrConv = WICConvertBitmapSource(GUID_WICPixelFormat32bppBGRA, fonte, &src);
        if (escR) escR->Release();
        if (FAILED(hrConv)) break;
        UINT w = 0, h = 0;
        src->GetSize(&w, &h);
        if (!w || !h || w > 2048 || h > 2048) break;
        if (FAILED(dev->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL))) { tex = NULL; break; }
        D3DLOCKED_RECT lr;
        if (FAILED(tex->LockRect(0, &lr, NULL, 0))) { tex->Release(); tex = NULL; break; }
        src->CopyPixels(NULL, lr.Pitch, lr.Pitch * h, (BYTE*)lr.pBits);
        tex->UnlockRect(0);
    } while (0);
    if (src) src->Release();
    if (frame) frame->Release();
    if (dec) dec->Release();
    if (fab) fab->Release();
    st->Release();
    return tex;
}

// poe a imagem (resolucao original do arquivo) no clipboard como DIB - cola em qualquer app
static bool CopiarImagemClipboard(HWND hwnd, const char* caminho) {
    wchar_t w[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, caminho, -1, w, MAX_PATH);
    IWICImagingFactory* fab = NULL;
    IWICBitmapDecoder* dec = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICBitmapSource* src = NULL;
    bool ok = false;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                IID_IWICImagingFactory, (void**)&fab))) return false;
    do {
        if (FAILED(fab->CreateDecoderFromFilename(w, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec))) break;
        if (FAILED(dec->GetFrame(0, &frame))) break;
        if (FAILED(WICConvertBitmapSource(GUID_WICPixelFormat32bppBGRA, frame, &src))) break;
        UINT wpx = 0, hpx = 0;
        src->GetSize(&wpx, &hpx);
        if (!wpx || !hpx || wpx > 8192 || hpx > 8192) break;
        int stride = wpx * 4;
        BYTE* tmp = (BYTE*)malloc((size_t)stride * hpx);
        if (!tmp) break;
        if (FAILED(src->CopyPixels(NULL, stride, stride * hpx, tmp))) { free(tmp); break; }
        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + (size_t)stride * hpx);
        if (!hg) { free(tmp); break; }
        BITMAPINFOHEADER* bi = (BITMAPINFOHEADER*)GlobalLock(hg);
        memset(bi, 0, sizeof(*bi));
        bi->biSize = sizeof(*bi);
        bi->biWidth = wpx;
        bi->biHeight = hpx; // positivo = bottom-up: inverte as linhas
        bi->biPlanes = 1;
        bi->biBitCount = 32;
        bi->biCompression = BI_RGB;
        bi->biSizeImage = stride * hpx;
        BYTE* bits = (BYTE*)(bi + 1);
        for (UINT y = 0; y < hpx; y++)
            memcpy(bits + (size_t)(hpx - 1 - y) * stride, tmp + (size_t)y * stride, stride);
        GlobalUnlock(hg);
        free(tmp);
        if (OpenClipboard(hwnd)) {
            EmptyClipboard();
            ok = SetClipboardData(CF_DIB, hg) != NULL;
            CloseClipboard();
        }
        if (!ok) GlobalFree(hg);
    } while (0);
    if (src) src->Release();
    if (frame) frame->Release();
    if (dec) dec->Release();
    if (fab) fab->Release();
    return ok;
}

// ===================== capas padrao (pasta capas\ ao lado do exe) =====================
// nenhum favorito comeca sem imagem: sorteia uma capa embarcada, sem repetir enquanto
// houver capa livre (imagem escolhida A MAO pelo usuario mora em imagens\ e nao conta)
static void SortearCapaPadrao(char* out, int outsz) {
    out[0] = 0;
    char dir[MAX_PATH];
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    char* b = strrchr(dir, '\\');
    if (b) *b = 0;
    char busca[MAX_PATH + 16];
    _snprintf(busca, sizeof(busca) - 1, "%s\\capas\\*.jpg", dir);
    busca[sizeof(busca) - 1] = 0;
    char nomes[64][96];
    int n = 0;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(busca, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (n < 64) { strncpy(nomes[n], fd.cFileName, 95); nomes[n][95] = 0; n++; }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (n == 0) return;
    bool usada[64] = { false };
    for (int i = 0; i < gNumSrv + gNumDatas; i++) { // capas ja usadas em favoritos E datas
        const char* img = (i < gNumSrv) ? gSrv[i].img : gDatas[i - gNumSrv].img;
        if (!strstr(img, "\\capas\\")) continue;
        const char* nomeArq = strrchr(img, '\\');
        if (!nomeArq) continue;
        for (int k = 0; k < n; k++)
            if (_stricmp(nomeArq + 1, nomes[k]) == 0) { usada[k] = true; break; }
    }
    int livres[64], nl = 0;
    for (int k = 0; k < n; k++) if (!usada[k]) livres[nl++] = k;
    int esc = (nl > 0) ? livres[rand() % nl] : (rand() % n); // todas em uso: pode repetir
    _snprintf(out, outsz - 1, "%s\\capas\\%s", dir, nomes[esc]);
    out[outsz - 1] = 0;
}

static void SalvarServidores(); // definidas adiante
static void SalvarDatas();

// capas prontas mostradas nos modais de editar favorito/data (thumbs carregadas UMA vez;
// a versao mono e a previa de como a capa fica num card de DATA)
static int gNumCapasUI = 0;
static char gCapasArqUI[64][MAX_PATH];
static IDirect3DTexture9* gCapasTexUI[64];
static IDirect3DTexture9* gCapasTexUIMono[64];
static void EscurecerMonocromatico(IDirect3DTexture9* t); // definida logo abaixo
static void GarantirCapasUI() {
    if (gNumCapasUI > 0) return;
    char dir[MAX_PATH];
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    char* b = strrchr(dir, '\\');
    if (b) *b = 0;
    char busca[MAX_PATH + 16];
    _snprintf(busca, sizeof(busca) - 1, "%s\\capas\\*.jpg", dir);
    busca[sizeof(busca) - 1] = 0;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(busca, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (gNumCapasUI >= 64) break;
        _snprintf(gCapasArqUI[gNumCapasUI], MAX_PATH - 1, "%s\\capas\\%s", dir, fd.cFileName);
        gCapasArqUI[gNumCapasUI][MAX_PATH - 1] = 0;
        gCapasTexUI[gNumCapasUI] = CarregarImagemMax(gDev, gCapasArqUI[gNumCapasUI], 288);
        gCapasTexUIMono[gNumCapasUI] = CarregarImagemMax(gDev, gCapasArqUI[gNumCapasUI], 288);
        EscurecerMonocromatico(gCapasTexUIMono[gNumCapasUI]);
        gNumCapasUI++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

// capa padrao numa DATA: vira monocromatica escura (tom dos cards), sem gerar arquivo novo
static void EscurecerMonocromatico(IDirect3DTexture9* t) {
    if (!t) return;
    D3DSURFACE_DESC d2;
    if (FAILED(t->GetLevelDesc(0, &d2))) return;
    D3DLOCKED_RECT lr;
    if (FAILED(t->LockRect(0, &lr, NULL, 0))) return;
    for (UINT y = 0; y < d2.Height; y++) {
        unsigned char* px = (unsigned char*)lr.pBits + y * lr.Pitch;
        for (UINT x = 0; x < d2.Width; x++, px += 4) {
            int lum = (px[2] * 77 + px[1] * 150 + px[0] * 29) >> 8;
            int v = 14 + lum * 2 / 5; // preto ~14 ate cinza ~116: bem escuro
            px[0] = px[1] = px[2] = (unsigned char)v;
        }
    }
    t->UnlockRect(0);
}

static void CarregarImagensDatas(LPDIRECT3DDEVICE9 dev) {
    char dirExe[MAX_PATH];
    GetModuleFileNameA(NULL, dirExe, MAX_PATH);
    char* b = strrchr(dirExe, '\\');
    if (b) *b = 0;
    bool sorteouD = false;
    for (int i = 0; i < gNumDatas; i++) {
        if (!gDatas[i].img[0]) { // data sem imagem tambem ganha capa padrao (vira mono escura)
            SortearCapaPadrao(gDatas[i].img, sizeof(gDatas[i].img));
            if (gDatas[i].img[0]) sorteouD = true;
        }
        if (!gDatas[i].img[0] || gDatas[i].tex) continue;
        char full[MAX_PATH];
        if (gDatas[i].img[1] == ':' ) strncpy(full, gDatas[i].img, sizeof(full) - 1);
        else sprintf(full, "%s\\%s", dirExe, gDatas[i].img); // relativo ao launcher
        gDatas[i].tex = CarregarImagemMax(dev, full, 800); // card ~430px: 4K aqui e desperdicio
        if (strstr(gDatas[i].img, "\\capas\\")) EscurecerMonocromatico(gDatas[i].tex);
    }
    if (sorteouD) SalvarDatas();
    bool sorteou = false;
    for (int i = 0; i < gNumSrv; i++) { // imagens de exibicao e logos dos favoritos
        char full[MAX_PATH];
        if (!gSrv[i].img[0]) { // favorito antigo sem imagem: ganha capa padrao
            SortearCapaPadrao(gSrv[i].img, sizeof(gSrv[i].img));
            if (gSrv[i].img[0]) sorteou = true;
        }
        if (gSrv[i].img[0] && !gSrv[i].tex) {
            if (gSrv[i].img[1] == ':') strncpy(full, gSrv[i].img, sizeof(full) - 1);
            else sprintf(full, "%s\\%s", dirExe, gSrv[i].img);
            gSrv[i].tex = CarregarImagemMax(dev, full, 1600); // capa cobre a janela (max 1600)
        }
        if (gSrv[i].logo[0] && !gSrv[i].texLogo) {
            if (gSrv[i].logo[1] == ':') strncpy(full, gSrv[i].logo, sizeof(full) - 1);
            else sprintf(full, "%s\\%s", dirExe, gSrv[i].logo);
            gSrv[i].texLogo = CarregarImagemMax(dev, full, 640); // logo desenha com no maximo ~540px
        }
    }
    if (sorteou) SalvarServidores(); // capas sorteadas ficam gravadas
}

// ===================== query UDP SA-MP =====================
// pacote: 'SAMP' + ip(4) + porta(2 LE) + 'i'  ->  resposta: header de volta + dados do servidor

static bool QueryServidor(Servidor& s) {
    char ipOriginal[64], host[64];
    EnterCriticalSection(&gLock); // snapshot: o usuario pode reordenar/remover durante a query
    strncpy(ipOriginal, s.ip, sizeof(ipOriginal) - 1);
    ipOriginal[63] = 0;
    LeaveCriticalSection(&gLock);
    strncpy(host, ipOriginal, sizeof(host) - 1); host[63] = 0;
    char* dp = strchr(host, ':');
    int porta = 7777;
    if (dp) { *dp = 0; porta = atoi(dp + 1); }

    addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res) return false;
    sockaddr_in addr = *(sockaddr_in*)res->ai_addr;
    addr.sin_port = htons((u_short)porta);
    freeaddrinfo(res);

    SOCKET sk = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sk == INVALID_SOCKET) return false;
    DWORD tmo = 1200;
    setsockopt(sk, SOL_SOCKET, SO_RCVTIMEO, (char*)&tmo, sizeof(tmo));

    unsigned char pac[11];
    memcpy(pac, "SAMP", 4);
    memcpy(pac + 4, &addr.sin_addr.s_addr, 4);
    pac[8] = (unsigned char)(porta & 0xFF);
    pac[9] = (unsigned char)(porta >> 8);
    pac[10] = 'i';

    // UDP perde pacote a toa: ate 3 tentativas antes de dar o servidor como mudo
    // (o browser original do SA-MP faz o mesmo - 1 pacote so gera falso "offline")
    unsigned char buf[600];
    int r = -1;
    DWORD dt = 0;
    for (int tent = 0; tent < 3; tent++) {
        DWORD t0 = GetTickCount();
        sendto(sk, (char*)pac, 11, 0, (sockaddr*)&addr, sizeof(addr));
        r = recvfrom(sk, (char*)buf, sizeof(buf), 0, NULL, NULL);
        dt = GetTickCount() - t0;
        if (r >= 16 && memcmp(buf, "SAMP", 4) == 0) break;
        r = -1;
    }
    closesocket(sk);
    if (r < 16 || memcmp(buf, "SAMP", 4) != 0) return false;

    // resposta: [11 header]['i'? nao - o opcode ja vem no header de eco] password(1) players(2) max(2) hostlen(4) host...
    const unsigned char* p = buf + 11;
    const unsigned char* fim = buf + r;
    if (p + 5 > fim) return false;
    int temSenha = p[0]; p += 1; // password
    int online = p[0] | (p[1] << 8); p += 2;
    int maxp   = p[0] | (p[1] << 8); p += 2;
    if (p + 4 > fim) return false;
    int hlen = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); p += 4;
    if (hlen < 0 || hlen > 512) return false; // tamanho maluco = pacote invalido (evita aritmetica de ponteiro UB)
    char hn[96] = "", gm[64] = "";
    if (hlen > 0 && p + hlen <= fim) {
        int c = hlen < 95 ? hlen : 95;
        memcpy(hn, p, c); hn[c] = 0; p += hlen;
    }
    if (p + 4 <= fim) {
        int glen = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); p += 4;
        if (glen < 0 || glen > 512) glen = 0;
        if (glen > 0 && p + glen <= fim) {
            int c = glen < 63 ? glen : 63;
            memcpy(gm, p, c); gm[c] = 0;
        }
    }
    EnterCriticalSection(&gLock);
    if (_stricmp(s.ip, ipOriginal) == 0) { // so grava se o slot ainda e o MESMO servidor
        s.online = online; s.maxp = maxp; s.ping = (int)dt; s.senha = temSenha ? 1 : 0;
        if (hn[0]) strncpy(s.hostnameQ, hn, sizeof(s.hostnameQ) - 1);
        if (gm[0]) strncpy(s.modoQ, gm, sizeof(s.modoQ) - 1);
    }
    LeaveCriticalSection(&gLock);
    return true;
}

static DWORD WINAPI ThreadQueryUm(LPVOID p) {
    int i = (int)(INT_PTR)p;
    if (i >= 0 && i < gNumSrv) QueryServidor(gSrv[i]);
    return 0;
}

static DWORD WINAPI ThreadQuery(LPVOID) {
    while (gRodando) {
        // uma thread por favorito: 16 servidores fora do ar respondem em ~1,5s, nao 24s
        HANDLE hs[MAX_SERVIDORES];
        int n = 0;
        for (int i = 0; i < gNumSrv && i < MAX_SERVIDORES; i++) {
            HANDLE h = CreateThread(NULL, 0, ThreadQueryUm, (LPVOID)(INT_PTR)i, 0, NULL);
            if (h) hs[n++] = h; else QueryServidor(gSrv[i]);
        }
        for (int i = 0; i < n; i++) { WaitForSingleObject(hs[i], 6000); CloseHandle(hs[i]); } // 3 tentativas x 1,2s cabem
        for (int t = 0; t < 300 && gRodando; t++) Sleep(100); // ~30 s entre rodadas
    }
    return 0;
}

// ===================== nick: o registro do SA-MP e a fonte da verdade =====================
// (mesmo valor que o launcher original usa - mudou aqui, muda la, e vice-versa)

static void LerNickRegistro() {
    HKEY k;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\SAMP", 0, KEY_QUERY_VALUE, &k) == ERROR_SUCCESS) {
        char v[32] = "";
        DWORD tam = sizeof(v), tipo;
        if (RegQueryValueExA(k, "PlayerName", 0, &tipo, (BYTE*)v, &tam) == ERROR_SUCCESS && v[0])
            strncpy(gNick, v, sizeof(gNick) - 1);
        RegCloseKey(k);
    }
}

static void GravarNickRegistro() {
    HKEY k;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\SAMP", 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(k, "PlayerName", 0, REG_SZ, (BYTE*)gNick, (DWORD)strlen(gNick) + 1);
        RegCloseKey(k);
    }
}

// ===================== iniciar com o windows (chave Run do usuario) =====================

static bool IniciarComWindowsAtivo() {
    HKEY k;
    bool tem = false;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &k) == ERROR_SUCCESS) {
        char v[MAX_PATH * 2] = "";
        DWORD tam = sizeof(v), tipo;
        tem = RegQueryValueExA(k, "Trok Launcher", 0, &tipo, (BYTE*)v, &tam) == ERROR_SUCCESS && v[0];
        RegCloseKey(k);
    }
    return tem;
}

static void DefinirIniciarComWindows(bool ativo) {
    HKEY k;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) != ERROR_SUCCESS) return;
    if (ativo) {
        char exe[MAX_PATH], linha[MAX_PATH + 4];
        GetModuleFileNameA(NULL, exe, MAX_PATH);
        sprintf(linha, "\"%s\"", exe);
        RegSetValueExA(k, "Trok Launcher", 0, REG_SZ, (BYTE*)linha, (DWORD)strlen(linha) + 1);
    } else {
        RegDeleteValueA(k, "Trok Launcher");
    }
    RegCloseKey(k);
}

// ===================== lista publica (https://api.open.mp/servers) =====================

static bool PegaStr(const char* o, int n, const char* chave, char* out, int outsz) {
    char busca[32];
    int bl = sprintf(busca, "\"%s\":\"", chave);
    for (int i = 0; i + bl < n; i++) {
        if (memcmp(o + i, busca, bl) != 0) continue;
        int p = i + bl, w = 0;
        while (p < n && o[p] != '"' && w < outsz - 1) {
            char c2 = o[p];
            if (c2 == '\\' && p + 1 < n) { // escapes JSON de verdade (\uXXXX inclusive)
                p++;
                char e = o[p];
                if (e == 'u' && p + 4 < n) {
                    int cp = 0;
                    for (int h = 1; h <= 4; h++) {
                        char hc = o[p + h];
                        cp <<= 4;
                        if (hc >= '0' && hc <= '9') cp |= hc - '0';
                        else if (hc >= 'a' && hc <= 'f') cp |= hc - 'a' + 10;
                        else if (hc >= 'A' && hc <= 'F') cp |= hc - 'A' + 10;
                    }
                    p += 5;
                    out[w++] = (cp > 31 && cp < 256) ? (char)cp : '?';
                    continue;
                }
                if (e == 'n' || e == 't' || e == 'r') { out[w++] = ' '; p++; continue; }
                out[w++] = e;
                p++;
                continue;
            }
            out[w++] = c2;
            p++;
        }
        out[w] = 0;
        return true;
    }
    return false;
}

static int PegaInt(const char* o, int n, const char* chave) {
    char busca[32];
    int bl = sprintf(busca, "\"%s\":", chave);
    for (int i = 0; i + bl < n; i++) {
        if (memcmp(o + i, busca, bl) != 0) continue;
        return atoi(o + i + bl);
    }
    return 0;
}

static int PegaBool(const char* o, int n, const char* chave) {
    char busca[32];
    int bl = sprintf(busca, "\"%s\":", chave);
    for (int i = 0; i + bl < n; i++) {
        if (memcmp(o + i, busca, bl) != 0) continue;
        return o[i + bl] == 't' ? 1 : 0;
    }
    return 0;
}

static DWORD WINAPI ThreadPublicos(LPVOID) {
    gPubEstado = 1;
    HINTERNET h = InternetOpenA("TrokLauncher/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!h) { gPubEstado = -1; return 0; }
    HINTERNET u = InternetOpenUrlA(h, "https://api.open.mp/servers", NULL, 0,
        INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!u) { InternetCloseHandle(h); gPubEstado = -1; return 0; }
    int cap = 1 << 20, tam = 0;
    char* js = (char*)malloc(cap);
    DWORD lidos = 0;
    while (js && InternetReadFile(u, js + tam, cap - tam - 1, &lidos) && lidos > 0) {
        tam += (int)lidos;
        if (cap - tam < 65536) {
            if (cap >= (8 << 20)) break; // teto de 8 MB: resposta maior que isso nao e a masterlist
            cap *= 2;
            char* nv = (char*)realloc(js, cap);
            if (!nv) break;
            js = nv;
        }
    }
    InternetCloseHandle(u);
    InternetCloseHandle(h);
    if (!js || tam < 64) { free(js); gPubEstado = -1; return 0; }
    js[tam] = 0;
    // percorre objetos de nivel 1 do array raiz (respeitando strings e escapes)
    int prof = 0, ini = -1, cont = 0;
    bool str = false;
    for (int i = 0; i < tam && cont < MAX_PUB; i++) {
        char c = js[i];
        if (str) { if (c == '\\') i++; else if (c == '"') str = false; continue; }
        if (c == '"') { str = true; continue; }
        if (c == '{') { if (++prof == 1) ini = i; }
        else if (c == '}') {
            if (prof-- == 1 && ini >= 0) {
                SrvPub& s = gPub[cont];
                memset(&s, 0, sizeof(s));
                int len = i - ini + 1;
                if (PegaStr(js + ini, len, "ip", s.ip, sizeof(s.ip))) {
                    if (!PegaStr(js + ini, len, "hn", s.nome, sizeof(s.nome)) || !s.nome[0])
                        strncpy(s.nome, s.ip, sizeof(s.nome) - 1);
                    PegaStr(js + ini, len, "gm", s.modo, sizeof(s.modo));
                    s.on = PegaInt(js + ini, len, "pc");
                    s.maxp = PegaInt(js + ini, len, "pm");
                    s.pw = PegaBool(js + ini, len, "pa");
                    if (!strchr(s.ip, ':')) strcat(s.ip, ":7777");
                    cont++;
                }
                ini = -1;
            }
        }
    }
    free(js);
    // mais cheios primeiro
    qsort(gPub, cont, sizeof(SrvPub), [](const void* a, const void* b) {
        return ((const SrvPub*)b)->on - ((const SrvPub*)a)->on;
    });
    gNumPub = cont;
    gPubEstado = cont > 0 ? 2 : -1;
    return 0;
}

// ===================== thread de imagens (decodifica FORA da UI - sem engasgo) =====================
// A UI pede (PedirImagem) e segue desenhando; a thread decodifica e devolve a textura pronta,
// que a UI recolhe no comeco do frame (ReceberImagens). Exige device D3DCREATE_MULTITHREADED.

struct JobImg { char caminho[MAX_PATH]; int maxLado; int tipo; }; // tipo: 0 = thumb da galeria, 1 = viewer
struct ResImg { char caminho[MAX_PATH]; int tipo; IDirect3DTexture9* tex; };
static JobImg gJobsImg[32];
static ResImg gResImg[32];
static volatile int gNumJobsImg = 0, gNumResImg = 0;
static CRITICAL_SECTION gLockImg;
static HANDLE gEvImg = NULL;

// cache de thumbs EM DISCO: as screens do F8 sao PNG 1920x1080 (decode caro, 30-60ms cada);
// geramos um jpeg de 288px UMA vez e dai em diante a galeria le arquivinhos de ~15KB (~2ms)
static void CaminhoThumbCache(const char* origem, char* out, int outsz) {
    char dir[MAX_PATH];
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    char* b = strrchr(dir, '\\');
    if (b) *b = 0;
    unsigned int h = 2166136261u; // FNV-1a do caminho
    for (const char* c = origem; *c; c++) { h ^= (unsigned char)tolower((unsigned char)*c); h *= 16777619u; }
    unsigned int marca = 0; // tamanho+data no NOME: arquivo mudou = cache novo sozinho
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (GetFileAttributesExA(origem, GetFileExInfoStandard, &fa))
        marca = fa.nFileSizeLow ^ fa.ftLastWriteTime.dwLowDateTime;
    char pasta[MAX_PATH];
    _snprintf(pasta, MAX_PATH - 1, "%s\\cache", dir);
    pasta[MAX_PATH - 1] = 0;
    CreateDirectoryA(pasta, NULL);
    _snprintf(out, outsz - 1, "%s\\%08x_%08x.jpg", pasta, h, marca);
    out[outsz - 1] = 0;
}

static bool GerarThumbDisco(const char* origem, const char* destino, int maxLado) {
    wchar_t wo[MAX_PATH], wd[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, origem, -1, wo, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, destino, -1, wd, MAX_PATH);
    IWICImagingFactory* fab = NULL;
    IWICBitmapDecoder* dec = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICBitmapScaler* esc = NULL;
    IWICFormatConverter* conv = NULL;
    IWICStream* st = NULL;
    IWICBitmapEncoder* enc = NULL;
    IWICBitmapFrameEncode* fe = NULL;
    IPropertyBag2* pb = NULL;
    bool ok = false;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                IID_IWICImagingFactory, (void**)&fab))) return false;
    do {
        if (FAILED(fab->CreateDecoderFromFilename(wo, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec))) break;
        if (FAILED(dec->GetFrame(0, &frame))) break;
        UINT ow = 0, oh = 0;
        frame->GetSize(&ow, &oh);
        if (!ow || !oh) break;
        UINT nw = ow, nh = oh;
        if ((int)ow > maxLado || (int)oh > maxLado) {
            if (ow >= oh) { nw = maxLado; nh = (UINT)((float)oh * maxLado / ow); }
            else          { nh = maxLado; nw = (UINT)((float)ow * maxLado / oh); }
            if (nw < 1) nw = 1;
            if (nh < 1) nh = 1;
        }
        if (FAILED(fab->CreateBitmapScaler(&esc))) break;
        if (FAILED(esc->Initialize(frame, nw, nh, WICBitmapInterpolationModeLinear))) break;
        if (FAILED(fab->CreateFormatConverter(&conv))) break;
        if (FAILED(conv->Initialize(esc, GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom))) break;
        if (FAILED(fab->CreateStream(&st))) break;
        if (FAILED(st->InitializeFromFilename(wd, GENERIC_WRITE))) break;
        if (FAILED(fab->CreateEncoder(GUID_ContainerFormatJpeg, NULL, &enc))) break;
        if (FAILED(enc->Initialize(st, WICBitmapEncoderNoCache))) break;
        if (FAILED(enc->CreateNewFrame(&fe, &pb))) break;
        if (FAILED(fe->Initialize(pb))) break;
        if (FAILED(fe->WriteSource(conv, NULL))) break;
        if (FAILED(fe->Commit())) break;
        if (FAILED(enc->Commit())) break;
        ok = true;
    } while (0);
    if (pb) pb->Release();
    if (fe) fe->Release();
    if (enc) enc->Release();
    if (st) st->Release();
    if (conv) conv->Release();
    if (esc) esc->Release();
    if (frame) frame->Release();
    if (dec) dec->Release();
    if (fab) fab->Release();
    if (!ok) DeleteFileA(destino); // meio-arquivo nao vale como cache
    return ok;
}

static bool PedirImagem(const char* caminho, int maxLado, int tipo) { // false = fila cheia (tente no proximo frame)
    EnterCriticalSection(&gLockImg);
    bool ja = false, entrou = false;
    for (int i = 0; i < gNumJobsImg && !ja; i++)
        if (gJobsImg[i].tipo == tipo && _stricmp(gJobsImg[i].caminho, caminho) == 0) ja = true;
    if (!ja && gNumJobsImg < 32) {
        JobImg& j = gJobsImg[gNumJobsImg++];
        strncpy(j.caminho, caminho, MAX_PATH - 1);
        j.caminho[MAX_PATH - 1] = 0;
        j.maxLado = maxLado;
        j.tipo = tipo;
        entrou = true;
    }
    LeaveCriticalSection(&gLockImg);
    if (gEvImg) SetEvent(gEvImg);
    return ja || entrou;
}

static DWORD WINAPI ThreadImagens(LPVOID) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED); // WIC precisa de COM NESTA thread
    while (gRodando) {
        WaitForSingleObject(gEvImg, 500);
        for (;;) {
            JobImg j;
            EnterCriticalSection(&gLockImg);
            if (gNumJobsImg == 0) { LeaveCriticalSection(&gLockImg); break; }
            j = gJobsImg[0];
            memmove(gJobsImg, gJobsImg + 1, sizeof(JobImg) * (gNumJobsImg - 1));
            gNumJobsImg--;
            LeaveCriticalSection(&gLockImg);
            IDirect3DTexture9* t = NULL;
            if (j.tipo == 0) { // thumb: passa pelo cache em disco
                char thumb[MAX_PATH];
                CaminhoThumbCache(j.caminho, thumb, sizeof(thumb));
                if (GetFileAttributesA(thumb) == INVALID_FILE_ATTRIBUTES)
                    GerarThumbDisco(j.caminho, thumb, j.maxLado);
                t = CarregarImagemMax(gDev, thumb, j.maxLado);
                if (!t) t = CarregarImagemMax(gDev, j.caminho, j.maxLado); // cache falhou: original
            } else {
                t = CarregarImagemMax(gDev, j.caminho, j.maxLado);
            }
            EnterCriticalSection(&gLockImg);
            if (gNumResImg < 32) {
                ResImg& r = gResImg[gNumResImg++];
                strncpy(r.caminho, j.caminho, MAX_PATH - 1);
                r.caminho[MAX_PATH - 1] = 0;
                r.tipo = j.tipo;
                r.tex = t;
            } else if (t) t->Release();
            LeaveCriticalSection(&gLockImg);
        }
    }
    CoUninitialize();
    return 0;
}

// --- cache de full-res do visualizador (atual + vizinhas) ---
static FGSlot* FGSlotDe(const char* caminho) {
    for (int i = 0; i < N_FG; i++)
        if (gFG[i].caminho[0] && _stricmp(gFG[i].caminho, caminho) == 0) return &gFG[i];
    return NULL;
}

static void FGGarantir(const char* caminho) { // reserva um slot e pede o decode na thread
    FGSlot* ja = FGSlotDe(caminho);
    if (ja) { // decode falhou antes? tenta mais 2x (arquivo pode estar em gravacao/OneDrive)
        if (!ja->tex && !ja->pedida && ja->falhas > 0 && ja->falhas < 3) {
            if (PedirImagem(caminho, 1280, 1)) ja->pedida = true;
        }
        return;
    }
    for (int i = 0; i < N_FG; i++)
        if (!gFG[i].caminho[0]) {
            if (!PedirImagem(caminho, 1280, 1)) return; // fila cheia: slot livre p/ tentar depois
            strncpy(gFG[i].caminho, caminho, MAX_PATH - 1);
            gFG[i].caminho[MAX_PATH - 1] = 0;
            gFG[i].tex = NULL;
            gFG[i].pedida = true;
            gFG[i].falhas = 0;
            return;
        }
}

static void FGPodar() { // solta o que nao e mais a foto aberta nem vizinha dela
    for (int i = 0; i < N_FG; i++) {
        if (!gFG[i].caminho[0]) continue;
        bool fica = false;
        for (int d = -1; d <= 1 && !fica; d++) {
            int k = gFotoVista + d;
            if (k >= 0 && k < gNumFotos && _stricmp(gFotos[k].caminho, gFG[i].caminho) == 0) fica = true;
        }
        if (!fica) {
            AdiarRelease(gFG[i].tex);
            gFG[i].tex = NULL;
            gFG[i].caminho[0] = 0;
            gFG[i].pedida = false;
        }
    }
}

static void FGLimpar() {
    for (int i = 0; i < N_FG; i++) {
        AdiarRelease(gFG[i].tex);
        gFG[i].tex = NULL;
        gFG[i].caminho[0] = 0;
        gFG[i].pedida = false;
    }
    gFGFadeIdx = -1;
}

// roda no comeco do frame: entrega as texturas prontas a quem pediu (roteia pelo CAMINHO,
// porque a lista pode ter mudado - excluir/reescanear - enquanto a thread trabalhava)
static void ReceberImagens() {
    EnterCriticalSection(&gLockImg);
    for (int i = 0; i < gNumResImg; i++) {
        ResImg& r = gResImg[i];
        bool usado = false;
        if (r.tipo == 0) {
            for (int k = 0; k < gNumFotos; k++)
                if (!gFotos[k].tex && _stricmp(gFotos[k].caminho, r.caminho) == 0) {
                    if (r.tex) { gFotos[k].tex = r.tex; usado = true; }
                    else gFotos[k].falhou = true; // corrompida: nao re-decodifica em loop
                    break;
                }
        } else {
            FGSlot* s = FGSlotDe(r.caminho);
            if (s && !s->tex) {
                if (r.tex) { s->tex = r.tex; usado = true; }
                else s->falhas++; // FGGarantir tenta de novo (max 3)
                s->pedida = false;
            }
        }
        if (!usado && r.tex) AdiarRelease(r.tex); // chegou tarde (foto excluida/trocada)
    }
    gNumResImg = 0;
    LeaveCriticalSection(&gLockImg);
}

// ===================== avatares por imagem (pasta avatars\ ao lado do exe) =====================

static int CmpAvatar(const void* A, const void* B) {
    return CmpNatural(((const AvatarImg*)A)->arquivo, ((const AvatarImg*)B)->arquivo); // 2 antes de 10
}

static void CarregarAvatares(LPDIRECT3DDEVICE9 dev) {
    char dir[MAX_PATH];
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    char* b = strrchr(dir, '\\');
    if (b) *b = 0;
    char pasta[MAX_PATH];
    _snprintf(pasta, sizeof(pasta) - 1, "%s\\avatars", dir);
    CreateDirectoryA(pasta, NULL); // garante a pasta (o usuario so precisa soltar os png nela)
    gNumAvatares = 0;
    static const char* EXT[2] = { "*.png", "*.jpg" };
    for (int e = 0; e < 2; e++) {
        char busca[MAX_PATH + 8];
        _snprintf(busca, sizeof(busca) - 1, "%s\\%s", pasta, EXT[e]);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(busca, &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (gNumAvatares >= MAX_AVATARES) break;
            AvatarImg& av = gAvatares[gNumAvatares++];
            strncpy(av.arquivo, fd.cFileName, sizeof(av.arquivo) - 1);
            av.arquivo[sizeof(av.arquivo) - 1] = 0;
            av.tex = NULL;
        } while (FindNextFileA(h, &fd) && gNumAvatares < MAX_AVATARES);
        FindClose(h);
    }
    qsort(gAvatares, gNumAvatares, sizeof(AvatarImg), CmpAvatar);
    for (int i = 0; i < gNumAvatares; i++) {
        char cam[MAX_PATH + 72];
        _snprintf(cam, sizeof(cam) - 1, "%s\\%s", pasta, gAvatares[i].arquivo);
        gAvatares[i].tex = CarregarImagemMax(dev, cam, 256);
    }
}

static int AvatarIdx(const char* arquivo) {
    if (!arquivo || !arquivo[0]) return -1;
    for (int i = 0; i < gNumAvatares; i++)
        if (_stricmp(gAvatares[i].arquivo, arquivo) == 0) return i;
    return -1;
}

// ===================== atualizacao do launcher (arquivo de versao hospedado) =====================

// wininet recusa file:// em varias maquinas (ERROR_INVALID_NAME): os exes de amostra
// usam URL local, entao file:// vira caminho e e lido direto do disco
static bool UrlParaCaminhoLocal(const char* url, char* out, int outsz) {
    if (_strnicmp(url, "file://", 7) != 0) return false;
    const char* p = url + 7;
    while (*p == '/') p++; // file:///C:/... -> C:/...
    int n = 0;
    while (*p && n < outsz - 1) {
        char c = *p++;
        if (c == '%' && p[0] && p[1]) { // %20 e afins
            char hx[3] = { p[0], p[1], 0 };
            c = (char)strtol(hx, NULL, 16);
            p += 2;
        }
        if (c == '/') c = '\\';
        out[n++] = c;
    }
    out[n] = 0;
    return true;
}

static DWORD WINAPI ThreadAtualizacao(LPVOID) {
    char txt[4096];
    DWORD tam = 0;
    char loc[MAX_PATH];
    if (UrlParaCaminhoLocal(URL_VERSAO, loc, MAX_PATH)) {
        HANDLE f = CreateFileA(loc, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (f == INVALID_HANDLE_VALUE) return 0;
        ReadFile(f, txt, sizeof(txt) - 1, &tam, NULL);
        CloseHandle(f);
    } else {
        HINTERNET h = InternetOpenA("TrokLauncher/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!h) return 0;
        DWORD flagsV = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
        if (_strnicmp(URL_VERSAO, "https", 5) == 0) flagsV |= INTERNET_FLAG_SECURE;
        HINTERNET u = InternetOpenUrlA(h, URL_VERSAO, NULL, 0, flagsV, 0);
        if (!u) { InternetCloseHandle(h); return 0; }
        DWORD lidos = 0;
        while (tam < sizeof(txt) - 1 && InternetReadFile(u, txt + tam, sizeof(txt) - 1 - tam, &lidos) && lidos > 0)
            tam += lidos;
        InternetCloseHandle(u);
        InternetCloseHandle(h);
    }
    txt[tam] = 0;
    if (tam < 3) return 0;
    // linha 1 = versao, linha 2 = link, resto = novidades
    char* ctx = txt;
    char* l1 = ctx; char* nl = strchr(ctx, '\n');
    if (!nl) return 0;
    *nl = 0; ctx = nl + 1;
    char* l2 = ctx; nl = strchr(ctx, '\n');
    if (nl) { *nl = 0; ctx = nl + 1; } else ctx = ctx + strlen(ctx);
    for (char* c = l1; *c; c++) if (*c == '\r') *c = 0;
    for (char* c = l2; *c; c++) if (*c == '\r') *c = 0;
    if (!l1[0] || _stricmp(l1, VERSAO) == 0) return 0; // ja esta na ultima
    strncpy(gAttVersao, l1, sizeof(gAttVersao) - 1);
    strncpy(gAttUrl, l2, sizeof(gAttUrl) - 1);
    strncpy(gAttNotas, ctx, sizeof(gAttNotas) - 1);
    gAttEstado = 1;
    return 0;
}

// onde o setup novo e gravado. NAO usamos %TEMP%: exe rodando da pasta temporaria e o
// padrao classico de dropper e faz antivirus heuristico (ex: Bearfoos.A!ml) marcar falso
// positivo. Pasta propria do app, ao lado da instalacao, e o caminho "de gente honesta".
static void CaminhoUpdate(char* out) {
    char base[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, base))) {
        _snprintf(out, MAX_PATH - 1, "%s\\Trok Launcher", base);
        CreateDirectoryA(out, NULL);
        _snprintf(out, MAX_PATH - 1, "%s\\Trok Launcher\\update", base);
        CreateDirectoryA(out, NULL);
        strncat(out, "\\TrokLauncher-Setup.exe", MAX_PATH - strlen(out) - 1);
    } else {
        GetTempPathA(MAX_PATH, out); // ultimo recurso
        strncat(out, "TrokLauncher-Setup.exe", MAX_PATH - strlen(out) - 1);
    }
    out[MAX_PATH - 1] = 0;
}

// baixa o setup novo SEM sair do launcher (barra de progresso no modal); quando termina,
// o frame seguinte roda o setup em modo --atualizar (fecha, troca os arquivos e reabre)
static DWORD WINAPI ThreadBaixarUpdate(LPVOID) {
    gAttPct = 0;
    char locB[MAX_PATH];
    if (UrlParaCaminhoLocal(gAttUrl, locB, MAX_PATH)) { // amostra: "baixa" copiando do disco
        char tmpL[MAX_PATH];
        CaminhoUpdate(tmpL);
        if (CopyFileA(locB, tmpL, FALSE)) {
            HANDLE v = CreateFileA(tmpL, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            char mzL[2] = { 0, 0 };
            DWORD lL = 0;
            if (v != INVALID_HANDLE_VALUE) { ReadFile(v, mzL, 2, &lL, NULL); CloseHandle(v); }
            if (lL == 2 && mzL[0] == 'M' && mzL[1] == 'Z') {
                Sleep(900); // da tempo de VER a barra no demo
                strncpy(gAttArquivo, tmpL, MAX_PATH - 1);
                gAttPct = 100;
                gAttBaixa = 3;
                return 0;
            }
        }
        gAttBaixa = 2;
        return 0;
    }
    HINTERNET h = InternetOpenA("TrokLauncher/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!h) { gAttBaixa = 2; return 0; }
    DWORD flagsB = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (_strnicmp(gAttUrl, "https", 5) == 0) flagsB |= INTERNET_FLAG_SECURE;
    HINTERNET u = InternetOpenUrlA(h, gAttUrl, NULL, 0, flagsB, 0);
    if (!u) { InternetCloseHandle(h); gAttBaixa = 2; return 0; }
    DWORD total = 0, tt = sizeof(total), idx = 0;
    if (!HttpQueryInfoA(u, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &total, &tt, &idx)) total = 0;
    char tmp[MAX_PATH];
    CaminhoUpdate(tmp);
    HANDLE f = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { InternetCloseHandle(u); InternetCloseHandle(h); gAttBaixa = 2; return 0; }
    static char buf[65536];
    DWORD lidos = 0, soma = 0;
    bool ok = true;
    const DWORD CAP = 30u * 1024u * 1024u; // trava de seguranca: setup jamais passa de 30MB
    while (InternetReadFile(u, buf, sizeof(buf), &lidos) && lidos > 0) {
        DWORD esc = 0;
        if (!WriteFile(f, buf, lidos, &esc, NULL) || esc != lidos) { ok = false; break; }
        soma += lidos;
        if (soma > CAP) { ok = false; break; }
        gAttPct = total ? (int)((unsigned long long)soma * 100 / total) : -1;
    }
    CloseHandle(f);
    InternetCloseHandle(u);
    InternetCloseHandle(h);
    if (ok && soma > 200000) { // exe de verdade? (MZ) - nao roda pagina de erro de CDN
        HANDLE v = CreateFileA(tmp, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        char mz[2] = { 0, 0 };
        DWORD l2 = 0;
        if (v != INVALID_HANDLE_VALUE) { ReadFile(v, mz, 2, &l2, NULL); CloseHandle(v); }
        if (l2 == 2 && mz[0] == 'M' && mz[1] == 'Z') {
            strncpy(gAttArquivo, tmp, MAX_PATH - 1);
            gAttPct = 100;
            gAttBaixa = 3;
            return 0;
        }
    }
    DeleteFileA(tmp);
    gAttBaixa = 2;
    return 0;
}

// ===================== aba mods: feed RSS do blog (Blogger) =====================

static void ExtrairTagXml(const char* bloco, const char* tag, char* out, int outsz) {
    out[0] = 0;
    char abre[32], fecha[32];
    _snprintf(abre, sizeof(abre) - 1, "<%s>", tag);
    _snprintf(fecha, sizeof(fecha) - 1, "</%s>", tag);
    const char* a = strstr(bloco, abre);
    if (!a) return;
    a += strlen(abre);
    const char* f = strstr(a, fecha);
    if (!f) return;
    if (_strnicmp(a, "<![CDATA[", 9) == 0) { // Blogger costuma embrulhar o titulo em CDATA
        a += 9;
        const char* fc = strstr(a, "]]>");
        if (fc && fc < f) f = fc;
    }
    int n = (int)(f - a);
    if (n > outsz - 1) n = outsz - 1;
    memcpy(out, a, n);
    out[n] = 0;
}

static void DecodificarEntidades(char* s) { // &amp; &lt; &gt; &quot; &#39;
    char* w = s;
    for (char* p = s; *p; ) {
        if (*p == '&') {
            if (_strnicmp(p, "&amp;", 5) == 0) { *w++ = '&'; p += 5; continue; }
            if (_strnicmp(p, "&lt;", 4) == 0) { *w++ = '<'; p += 4; continue; }
            if (_strnicmp(p, "&gt;", 4) == 0) { *w++ = '>'; p += 4; continue; }
            if (_strnicmp(p, "&quot;", 6) == 0) { *w++ = '"'; p += 6; continue; }
            if (_strnicmp(p, "&#39;", 5) == 0) { *w++ = '\''; p += 5; continue; }
        }
        *w++ = *p++;
    }
    *w = 0;
}

static void DirDoExe(char* out, int outsz); // definida na secao de backup

// desenha texto cortado com "..." quando nao cabe na largura (mede com a fonte ATIVA)
// corta o texto com reticencias pra caber em maxW (mede com a fonte em uso)
static void TruncarPara(char* dst, size_t n, const char* txt, float maxW) {
    strncpy(dst, txt, n - 1);
    dst[n - 1] = 0;
    if (ImGui::CalcTextSize(dst).x <= maxW) return;
    float w3 = ImGui::CalcTextSize("...").x;
    int len = (int)strlen(dst);
    while (len > 0) {
        dst[--len] = 0;
        if (ImGui::CalcTextSize(dst).x + w3 <= maxW) break;
    }
    while (len > 0 && dst[len - 1] == ' ') dst[--len] = 0; // sem espaco solto antes do "..."
    strncat(dst, "...", n - strlen(dst) - 1);
}

static void TextoTruncado(ImDrawList* d, ImVec2 pos, float maxW, ImU32 cor, const char* txt) {
    char buf[224];
    TruncarPara(buf, sizeof(buf), txt, maxW);
    d->AddText(pos, cor, buf);
}

// quebra o titulo em ate 2 linhas que caibam em maxW (a segunda ganha "..." se sobrar)
static int TextoDuasLinhas(char* l1, char* l2, size_t n, const char* txt, float maxW) {
    l1[0] = l2[0] = 0;
    if (!txt || !txt[0]) return 0;
    if (ImGui::CalcTextSize(txt).x <= maxW) { strncpy(l1, txt, n - 1); l1[n - 1] = 0; return 1; }
    char buf[224];
    strncpy(buf, txt, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    int corte = 0;
    for (int k = 0; buf[k]; k++) {          // maior pedaco que cabe, cortando num espaco
        if (buf[k] != ' ') continue;
        buf[k] = 0;
        bool cabe = ImGui::CalcTextSize(buf).x <= maxW;
        buf[k] = ' ';
        if (cabe) corte = k; else break;
    }
    if (corte <= 0) { TruncarPara(l1, n, txt, maxW); return 1; }
    int c2 = corte < (int)n - 1 ? corte : (int)n - 1;
    memcpy(l1, txt, c2);
    l1[c2] = 0;
    TruncarPara(l2, n, txt + corte + 1, maxW);
    return 2;
}

// mesma coisa, mas centrado em cx (o card do blog centraliza titulo, data e botao)
static void TextoTruncadoCentro(ImDrawList* d, float cx, float y, float maxW, ImU32 cor, const char* txt) {
    if (!txt || !txt[0]) return;
    char buf[224];
    TruncarPara(buf, sizeof(buf), txt, maxW);
    d->AddText(ImVec2(cx - ImGui::CalcTextSize(buf).x * 0.5f, y), cor, buf);
}

static void ExtrairResumoHtml(const char* html, char* out, int outsz) { // tira tags, achata espacos
    int w = 0;
    bool dentro = false, espaco = true;
    for (const char* p = html; *p && w < outsz - 1; p++) {
        if (*p == '<') { dentro = true; continue; }
        if (*p == '>') { dentro = false; if (!espaco && w < outsz - 1) { out[w++] = ' '; espaco = true; } continue; }
        if (dentro) continue;
        char c = *p;
        if (c == '\r' || c == '\n' || c == '\t') c = ' ';
        if (c == ' ' && espaco) continue;
        espaco = (c == ' ');
        out[w++] = c;
    }
    out[w] = 0;
}

// baixa a capa do post UMA vez pro cache\ (nome pelo hash da url: nunca re-baixa)
// A URL do Blogger tem um segmento de tamanho logo antes do nome do arquivo
// (".../s1280-c/foto.jpg", ".../w1280/foto.jpg"). Dependendo do que o post usou, vem
// quadrado - e ai a capa do card aparece ampliada. Troca sempre por um 16:9 exato.
static void CapaBlogger16x9(char* url, int cap) {
    char* barra = strrchr(url, '/');
    if (!barra || barra == url) return;
    *barra = 0;
    char* ini = strrchr(url, '/');
    *barra = '/';
    if (!ini) return;
    ini++;                                    // inicio do segmento de tamanho
    if (!(*ini == 's' || *ini == 'w') || ini[1] < '0' || ini[1] > '9') return;
    char resto[400];
    strncpy(resto, barra, sizeof(resto) - 1); // "/foto.jpg"
    resto[sizeof(resto) - 1] = 0;
    if ((int)(ini - url) + 12 + (int)strlen(resto) >= cap) return;
    strcpy(ini, "w640-h360-c");
    strcat(ini, resto);
}

static void BaixarImagemMods(const char* url, char* outCache, int outSz) {
    outCache[0] = 0;
    if (_strnicmp(url, "http", 4) != 0) return;
    char dir[MAX_PATH];
    DirDoExe(dir, sizeof(dir));
    char pasta[MAX_PATH];
    _snprintf(pasta, sizeof(pasta) - 1, "%s\\cache", dir);
    pasta[sizeof(pasta) - 1] = 0;
    CreateDirectoryA(pasta, NULL);
    unsigned int hh = 2166136261u;
    for (const char* c = url; *c; c++) { hh ^= (unsigned char)*c; hh *= 16777619u; }
    _snprintf(outCache, outSz - 1, "%s\\cache\\mods_%08x.img", dir, hh);
    outCache[outSz - 1] = 0;
    if (GetFileAttributesA(outCache) != INVALID_FILE_ATTRIBUTES) return; // já no cache
    HINTERNET h = InternetOpenA("TrokLauncher/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!h) { outCache[0] = 0; return; }
    DWORD fl = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (_strnicmp(url, "https", 5) == 0) fl |= INTERNET_FLAG_SECURE;
    HINTERNET u = InternetOpenUrlA(h, url, NULL, 0, fl, 0);
    if (!u) { InternetCloseHandle(h); outCache[0] = 0; return; }
    HANDLE f = CreateFileA(outCache, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { InternetCloseHandle(u); InternetCloseHandle(h); outCache[0] = 0; return; }
    static char bufI[32768];
    DWORD lidos = 0, soma = 0, esc = 0;
    bool ok = true;
    while (InternetReadFile(u, bufI, sizeof(bufI), &lidos) && lidos > 0) {
        if (!WriteFile(f, bufI, lidos, &esc, NULL) || esc != lidos) { ok = false; break; }
        soma += lidos;
        if (soma > 4u * 1024u * 1024u) { ok = false; break; } // capa de post nao passa de 4MB
    }
    CloseHandle(f);
    InternetCloseHandle(u);
    InternetCloseHandle(h);
    if (!ok || soma < 128) { DeleteFileA(outCache); outCache[0] = 0; }
}

static DWORD WINAPI ThreadMods(LPVOID) {
    gModsEstado = 1;
    HINTERNET h = InternetOpenA("TrokLauncher/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!h) { gModsEstado = 3; return 0; }
    DWORD fl = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (_strnicmp(gModsFeed, "https", 5) == 0) fl |= INTERNET_FLAG_SECURE;
    HINTERNET u = InternetOpenUrlA(h, gModsFeed, NULL, 0, fl, 0);
    if (!u) { InternetCloseHandle(h); gModsEstado = 3; return 0; }
    static char xml[262144]; // feed cabe folgado em 256KB
    DWORD tam = 0, lidos = 0;
    while (tam < sizeof(xml) - 1 && InternetReadFile(u, xml + tam, sizeof(xml) - 1 - tam, &lidos) && lidos > 0)
        tam += lidos;
    xml[tam] = 0;
    InternetCloseHandle(u);
    InternetCloseHandle(h);
    if (tam < 64) { gModsEstado = 3; return 0; }
    int n = 0;
    const char* p = xml;
    while (n < MAX_MODS) {
        const char* it = strstr(p, "<item>");
        if (!it) break;
        const char* fimIt = strstr(it, "</item>");
        if (!fimIt) break;
        // recorta o item NO PROPRIO xml (fecha com \0 e devolve no fim do loop). Copiar pra um
        // buffer de 8KB perdia o <link> e o <media:thumbnail>, que o Blogger manda DEPOIS da
        // <description>: post grande (o de lancamento tem 31KB) ficava com link vazio e sumia
        char* fimW = (char*)fimIt;
        char salvoFim = *fimW;
        *fimW = 0;
        const char* bloco = it;
        ExtrairTagXml(bloco, "title", gMods[n].titulo, sizeof(gMods[n].titulo));
        ExtrairTagXml(bloco, "link", gMods[n].url, sizeof(gMods[n].url));
        char pd[64];
        ExtrairTagXml(bloco, "pubDate", pd, sizeof(pd));
        const char* v = strchr(pd, ',');
        v = v ? v + 2 : pd;
        { // "Fri, 04 Sep 2026 00:55:45 +0000" -> "quinta-feira, 3 de setembro de 2026"
            // (o RSS vem em UTC; sem converter pro fuso daqui o dia sai adiantado)
            static const char* MES[12] = { "janeiro", "fevereiro", "março", "abril", "maio", "junho",
                                           "julho", "agosto", "setembro", "outubro", "novembro", "dezembro" };
            static const char* DIA[7] = { "domingo", "segunda-feira", "terça-feira", "quarta-feira",
                                          "quinta-feira", "sexta-feira", "sábado" };
            static const char* ABR_M = "JanFebMarAprMayJunJulAugSepOctNovDec";
            char sm[4] = { 0 };
            int dia = atoi(v), ano = 0, hh = 0, mm = 0;
            if (strlen(v) >= 11) { memcpy(sm, v + 3, 3); ano = atoi(v + 7); }
            if (strlen(v) >= 20) { hh = atoi(v + 12); mm = atoi(v + 15); }
            const char* pm = sm[0] ? strstr(ABR_M, sm) : NULL;
            int mi = pm ? (int)(pm - ABR_M) / 3 : -1;
            int offMin = 0; // fuso escrito no fim do pubDate ("+0000", "-0300")
            const char* z = strrchr(v, ' ');
            if (z && (z[1] == '+' || z[1] == '-') && strlen(z) >= 6) {
                int hz = (z[2] - '0') * 10 + (z[3] - '0'), mz = (z[4] - '0') * 10 + (z[5] - '0');
                offMin = (hz * 60 + mz) * (z[1] == '-' ? -1 : 1);
            }
            gMods[n].data[0] = 0;
            if (dia > 0 && mi >= 0 && ano > 1900) {
                SYSTEMTIME st = { 0 };
                st.wYear = (WORD)ano; st.wMonth = (WORD)(mi + 1); st.wDay = (WORD)dia;
                st.wHour = (WORD)hh; st.wMinute = (WORD)mm;
                FILETIME ft;
                SYSTEMTIME utc, loc;
                if (SystemTimeToFileTime(&st, &ft)) {
                    ULARGE_INTEGER u;
                    u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
                    u.QuadPart -= (LONGLONG)offMin * 60LL * 10000000LL;   // vira UTC de verdade
                    ft.dwLowDateTime = u.LowPart; ft.dwHighDateTime = u.HighPart;
                    if (FileTimeToSystemTime(&ft, &utc) &&
                        SystemTimeToTzSpecificLocalTime(NULL, &utc, &loc))
                        sprintf(gMods[n].data, "%s, %d de %s de %d", DIA[loc.wDayOfWeek % 7],
                                loc.wDay, MES[(loc.wMonth - 1) % 12], loc.wYear);
                }
                if (!gMods[n].data[0])
                    sprintf(gMods[n].data, "%d de %s de %d", dia, MES[mi], ano);
            }
            if (!gMods[n].data[0]) { strncpy(gMods[n].data, v, 11); gMods[n].data[11] = 0; }
        }
        DecodificarEntidades(gMods[n].titulo);
        // capa + resumo saem do corpo do post (<description> vem como HTML escapado)
        gMods[n].resumo[0] = 0;
        gMods[n].imgCache[0] = 0;
        char imgUrl[300] = "";
        { // 1a escolha de capa: <media:thumbnail url="..."> (Blogger sempre manda)
            const char* mt = strstr(bloco, "media:thumbnail");
            if (mt) {
                const char* uq = strstr(mt, "url=\"");
                if (uq) {
                    uq += 5;
                    const char* fq = strchr(uq, '"');
                    if (fq && fq - uq < (int)sizeof(imgUrl) - 1) {
                        memcpy(imgUrl, uq, fq - uq);
                        imgUrl[fq - uq] = 0;
                        CapaBlogger16x9(imgUrl, (int)sizeof(imgUrl)); // thumb de 72px -> capa 16:9
                    }
                }
            }
        }
        {
            static char desc[8192];
            ExtrairTagXml(bloco, "description", desc, sizeof(desc));
            DecodificarEntidades(desc); // &lt;p&gt; -> <p> (o feed escapa o html)
            if (!imgUrl[0]) { // sem thumbnail: pega a 1a <img src="..."> do corpo
                const char* im = strstr(desc, "<img");
                if (im) {
                    const char* sq = strstr(im, "src=\"");
                    if (sq) {
                        sq += 5;
                        const char* fq = strchr(sq, '"');
                        if (fq && fq - sq < (int)sizeof(imgUrl) - 1) {
                            memcpy(imgUrl, sq, fq - sq);
                            imgUrl[fq - sq] = 0;
                            CapaBlogger16x9(imgUrl, (int)sizeof(imgUrl)); // sem recorte quadrado
                        }
                    }
                }
            }
            ExtrairResumoHtml(desc, gMods[n].resumo, sizeof(gMods[n].resumo));
        }
        if (imgUrl[0]) BaixarImagemMods(imgUrl, gMods[n].imgCache, sizeof(gMods[n].imgCache));
        if (gMods[n].titulo[0] && _strnicmp(gMods[n].url, "http", 4) == 0) n++;
        *fimW = salvoFim; // devolve o '<' de </item> pro strstr seguinte enxergar
        p = fimIt + 7;
    }
    gNumMods = n;
    gModsEstado = (n > 0) ? 2 : 3;
    if (n > 0 && _stricmp(gMods[0].url, gModsUltimo) != 0) gModsNovo = true; // post novo!
    return 0;
}

// ===================== discord rich presence (pipe local, sem dll) =====================

static HANDLE gPipeDiscord = INVALID_HANDLE_VALUE;

static bool DiscordEnviar(int op, const char* json) {
    if (gPipeDiscord == INVALID_HANDLE_VALUE) return false;
    int len = (int)strlen(json);
    char buf[1600];
    if (len + 8 > (int)sizeof(buf)) return false;
    memcpy(buf, &op, 4);
    memcpy(buf + 4, &len, 4);
    memcpy(buf + 8, json, len);
    DWORD esc = 0;
    if (!WriteFile(gPipeDiscord, buf, len + 8, &esc, NULL)) {
        CloseHandle(gPipeDiscord);
        gPipeDiscord = INVALID_HANDLE_VALUE;
        return false;
    }
    return true;
}

static DWORD WINAPI ThreadDiscord(LPVOID) {
    if (!DISCORD_APP_ID[0] || (DISCORD_APP_ID[0] == '0' && !DISCORD_APP_ID[1])) return 0; // sem app id
    DWORD pid = GetCurrentProcessId();
    while (gRodando) {
        if (!gDiscordRP) {
            if (gPipeDiscord != INVALID_HANDLE_VALUE) { CloseHandle(gPipeDiscord); gPipeDiscord = INVALID_HANDLE_VALUE; }
            Sleep(1000);
            continue;
        }
        if (gPipeDiscord == INVALID_HANDLE_VALUE) {
            for (int i = 0; i < 10 && gPipeDiscord == INVALID_HANDLE_VALUE; i++) {
                char nome[64]; sprintf(nome, "\\\\.\\pipe\\discord-ipc-%d", i);
                gPipeDiscord = CreateFileA(nome, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            }
            if (gPipeDiscord == INVALID_HANDLE_VALUE) { // discord fechado: tenta de novo depois
                for (int t = 0; t < 300 && gRodando; t++) Sleep(100);
                continue;
            }
            char hs[128];
            sprintf(hs, "{\"v\":1,\"client_id\":\"%s\"}", DISCORD_APP_ID);
            DiscordEnviar(0, hs);
            Sleep(300);
        }
        DWORD disp = 0; // drena respostas para o pipe nao encher
        if (PeekNamedPipe(gPipeDiscord, NULL, 0, NULL, &disp, NULL) && disp > 0) {
            char lixo[1024]; DWORD lidos = 0;
            ReadFile(gPipeDiscord, lixo, disp > sizeof(lixo) ? sizeof(lixo) : disp, &lidos, NULL);
        }
        char srvEsc[96];
        strncpy(srvEsc, gRPServidor, sizeof(srvEsc) - 1);
        srvEsc[sizeof(srvEsc) - 1] = 0;
        for (char* c = srvEsc; *c; c++) if (*c == '"' || *c == '\\') *c = '\''; // json seguro
        char act[900];
        if (gRPJogando)
            sprintf(act, "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%lu,\"activity\":{"
                "\"details\":\"Jogando SA-MP\",\"state\":\"%s\",\"timestamps\":{\"start\":%lld},"
                "\"assets\":{\"large_image\":\"logo\",\"large_text\":\"Trok Launcher\"}}},\"nonce\":\"%lu\"}",
                pid, srvEsc, gRPDesde, GetTickCount());
        else
            sprintf(act, "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%lu,\"activity\":{"
                "\"details\":\"No Trok Launcher\",\"state\":\"Escolhendo servidor\","
                "\"assets\":{\"large_image\":\"logo\",\"large_text\":\"Trok Launcher\"}}},\"nonce\":\"%lu\"}",
                pid, GetTickCount());
        DiscordEnviar(1, act);
        for (int t = 0; t < 150 && gRodando; t++) Sleep(100); // atualiza a cada 15 s
    }
    if (gPipeDiscord != INVALID_HANDLE_VALUE) CloseHandle(gPipeDiscord);
    return 0;
}

// ===================== edicao de datas =====================

static void SalvarDatas() {
    char chave[16], linha[1200];
    for (int i = 0; i < MAX_DATAS; i++) {
        sprintf(chave, "data%d", i);
        if (i < gNumDatas) {
            DataGta& d = gDatas[i];
            sprintf(linha, "%s|%s|%s|%s|%s", d.nome, d.caminho, d.desc, d.img, d.userfiles);
            WritePrivateProfileStringA("datas", chave, linha, gIniPath);
        } else {
            WritePrivateProfileStringA("datas", chave, NULL, gIniPath); // apaga sobras
        }
    }
}

// links oficiais: cada slot guarda "rotulo>url"; legado (so url) ganha o rotulo padrao do slot
static void SepararLink(const char* bruto, int slot, char* rot, int rsz, char* url, int usz) {
    static const char* PADRAO[4] = { "Discord", "Site", "Fórum", "YouTube" };
    const char* sep = strchr(bruto, '>');
    if (sep) {
        int n = (int)(sep - bruto);
        if (n > rsz - 1) n = rsz - 1;
        memcpy(rot, bruto, n); rot[n] = 0;
        strncpy(url, sep + 1, usz - 1); url[usz - 1] = 0;
    } else {
        strncpy(rot, PADRAO[slot & 3], rsz - 1); rot[rsz - 1] = 0;
        strncpy(url, bruto, usz - 1); url[usz - 1] = 0;
    }
    if (!rot[0]) { rot[0] = 'L'; rot[1] = 'i'; rot[2] = 'n'; rot[3] = 'k'; rot[4] = 0; }
}

// compacta os slots de link (sem buracos) e normaliza legado p/ "rotulo>url" explicito
static void CompactarLinks(Servidor& s) {
    char tmp[4][160];
    int n = 0;
    for (int q = 0; q < 4; q++) {
        if (!s.sites[q][0]) continue;
        char rot[24], url[150];
        SepararLink(s.sites[q], q, rot, sizeof(rot), url, sizeof(url));
        sprintf(tmp[n], "%.22s>%.125s", rot, url);
        n++;
    }
    for (int q = 0; q < 4; q++) {
        if (q < n) memcpy(s.sites[q], tmp[q], sizeof(s.sites[0]));
        else s.sites[q][0] = 0;
    }
}

static void MoverLink(Servidor& s, int de, int para) {
    int n = 0;
    for (int q = 0; q < 4; q++) if (s.sites[q][0]) n++;
    if (de == para || de < 0 || para < 0 || de >= n || para >= n) return;
    char tmp[160];
    memcpy(tmp, s.sites[de], sizeof(tmp));
    if (de < para) for (int j = de; j < para; j++) memcpy(s.sites[j], s.sites[j + 1], sizeof(tmp));
    else for (int j = de; j > para; j--) memcpy(s.sites[j], s.sites[j - 1], sizeof(tmp));
    memcpy(s.sites[para], tmp, sizeof(tmp));
}

static const char* NomeExib(const Servidor& s) {
    if (s.apelido[0]) return s.apelido;               // apelido do usuario manda
    if (s.hostnameQ[0]) return s.hostnameQ;           // depois o hostname ao vivo
    return s.nome;
}

static void SalvarServidores() {
    char chave[24], linha[2048];
    for (int i = 0; i < MAX_SERVIDORES; i++) {
        sprintf(chave, "servidor%d", i);
        if (i < gNumSrv) {
            Servidor& s = gSrv[i];
            sprintf(linha, "%s|%s|%s|%d|%s|%s|%s|%s|%s|%s|%s|%s", s.nome, s.ip, s.modo, s.sky,
                    s.apelido, s.img, s.logo, s.sites[0], s.sites[1], s.sites[2], s.sites[3], s.contaPref);
            WritePrivateProfileStringA("servidores", chave, linha, gIniPath);
        } else {
            WritePrivateProfileStringA("servidores", chave, NULL, gIniPath);
        }
    }
}

// muda a posicao de um favorito (arrastar e soltar); persiste no ini
static void MoverFavorito(int de, int para) {
    if (de == para || de < 0 || para < 0 || de >= gNumSrv || para >= gNumSrv) return;
    EnterCriticalSection(&gLock);
    Servidor tmp = gSrv[de];
    if (de < para) for (int j = de; j < para; j++) gSrv[j] = gSrv[j + 1];
    else for (int j = de; j > para; j--) gSrv[j] = gSrv[j - 1];
    gSrv[para] = tmp;
    if (gSel == de) gSel = para;
    else if (de < para && gSel > de && gSel <= para) gSel--;
    else if (para < de && gSel >= para && gSel < de) gSel++;
    LeaveCriticalSection(&gLock);
    SalvarServidores();
}

// ordenacao das listas (clique no cabecalho da coluna alterna crescente/decrescente)
static int  gOrdCol = -1;   // -1 = ordem original; 0 nome, 1 modo, 2 jogadores, 3 ping
static bool gOrdAsc = true;
static int CmpFav(const void* A, const void* B) {
    const Servidor& a = gSrv[*(const int*)A];
    const Servidor& b = gSrv[*(const int*)B];
    int r = 0;
    switch (gOrdCol) {
    case 0: r = _stricmp(NomeExib(a), NomeExib(b)); break;
    case 1: r = _stricmp(a.modoQ[0] ? a.modoQ : a.modo, b.modoQ[0] ? b.modoQ : b.modo); break;
    case 2: { bool va = a.ping >= 0, vb = b.ping >= 0; if (va != vb) return va ? -1 : 1; r = a.online - b.online; break; }
    case 3: { bool va = a.ping >= 0, vb = b.ping >= 0; if (va != vb) return va ? -1 : 1; r = a.ping - b.ping; break; }
    }
    return gOrdAsc ? r : -r;
}
static int CmpPub(const void* A, const void* B) {
    const SrvPub& a = gPub[*(const int*)A];
    const SrvPub& b = gPub[*(const int*)B];
    int r = 0;
    switch (gOrdCol) {
    case 0: r = _stricmp(a.nome, b.nome); break;
    case 1: r = _stricmp(a.modo, b.modo); break;
    default: r = a.on - b.on; break;  // jogadores/ping: na internet ordena por lotacao
    }
    return gOrdAsc ? r : -r;
}

static void SalvarEdicaoData(int i) {
    if (i < 0 || i >= gNumDatas) return;
    for (char* c = gEditNome; *c; c++) if (*c == '|') *c = '/';
    for (char* c = gEditDesc; *c; c++) if (*c == '|') *c = '/';
    strncpy(gDatas[i].nome, gEditNome, sizeof(gDatas[0].nome) - 1);
    strncpy(gDatas[i].desc, gEditDesc, sizeof(gDatas[0].desc) - 1);
    SalvarDatas();
}

// dialogo nativo do Windows; bloqueia o render enquanto aberto (ok para um launcher)
static bool EscolherArquivo(const char* filtro, const char* titulo, char* out, int outsz) {
    char arq[MAX_PATH] = "";
    OPENFILENAMEA of = { sizeof(of) };
    of.lpstrFilter = filtro;
    of.lpstrFile = arq;
    of.nMaxFile = MAX_PATH;
    of.lpstrTitle = titulo;
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&of)) return false;
    strncpy(out, arq, outsz - 1);
    out[outsz - 1] = 0;
    return true;
}

static bool EscolherArquivoSalvar(const char* filtro, const char* titulo, const char* nomePadrao,
                                  const char* ext, char* out, int outsz) {
    strncpy(out, nomePadrao, outsz - 1);
    out[outsz - 1] = 0;
    OPENFILENAMEA of = { sizeof(of) };
    of.lpstrFilter = filtro;
    of.lpstrFile = out;
    of.nMaxFile = outsz;
    of.lpstrTitle = titulo;
    of.lpstrDefExt = ext;
    of.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    return GetSaveFileNameA(&of) != 0;
}

// ===================== exportar / importar configuracoes =====================
// arquivo unico .trokcfg (container): ini + imagens que o usuario subiu. Capas e avatares
// padrao ja vem com o app. Na importacao, os caminhos absolutos sao re-baseados pro PC novo.

static bool gPedirExportCfg = false, gPedirImportCfg = false;

static void DirDoExe(char* out, int outsz) {
    GetModuleFileNameA(NULL, out, outsz);
    char* b = strrchr(out, '\\');
    if (b) *b = 0;
}

static bool ExportarConfig(const char* destino) {
    char dir[MAX_PATH];
    DirDoExe(dir, sizeof(dir));
    static char rel[200][96];
    static char full[200][MAX_PATH];
    static unsigned int tams[200];
    int n = 0;
    strcpy(rel[n], "_basedir"); full[n][0] = 0; n++; // meta: pasta original (re-base no destino)
    strcpy(rel[n], "Trok Launcher.ini");
    strncpy(full[n], gIniPath, MAX_PATH - 1); full[n][MAX_PATH - 1] = 0; n++;
    char busca[MAX_PATH];
    _snprintf(busca, sizeof(busca) - 1, "%s\\imagens\\*", dir);
    busca[sizeof(busca) - 1] = 0;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(busca, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (n >= 200) break;
            _snprintf(rel[n], sizeof(rel[n]) - 1, "imagens\\%s", fd.cFileName);
            rel[n][sizeof(rel[n]) - 1] = 0;
            _snprintf(full[n], MAX_PATH - 1, "%s\\imagens\\%s", dir, fd.cFileName);
            full[n][MAX_PATH - 1] = 0;
            n++;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    HANDLE fo = CreateFileA(destino, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fo == INVALID_HANDLE_VALUE) return false;
    DWORD esc = 0;
    bool ok = WriteFile(fo, "TROKCFG1", 8, &esc, NULL) && WriteFile(fo, &n, 4, &esc, NULL);
    for (int i = 0; i < n && ok; i++) { // tabela
        unsigned int tam = 0;
        if (i == 0) tam = (unsigned int)strlen(dir);
        else {
            HANDLE fi = CreateFileA(full[i], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (fi != INVALID_HANDLE_VALUE) { tam = GetFileSize(fi, NULL); CloseHandle(fi); }
        }
        tams[i] = tam;
        unsigned short nl = (unsigned short)strlen(rel[i]);
        ok = WriteFile(fo, &nl, 2, &esc, NULL) && WriteFile(fo, rel[i], nl, &esc, NULL) &&
             WriteFile(fo, &tams[i], 4, &esc, NULL);
    }
    static char bufc[65536];
    for (int i = 0; i < n && ok; i++) { // blobs
        if (i == 0) { ok = WriteFile(fo, dir, tams[0], &esc, NULL) != 0; continue; }
        HANDLE fi = CreateFileA(full[i], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (fi == INVALID_HANDLE_VALUE) { ok = (tams[i] == 0); continue; }
        DWORD resta = tams[i], l2 = 0;
        while (resta > 0 && ok) {
            DWORD ped = resta > sizeof(bufc) ? (DWORD)sizeof(bufc) : resta;
            ok = ReadFile(fi, bufc, ped, &l2, NULL) && l2 == ped && WriteFile(fo, bufc, ped, &esc, NULL);
            resta -= ped;
        }
        CloseHandle(fi);
    }
    CloseHandle(fo);
    if (!ok) DeleteFileA(destino);
    return ok;
}

static void TrocarPrefixoIni(char* txt, int cap, const char* de, const char* para) {
    // troca (sem diferenciar maiusculas) toda ocorrencia da pasta antiga pela nova
    static char saida[131072];
    int w = 0, lenDe = (int)strlen(de), lenPara = (int)strlen(para);
    if (lenDe == 0) return;
    for (char* p = txt; *p && w < (int)sizeof(saida) - lenPara - 2; ) {
        if (_strnicmp(p, de, lenDe) == 0) {
            memcpy(saida + w, para, lenPara);
            w += lenPara;
            p += lenDe;
        } else saida[w++] = *p++;
    }
    saida[w] = 0;
    strncpy(txt, saida, cap - 1);
    txt[cap - 1] = 0;
}

static bool ImportarConfig(const char* origem) {
    HANDLE f = CreateFileA(origem, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD tam = GetFileSize(f, NULL), lidos = 0;
    if (tam < 16 || tam > 200u * 1024u * 1024u) { CloseHandle(f); return false; }
    unsigned char* buf = (unsigned char*)malloc(tam);
    BOOL okr = buf && ReadFile(f, buf, tam, &lidos, NULL);
    CloseHandle(f);
    if (!okr || lidos != tam || memcmp(buf, "TROKCFG1", 8) != 0) { free(buf); return false; }
    unsigned char* p = buf + 8;
    unsigned char* fim = buf + tam;
    int n = *(int*)p; p += 4;
    if (n <= 0 || n > 200) { free(buf); return false; }
    static char rel[200][96];
    static unsigned int tams[200];
    for (int i = 0; i < n; i++) {
        if ((int)(fim - p) < 2) { free(buf); return false; }
        unsigned short nl = *(unsigned short*)p; p += 2;
        if (nl == 0 || nl > 95 || (int)(fim - p) < nl + 4) { free(buf); return false; }
        memcpy(rel[i], p, nl); rel[i][nl] = 0; p += nl;
        tams[i] = *(unsigned int*)p; p += 4;
    }
    char dir[MAX_PATH];
    DirDoExe(dir, sizeof(dir));
    char baseAntiga[MAX_PATH] = "";
    bool ok = true;
    for (int i = 0; i < n && ok; i++) {
        if (tams[i] > (unsigned int)(fim - p)) { ok = false; break; }
        const unsigned char* dados = p;
        p += tams[i];
        if (strstr(rel[i], "..")) continue; // seguranca: nome nunca sai da pasta do launcher
        if (_stricmp(rel[i], "_basedir") == 0) {
            unsigned int cp = tams[i] < MAX_PATH - 1 ? tams[i] : MAX_PATH - 1;
            memcpy(baseAntiga, dados, cp);
            baseAntiga[cp] = 0;
        } else if (_stricmp(rel[i], "Trok Launcher.ini") == 0) {
            char bkp[MAX_PATH + 24]; // a config atual vira backup antes de ser sobrescrita
            _snprintf(bkp, sizeof(bkp) - 1, "%s.antes-import", gIniPath);
            bkp[sizeof(bkp) - 1] = 0;
            CopyFileA(gIniPath, bkp, FALSE);
            static char txt[131072];
            unsigned int cp = tams[i] < sizeof(txt) - 1 ? tams[i] : (unsigned int)sizeof(txt) - 1;
            memcpy(txt, dados, cp);
            txt[cp] = 0;
            if (baseAntiga[0] && _stricmp(baseAntiga, dir) != 0)
                TrocarPrefixoIni(txt, sizeof(txt), baseAntiga, dir); // PC novo, caminhos novos
            HANDLE fo = CreateFileA(gIniPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (fo == INVALID_HANDLE_VALUE) { ok = false; break; }
            DWORD esc = 0;
            ok = WriteFile(fo, txt, (DWORD)strlen(txt), &esc, NULL) != 0;
            CloseHandle(fo);
        } else if (_strnicmp(rel[i], "imagens\\", 8) == 0) {
            char pastaI[MAX_PATH];
            _snprintf(pastaI, sizeof(pastaI) - 1, "%s\\imagens", dir);
            pastaI[sizeof(pastaI) - 1] = 0;
            CreateDirectoryA(pastaI, NULL);
            char destino[MAX_PATH];
            _snprintf(destino, MAX_PATH - 1, "%s\\%s", dir, rel[i]);
            destino[MAX_PATH - 1] = 0;
            HANDLE fo = CreateFileA(destino, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (fo == INVALID_HANDLE_VALUE) continue; // uma imagem falhar nao aborta o resto
            DWORD esc = 0;
            WriteFile(fo, dados, tams[i], &esc, NULL);
            CloseHandle(fo);
        }
    }
    free(buf);
    return ok;
}

static bool EscolherPasta(const char* titulo, char* out, int outsz) {
    BROWSEINFOA bi = { 0 };
    bi.lpszTitle = titulo;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (!pidl) return false;
    char pasta[MAX_PATH];
    bool ok = SHGetPathFromIDListA(pidl, pasta) != FALSE;
    CoTaskMemFree(pidl);
    if (ok) { strncpy(out, pasta, outsz - 1); out[outsz - 1] = 0; }
    return ok;
}

// User Files da data: o campo dela, ou o padrao em Documentos
static void UserFilesDaData(const DataGta& d, char* out, int outsz) {
    if (d.userfiles[0]) { strncpy(out, d.userfiles, outsz - 1); out[outsz - 1] = 0; return; }
    char doc[MAX_PATH] = "";
    SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, doc);
    _snprintf(out, outsz - 1, "%s\\GTA San Andreas User Files", doc);
    out[outsz - 1] = 0;
}

// ===================== galeria (screenshots) =====================

static int CmpFoto(const void* A, const void* B) {
    return CompareFileTime(&((const Foto*)B)->quando, &((const Foto*)A)->quando); // recentes primeiro
}

// manda para a LIXEIRA (FOF_ALLOWUNDO) - nunca apaga definitivo
static bool ExcluirParaLixeira(const char* caminho) {
    char duplo[MAX_PATH + 2];
    memset(duplo, 0, sizeof(duplo)); // SHFileOperation exige DOIS zeros no fim
    strncpy(duplo, caminho, MAX_PATH - 1);
    SHFILEOPSTRUCTA op = { 0 };
    op.wFunc = FO_DELETE;
    op.pFrom = duplo;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
    return SHFileOperationA(&op) == 0;
}

// ao trocar/remover uma arte, descarta a COPIA antiga da pasta imagens\ (se nada mais a usa);
// sem isso a pasta acumula orfaos a cada troca. Vai para a Lixeira, como tudo no app.
static void DescartarImagemImportada(const char* caminho) {
    if (!caminho || !caminho[0]) return;
    char dir[MAX_PATH];
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    char* b = strrchr(dir, '\\');
    if (b) *b = 0;
    char pasta[MAX_PATH];
    _snprintf(pasta, MAX_PATH - 1, "%s\\imagens\\", dir); // barra final: "imagens-antigas" NAO e nossa
    pasta[MAX_PATH - 1] = 0;
    if (_strnicmp(caminho, pasta, strlen(pasta)) != 0) return; // so mexe no que e copia NOSSA
    for (int i = 0; i < gNumSrv; i++)
        if (_stricmp(gSrv[i].img, caminho) == 0 || _stricmp(gSrv[i].logo, caminho) == 0) return;
    for (int i = 0; i < gNumDatas; i++)
        if (_stricmp(gDatas[i].img, caminho) == 0) return;
    ExcluirParaLixeira(caminho);
}

static void EscanearFotos() {
    for (int i = 0; i < gNumFotos; i++) { AdiarRelease(gFotos[i].tex); gFotos[i].tex = NULL; }
    FGLimpar();
    gNumFotos = 0;
    gFotoVista = -1;
    static const char* EXT[3] = { "*.png", "*.jpg", "*.jpeg" };
    for (int e = 0; e < 3; e++) {
        char busca[MAX_PATH + 16];
        _snprintf(busca, sizeof(busca) - 1, "%s\\%s", gFotosDir, EXT[e]);
        busca[sizeof(busca) - 1] = 0;
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(busca, &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            int alvo = -1;
            if (gNumFotos < MAX_FOTOS) {
                alvo = gNumFotos++;
            } else {
                // lista cheia: substitui a mais ANTIGA se esta for mais nova
                // (sem isso, 5000 prints deixariam de fora justamente as recentes)
                int vel = 0;
                for (int k = 1; k < gNumFotos; k++)
                    if (CompareFileTime(&gFotos[k].quando, &gFotos[vel].quando) < 0) vel = k;
                if (CompareFileTime(&fd.ftLastWriteTime, &gFotos[vel].quando) > 0) {
                    if (gFotos[vel].tex) { AdiarRelease(gFotos[vel].tex); gFotos[vel].tex = NULL; }
                    alvo = vel;
                }
            }
            if (alvo >= 0) {
                Foto& f = gFotos[alvo];
                _snprintf(f.caminho, MAX_PATH - 1, "%s\\%s", gFotosDir, fd.cFileName);
                f.caminho[MAX_PATH - 1] = 0;
                f.quando = fd.ftLastWriteTime;
                f.tex = NULL;
                f.falhou = false;
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    qsort(gFotos, gNumFotos, sizeof(Foto), CmpFoto);
}

// copia a imagem escolhida para a pasta imagens\ do launcher e devolve o caminho da COPIA:
// o usuario pode apagar/mover o original que nada quebra
// re-encoda a imagem para o tamanho/formato do uso: um print 4K de 8MB vira um jpeg
// de ~200KB sem diferenca visivel no launcher; logo mantem png (transparencia)
static bool ReencodarImagem(const char* origem, const char* destino, int maxLado, bool comAlpha) {
    wchar_t wo[MAX_PATH], wd[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, origem, -1, wo, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, destino, -1, wd, MAX_PATH);
    IWICImagingFactory* fab = NULL;
    IWICBitmapDecoder* dec = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICBitmapScaler* esc = NULL;
    IWICFormatConverter* conv = NULL;
    IWICStream* st = NULL;
    IWICBitmapEncoder* enc = NULL;
    IWICBitmapFrameEncode* fe = NULL;
    IPropertyBag2* pb = NULL;
    bool ok = false;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                IID_IWICImagingFactory, (void**)&fab))) return false;
    do {
        if (FAILED(fab->CreateDecoderFromFilename(wo, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec))) break;
        if (FAILED(dec->GetFrame(0, &frame))) break;
        UINT ow = 0, oh = 0;
        frame->GetSize(&ow, &oh);
        if (!ow || !oh) break;
        UINT nw = ow, nh = oh;
        if ((int)ow > maxLado || (int)oh > maxLado) {
            if (ow >= oh) { nw = maxLado; nh = (UINT)((float)oh * maxLado / ow); }
            else          { nh = maxLado; nw = (UINT)((float)ow * maxLado / oh); }
            if (nw < 1) nw = 1;
            if (nh < 1) nh = 1;
        }
        IWICBitmapSource* fonte = frame;
        if (nw != ow || nh != oh) {
            if (FAILED(fab->CreateBitmapScaler(&esc))) break;
            if (FAILED(esc->Initialize(frame, nw, nh, WICBitmapInterpolationModeFant))) break;
            fonte = esc;
        }
        if (FAILED(fab->CreateFormatConverter(&conv))) break;
        if (FAILED(conv->Initialize(fonte, comAlpha ? GUID_WICPixelFormat32bppBGRA : GUID_WICPixelFormat24bppBGR,
                                    WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom))) break;
        if (FAILED(fab->CreateStream(&st))) break;
        if (FAILED(st->InitializeFromFilename(wd, GENERIC_WRITE))) break;
        if (FAILED(fab->CreateEncoder(comAlpha ? GUID_ContainerFormatPng : GUID_ContainerFormatJpeg, NULL, &enc))) break;
        if (FAILED(enc->Initialize(st, WICBitmapEncoderNoCache))) break;
        if (FAILED(enc->CreateNewFrame(&fe, &pb))) break;
        if (!comAlpha && pb) { // jpeg 0.86: otimo equilibrio qualidade/peso
            PROPBAG2 opt = { 0 };
            opt.pstrName = (LPOLESTR)L"ImageQuality";
            VARIANT v;
            memset(&v, 0, sizeof(v));
            v.vt = VT_R4;
            v.fltVal = 0.86f;
            pb->Write(1, &opt, &v);
        }
        if (FAILED(fe->Initialize(pb))) break;
        if (FAILED(fe->WriteSource(conv, NULL))) break;
        if (FAILED(fe->Commit())) break;
        if (FAILED(enc->Commit())) break;
        ok = true;
    } while (0);
    if (pb) pb->Release();
    if (fe) fe->Release();
    if (enc) enc->Release();
    if (st) st->Release();
    if (conv) conv->Release();
    if (esc) esc->Release();
    if (frame) frame->Release();
    if (dec) dec->Release();
    if (fab) fab->Release();
    if (!ok) DeleteFileA(destino); // meio-arquivo nao vale
    return ok;
}

// importa OTIMIZANDO: capa 1600 / data 800 / logo 640 (png se logoAlpha, jpeg no resto)
static void ImportarImagem(const char* origem, char* out, int outsz, int maxLado, bool logoAlpha) {
    strncpy(out, origem, outsz - 1); // se algo falhar, fica o caminho original
    out[outsz - 1] = 0;
    char dir[MAX_PATH];
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    char* b = strrchr(dir, '\\');
    if (b) *b = 0;
    char pasta[MAX_PATH];
    _snprintf(pasta, sizeof(pasta) - 1, "%s\\imagens", dir);
    pasta[sizeof(pasta) - 1] = 0;
    if (_strnicmp(origem, pasta, strlen(pasta)) == 0) return; // ja mora na pasta do launcher
    CreateDirectoryA(pasta, NULL);
    const char* nomeArq = strrchr(origem, '\\');
    nomeArq = nomeArq ? nomeArq + 1 : origem;
    char base[96];
    strncpy(base, nomeArq, sizeof(base) - 1);
    base[sizeof(base) - 1] = 0;
    char* ext = strrchr(base, '.');
    if (ext) *ext = 0; // a extensao nova vem do formato re-encodado
    const char* extNova = logoAlpha ? "png" : "jpg";
    char destino[MAX_PATH];
    _snprintf(destino, MAX_PATH - 1, "%s\\%s.%s", pasta, base, extNova);
    destino[MAX_PATH - 1] = 0;
    if (GetFileAttributesA(destino) != INVALID_FILE_ATTRIBUTES) // nome ja usado: gera unico
        _snprintf(destino, MAX_PATH - 1, "%s\\%lu_%s.%s", pasta, GetTickCount() % 100000, base, extNova);
    destino[MAX_PATH - 1] = 0;
    if (ReencodarImagem(origem, destino, maxLado, logoAlpha)) {
        strncpy(out, destino, outsz - 1);
        out[outsz - 1] = 0;
        return;
    }
    // formato que o WIC nao decodificou: copia crua (comportamento antigo)
    _snprintf(destino, MAX_PATH - 1, "%s\\%s", pasta, nomeArq);
    destino[MAX_PATH - 1] = 0;
    if (GetFileAttributesA(destino) != INVALID_FILE_ATTRIBUTES)
        _snprintf(destino, MAX_PATH - 1, "%s\\%lu_%s", pasta, GetTickCount() % 100000, nomeArq);
    destino[MAX_PATH - 1] = 0;
    if (CopyFileA(origem, destino, FALSE)) {
        strncpy(out, destino, outsz - 1);
        out[outsz - 1] = 0;
    }
}

// roda no loop principal, FORA do frame imgui (dialogos nativos tem message pump propria)
static void ProcessarPedidosDatas() {
    if (gPickImagem >= 0 && gPickImagem < gNumDatas) {
        int i = gPickImagem; gPickImagem = -1;
        char arq[MAX_PATH];
        if (EscolherArquivo("Imagens (png, jpg, bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0\0", "Imagem 16:9 da data", arq, sizeof(arq))) {
            char velho[MAX_PATH];
            strncpy(velho, gDatas[i].img, sizeof(velho) - 1); velho[sizeof(velho) - 1] = 0;
            char local[MAX_PATH];
            ImportarImagem(arq, local, sizeof(local), 800, false); // card de data: 800 basta
            strncpy(gDatas[i].img, local, sizeof(gDatas[i].img) - 1);
            if (gDatas[i].tex) { gDatas[i].tex->Release(); gDatas[i].tex = NULL; }
            gDatas[i].tex = CarregarImagemMax(gDev, local, 800);
            SalvarDatas();
            DescartarImagemImportada(velho); // a copia antiga nao fica de orfa
        }
    }
    if (gPickCaminho >= 0 && gPickCaminho < gNumDatas) {
        int i = gPickCaminho; gPickCaminho = -1;
        char arq[MAX_PATH];
        if (EscolherArquivo("gta_sa.exe\0gta_sa.exe\0Executaveis\0*.exe\0\0", "Selecione o gta_sa.exe da instalacao", arq, sizeof(arq))) {
            char* b = strrchr(arq, '\\');
            if (b) *b = 0;
            strncpy(gDatas[i].caminho, arq, sizeof(gDatas[i].caminho) - 1);
            if (i == gDataSel) { strncpy(gPastaGta, arq, sizeof(gPastaGta) - 1); SalvarConfig(); }
            SalvarDatas();
            char sampChk[MAX_PATH]; // pasta valida mas sem SA-MP? avisa na hora
            _snprintf(sampChk, MAX_PATH - 1, "%s\\samp.exe", arq);
            sampChk[MAX_PATH - 1] = 0;
            if (GetFileAttributesA(sampChk) == INVALID_FILE_ATTRIBUTES)
                Avisar("Essa pasta nao tem samp.exe - instale o SA-MP nela para jogar.");
            gSampOk = SampValido();
        } else if (i == gDataSel && !SampValido()) {
            gBoasVindas = true; // cancelou o picker sem SA-MP: o aviso volta
        }
    }
    if (gPickImgSrv >= 0 && gPickImgSrv < gNumSrv) {
        int i = gPickImgSrv; gPickImgSrv = -1;
        char arq[MAX_PATH];
        if (EscolherArquivo("Imagens (png, jpg, bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0\0", "Imagem de exibição do servidor", arq, sizeof(arq))) {
            char velho[MAX_PATH];
            strncpy(velho, gSrv[i].img, sizeof(velho) - 1); velho[sizeof(velho) - 1] = 0;
            char local[MAX_PATH];
            ImportarImagem(arq, local, sizeof(local), 1600, false); // capa vira fundo da janela
            strncpy(gSrv[i].img, local, sizeof(gSrv[i].img) - 1);
            if (gSrv[i].tex) {
                if (gSrv[i].tex == gFundoAtual) gFundoAtual = NULL; // pode estar em fade no fundo
                if (gSrv[i].tex == gFundoAnt) gFundoAnt = NULL;
                gSrv[i].tex->Release(); gSrv[i].tex = NULL;
            }
            gSrv[i].tex = CarregarImagemMax(gDev, local, 1600);
            SalvarServidores();
            DescartarImagemImportada(velho);
        }
    }
    if (gPickUserFiles >= 0 && gPickUserFiles < gNumDatas) {
        int i = gPickUserFiles; gPickUserFiles = -1;
        char pasta[MAX_PATH];
        if (EscolherPasta("Escolha a pasta User Files desta data", pasta, sizeof(pasta))) {
            strncpy(gDatas[i].userfiles, pasta, sizeof(gDatas[0].userfiles) - 1);
            SalvarDatas();
            gFotosDir[0] = 0; // forca a galeria a reescanear
        }
    }
    if (gPickLogoSrv >= 0 && gPickLogoSrv < gNumSrv) {
        int i = gPickLogoSrv; gPickLogoSrv = -1;
        char arq[MAX_PATH];
        if (EscolherArquivo("Imagens (png, jpg, bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0\0", "Logo do servidor (substitui o nome no card)", arq, sizeof(arq))) {
            char velho[MAX_PATH];
            strncpy(velho, gSrv[i].logo, sizeof(velho) - 1); velho[sizeof(velho) - 1] = 0;
            char local[MAX_PATH];
            ImportarImagem(arq, local, sizeof(local), 640, true); // logo: png preserva transparencia
            strncpy(gSrv[i].logo, local, sizeof(gSrv[i].logo) - 1);
            if (gSrv[i].texLogo) { gSrv[i].texLogo->Release(); gSrv[i].texLogo = NULL; }
            gSrv[i].texLogo = CarregarImagemMax(gDev, local, 640);
            SalvarServidores();
            DescartarImagemImportada(velho);
        }
    }
    if (gPedirExportCfg) {
        gPedirExportCfg = false;
        char arq[MAX_PATH];
        if (EscolherArquivoSalvar("Backup do Trok Launcher (*.trokcfg)\0*.trokcfg\0\0",
                                  "Exportar configurações", "TrokLauncher-backup.trokcfg",
                                  "trokcfg", arq, sizeof(arq))) {
            if (ExportarConfig(arq)) Avisar("Configurações exportadas! Leve o arquivo pro outro PC.");
            else Avisar("Não consegui exportar as configurações.");
        }
    }
    if (gPedirImportCfg) {
        gPedirImportCfg = false;
        char arq[MAX_PATH];
        if (EscolherArquivo("Backup do Trok Launcher (*.trokcfg)\0*.trokcfg\0\0",
                            "Importar configurações", arq, sizeof(arq))) {
            if (ImportarConfig(arq)) { // reabre pra recarregar tudo (a config antiga vira .antes-import)
                gPulaSalvarSaida = true; // senao o SalvarConfig da saida desfaz o que acabou de entrar
                char eu[MAX_PATH];
                GetModuleFileNameA(NULL, eu, MAX_PATH);
                ShellExecuteA(NULL, "open", eu, NULL, NULL, SW_SHOWNORMAL);
                gRodando = false;
            } else Avisar("Arquivo de backup inválido.");
        }
    }
    if (gAddData) {
        gAddData = false;
        if (gNumDatas < MAX_DATAS) {
            char arq[MAX_PATH];
            if (EscolherArquivo("gta_sa.exe\0gta_sa.exe\0Executaveis\0*.exe\0\0", "Selecione o gta_sa.exe da nova data", arq, sizeof(arq))) {
                char* b = strrchr(arq, '\\');
                if (b) *b = 0;
                DataGta& d = gDatas[gNumDatas];
                memset(&d, 0, sizeof(d));
                strncpy(d.caminho, arq, sizeof(d.caminho) - 1);
                const char* nomePasta = strrchr(arq, '\\');
                strncpy(d.nome, nomePasta ? nomePasta + 1 : "Nova data", sizeof(d.nome) - 1);
                strcpy(d.desc, "");
                SortearCapaPadrao(d.img, sizeof(d.img)); // data nova ja nasce com capa (mono)
                if (d.img[0]) {
                    d.tex = CarregarImagemMax(gDev, d.img, 800);
                    if (strstr(d.img, "\\capas\\")) EscurecerMonocromatico(d.tex);
                }
                gNumDatas++;
                SalvarDatas();
            }
        }
    }
}

static void FavoritarPublico(int i) {
    if (gNumSrv >= MAX_SERVIDORES || i < 0 || i >= gNumPub) return;
    static Servidor tmp; // monta fora do lock (decode de imagem e lento)
    memset(&tmp, 0, sizeof(tmp));
    strncpy(tmp.nome, gPub[i].nome, sizeof(tmp.nome) - 1);
    strncpy(tmp.ip, gPub[i].ip, sizeof(tmp.ip) - 1);
    strncpy(tmp.modo, gPub[i].modo, sizeof(tmp.modo) - 1);
    for (char* c = tmp.nome; *c; c++) if (*c == '|') *c = '/';
    for (char* c = tmp.modo; *c; c++) if (*c == '|') *c = '/';
    tmp.sky = gNumSrv % 4;
    tmp.ping = -1;
    SortearCapaPadrao(tmp.img, sizeof(tmp.img)); // nenhum favorito comeca sem imagem
    if (tmp.img[0]) tmp.tex = CarregarImagemMax(gDev, tmp.img, 1600);
    EnterCriticalSection(&gLock); // publica pronto: a thread de query nunca ve slot pela metade
    gSrv[gNumSrv] = tmp;
    gNumSrv++;
    LeaveCriticalSection(&gLock);
    SalvarServidores();
}

// ===================== jogar =====================

// um jogo por vez: abrir outro servidor fecha o gta/samp que estiver rodando
static void FecharJogoAnterior() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32 pe = { sizeof(pe) };
    if (Process32First(snap, &pe)) do {
        if (_stricmp(pe.szExeFile, "gta_sa.exe") != 0 && _stricmp(pe.szExeFile, "samp.exe") != 0) continue;
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
        if (h) { TerminateProcess(h, 0); CloseHandle(h); }
    } while (Process32Next(snap, &pe));
    CloseHandle(snap);
}

// ===================== senhas do SA-MP original (USERDATA.DAT + registro) =====================
// mesmas opcoes do samp.exe: SaveServPasses/SaveRconPasses em HKCU\Software\SAMP, e a senha
// em si no USERDATA.DAT (texto puro, formato do browser original - fonte unica compartilhada)

static void LerOpcoesSampRegistro() {
    HKEY k;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\SAMP", 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS) return;
    DWORD v = 0, tam = sizeof(v), tipo = 0;
    if (RegQueryValueExA(k, "SaveServPasses", NULL, &tipo, (BYTE*)&v, &tam) == ERROR_SUCCESS && tipo == REG_DWORD)
        gSalvarSenhaServ = v != 0;
    v = 0; tam = sizeof(v);
    if (RegQueryValueExA(k, "SaveRconPasses", NULL, &tipo, (BYTE*)&v, &tam) == ERROR_SUCCESS && tipo == REG_DWORD)
        gSalvarSenhaRcon = v != 0;
    RegCloseKey(k);
}

static void GravarOpcaoSampRegistro(const char* nome, bool val) {
    HKEY k;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\SAMP", 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) != ERROR_SUCCESS) return;
    DWORD v = val ? 1 : 0;
    RegSetValueExA(k, nome, 0, REG_DWORD, (BYTE*)&v, sizeof(v));
    RegCloseKey(k);
}

static void CaminhoUserdata(char* out, int outsz) {
    char docs[MAX_PATH];
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, docs))) { out[0] = 0; return; }
    _snprintf(out, outsz - 1, "%s\\GTA San Andreas User Files\\SAMP\\USERDATA.DAT", docs);
    out[outsz - 1] = 0;
}

static void SepararIpPorta(const char* ipPorta, char* host, int hostSz, int* porta) {
    strncpy(host, ipPorta, hostSz - 1);
    host[hostSz - 1] = 0;
    *porta = 7777;
    char* dp = strchr(host, ':');
    if (dp) { *dp = 0; *porta = atoi(dp + 1); }
}

// le a senha salva deste servidor no USERDATA.DAT (a mesma que o browser original usa)
static bool LerSenhaFavorito(const char* ipPorta, char* out, int outsz) {
    out[0] = 0;
    char host[64]; int porta;
    SepararIpPorta(ipPorta, host, sizeof(host), &porta);
    if (!host[0]) return false;
    char arq[MAX_PATH];
    CaminhoUserdata(arq, sizeof(arq));
    HANDLE f = CreateFileA(arq, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD tam = GetFileSize(f, NULL), lidos = 0;
    if (tam < 12 || tam > 1048576) { CloseHandle(f); return false; }
    unsigned char* buf = (unsigned char*)malloc(tam);
    BOOL okr = buf && ReadFile(f, buf, tam, &lidos, NULL);
    CloseHandle(f);
    if (!okr || lidos != tam || memcmp(buf, "SAMP", 4) != 0) { free(buf); return false; }
    unsigned char* p = buf + 8;
    unsigned char* fim = buf + tam;
    unsigned int n = *(unsigned int*)p; p += 4;
    bool achou = false;
    for (unsigned int e = 0; e < n && !achou; e++) {
        unsigned int il, pt, nl2, pl, rl;
        if ((unsigned int)(fim - p) < 4) break; il = *(unsigned int*)p; p += 4;
        if (il > 256 || (unsigned int)(fim - p) < il) break; const char* ip2 = (const char*)p; p += il;
        if ((unsigned int)(fim - p) < 4) break; pt = *(unsigned int*)p; p += 4;
        if ((unsigned int)(fim - p) < 4) break; nl2 = *(unsigned int*)p; p += 4;
        if (nl2 > 256 || (unsigned int)(fim - p) < nl2) break; p += nl2;
        if ((unsigned int)(fim - p) < 4) break; pl = *(unsigned int*)p; p += 4;
        if (pl > 256 || (unsigned int)(fim - p) < pl) break; const char* pw = (const char*)p; p += pl;
        if ((unsigned int)(fim - p) < 4) break; rl = *(unsigned int*)p; p += 4;
        if (rl > 256 || (unsigned int)(fim - p) < rl) break; p += rl;
        if ((int)pt == porta && il == (unsigned int)strlen(host) && _strnicmp(ip2, host, il) == 0 && pl > 0) {
            unsigned int cp = pl < (unsigned int)(outsz - 1) ? pl : (unsigned int)(outsz - 1);
            memcpy(out, pw, cp);
            out[cp] = 0;
            achou = true;
        }
    }
    free(buf);
    return achou;
}

// grava/atualiza a senha deste servidor no USERDATA.DAT (cria a entrada se nao existir);
// escrita atomica (.tmp + Move) porque a pasta costuma viver no OneDrive
static void GravarSenhaFavorito(const char* ipPorta, const char* nomeSrv, const char* senha) {
    char host[64]; int porta;
    SepararIpPorta(ipPorta, host, sizeof(host), &porta);
    if (!host[0]) return;
    char arq[MAX_PATH];
    CaminhoUserdata(arq, sizeof(arq));
    if (!arq[0]) return;
    unsigned char* buf = NULL;
    DWORD tam = 0;
    HANDLE f = CreateFileA(arq, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        tam = GetFileSize(f, NULL);
        if (tam >= 12 && tam <= 1048576) {
            buf = (unsigned char*)malloc(tam);
            DWORD l2 = 0;
            if (!buf || !ReadFile(f, buf, tam, &l2, NULL) || l2 != tam || memcmp(buf, "SAMP", 4) != 0) {
                free(buf); buf = NULL; tam = 0;
            }
        } else tam = 0;
        CloseHandle(f);
    }
    // monta o arquivo novo em memoria (entradas copiadas; a do servidor ganha a senha nova)
    unsigned int cap = tam + 1024;
    unsigned char* novo = (unsigned char*)malloc(cap);
    if (!novo) { free(buf); return; }
    memcpy(novo, "SAMP", 4);
    unsigned int ver = 1, cont = 0;
    unsigned char* w = novo + 12;
    bool trocou = false;
    if (buf) {
        ver = *(unsigned int*)(buf + 4);
        unsigned int n = *(unsigned int*)(buf + 8);
        unsigned char* p = buf + 12;
        unsigned char* fim = buf + tam;
        for (unsigned int e = 0; e < n; e++) {
            unsigned int il, pt, nl2, pl, rl;
            if ((unsigned int)(fim - p) < 4) break; il = *(unsigned int*)p; p += 4;
            if (il > 256 || (unsigned int)(fim - p) < il) break; const char* ip2 = (const char*)p; p += il;
            if ((unsigned int)(fim - p) < 4) break; pt = *(unsigned int*)p; p += 4;
            if ((unsigned int)(fim - p) < 4) break; nl2 = *(unsigned int*)p; p += 4;
            if (nl2 > 256 || (unsigned int)(fim - p) < nl2) break; const char* nm2 = (const char*)p; p += nl2;
            if ((unsigned int)(fim - p) < 4) break; pl = *(unsigned int*)p; p += 4;
            if (pl > 256 || (unsigned int)(fim - p) < pl) break; const char* pw = (const char*)p; p += pl;
            if ((unsigned int)(fim - p) < 4) break; rl = *(unsigned int*)p; p += 4;
            if (rl > 256 || (unsigned int)(fim - p) < rl) break; const char* rc = (const char*)p; p += rl;
            bool essa = ((int)pt == porta && il == (unsigned int)strlen(host) && _strnicmp(ip2, host, il) == 0);
            const char* pwNovo = essa ? senha : pw;
            unsigned int plNovo = essa ? (unsigned int)strlen(senha) : pl;
            if (essa) trocou = true;
            unsigned int precisa = 4 + il + 4 + 4 + nl2 + 4 + plNovo + 4 + rl;
            if ((unsigned int)(w - novo) + precisa + 64 > cap) break; // nunca estoura
            *(unsigned int*)w = il; w += 4; memcpy(w, ip2, il); w += il;
            *(unsigned int*)w = pt; w += 4;
            *(unsigned int*)w = nl2; w += 4; memcpy(w, nm2, nl2); w += nl2;
            *(unsigned int*)w = plNovo; w += 4; memcpy(w, pwNovo, plNovo); w += plNovo;
            *(unsigned int*)w = rl; w += 4; memcpy(w, rc, rl); w += rl;
            cont++;
        }
    }
    if (!trocou) { // servidor ainda nao esta no USERDATA: apenda entrada nova
        unsigned int il = (unsigned int)strlen(host);
        unsigned int nl2 = (unsigned int)strlen(nomeSrv ? nomeSrv : "");
        if (nl2 > 96) nl2 = 96;
        unsigned int pl = (unsigned int)strlen(senha);
        if ((unsigned int)(w - novo) + 4 + il + 4 + 4 + nl2 + 4 + pl + 4 + 64 <= cap) {
            *(unsigned int*)w = il; w += 4; memcpy(w, host, il); w += il;
            *(unsigned int*)w = (unsigned int)porta; w += 4;
            *(unsigned int*)w = nl2; w += 4; memcpy(w, nomeSrv ? nomeSrv : "", nl2); w += nl2;
            *(unsigned int*)w = pl; w += 4; memcpy(w, senha, pl); w += pl;
            *(unsigned int*)w = 0; w += 4; // rcon vazio
            cont++;
        }
    }
    *(unsigned int*)(novo + 4) = ver;
    *(unsigned int*)(novo + 8) = cont;
    free(buf);
    // pasta pode nao existir (PC sem SA-MP nunca rodado): cria o caminho
    char pastaU[MAX_PATH];
    strncpy(pastaU, arq, sizeof(pastaU) - 1); pastaU[sizeof(pastaU) - 1] = 0;
    char* bU = strrchr(pastaU, '\\');
    if (bU) { *bU = 0; char* b2 = strrchr(pastaU, '\\'); if (b2) { *b2 = 0; CreateDirectoryA(pastaU, NULL); *b2 = '\\'; } CreateDirectoryA(pastaU, NULL); }
    char tmpArq[MAX_PATH + 8];
    _snprintf(tmpArq, sizeof(tmpArq) - 1, "%s.tmp", arq);
    tmpArq[sizeof(tmpArq) - 1] = 0;
    HANDLE fo = CreateFileA(tmpArq, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fo == INVALID_HANDLE_VALUE) { free(novo); return; }
    DWORD esc = 0;
    BOOL okw = WriteFile(fo, novo, (DWORD)(w - novo), &esc, NULL);
    CloseHandle(fo);
    free(novo);
    if (okw && esc == (DWORD)(w - novo)) MoveFileExA(tmpArq, arq, MOVEFILE_REPLACE_EXISTING);
    else DeleteFileA(tmpArq);
}

static bool SampValido() { // a data em uso tem samp.exe?
#ifdef TROK_TESTE_SEM_SAMP
    return false; // exe de AMOSTRA: simula um PC sem nenhum SA-MP instalado
#endif
    char c[MAX_PATH];
    _snprintf(c, MAX_PATH - 1, "%s\\samp.exe", gPastaGta);
    c[MAX_PATH - 1] = 0;
    return GetFileAttributesA(c) != INVALID_FILE_ATTRIBUTES;
}

static void IniciarConexaoReal(const char* nome, const char* ip) {
    if (gUltimoPrimeiro) { // "mover o último jogado para o início": acha pelo ip e sobe
        int achou = -1;
        EnterCriticalSection(&gLock);
        for (int i = 0; i < gNumSrv; i++) if (_stricmp(gSrv[i].ip, ip) == 0) { achou = i; break; }
        LeaveCriticalSection(&gLock);
        if (achou > 0) MoverFavorito(achou, 0); // MoverFavorito ja salva e ajusta gSel
    }

    if (gConectando) return;
    if (!gSampOk) { // sem SA-MP nada de matar processo/overlay: avisa e guia
        Avisar("Nenhum SA-MP encontrado - aponte o gta_sa.exe na aba Datas.");
        gBoasVindas = true;
        return;
    }
    // favorito com conta pre-selecionada: troca sozinho antes de conectar -
    // MAS se o usuario trocou de conta na mao nesta sessao, a escolha DELE vence
    if (!gTrocouManual) {
        for (int i = 0; i < gNumSrv; i++) {
            if (_stricmp(gSrv[i].ip, ip) != 0 || !gSrv[i].contaPref[0]) continue;
            for (int q = 0; q < gNumPerfis; q++)
                if (_stricmp(gPerfis[q].nick, gSrv[i].contaPref) == 0) { AplicarPerfil(q); break; }
            break;
        }
    }
    FecharJogoAnterior(); // o jogo morre enquanto o overlay roda
    strncpy(gRPServidor, nome, sizeof(gRPServidor) - 1); // discord rich presence
    strncpy(gConnNome, nome, sizeof(gConnNome) - 1);
    strncpy(gConnIp, ip, sizeof(gConnIp) - 1);
    gConectando = true;
    gConnT = 0;
}

// comSenha: -1 = descobrir pelo favorito (query); 0/1 explicito (lista da internet)
static void IniciarConexao(const char* nome, const char* ip, int comSenha = -1) {
    if (gConectando) return;
    if (!gSampOk) {
        Avisar("Nenhum SA-MP encontrado - aponte o gta_sa.exe na aba Datas.");
        gBoasVindas = true;
        return;
    }
    if (comSenha < 0) {
        comSenha = 0;
        for (int i = 0; i < gNumSrv; i++)
            if (_stricmp(gSrv[i].ip, ip) == 0) { comSenha = gSrv[i].senha; break; }
    }
    gConnSenha[0] = 0;
    if (comSenha) { // servidor trancado: pergunta a senha antes
        if (gSalvarSenhaServ && LerSenhaFavorito(ip, gConnSenha, sizeof(gConnSenha)) && gConnSenha[0]) {
            IniciarConexaoReal(nome, ip); // senha salva no USERDATA: conecta direto
            return;
        }
        strncpy(gPendNome, nome, sizeof(gPendNome) - 1);
        strncpy(gPendIp, ip, sizeof(gPendIp) - 1);
        gPedirSenha = true;
        return;
    }
    IniciarConexaoReal(nome, ip);
}

static void Jogar() {
    // datas mudam de PC para PC: valida ANTES e avisa em vez de falhar mudo
    {
        char sampChk[MAX_PATH];
        sprintf(sampChk, "%s\\samp.exe", gPastaGta);
        if (GetFileAttributesA(sampChk) == INVALID_FILE_ATTRIBUTES) {
            char m[200];
            _snprintf(m, sizeof(m) - 1, "samp.exe nao encontrado na data em uso - confira a pasta na aba de datas");
            m[sizeof(m) - 1] = 0;
            Avisar(m);
            return;
        }
    }
    // linha de comando vai para o samp.exe: corta qualquer caractere estranho do ip
    for (char* c = gConnIp; *c; c++) {
        bool ok = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') ||
                  *c == '.' || *c == ':' || *c == '-' || *c == '_';
        if (!ok) { *c = 0; break; }
    }
    for (char* c = gConnSenha; *c; c++) if (*c == '"') *c = '\'';
    GravarNickRegistro(); // garante que o samp.exe abre com o nick atual
    // o samp.exe decide QUAL gta_sa.exe abrir pelo registro - aponta para a DATA selecionada
    char gtaExe[MAX_PATH];
    sprintf(gtaExe, "%s\\gta_sa.exe", gPastaGta);
    HKEY k;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\SAMP", 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(k, "gta_sa_exe", 0, REG_SZ, (BYTE*)gtaExe, (DWORD)strlen(gtaExe) + 1);
        RegCloseKey(k);
    }
    char sampExe[MAX_PATH];
    sprintf(sampExe, "%s\\samp.exe", gPastaGta);
    char args[128];
    if (gConnSenha[0]) sprintf(args, "%s %s", gConnIp, gConnSenha); // senha vai na linha de comando
    else strcpy(args, gConnIp);
    gConnSenha[0] = 0;
    HINSTANCE h = ShellExecuteA(NULL, "open", sampExe, args, gPastaGta, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32) Avisar("O Windows nao conseguiu abrir o samp.exe desta data.");
    if ((INT_PTR)h > 32) {
        gRPJogando = true; // discord: "Jogando em <servidor>"
        gRPDesde = (long long)time(NULL);
        if (gFecharAoJogar) {
            // com a bandeja ligada, "fechar ao jogar" vira esconder: o rich presence continua vivo
            if (gFecharBandeja && gHwnd) ShowWindow(gHwnd, SW_HIDE);
            else gRodando = false;
        }
    }
}

// ===================== desenho do ceu =====================

struct SkyDef { ImU32 c0, c1, c2, c3; bool sol; float solX, solY, solR; };
static const SkyDef SKIES[4] = {
    { Cinza(16), Cinza(46), Cinza(140), Cinza(214), true, 0.82f, 0.62f, 62.0f },  // por-do-sol
    { Cinza(3),  Cinza(11), Cinza(22),  Cinza(34),  true, 0.76f, 0.20f, 30.0f },  // noite (lua)
    { Cinza(22), Cinza(59), Cinza(110), Cinza(150), true, 0.24f, 0.66f, 46.0f },  // amanhecer
    { Cinza(7),  Cinza(20), Cinza(33),  Cinza(46),  false, 0, 0, 0 },             // tempestade
};

static void DesenhaPalmeira(ImDrawList* dl, float x, float baseY, float h, ImU32 cor) {
    float topoX = x + h * 0.12f, topoY = baseY - h;
    dl->AddBezierQuadratic(ImVec2(x, baseY), ImVec2(x + h * 0.04f, baseY - h * 0.55f),
                           ImVec2(topoX, topoY), cor, h * 0.055f);
    for (int i = 0; i < 7; i++) {
        float ang = -2.8f + i * 0.42f;
        float fx = topoX + cosf(ang) * h * 0.42f;
        float fy = topoY + sinf(ang) * h * 0.30f + h * 0.10f;
        dl->AddBezierQuadratic(ImVec2(topoX, topoY),
                               ImVec2(topoX + cosf(ang) * h * 0.26f, topoY + sinf(ang) * h * 0.10f - h * 0.06f),
                               ImVec2(fx, fy), cor, h * 0.035f);
    }
}

static void DesenhaCeu(ImDrawList* dl, ImVec2 p0, ImVec2 p1, const SkyDef& sky, float alpha) {
    float H = p1.y - p0.y, W = p1.x - p0.x;
    float f[3] = { 0.34f, 0.62f, 0.82f };
    ImU32 cs[4] = { ComAlpha(sky.c0, alpha), ComAlpha(sky.c1, alpha), ComAlpha(sky.c2, alpha), ComAlpha(sky.c3, alpha) };
    float ys[5] = { p0.y, p0.y + H * f[0], p0.y + H * f[1], p0.y + H * f[2], p1.y };
    for (int i = 0; i < 4; i++) {
        ImU32 a = cs[i], b = cs[i + 1 > 3 ? 3 : i + 1];
        dl->AddRectFilledMultiColor(ImVec2(p0.x, ys[i]), ImVec2(p1.x, ys[i + 1]), a, a, b, b);
    }
    if (sky.sol) {
        ImVec2 c(p0.x + W * sky.solX, p0.y + H * sky.solY);
        for (int i = 4; i >= 1; i--)
            dl->AddCircleFilled(c, sky.solR * (0.4f + i * 0.25f), ComAlpha(Cinza(255), alpha * 0.10f * (5 - i)), 48);
        dl->AddCircleFilled(c, sky.solR * 0.5f, ComAlpha(Cinza(255), alpha * 0.9f), 48);
    }
    // morro a esquerda
    ImU32 corM = ComAlpha(Cinza(14), alpha);
    ImVec2 m[5] = { ImVec2(p0.x, p1.y), ImVec2(p0.x, p1.y - H * 0.30f), ImVec2(p0.x + W * 0.10f, p1.y - H * 0.42f),
                    ImVec2(p0.x + W * 0.22f, p1.y - H * 0.26f), ImVec2(p0.x + W * 0.30f, p1.y) };
    dl->AddConvexPolyFilled(m, 5, corM);
    // skyline ao centro-direita
    ImU32 corB = ComAlpha(Cinza(9), alpha);
    float bx[6] = { 0.46f, 0.51f, 0.55f, 0.60f, 0.645f, 0.68f };
    float bh[6] = { 0.34f, 0.46f, 0.30f, 0.52f, 0.38f, 0.28f };
    float bw[6] = { 0.032f, 0.026f, 0.036f, 0.028f, 0.022f, 0.034f };
    for (int i = 0; i < 6; i++)
        dl->AddRectFilled(ImVec2(p0.x + W * bx[i], p1.y - H * bh[i]), ImVec2(p0.x + W * (bx[i] + bw[i]), p1.y), corB);
    dl->AddRectFilled(ImVec2(p0.x + W * 0.607f, p1.y - H * 0.56f), ImVec2(p0.x + W * 0.613f, p1.y - H * 0.52f), corB);
    // palmeiras
    ImU32 corP = ComAlpha(Cinza(5), alpha);
    DesenhaPalmeira(dl, p0.x + W * 0.855f, p1.y, H * 0.46f, corP);
    DesenhaPalmeira(dl, p0.x + W * 0.925f, p1.y, H * 0.32f, corP);
    DesenhaPalmeira(dl, p0.x + W * 0.10f,  p1.y, H * 0.38f, corP);
    // veu de leitura (esquerda escura + topo/base)
    dl->AddRectFilledMultiColor(p0, ImVec2(p0.x + W * 0.62f, p1.y),
        ComAlpha(Cinza(10), alpha * 0.94f), ComAlpha(Cinza(10), alpha * 0.16f),
        ComAlpha(Cinza(10), alpha * 0.16f), ComAlpha(Cinza(10), alpha * 0.94f));
    dl->AddRectFilledMultiColor(ImVec2(p0.x, p1.y - H * 0.30f), p1,
        ComAlpha(Cinza(10), 0), ComAlpha(Cinza(10), 0),
        ComAlpha(Cinza(10), alpha * 0.62f), ComAlpha(Cinza(10), alpha * 0.62f));
}

// ===================== UI =====================

static ImU32 LerpCor(ImU32 a, ImU32 b, float t) {
    int ar = (a >> 0) & 0xFF, ag = (a >> 8) & 0xFF, ab2 = (a >> 16) & 0xFF, aa = (a >> 24) & 0xFF;
    int br = (b >> 0) & 0xFF, bg = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF, ba = (b >> 24) & 0xFF;
    return IM_COL32(
        ar + (int)((br - ar) * t), ag + (int)((bg - ag) * t),
        ab2 + (int)((bb - ab2) * t), aa + (int)((ba - aa) * t));
}

// retangulo arredondado com gradiente vertical (o MultiColor do imgui nao arredonda canto).
// As "tampas" tem a altura do raio - senao o imgui clampa o canto a altura da fatia e ele some.
static void RectGradVertical(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 topo, ImU32 base, float r) {
    // UMA primitiva com os vertices tingidos depois: sem emendas nem frestas de AA,
    // perfeito ate translucido (fade) - e o jeito oficial do proprio imgui
    int v0 = dl->VtxBuffer.Size;
    dl->AddRectFilled(a, b, IM_COL32_WHITE, r);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, dl->VtxBuffer.Size,
                                                  a, ImVec2(a.x, b.y), topo, base);
}

// avatar de uma conta: a IMAGEM escolhida; sem escolha, um retrato fixo derivado da cor
// (quadrado de cor so quando a pasta avatars esta vazia)
static void DesenhaAvatar(ImDrawList* d, ImVec2 a, ImVec2 b, const char* arquivo, int cor, float raio) {
    int ai = AvatarIdx(arquivo);
    if (ai < 0 && gNumAvatares > 0) ai = cor % gNumAvatares;
    if (ai >= 0 && gAvatares[ai].tex) {
        d->AddImageRounded((ImTextureID)gAvatares[ai].tex, a, b, ImVec2(0, 0), ImVec2(1, 1),
                           IM_COL32(255, 255, 255, 255), raio);
    } else {
        RectGradVertical(d, a, b, ACCENTS[cor % N_ACCENTS].hi, ACCENTS[cor % N_ACCENTS].cor, raio);
    }
}

// textura cobrindo o retangulo (corte central, sem distorcer) + mascara escura p/ leitura;
// zoom > 1 fecha o corte no centro (miniatura com a imagem inteira vira ruido)
static void ImagemCapa(ImDrawList* d, IDirect3DTexture9* tex, ImVec2 a, ImVec2 b, float raio, int mascara, float zoom = 1.0f) {
    if (!tex) return;
    D3DSURFACE_DESC td;
    if (FAILED(tex->GetLevelDesc(0, &td)) || !td.Width || !td.Height) return;
    float arCard = (b.x - a.x) / (b.y - a.y);
    float arImg = (float)td.Width / (float)td.Height;
    ImVec2 uv0(0, 0), uv1(1, 1);
    if (arImg > arCard) { float f = arCard / arImg; uv0.x = 0.5f - f * 0.5f; uv1.x = 0.5f + f * 0.5f; }
    else                { float f = arImg / arCard; uv0.y = 0.5f - f * 0.5f; uv1.y = 0.5f + f * 0.5f; }
    if (zoom > 1.0f) {
        float cxu = (uv0.x + uv1.x) * 0.5f, cyu = (uv0.y + uv1.y) * 0.5f;
        uv0.x = cxu + (uv0.x - cxu) / zoom; uv1.x = cxu + (uv1.x - cxu) / zoom;
        uv0.y = cyu + (uv0.y - cyu) / zoom; uv1.y = cyu + (uv1.y - cyu) / zoom;
    }
    d->AddImageRounded((ImTextureID)tex, a, b, uv0, uv1, IM_COL32(255, 255, 255, 255), raio);
    if (mascara > 0) d->AddRectFilled(a, b, IM_COL32(8, 8, 8, mascara), raio);
}

// botao secundario padrao do app: mesmo look do card "Adicionar data" — fundo escuro sutil,
// contorno cinza que acende no hover (cor opcional p/ acoes destrutivas)
static bool BotaoSec(const char* rotulo, ImVec2 tam, ImU32 cor = 0) {
    ImGui::PushID(rotulo);
    bool cl = ImGui::InvisibleButton("##sec", tam);
    ImGui::PopID();
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    bool hov = ImGui::IsItemHovered();
    ImDrawList* d = ImGui::GetWindowDrawList();
    if (hov) d->AddRectFilled(a, b, Cinza(255, 12), 9.0f); // vazado: so um veu leve no hover
    ImU32 tra = cor ? ComAlpha(cor, hov ? 1.0f : 0.55f) : (hov ? Cinza(200) : Cinza(120));
    d->AddRect(a, b, tra, 9.0f, 0, 1.5f);
    const char* fim = strstr(rotulo, "##"); // NULL = texto inteiro
    ImVec2 tsz = ImGui::CalcTextSize(rotulo, fim);
    d->AddText(ImVec2((a.x + b.x - tsz.x) * 0.5f, (a.y + b.y - tsz.y) * 0.5f),
               cor ? tra : (hov ? Cinza(230) : Cinza(165)), rotulo, fim);
    return cl;
}

// linha de opcao estilo lista de servidores: hover, divisoria e SWITCH a direita
// (clique em qualquer ponto da linha alterna; trilho aceso = cor de destaque)
static bool LinhaOpcao(const char* rot, bool* v, float w) {
    ImGui::PushID(rot);
    bool cl = ImGui::InvisibleButton("##lo", ImVec2(w, 46));
    ImGui::PopID();
    if (cl) *v = !*v;
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    bool hov = ImGui::IsItemHovered();
    ImDrawList* d = ImGui::GetWindowDrawList();
    const Accent& ac = AccentAtual();
    if (hov) d->AddRectFilled(a, b, Cinza(30, 165), 8);
    d->AddText(ImVec2(a.x + 10, (a.y + b.y) * 0.5f - 9), Cinza(hov ? 240 : 222), rot);
    // switch com deslize animado (anim guardada por variavel; ate 8 na tela)
    static const bool* chaves[8]; static float anims[8]; static int nT = 0;
    int slot = -1;
    for (int i = 0; i < nT; i++) if (chaves[i] == v) slot = i;
    if (slot < 0 && nT < 8) { chaves[nT] = v; anims[nT] = *v ? 1.0f : 0.0f; slot = nT++; }
    float t = *v ? 1.0f : 0.0f;
    if (slot >= 0) { anims[slot] += (t - anims[slot]) * 0.35f; t = anims[slot]; }
    ImVec2 ta(b.x - 10 - 46, (a.y + b.y) * 0.5f - 12), tb(ta.x + 46, ta.y + 24);
    d->AddRectFilled(ta, tb, *v ? ac.cor : Cinza(hov ? 64 : 52), 12.0f);
    // miolo com contraste automatico: em destaque claro (mostarda, branco) ele vira escuro
    d->AddCircleFilled(ImVec2(ta.x + 12.0f + 22.0f * t, (ta.y + tb.y) * 0.5f),
                       9.0f, *v ? TextoSobreAccent(ac.cor) : Cinza(hov ? 235 : 208), 24);
    d->AddLine(ImVec2(a.x, b.y + 1), ImVec2(b.x, b.y + 1), Cinza(31), 1.0f); // divisoria
    return cl;
}

// linha de filtro com checkbox (padrao dos dropdowns do app; marcado = accent)
static bool LinhaFiltro(const char* rot, bool* v) {
    ImGui::PushID(rot);
    bool cl = ImGui::InvisibleButton("##lf", ImVec2(222, 34));
    ImGui::PopID();
    if (cl) *v = !*v;
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    bool hov = ImGui::IsItemHovered();
    ImDrawList* d = ImGui::GetWindowDrawList();
    const Accent& ac = AccentAtual();
    if (hov) d->AddRectFilled(a, b, Cinza(255, 14), 8.0f);
    ImVec2 ka(a.x + 8, (a.y + b.y) * 0.5f - 9), kb(ka.x + 18, ka.y + 18);
    if (*v) {
        d->AddRectFilled(ka, kb, ac.cor, 5.0f);
        d->PathLineTo(ImVec2(ka.x + 4.0f, ka.y + 9.5f)); // check em caminho unico (sem dente)
        d->PathLineTo(ImVec2(ka.x + 7.5f, ka.y + 13.0f));
        d->PathLineTo(ImVec2(ka.x + 14.0f, ka.y + 5.0f));
        d->PathStroke(TextoSobreAccent(ac.cor), 0, 2.2f);
    } else {
        d->AddRect(ka, kb, hov ? Cinza(180) : Cinza(110), 5.0f, 0, 1.5f);
    }
    ImVec2 tsz = ImGui::CalcTextSize(rot);
    d->AddText(ImVec2(ka.x + 28, (a.y + b.y - tsz.y) * 0.5f), (hov || *v) ? Cinza(235) : Cinza(185), rot);
    return cl;
}

static void BotaoJanela(ImDrawList* dl, HWND hwnd) {
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    // minimizar
    ImGui::SetCursorScreenPos(ImVec2(ds.x - 76, 10));
    if (ImGui::InvisibleButton("##min", ImVec2(30, 26))) ShowWindow(hwnd, SW_MINIMIZE);
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    if (ImGui::IsItemHovered()) dl->AddRectFilled(a, b, Cinza(255, 26), 6);
    Icone(dl, ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f), I_MENOS, Cinza(200), 17.0f);
    // fechar (com a opcao ligada: esconde p/ bandeja; o icone do relogio traz de volta)
    ImGui::SetCursorScreenPos(ImVec2(ds.x - 42, 10));
    if (ImGui::InvisibleButton("##close", ImVec2(30, 26))) {
        if (gFecharBandeja) ShowWindow(hwnd, SW_HIDE);
        else gRodando = false;
    }
    // modal de boas-vindas aberto = imgui bloqueia os botoes acima; hit-test manual
    // garante que minimizar/fechar SEMPRE funcionam, mesmo sem GTA instalado
    if (gBoasVindas) {
        ImGuiIO& ioJ = ImGui::GetIO();
        ImVec2 mp = ioJ.MousePos;
        bool sobreMin = mp.x >= ds.x - 76 && mp.x <= ds.x - 46 && mp.y >= 10 && mp.y <= 36;
        bool sobreX   = mp.x >= ds.x - 42 && mp.x <= ds.x - 12 && mp.y >= 10 && mp.y <= 36;
        ImDrawList* fgJ = ImGui::GetForegroundDrawList();
        if (sobreMin) {
            fgJ->AddRectFilled(ImVec2(ds.x - 76, 10), ImVec2(ds.x - 46, 36), Cinza(255, 26), 6);
            Icone(fgJ, ImVec2(ds.x - 61, 23), I_MENOS, Cinza(230), 17.0f);
        }
        if (sobreX) {
            fgJ->AddRectFilled(ImVec2(ds.x - 42, 10), ImVec2(ds.x - 12, 36), IM_COL32(200, 60, 60, 220), 6);
            Icone(fgJ, ImVec2(ds.x - 27, 23), I_FECHAR, Cinza(235), 17.0f);
        }
        if (ImGui::IsMouseClicked(0)) {
            if (sobreMin) ShowWindow(hwnd, SW_MINIMIZE);
            else if (sobreX) {
                if (gFecharBandeja) ShowWindow(hwnd, SW_HIDE);
                else gRodando = false;
            }
        }
    }
    a = ImGui::GetItemRectMin(); b = ImGui::GetItemRectMax();
    if (ImGui::IsItemHovered()) dl->AddRectFilled(a, b, IM_COL32(200, 60, 60, 190), 6);
    Icone(dl, ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f), I_FECHAR, Cinza(220), 17.0f);
}

// desenha um icone LUCIDE centralizado (tam em px logicos; nitido em qualquer escala)
static void Icone(ImDrawList* d, ImVec2 centro, const char* gl, ImU32 cor, float tam) {
    ImFont* f = (tam > 22.0f && gFtIcoG) ? gFtIcoG : gFtIco;
    if (!f) return;
    ImVec2 sz = f->CalcTextSizeA(tam, 99999.0f, 0.0f, gl);
    d->AddText(f, tam, ImVec2(centro.x - sz.x * 0.5f, centro.y - sz.y * 0.5f), cor, gl);
}

// logo do Discord desenhado a mao: a fonte Lucide nao tem marcas, e o logo oficial
// (Clyde) e simples o bastante pra sair limpo com bezier + circulos
static void IconeDiscord(ImDrawList* d, ImVec2 c, ImU32 cor, float tam) {
    const float e = tam / 71.0f;              // o desenho abaixo usa a caixa 71x55 do SVG oficial
    const float ox = c.x - 35.5f * e, oy = c.y - 27.5f * e;
    #define DPT(X, Y) ImVec2(ox + (X) * e, oy + (Y) * e)
    // corpo (contorno arredondado do escudo)
    d->PathClear();
    d->PathLineTo(DPT(24.0f, 6.0f));
    d->PathBezierCubicCurveTo(DPT(31.0f, 4.6f), DPT(40.0f, 4.6f), DPT(47.0f, 6.0f));
    d->PathBezierCubicCurveTo(DPT(56.0f, 9.5f), DPT(63.0f, 20.0f), DPT(65.5f, 40.0f));
    d->PathBezierCubicCurveTo(DPT(60.0f, 45.5f), DPT(53.0f, 49.0f), DPT(46.0f, 50.5f));
    d->PathLineTo(DPT(42.5f, 45.0f));
    d->PathBezierCubicCurveTo(DPT(38.0f, 46.2f), DPT(33.0f, 46.2f), DPT(28.5f, 45.0f));
    d->PathLineTo(DPT(25.0f, 50.5f));
    d->PathBezierCubicCurveTo(DPT(18.0f, 49.0f), DPT(11.0f, 45.5f), DPT(5.5f, 40.0f));
    d->PathBezierCubicCurveTo(DPT(8.0f, 20.0f), DPT(15.0f, 9.5f), DPT(24.0f, 6.0f));
    d->PathFillConvex(cor);
    // os dois olhos, vazados no tom do fundo do painel
    d->AddCircleFilled(DPT(26.5f, 30.0f), 4.8f * e, IM_COL32(12, 12, 12, 255), 16);
    d->AddCircleFilled(DPT(44.5f, 30.0f), 4.8f * e, IM_COL32(12, 12, 12, 255), 16);
    #undef DPT
}

static void IconeNav(ImDrawList* dl, ImVec2 c, int tipo, ImU32 cor) {
    // a aba TrokMods usa o globo: o conteudo vem do blog, na web
    static const char* GL[7] = { I_CASA, I_SERVIDORES, I_PASTA, I_ENGRENAGEM, I_GALERIA, I_GLOBO, I_INFO };
    if (tipo >= 0 && tipo < 7) Icone(dl, c, GL[tipo], cor, 19.0f);
}

static void DesenhaUI(HWND hwnd) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 ds = io.DisplaySize;
    ImDrawList* dl;
    const Accent& AC = AccentAtual();
    float dt = io.DeltaTime;
    ReceberImagens(); // texturas que a thread de imagens terminou desde o ultimo frame
    { // sem SA-MP os botoes Jogar somem; revalida a cada ~1s (data pode mudar/sumir)
        static DWORD tChkSamp = 0;
        if (GetTickCount() - tChkSamp > 1000) {
            tChkSamp = GetTickCount();
            gSampOk = SampValido();
        }
    }

    // atalhos de teclado (fora de campos de texto e sem popup aberto)
    {
        bool algumPopup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        if (!algumPopup && !io.WantTextInput) {
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) { gTela = 1; gFocarBusca = true; } // buscar
            if (ImGui::IsKeyPressed(ImGuiKey_F5)) { // atualizar o que estiver na tela
                if (gTela == 4) EscanearFotos();
                else if (gTela == 1 && gSubAba == 1) gPubEstado = 0;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { gEscolherAvatar = false; gFiltrosAberto = false; }
        }
    }

    // fundo/ceu com crossfade
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ds);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##app", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    dl = ImGui::GetWindowDrawList();

    // fundo limpo; se o servidor selecionado tem imagem de exibicao, ela cobre o palco
    ImVec2 cen0(76, 0), cen1(ds.x, ds.y);
    dl->AddRectFilled(ImVec2(0, 0), ds, Cinza(10));
    // imagem so na Home, com CROSSFADE ao trocar de favorito
    IDirect3DTexture9* alvoF = (gTela == 0) ? gSrv[gSel].tex : NULL;
    if (alvoF != gFundoAtual) { gFundoAnt = gFundoAtual; gFundoAtual = alvoF; gFundoFade = 0.0f; }
    gFundoFade += dt * 3.0f;
    if (gFundoFade >= 1.0f) { gFundoFade = 1.0f; gFundoAnt = NULL; }
    IDirect3DTexture9* fundos[2] = { gFundoAnt, gFundoAtual };
    float alphasF[2] = { 1.0f - gFundoFade, gFundoFade };
    bool desenhouFundo = false;
    for (int fz = 0; fz < 2; fz++) {
        IDirect3DTexture9* fundo = fundos[fz];
        if (!fundo || alphasF[fz] <= 0.004f) continue;
        D3DSURFACE_DESC dsc;
        if (SUCCEEDED(fundo->GetLevelDesc(0, &dsc)) && dsc.Width && dsc.Height) {
            float arTela = (cen1.x - cen0.x) / (cen1.y - cen0.y);
            float arImg = (float)dsc.Width / (float)dsc.Height;
            ImVec2 uv0(0, 0), uv1(1, 1); // recorte "cover" centralizado
            if (arImg > arTela) { float f = arTela / arImg; uv0.x = 0.5f - f * 0.5f; uv1.x = 0.5f + f * 0.5f; }
            else                { float f = arImg / arTela; uv0.y = 0.5f - f * 0.5f; uv1.y = 0.5f + f * 0.5f; }
            dl->AddImage((ImTextureID)fundo, cen0, cen1, uv0, uv1, IM_COL32(255, 255, 255, (int)(alphasF[fz] * 255.0f)));
            desenhouFundo = true;
        }
    }
    if (desenhouFundo) {
        // sobreposicao preta translucida na capa INTEIRA: garante contraste em qualquer fundo
        dl->AddRectFilled(cen0, cen1, Cinza(4, 56));
        // veu de leitura esquerdo em CURVA (cosseno): escuro pleno ate ~1/3 da largura e
        // rolagem suave ate ~88% - sem o degrau que o linear deixava na emenda
        {
            float wTela = cen1.x - cen0.x;
            const int NF = 12;
            const float FIM = 0.88f, INI = 0.36f; // pleno ate INI, zero em FIM
            const float AMAX = 178.0f; // "um pouco mais fraco" (era 215)
            float aAnt = AMAX;
            for (int sg = 0; sg < NF; sg++) {
                float t1 = (float)(sg + 1) / NF;
                float a1;
                if (t1 * FIM <= INI) a1 = AMAX;
                else {
                    float u = (t1 * FIM - INI) / (FIM - INI);
                    a1 = AMAX * (0.5f + 0.5f * cosf(3.14159f * u)); // easing: sem quina no inicio nem no fim
                }
                // bordas EXATAS no pixel: sem sobreposicao (escurece em dobro) e sem fresta de AA
                float px0 = floorf(cen0.x + wTela * FIM * sg / NF);
                float px1 = floorf(cen0.x + wTela * FIM * t1);
                dl->AddRectFilledMultiColor(ImVec2(px0, cen0.y), ImVec2(px1, cen1.y),
                    Cinza(10, (int)aAnt), Cinza(10, (int)a1), Cinza(10, (int)a1), Cinza(10, (int)aAnt));
                aAnt = a1;
            }
        }
        dl->AddRectFilledMultiColor(ImVec2(cen0.x, cen1.y * 0.58f), cen1,
            Cinza(10, 0), Cinza(10, 0), Cinza(10, 172), Cinza(10, 172));
        dl->AddRectFilledMultiColor(cen0, ImVec2(cen1.x, 92), // topo legivel (nick/avatar/pill de update)
            Cinza(8, 212), Cinza(8, 212), Cinza(8, 0), Cinza(8, 0));
    }

    // (a sidebar agora e desenhada no FIM do frame: expandida, ela cobre o conteudo
    //  como no launcher da Rockstar - so a faixa recolhida de 76px reserva o espaco)
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(76, ds.y), IM_COL32(6, 6, 6, 250));

    // ===== topo direita: nick + avatar = UM controle so (abre/fecha o dropdown de contas) =====
    gMouseNoDrop = gContasAnim > 0.01f &&
        io.MousePos.x >= gContasA.x && io.MousePos.x <= gContasB.x &&
        io.MousePos.y >= gContasA.y && io.MousePos.y <= gContasB.y;
    ImGui::PushFont(gFtBold);
    ImVec2 nsz = ImGui::CalcTextSize(gNick);
    gCtTopA = ImVec2(ds.x - 160 - nsz.x, 6); gCtTopB = ImVec2(ds.x - 84, 54);
    if (gAttEstado == 1) { // atualizacao disponivel: botao com icone de download antes do avatar
        char rotA[48];
        sprintf(rotA, "Atualização %s", gAttVersao);
        ImVec2 asz = ImGui::CalcTextSize(rotA);
        float bwA = asz.x + 44, bxA = gCtTopA.x - 12 - bwA;
        ImGui::SetCursorScreenPos(ImVec2(bxA, 14));
        if (ImGui::InvisibleButton("##attbtn", ImVec2(bwA, 32))) gAttPopup = true;
        ImVec2 aba = ImGui::GetItemRectMin(), abb = ImGui::GetItemRectMax();
        bool ahov = ImGui::IsItemHovered();
        dl->AddRectFilled(aba, abb, ComAlpha(AC.cor, ahov ? 0.30f : 0.16f), 10);
        dl->AddRect(aba, abb, AC.cor, 10, 0, 1.4f);
        Icone(dl, ImVec2(aba.x + 17, (aba.y + abb.y) * 0.5f), I_BAIXAR, AC.hi, 16.0f);
        dl->AddText(ImVec2(aba.x + 30, (aba.y + abb.y - asz.y) * 0.5f), AC.hi, rotA);
    }
    ImGui::SetCursorScreenPos(gCtTopA);
    if (ImGui::InvisibleButton("##contatopo", ImVec2(gCtTopB.x - gCtTopA.x, gCtTopB.y - gCtTopA.y))) {
        gEscolherAvatar = !gEscolherAvatar;
        if (gEscolherAvatar) gContaEdit = -1;
    }
    bool ctHov = ImGui::IsItemHovered() || gEscolherAvatar;
    dl->AddRectFilled(gCtTopA, gCtTopB, Cinza(255, ctHov ? 20 : 14), 13); // pill sempre visivel
    dl->AddText(ImVec2(ds.x - 148 - nsz.x, 22), ctHov ? Cinza(255) : Cinza(245), gNick);
    ImGui::PopFont();
    ImVec2 avA(ds.x - 133, 8), avB(ds.x - 88, 53); // 45 px
    DesenhaAvatar(dl, avA, avB, gPerfis[gPerfilSel].avatar, gAvatarCor, 13.0f);
    if (gEscolherAvatar) dl->AddRect(avA, avB, AC.cor, 12, 0, 2.0f); // aberto: circulo no destaque
    else if (ctHov) dl->AddRect(avA, avB, Cinza(255), 12, 0, 2.0f);
    // sem iniciais: o quadrado de cor e o avatar (imagens do usuario entram depois)
    BotaoJanela(dl, hwnd);

    // lembra o ultimo servidor selecionado (persiste na troca, nao no fechamento)
    {
        static int selAnt = -1;
        if (gSel != selAnt) { if (selAnt >= 0) SalvarConfig(); selAnt = gSel; }
    }

    // ===== telas =====
    EnterCriticalSection(&gLock);
    Servidor sSel = gSrv[gSel]; // copia local dos dados da query
    LeaveCriticalSection(&gLock);
    const char* nomeSel = NomeExib(sSel);
    const char* modoSel = sSel.modoQ[0] ? sSel.modoQ : sSel.modo;

    if (gTela == 0) {
        // ---- HOME ----
        float hx = 116, hy = ds.y * 0.20f;
        float yMeta = hy + 72; // FIXO: os botoes nao mudam de lugar com/sem logo
        // slide estilo Rockstar: ao ENTRAR na home ou TROCAR de servidor, o conteudo
        // do heroi desliza da direita pra esquerda (o rail fica parado sob o mouse)
        {
            static int telaAnt = -9, selAnt = -9;
            if (telaAnt != 0 || selAnt != gSel) gHomeSlide = 0.0f;
            telaAnt = 0;
            selAnt = gSel;
        }
        gHomeSlide += dt * 1.1f; // ~900ms: bem devagar, deslizada preguicosa
        if (gHomeSlide > 1.0f) gHomeSlide = 1.0f;
        float invH = 1.0f - gHomeSlide;
        float easeH = 1.0f - invH * invH * invH * invH * invH; // quint out: pousa de leve
        float offH = 7.0f * (1.0f - easeH); // deslocamento minimo, assenta macio
        hx += offH;
        int vtxHero0 = dl->VtxBuffer.Size; // fade de entrada aplicado nos vertices do heroi
        if (sSel.texLogo) {
            // logo do servidor (discreta) + nome pequeno e sutil embaixo
            D3DSURFACE_DESC ld;
            if (SUCCEEDED(sSel.texLogo->GetLevelDesc(0, &ld)) && ld.Height) {
                float lh = 60.0f, lw = lh * (float)ld.Width / (float)ld.Height;
                float lwMax = ds.x * 0.34f;
                if (lw > lwMax) { lw = lwMax; lh = lw * (float)ld.Height / (float)ld.Width; }
                dl->AddImage((ImTextureID)sSel.texLogo, ImVec2(hx, hy - 8), ImVec2(hx + lw, hy - 8 + lh));
                // yMeta fica FIXO: Jogar/Copiar IP nao mudam de lugar com ou sem logo
            }
        } else {
            ImGui::PushFont(gFtDisplay);
            char nomeUp[96];
            int ni = 0;
            for (const char* pc = nomeSel; *pc && ni < 94; pc++) nomeUp[ni++] = (*pc >= 'a' && *pc <= 'z') ? *pc - 32 : *pc;
            nomeUp[ni] = 0;
            ImVec2 nUpSz = ImGui::CalcTextSize(nomeUp);
            dl->AddText(ImVec2(hx, hy + 10), Cinza(245), nomeUp);
            ImGui::PopFont();
        }

        float metaX = hx + 2;
        if (sSel.texLogo) { // com logo, o nome abre a linha de infos em BOLD
            ImGui::PushFont(gFtBold);
            ImVec2 nmSz = ImGui::CalcTextSize(nomeSel);
            dl->AddText(ImVec2(metaX, yMeta - 1), Cinza(238), nomeSel);
            ImGui::PopFont();
            metaX += nmSz.x + 14;
        }
        ImGui::PushFont(gFtMono);
        ImVec2 ipSz = ImGui::CalcTextSize(sSel.ip);
        char resto[220];
        if (sSel.ping >= 0)
            sprintf(resto, "jogadores %d/%d   ping %d ms   %s", sSel.online, sSel.maxp, sSel.ping, modoSel);
        else
            sprintf(resto, "consultando servidor...   %s", modoSel);
        ImVec2 rSz = ImGui::CalcTextSize(resto);
        dl->AddText(ImVec2(metaX, yMeta), Cinza(182), sSel.ip);
        // icone COPIAR colado no ip (respiro maior antes do resto, p/ ler como par ip+copiar)
        {
            static float tCopiado = 0;
            if (tCopiado > 0) tCopiado -= dt;
            float icoX = metaX + ipSz.x + 6;
            ImGui::SetCursorScreenPos(ImVec2(icoX - 3, yMeta - 4));
            bool cpc = ImGui::InvisibleButton("##copiaip", ImVec2(24, 26));
            bool cph = ImGui::IsItemHovered();
            float ccx = icoX + 7, ccy = yMeta + ipSz.y * 0.5f;
            if (tCopiado > 0) Icone(dl, ImVec2(ccx, ccy), I_CHECK, AC.hi, 16.0f);
            else Icone(dl, ImVec2(ccx, ccy), I_COPIAR, Cinza(cph ? 250 : 150), 15.0f);
            Dica(tCopiado > 0 ? "Copiado!" : "Copiar o IP");
            if (cpc && OpenClipboard(hwnd)) {
                EmptyClipboard();
                HGLOBAL m = GlobalAlloc(GMEM_MOVEABLE, strlen(sSel.ip) + 1);
                if (m) { memcpy(GlobalLock(m), sSel.ip, strlen(sSel.ip) + 1); GlobalUnlock(m); SetClipboardData(CF_TEXT, m); }
                CloseClipboard();
                tCopiado = 1.4f;
            }
        }
        // 2a linha de info: jogadores/ping/modo abaixo do ip (respira melhor)
        dl->AddText(ImVec2(hx + 2, yMeta + 26), Cinza(170), resto);
        ImGui::PopFont();

        // botao Jogar - retangulo arredondado, gradiente limpo, sem luz nem sombra
        {
        ImGui::SetCursorScreenPos(ImVec2(hx, yMeta + 62)); // desceu: a info agora tem 2 linhas
        ImGui::PushFont(gFtBotao);
        bool clicou = ImGui::InvisibleButton("##jogar", ImVec2(256, 62));
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        bool hov = ImGui::IsItemHovered();
        // formato original (raio 16); fade LEVE (cor -> um passo em direcao ao hi) e halo bem sutil
        ImU32 gTopo = hov ? LerpCor(AC.cor, IM_COL32(255, 255, 255, 255), 0.08f) : AC.cor;
        ImU32 gBase = LerpCor(gTopo, AC.hi, 0.55f);
        BrilhoSuave(dl, a, b, AC.cor, hov ? 12.0f : 9.0f, hov ? 38 : 22); // hover = o "normal" de antes
        RectGradVertical(dl, a, b, gTopo, gBase, 16.0f);
        ImVec2 jsz = ImGui::CalcTextSize("Jogar");
        float cxm = (a.x + b.x) * 0.5f + 12.0f;
        dl->AddText(ImVec2(cxm - jsz.x * 0.5f, (a.y + b.y - jsz.y) * 0.5f - 1), TextoSobreAccent(AC.cor), "Jogar");
        float ty = (a.y + b.y) * 0.5f, tx = cxm - jsz.x * 0.5f - 30.0f;
        dl->AddTriangleFilled(ImVec2(tx, ty - 9), ImVec2(tx, ty + 9), ImVec2(tx + 15, ty), TextoSobreAccent(AC.cor));
        ImGui::PopFont();
        if (clicou && !gConectando) IniciarConexao(nomeSel, sSel.ip);
        }
        // (o Copiar IP virou icone sutil na linha de infos, ao lado do ip)

        // links oficiais do servidor (configurados no Editar servidor)
        {
            float sx = hx + 2;
            ImGui::PushFont(gFtBold);
            for (int q = 0; q < 4; q++) {
                if (!sSel.sites[q][0]) continue;
                char rotL[24], urlL[150];
                SepararLink(sSel.sites[q], q, rotL, sizeof(rotL), urlL, sizeof(urlL));
                ImVec2 ssz = ImGui::CalcTextSize(rotL);
                ImGui::SetCursorScreenPos(ImVec2(sx, yMeta + 140)); // acompanha as 2 linhas de info
                char sid[16]; sprintf(sid, "##lnk%d", q);
                bool clL = ImGui::InvisibleButton(sid, ImVec2(ssz.x + 8, 28));
                ImVec2 lba = ImGui::GetItemRectMin();
                bool lbh = ImGui::IsItemHovered();
                // sem borda: so o texto; sublinhado no hover
                dl->AddText(ImVec2(lba.x + 4, lba.y + 4), Cinza(lbh ? 255 : 205), rotL);
                if (lbh) dl->AddRectFilled(ImVec2(lba.x + 4, lba.y + 25), ImVec2(lba.x + 4 + ssz.x, lba.y + 26.5f), Cinza(235));
                if (lbh) { // pra onde o link vai, como a barra de status do navegador.
                    char dica[224];  // texto solto em cinza claro, sem caixa (o tooltip padrao
                    if (strstr(urlL, "://")) { strncpy(dica, urlL, sizeof(dica) - 1); dica[sizeof(dica) - 1] = 0; }
                    else sprintf(dica, "https://%.190s", urlL);          // sairia sem respiro: a tela roda com padding 0)
                    ImGui::PushFont(gFtMiniLeve);      // so o texto, seguindo o mouse (sem caixa)
                    ImVec2 tsz = ImGui::CalcTextSize(dica);
                    ImVec2 mp = ImGui::GetMousePos(), tela = ImGui::GetIO().DisplaySize;
                    float px = mp.x + 18, py = mp.y + 20;
                    if (px + tsz.x > tela.x - 12) px = mp.x - 18 - tsz.x;  // vira pro outro lado na borda
                    if (px < 12) px = 12;
                    if (py + tsz.y > tela.y - 10) py = mp.y - 10 - tsz.y;
                    ImGui::GetForegroundDrawList()->AddText(ImVec2(px, py), Cinza(175), dica);
                    ImGui::PopFont();
                }
                if (clL) {
                    char url[224];
                    if (strstr(urlL, "://")) { strncpy(url, urlL, sizeof(url) - 1); url[sizeof(url) - 1] = 0; }
                    else sprintf(url, "https://%.190s", urlL);
                    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
                }
                sx += ssz.x + 8 + 20;
            }
            ImGui::PopFont();
        }

        // fade de ENTRADA do heroi: comeca em 35% e fecha rapido (em ~60% do slide) -
        // alto o bastante pra nao "descolar" as camadas de sombra (o artefato de antes)
        if (gHomeSlide < 0.6f) {
            float fT = gHomeSlide / 0.6f;
            int fA = (int)(255.0f * (0.35f + 0.65f * fT));
            for (int vv = vtxHero0; vv < dl->VtxBuffer.Size; vv++) {
                ImU32 c = dl->VtxBuffer[vv].col;
                ImU32 a = (c >> 24) & 255;
                a = a * fA / 255;
                dl->VtxBuffer[vv].col = (c & 0x00FFFFFF) | (a << 24);
            }
        }
        // ---- rail de favoritos (com scroll horizontal) ----
        hx -= offH; // o slide e SO do conteudo de cima: o rail nao sai do lugar
        float ry = ds.y - 202;
        ImGui::PushFont(gFtMini);
        ImVec2 favSz = ImGui::CalcTextSize("F A V O R I T O S");
        dl->AddText(ImVec2(hx, ry - 22), Cinza(156), "F A V O R I T O S");
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(ImVec2(hx, ry));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 9.0f);
        ImGui::BeginChild("##rail", ImVec2(ds.x - hx - 36, 170), false, ImGuiWindowFlags_HorizontalScrollbar);
        ImDrawList* rl = ImGui::GetWindowDrawList();
        float cx = 0;
        for (int i = 0; i < gNumSrv; i++) {
            EnterCriticalSection(&gLock);
            Servidor sc = gSrv[i];
            LeaveCriticalSection(&gLock);
            ImGui::SetCursorPos(ImVec2(cx, 6));
            char id[12]; sprintf(id, "##card%d", i);
            bool cl = ImGui::InvisibleButton(id, ImVec2(274, 144));
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { // botao direito = opcoes
                gEditSrv = i;
                strncpy(gEditApelido, sc.apelido, sizeof(gEditApelido) - 1);
            }
            // arrastar o card: ele some daqui e o card completo segue o mouse (ghost global)
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
                ImGui::SetDragDropPayload("TROK_FAV", &i, sizeof(int));
                ImGui::EndDragDropSource();
            }
            ImVec2 ca = ImGui::GetItemRectMin(), cb = ImGui::GetItemRectMax();
            bool ch = ImGui::IsItemHovered();
            // hover animado: zoom da capa, logo e o levantar do card interpolam suave
            float velH = dt * 10.0f; if (velH > 1.0f) velH = 1.0f;
            gHovCard[i] += ((ch ? 1.0f : 0.0f) - gHovCard[i]) * velH;
            float ha = gHovCard[i];
            ca.y -= 4.0f * ha; cb.y -= 4.0f * ha;
            const ImGuiPayload* payAtivo = ImGui::GetDragDropPayload();
            bool euArrastado = payAtivo && payAtivo->IsDataType("TROK_FAV") && *(const int*)payAtivo->Data == i;
            bool mhc = false;
            if (!euArrastado) {
            rl->AddRectFilled(ca, cb, IM_COL32(18, 18, 18, ch ? 236 : 205), 13);
            if (sc.tex) // capa do servidor no card: zoom por ser miniatura (+ um pouco no hover)
                ImagemCapa(rl, sc.tex, ca, cb, 13.0f, (int)(194.0f - 22.0f * ha), 1.35f + 0.20f * ha);
            ImU32 borda = (i == gSel) ? AC.cor : (ch ? Cinza(74) : Cinza(40));
            rl->AddRect(ca, cb, borda, 13, 0, (i == gSel) ? 2.0f : 1.0f);
            // bolinha de status + nome clipado ANTES do "..." (que fica no topo direito)
            BolaStatus(rl, ImVec2(ca.x + 19, ca.y + 22), sc.ping, sc.senha);
            ImGui::PushClipRect(ImVec2(ca.x, ca.y), ImVec2(cb.x - 44, ca.y + 36), true);
            ImGui::PushFont(gFtCardNome);
            rl->AddText(ImVec2(ca.x + 30, ca.y + 12), Cinza(240), NomeExib(sc));
            ImGui::PopFont();
            ImGui::PopClipRect();
            if (!sc.texLogo) { // descricao so nos cards SEM logo
                ImGui::PushClipRect(ImVec2(ca.x, ca.y), ImVec2(cb.x - 12, cb.y), true);
                ImGui::PushFont(gFtCardDesc);
                rl->AddText(ImVec2(ca.x + 15, ca.y + 34), Cinza(152), sc.modoQ[0] ? sc.modoQ : sc.modo);
                ImGui::PopFont();
                ImGui::PopClipRect();
            } else { // logo maior, centralizada no espaco livre entre o nome e os numeros
                D3DSURFACE_DESC ld2;
                if (SUCCEEDED(sc.texLogo->GetLevelDesc(0, &ld2)) && ld2.Height) {
                    float escL = 1.0f + 0.10f * ha; // hover: logo cresce junto com o zoom da capa
                    float lh2 = 40.0f * escL, lw2 = lh2 * (float)ld2.Width / (float)ld2.Height;
                    if (lw2 > 160.0f * escL) { lw2 = 160.0f * escL; lh2 = lw2 * (float)ld2.Height / (float)ld2.Width; }
                    float topoL = ca.y + 36, baseL = cb.y - 28;
                    float lx = (ca.x + cb.x - lw2) * 0.5f, ly = topoL + (baseL - topoL - lh2) * 0.5f;
                    rl->AddImage((ImTextureID)sc.texLogo, ImVec2(lx, ly), ImVec2(lx + lw2, ly + lh2));
                }
            }
            ImGui::PushFont(gFtMonoS);
            char pinfo[48];
            if (sc.ping >= 0) sprintf(pinfo, "%d/%d", sc.online, sc.maxp); else strcpy(pinfo, "--/--");
            rl->AddText(ImVec2(ca.x + 15, cb.y - 28), Cinza(158), pinfo);
            char ptxt[24];
            if (sc.ping >= 0) sprintf(ptxt, "%d ms", sc.ping); else strcpy(ptxt, "-- ms");
            ImVec2 psz = ImGui::CalcTextSize(ptxt);
            rl->AddText(ImVec2(cb.x - 15 - psz.x, cb.y - 28), CorPing(sc.ping), ptxt);
            ImGui::PopFont();
            // "..." do card: editar apelido/imagem/remover
            ImVec2 ma(cb.x - 36, ca.y + 8), mb(cb.x - 10, ca.y + 28);
            mhc = ImGui::IsMouseHoveringRect(ma, mb);
            DesenhaReticencias(rl, ma, mb, mhc);
            if (mhc && !gMouseNoDrop) ImGui::SetTooltip("Editar servidor");
            if (mhc && !gMouseNoDrop && ImGui::IsMouseClicked(0)) {
                gEditSrv = i;
                strncpy(gEditApelido, sc.apelido, sizeof(gEditApelido) - 1);
            }
            } else {
                rl->AddRect(ca, cb, Cinza(46, 200), 13, 0, 1.0f); // slot vazio: o card esta no mouse
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pay = ImGui::AcceptDragDropPayload("TROK_FAV",
                        ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
                    float pulso = 0.55f + 0.45f * sinf((float)ImGui::GetTime() * 7.0f);
                    rl->AddRectFilled(ImVec2(ca.x - 8, ca.y + 8), ImVec2(ca.x - 4, cb.y - 8), ComAlpha(AC.cor, pulso), 2);
                    rl->AddRect(ca, cb, ComAlpha(AC.cor, 0.35f + 0.35f * pulso), 13, 0, 2.0f);
                    if (pay->IsDelivery()) MoverFavorito(*(const int*)pay->Data, i);
                }
                ImGui::EndDragDropTarget();
            }
            if (cl && !mhc) gSel = i;
            cx += 288;
        }
        ImGui::SetCursorPos(ImVec2(cx > 0 ? cx - 14 : 0, 6));
        ImGui::Dummy(ImVec2(1, 1)); // estende a area de scroll ate o ultimo card
        bool railTemMais = ImGui::GetScrollX() < ImGui::GetScrollMaxX() - 2.0f;
        bool railTemAntes = ImGui::GetScrollX() > 2.0f;
        // fades anticorte no FOREGROUND: acima de card, borda e degrade, garantido
        // (guarda p/ nao furar o dim de modal/dropdown/overlay)
        {
            bool coberto = gConectando ||
                ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
            // dropdown de contas so esconde o fade se ELE estiver por cima (dropdown curto
            // fica longe do rail; sumir o fade com ele aberto deixava o card com corte seco)
            bool dropNoRail = gContasAnim > 0.01f && gContasB.y > ry;
            ImDrawList* fgR = ImGui::GetForegroundDrawList();
            if (railTemMais && !coberto && !(dropNoRail && gContasB.x > ds.x - 78))
                fgR->AddRectFilledMultiColor(ImVec2(ds.x - 78, ry), ImVec2(ds.x - 36, ry + 156),
                    Cinza(10, 0), Cinza(10, 235), Cinza(10, 235), Cinza(10, 0));
            if (railTemAntes && !coberto && !(dropNoRail && gContasA.x < hx + 42))
                fgR->AddRectFilledMultiColor(ImVec2(hx, ry), ImVec2(hx + 42, ry + 156),
                    Cinza(10, 235), Cinza(10, 0), Cinza(10, 0), Cinza(10, 235));
        }
        // a roda rola o rail de QUALQUER lugar da Home (menos com modal/dropdown aberto)
        if (io.MouseWheel != 0 && gContasAnim <= 0.01f && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) && !gConectando)
            ImGui::SetScrollX(ImGui::GetScrollX() - io.MouseWheel * 70.0f);
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
    else if (gTela == 1) {
        // ---- SERVIDORES ----
        ImGui::SetCursorScreenPos(ImVec2(116, 78));
        ImGui::BeginChild("##painel", ImVec2(ds.x - 148, ds.y - 130), false);
        ImDrawList* pl = ImGui::GetWindowDrawList();
        ImVec2 pa = ImGui::GetWindowPos(), pb = ImVec2(pa.x + ImGui::GetWindowSize().x, pa.y + ImGui::GetWindowSize().y);
        pl->AddRectFilled(pa, pb, IM_COL32(12, 12, 12, 226), 16);
        pl->AddRect(pa, pb, Cinza(40), 16, 0, 1);
        ImGui::SetCursorPos(ImVec2(24, 20));
        ImGui::PushFont(gFtBotao);
        ImGui::TextColored(ImColor(Cinza(245)), "SERVIDORES");
        ImGui::PopFont();
        // sub-abas estilo Riot: texto com sublinhado no ativo
        ImGui::PushFont(gFtBold);
        char abaF[32], abaI[32];
        sprintf(abaF, "Favoritos (%d)", gNumSrv);
        sprintf(abaI, gPubEstado == 2 ? "Internet (%d)" : "Internet", gNumPub);
        for (int t2 = 0; t2 < 2; t2++) {
            const char* rot = t2 == 0 ? abaF : abaI;
            ImVec2 rsz = ImGui::CalcTextSize(rot);
            ImGui::SetCursorPos(ImVec2(230.0f + t2 * 150.0f, 28.0f));
            char idab[12]; sprintf(idab, "##aba%d", t2);
            if (ImGui::InvisibleButton(idab, ImVec2(rsz.x + 8, 30))) gSubAba = t2;
            ImVec2 ta = ImGui::GetItemRectMin();
            bool at = (gSubAba == t2), hab = ImGui::IsItemHovered();
            pl->AddText(ImVec2(ta.x + 4, ta.y + 2), at ? Cinza(250) : (hab ? Cinza(210) : Cinza(140)), rot);
            if (at) pl->AddRectFilled(ImVec2(ta.x + 4, ta.y + 26), ImVec2(ta.x + 4 + rsz.x, ta.y + 29), AC.cor, 2);
        }
        ImGui::PopFont();
        ImGui::SetCursorPos(ImVec2(24, 58));
        ImGui::PushFont(gFtMono);
        float wBusca = pb.x - pa.x - 48 - 44.0f; // reserva o funil (favoritos e internet)
        ImGui::PushItemWidth(wBusca);
        if (gFocarBusca) { ImGui::SetKeyboardFocusHere(); gFocarBusca = false; } // Ctrl+F
        ImGui::InputTextWithHint("##busca", "Buscar por nome, modo ou IP...", gBusca, sizeof(gBusca));
        ImGui::PopItemWidth();
        {   // lupa no canto direito da barra de busca
            ImVec2 ba2 = ImGui::GetItemRectMin(), bb2 = ImGui::GetItemRectMax();
            float lcx = bb2.x - 24, lcy = (ba2.y + bb2.y) * 0.5f - 2;
            pl->AddCircle(ImVec2(lcx, lcy), 5.5f, Cinza(125), 16, 1.7f);
            pl->AddLine(ImVec2(lcx + 4.2f, lcy + 4.2f), ImVec2(lcx + 8.5f, lcy + 8.5f), Cinza(125), 1.7f);
        }
        ImGui::PopFont();
        {
            // funil de filtros ao lado da busca (favoritos e internet); opcoes num dropdown
            ImGui::SetCursorPos(ImVec2(24 + wBusca + 10, 58));
            if (ImGui::InvisibleButton("##btnfiltro", ImVec2(34, 30))) gFiltrosAberto = !gFiltrosAberto;
            ImVec2 fa = ImGui::GetItemRectMin(), fb = ImGui::GetItemRectMax();
            bool fh = ImGui::IsItemHovered();
            gFiltroBtnA = fa; gFiltroBtnB = fb;
            if (fh || gFiltrosAberto) pl->AddRectFilled(fa, fb, Cinza(255, 14), 8);
            float fcx = (fa.x + fb.x) * 0.5f, fcy = (fa.y + fb.y) * 0.5f;
            ImU32 fc = gFiltrosAberto ? AC.cor : (fh ? Cinza(230) : Cinza(160)); // aberto = funil no accent
            Icone(pl, ImVec2(fcx, fcy), I_FUNIL, fc, 17.0f);
            if (gOcCheios || gOcSenha || gOcVazios || gOcOff) // badge: tem filtro ativo
                pl->AddCircleFilled(ImVec2(fb.x - 3, fa.y + 3), 3.5f, AC.cor, 16);
            Dica("Filtros da lista");
        }
        // colunas proporcionais a largura (alinhadas entre header e linhas)
        float wLista = pb.x - pa.x - 48;
        float reserva = (gSubAba == 0) ? 100.0f : 200.0f; // espaco dos botoes a direita
        float colModo = (wLista - reserva) * 0.42f;
        float colJog  = (wLista - reserva) * 0.68f;
        float colPing = (wLista - reserva) * 0.85f;
        // cabecalho clicavel: 1 clique crescente, 2 decrescente, 3 volta a ordem original
        {
            const char* HC[4] = { "SERVIDOR", "MODO", "JOGADORES", "PING" };
            float hx4[4] = { 34.0f, 24.0f + colModo, 24.0f + colJog, 24.0f + colPing };
            int nCols = (gSubAba == 0) ? 4 : 3; // internet nao tem coluna de ping
            ImGui::PushFont(gFtMini);
            for (int c = 0; c < nCols; c++) {
                ImGui::SetCursorPos(ImVec2(hx4[c], 104));
                ImVec2 hsz = ImGui::CalcTextSize(HC[c]);
                char idh[8]; sprintf(idh, "##hc%d", c);
                if (ImGui::InvisibleButton(idh, ImVec2(hsz.x + 16, 18))) {
                    if (gOrdCol != c) { gOrdCol = c; gOrdAsc = true; }
                    else if (gOrdAsc) gOrdAsc = false;
                    else gOrdCol = -1;
                }
                ImVec2 ha = ImGui::GetItemRectMin();
                ImU32 hcc = (gOrdCol == c) ? Cinza(240) : (ImGui::IsItemHovered() ? Cinza(200) : Cinza(125));
                pl->AddText(ImVec2(ha.x, ha.y + 2), hcc, HC[c]);
                if (gOrdCol == c) {
                    float sx = ha.x + hsz.x + 6, sy = ha.y + 6;
                    if (gOrdAsc) pl->AddTriangleFilled(ImVec2(sx, sy + 6), ImVec2(sx + 8, sy + 6), ImVec2(sx + 4, sy), hcc);
                    else pl->AddTriangleFilled(ImVec2(sx, sy), ImVec2(sx + 8, sy), ImVec2(sx + 4, sy + 6), hcc);
                }
            }
            ImGui::PopFont();
        }
        ImGui::SetCursorPos(ImVec2(24, 128));
        ImGui::BeginChild("##lista", ImVec2(pb.x - pa.x - 48, pb.y - pa.y - 148), false);
        if (gSubAba == 0) {
        int idxs[MAX_SERVIDORES];
        for (int k = 0; k < gNumSrv; k++) idxs[k] = k;
        if (gOrdCol >= 0) qsort(idxs, gNumSrv, sizeof(int), CmpFav);
        for (int k = 0; k < gNumSrv; k++) {
            int i = idxs[k];
            EnterCriticalSection(&gLock);
            Servidor sc = gSrv[i];
            LeaveCriticalSection(&gLock);
            const char* nm = NomeExib(sc);
            const char* md = sc.modoQ[0] ? sc.modoQ : sc.modo;
            // filtros (nos desconhecidos - sem resposta - so o "ocultar sem resposta" age)
            if (gOcOff && sc.ping < 0) continue;
            if (gOcCheios && sc.ping >= 0 && sc.maxp > 0 && sc.online >= sc.maxp) continue;
            if (gOcSenha && sc.ping >= 0 && sc.senha) continue;
            if (gOcVazios && sc.ping >= 0 && sc.online == 0) continue;
            if (gBusca[0]) {
                char tudo[256];
                sprintf(tudo, "%s %s %s", nm, md, sc.ip);
                bool acha = false;
                for (char* pp = tudo; *pp; pp++) if (_strnicmp(pp, gBusca, strlen(gBusca)) == 0) { acha = true; break; }
                if (!acha) continue;
            }
            ImGui::PushID(i);
            ImVec2 la = ImGui::GetCursorScreenPos();
            float wRow = ImGui::GetContentRegionAvail().x;
            if (ImGui::InvisibleButton("##linha", ImVec2(wRow - 100, 54))) { gSel = i; gTela = 0; }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { // botao direito = opcoes do servidor
                gEditSrv = i;
                strncpy(gEditApelido, sc.apelido, sizeof(gEditApelido) - 1);
            }
            ImVec2 lb = ImGui::GetItemRectMax();
            ImDrawList* ll = ImGui::GetWindowDrawList();
            if (ImGui::IsItemHovered()) ll->AddRectFilled(la, ImVec2(la.x + wRow, lb.y), Cinza(30, 165), 8);
            BolaStatus(ll, ImVec2(la.x + 14, la.y + 18), sc.ping, sc.senha); // alinhada ao NOME
            ImGui::PushFont(gFtBold);
            ll->AddText(ImVec2(la.x + 26, la.y + 8), Cinza(240), nm);
            ImGui::PopFont();
            ImGui::PushFont(gFtMini);
            ll->AddText(ImVec2(la.x + 26, la.y + 30), Cinza(140), sc.ip);
            ImGui::PopFont();
            ImGui::PushFont(gFtMono);
            char col[64];
            if (sc.ping >= 0) sprintf(col, "%d/%d", sc.online, sc.maxp); else strcpy(col, "--/--");
            ImGui::PushClipRect(ImVec2(la.x + colModo, la.y), ImVec2(la.x + colJog - 14, lb.y), true);
            ll->AddText(ImVec2(la.x + colModo, la.y + 17), Cinza(165), md);
            ImGui::PopClipRect();
            ll->AddText(ImVec2(la.x + colJog, la.y + 17), Cinza(165), col);
            char pt[24];
            if (sc.ping >= 0) sprintf(pt, "%d ms", sc.ping); else strcpy(pt, "-- ms");
            ll->AddText(ImVec2(la.x + colPing, la.y + 17), CorPing(sc.ping), pt);
            ImGui::PopFont();
            // arrastar e soltar para reordenar (o card completo segue o mouse - ghost global)
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
                ImGui::SetDragDropPayload("TROK_FAV", &i, sizeof(int));
                ImGui::EndDragDropSource();
            }
            const ImGuiPayload* payAtivo = ImGui::GetDragDropPayload();
            if (payAtivo && payAtivo->IsDataType("TROK_FAV") && *(const int*)payAtivo->Data == i)
                ll->AddRectFilled(la, ImVec2(la.x + wRow, lb.y), Cinza(10, 140), 8);
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pay = ImGui::AcceptDragDropPayload("TROK_FAV",
                        ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
                    float pulso = 0.55f + 0.45f * sinf((float)ImGui::GetTime() * 7.0f);
                    ll->AddRectFilled(ImVec2(la.x, la.y - 3), ImVec2(la.x + wRow, la.y), ComAlpha(AC.cor, pulso), 2);
                    if (pay->IsDelivery()) MoverFavorito(*(const int*)pay->Data, i);
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::SameLine();
            {
            ImGui::SetCursorScreenPos(ImVec2(la.x + wRow - 88, la.y + 11)); // ancorado a direita
            ImGui::PushFont(gFtBold);
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(AC.cor));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(AC.hi));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(AC.hi));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(TextoSobreAccent(AC.cor)));
            if (ImGui::Button("Jogar", ImVec2(80, 32)) && !gConectando) { gSel = i; IniciarConexao(nm, sc.ip); }
            ImGui::PopStyleColor(4);
            ImGui::PopFont();
            }
            ImGui::PopID();
            ImVec2 sep = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(ImVec2(la.x, sep.y), ImVec2(la.x + wRow, sep.y), Cinza(31), 1);
            ImGui::Dummy(ImVec2(0, 3));
        }
        }
        else {
            // ---- INTERNET: lista publica (baixa uma vez, favoritar grava no ini) ----
            if (gPubEstado == 0) CreateThread(NULL, 0, ThreadPublicos, NULL, 0, NULL);
            if (gPubEstado == 1) {
                ImGui::SetCursorPos(ImVec2(10, 18));
                ImGui::PushFont(gFtMono);
                ImGui::TextColored(ImColor(Cinza(150)), "baixando a lista de servidores...");
                ImGui::PopFont();
            } else if (gPubEstado == -1) {
                ImGui::SetCursorPos(ImVec2(10, 18));
                ImGui::PushFont(gFtBold);
                if (BotaoSec("A lista não veio. Tentar de novo", ImVec2(280, 36))) gPubEstado = 0;
                ImGui::PopFont();
            } else {
                static int idxsP[MAX_PUB];
                static int ordColAnt = -99, nPAnt = -1;
                static bool ordAscAnt = false;
                int nP = gNumPub;
                if (gOrdCol != ordColAnt || gOrdAsc != ordAscAnt || nP != nPAnt) {
                    // reordena SO quando algo muda (era um qsort de 500 itens POR FRAME)
                    for (int k = 0; k < nP; k++) idxsP[k] = k;
                    if (gOrdCol >= 0) qsort(idxsP, nP, sizeof(int), CmpPub);
                    ordColAnt = gOrdCol; ordAscAnt = gOrdAsc; nPAnt = nP;
                }
                int mostrados = 0;
                for (int k = 0; k < nP && mostrados < 150; k++) {
                    int i = idxsP[k];
                    SrvPub& sp = gPub[i];
                    if (gOcCheios && sp.maxp > 0 && sp.on >= sp.maxp) continue;
                    if (gOcSenha && sp.pw) continue;
                    if (gOcVazios && sp.on == 0) continue;
                    if (gBusca[0]) {
                        char tudo[256];
                        sprintf(tudo, "%s %s %s", sp.nome, sp.modo, sp.ip);
                        bool acha = false;
                        for (char* pp = tudo; *pp; pp++) if (_strnicmp(pp, gBusca, strlen(gBusca)) == 0) { acha = true; break; }
                        if (!acha) continue;
                    }
                    mostrados++;
                    bool jaFav = false;
                    for (int j = 0; j < gNumSrv; j++) if (_stricmp(gSrv[j].ip, sp.ip) == 0) { jaFav = true; break; }
                    ImGui::PushID(10000 + i);
                    ImVec2 la = ImGui::GetCursorScreenPos();
                    float wRow = ImGui::GetContentRegionAvail().x;
                    ImGui::InvisibleButton("##lpub", ImVec2(wRow - 186, 54));
                    ImVec2 lb = ImGui::GetItemRectMax();
                    ImDrawList* ll = ImGui::GetWindowDrawList();
                    if (ImGui::IsItemHovered()) ll->AddRectFilled(la, ImVec2(la.x + wRow, lb.y), Cinza(30, 165), 8);
                    BolaStatus(ll, ImVec2(la.x + 14, la.y + 18), 0, sp.pw); // na lista = online; alinhada ao NOME
                    ImGui::PushFont(gFtBold);
                    ImGui::PushClipRect(la, ImVec2(la.x + 316, lb.y), true);
                    ll->AddText(ImVec2(la.x + 26, la.y + 8), Cinza(240), sp.nome);
                    ImGui::PopClipRect();
                    ImGui::PopFont();
                    ImGui::PushFont(gFtMini);
                    ll->AddText(ImVec2(la.x + 26, la.y + 30), Cinza(140), sp.ip);
                    ImGui::PopFont();
                    ImGui::PushFont(gFtMono);
                    char col[64];
                    sprintf(col, "%d/%d", sp.on, sp.maxp);
                    ImGui::PushClipRect(ImVec2(la.x + colModo, la.y), ImVec2(la.x + colJog - 14, lb.y), true);
                    ll->AddText(ImVec2(la.x + colModo, la.y + 17), Cinza(165), sp.modo);
                    ImGui::PopClipRect();
                    ll->AddText(ImVec2(la.x + colJog, la.y + 17), sp.on > 0 ? Cinza(230) : Cinza(120), col);
                    ImGui::PopFont();
                    ImGui::SameLine();
                    // marcador (bookmark) de favorito: contorno = favoritar; cheio no accent = salvo
                    ImGui::SetCursorScreenPos(ImVec2(la.x + wRow - 124, la.y + 11));
                    ImGui::PushFont(gFtBold);
                    bool favCl = ImGui::InvisibleButton("##fav", ImVec2(36, 32));
                    {
                        ImVec2 fa3 = ImGui::GetItemRectMin(), fb3 = ImGui::GetItemRectMax();
                        bool fHov3 = ImGui::IsItemHovered();
                        if (fHov3 && !jaFav) ll->AddRectFilled(fa3, fb3, Cinza(255, 16), 8);
                        float scx = (fa3.x + fb3.x) * 0.5f, scy = (fa3.y + fb3.y) * 0.5f;
                        // bookmark lucide: salvo = accent, resto = cinza
                        Icone(ll, ImVec2(scx, scy), I_FAVORITO,
                              jaFav ? AC.cor : Cinza(fHov3 ? 245 : 150), 19.0f);
                    }
                    Dica(jaFav ? "Já está nos favoritos" : "Adicionar aos favoritos");
                    if (favCl && !jaFav) FavoritarPublico(i);
                    {
                    ImGui::SetCursorScreenPos(ImVec2(la.x + wRow - 78, la.y + 11)); // mesmo y do marcador
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(AC.cor));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(AC.hi));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(AC.hi));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(TextoSobreAccent(AC.cor)));
                    if (ImGui::Button("Jogar", ImVec2(70, 32)) && !gConectando) IniciarConexao(sp.nome, sp.ip, sp.pw);
                    ImGui::PopStyleColor(4);
                    }
                    ImGui::PopFont();
                    ImGui::PopID();
                    ImVec2 sep2 = ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddLine(ImVec2(la.x, sep2.y), ImVec2(la.x + wRow, sep2.y), Cinza(31), 1);
                    ImGui::Dummy(ImVec2(0, 3));
                }
            }
        }
        ImGui::EndChild();
        // dropdown de FILTROS, ancorado no funil (por cima da lista)
        if (gFiltrosAberto) {
            const float FW = 250, FH = 188;
            ImGui::SetCursorPos(ImVec2(pb.x - pa.x - 24 - FW, 92));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(IM_COL32(14, 14, 14, 252)));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
            ImGui::BeginChild("##dropfiltros", ImVec2(FW, FH), false, ImGuiWindowFlags_NoScrollbar);
            ImVec2 dA = ImGui::GetWindowPos(), dB = ImVec2(dA.x + FW, dA.y + FH);
            ImGui::SetCursorPos(ImVec2(16, 12));
            ImGui::PushFont(gFtMini);
            ImGui::TextColored(ImColor(Cinza(156)), "F I L T R O S");
            ImGui::PopFont();
            bool fMud = false;
            ImGui::PushFont(gFtBody);
            ImGui::SetCursorPos(ImVec2(14, 36));
            fMud |= LinhaFiltro("Ocultar cheios", &gOcCheios);
            ImGui::SetCursorPos(ImVec2(14, 72));
            fMud |= LinhaFiltro("Ocultar com senha", &gOcSenha);
            ImGui::SetCursorPos(ImVec2(14, 108));
            fMud |= LinhaFiltro("Ocultar vazios", &gOcVazios);
            ImGui::SetCursorPos(ImVec2(14, 144));
            fMud |= LinhaFiltro("Ocultar sem resposta", &gOcOff);
            ImGui::PopFont();
            if (fMud) SalvarConfig();
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            pl->AddRect(dA, dB, Cinza(58), 12, 0, 1.0f);
            // clique fora fecha (clicar no funil ja alterna sozinho)
            if (ImGui::IsMouseClicked(0) && !ImGui::IsMouseHoveringRect(dA, dB) &&
                !ImGui::IsMouseHoveringRect(gFiltroBtnA, gFiltroBtnB))
                gFiltrosAberto = false;
        }
        ImGui::EndChild();
    }
    else if (gTela == 2) {
        // ---- DATAS (instalacoes do jogo) ----
        ImGui::SetCursorScreenPos(ImVec2(116, 78));
        ImGui::BeginChild("##datas", ImVec2(ds.x - 148, ds.y - 130), false);
        ImDrawList* pl = ImGui::GetWindowDrawList();
        ImVec2 pa = ImGui::GetWindowPos(), pb = ImVec2(pa.x + ImGui::GetWindowSize().x, pa.y + ImGui::GetWindowSize().y);
        pl->AddRectFilled(pa, pb, IM_COL32(12, 12, 12, 226), 16);
        pl->AddRect(pa, pb, Cinza(40), 16, 0, 1);
        ImGui::SetCursorPos(ImVec2(24, 20));
        ImGui::PushFont(gFtBotao);
        ImGui::TextColored(ImColor(Cinza(245)), "DATAS");
        ImGui::PopFont();
        ImGui::PushFont(gFtMini);
        ImGui::SetCursorPos(ImVec2(24, 48));
        ImGui::TextColored(ImColor(Cinza(140)), "Cada data é uma instalação do jogo. Clique para escolher qual será aberta pelo JOGAR.");
        ImGui::PopFont();
        ImGui::SetCursorPos(ImVec2(24, 76));
        ImGui::BeginChild("##gridatas", ImVec2(pb.x - pa.x - 48, pb.y - pa.y - 96), false);
        // cards 16:9 esticados para fechar exatamente na borda direita da grade
        const float GAP = 20;
        float wGrid = pb.x - pa.x - 48;
        int porLinha = (int)((wGrid + GAP) / (330.0f + GAP)); // quantos cabem no tamanho base
        if (porLinha < 1) porLinha = 1;
        float CW = (wGrid - (porLinha - 1) * GAP) / porLinha;
        float CH = CW * 9.0f / 16.0f;
        int totalCards = gNumDatas + (gNumDatas < MAX_DATAS ? 1 : 0); // +1 = adicionar
        for (int i = 0; i < totalCards; i++) {
            bool ehAdd = (i == gNumDatas);
            int col = i % porLinha, lin = i / porLinha;
            ImGui::SetCursorPos(ImVec2(col * (CW + GAP), lin * (CH + GAP)));
            char id[16]; sprintf(id, "##data%d", i);
            bool cl = ImGui::InvisibleButton(id, ImVec2(CW, CH));
            ImVec2 ca = ImGui::GetItemRectMin(), cb = ImGui::GetItemRectMax();
            bool ch = ImGui::IsItemHovered();
            ImDrawList* wl = ImGui::GetWindowDrawList();
            if (ehAdd) {
                // card "+" para adicionar data
                wl->AddRectFilled(ca, cb, Cinza(16, ch ? 235 : 190), 13);
                ImU32 tra = ch ? Cinza(200) : Cinza(90);
                wl->AddRect(ca, cb, tra, 13, 0, 1.5f);
                ImVec2 c((ca.x + cb.x) * 0.5f, (ca.y + cb.y) * 0.5f - 10);
                wl->AddLine(ImVec2(c.x - 14, c.y), ImVec2(c.x + 14, c.y), tra, 3);
                wl->AddLine(ImVec2(c.x, c.y - 14), ImVec2(c.x, c.y + 14), tra, 3);
                ImGui::PushFont(gFtBold);
                ImVec2 asz = ImGui::CalcTextSize("Adicionar data");
                wl->AddText(ImVec2(c.x - asz.x * 0.5f, c.y + 26), tra, "Adicionar data");
                ImGui::PopFont();
                if (cl) gAddData = true;
                continue;
            }
            DataGta& d = gDatas[i];
            // imagem 16:9 ocupa o card todo
            if (d.tex) {
                wl->AddImageRounded((ImTextureID)d.tex, ca, cb, ImVec2(0, 0), ImVec2(1, 1),
                                    IM_COL32(255, 255, 255, ch ? 255 : 235), 13);
            } else {
                wl->AddRectFilled(ca, cb, Cinza(26), 13);
                ImGui::PushFont(gFtDisplay);
                char ini2[2] = { d.nome[0] ? d.nome[0] : '?', 0 };
                ImVec2 gsz = ImGui::CalcTextSize(ini2);
                wl->AddText(ImVec2((ca.x + cb.x - gsz.x) * 0.5f, (ca.y + cb.y - gsz.y) * 0.5f - 8), Cinza(70), ini2);
                ImGui::PopFont();
            }
            // fade tambem NO TOPO (leitura do EM USO, pasta e "...")
            wl->AddRectFilled(ca, ImVec2(cb.x, ca.y + 13), Cinza(8, 150), 13, ImDrawFlags_RoundCornersTop);
            wl->AddRectFilledMultiColor(ImVec2(ca.x, ca.y + 13), ImVec2(cb.x, ca.y + 98),
                Cinza(8, 150), Cinza(8, 150), Cinza(8, 0), Cinza(8, 0));
            // fade de leitura em 2 estagios (mais alto e mais forte na base) + nome/descricao
            wl->AddRectFilledMultiColor(ImVec2(ca.x, cb.y - 140), ImVec2(cb.x, cb.y - 48),
                Cinza(8, 0), Cinza(8, 0), Cinza(8, 175), Cinza(8, 175));
            wl->AddRectFilledMultiColor(ImVec2(ca.x, cb.y - 48), ImVec2(cb.x, cb.y - 13),
                Cinza(8, 175), Cinza(8, 175), Cinza(8, 235), Cinza(8, 235));
            wl->AddRectFilled(ImVec2(ca.x, cb.y - 13), cb, Cinza(8, 235), 13, ImDrawFlags_RoundCornersBottom);
            ImGui::PushFont(gFtBold);
            wl->AddText(ImVec2(ca.x + 14, cb.y - 52), Cinza(248), d.nome);
            ImGui::PopFont();
            ImGui::PushFont(gFtMini);
            ImGui::PushClipRect(ca, ImVec2(cb.x - 8, cb.y), true);
            wl->AddText(ImVec2(ca.x + 14, cb.y - 28), Cinza(165), d.desc[0] ? d.desc : " ");
            ImGui::PopClipRect();
            ImGui::PopFont();
            ImU32 borda = (i == gDataSel) ? AC.cor : (ch ? Cinza(90) : Cinza(46));
            wl->AddRect(ca, cb, borda, 13, 0, (i == gDataSel) ? 2.0f : 1.0f);
            if (i == gDataSel) { // pill EM USO
                ImGui::PushFont(gFtMini);
                ImVec2 psz = ImGui::CalcTextSize("EM USO");
                ImVec2 t0(ca.x + 12, ca.y + 10);
                wl->AddRectFilled(t0, ImVec2(t0.x + psz.x + 16, t0.y + 20), AC.cor, 10);
                wl->AddText(ImVec2(t0.x + 8, t0.y + 3), TextoSobreAccent(AC.cor), "EM USO");
                ImGui::PopFont();
            }
            // botao "..." no canto superior direito - teste de clique por retangulo puro,
            // imune a briga de itens sobrepostos do imgui
            ImVec2 ma(cb.x - 38, ca.y + 8), mb(cb.x - 8, ca.y + 32);
            bool mh = ImGui::IsMouseHoveringRect(ma, mb);
            DesenhaReticencias(wl, ma, mb, mh);
            // atalho: abrir a pasta raiz da data no explorador
            ImVec2 fa2(cb.x - 72, ca.y + 8), fb2(cb.x - 44, ca.y + 32);
            bool fh2 = ImGui::IsMouseHoveringRect(fa2, fb2);
            if (fh2) wl->AddRectFilled(fa2, fb2, Cinza(255, 22), 7);
            {
                float fcx2 = (fa2.x + fb2.x) * 0.5f, fcy2 = (fa2.y + fb2.y) * 0.5f;
                ImU32 fcc = Cinza(fh2 ? 255 : 225);
                wl->AddCircleFilled(ImVec2(fcx2, fcy2), 8.5f, IM_COL32(0, 0, 0, 26), 16); // sombra sutil
                Icone(wl, ImVec2(fcx2, fcy2), I_PASTA_ABRIR, fcc, 16.0f);
            }
            if (fh2 && !gMouseNoDrop && ImGui::IsMouseClicked(0))
                ShellExecuteA(NULL, "open", d.caminho, NULL, NULL, SW_SHOWNORMAL);
            if (mh && !gMouseNoDrop) ImGui::SetTooltip("Editar data");
            if (fh2 && !gMouseNoDrop) ImGui::SetTooltip("Abrir a pasta da data no Explorer");
            if (mh && !gMouseNoDrop && ImGui::IsMouseClicked(0)) {
                gRenomear = i; // abre o painel de edicao (desenhado no nivel raiz)
                strncpy(gEditNome, d.nome, sizeof(gEditNome) - 1);
                strncpy(gEditDesc, d.desc, sizeof(gEditDesc) - 1);
            }
            if (cl && !mh && !fh2) {
                gDataSel = i;
                strncpy(gPastaGta, d.caminho, sizeof(gPastaGta) - 1);
                SalvarConfig();
            }
        }
        int linhasTot = (totalCards + porLinha - 1) / porLinha;
        ImGui::SetCursorPos(ImVec2(0, linhasTot * (CH + GAP)));
        ImGui::Dummy(ImVec2(1, 1));
        ImGui::EndChild();
        ImGui::EndChild();
    }
    else if (gTela == 4) {
        // ---- GALERIA: uma aba por User Files UNICO (datas que compartilham viram UMA aba;
        // o User Files padrao dos Documentos aparece como "User Files", nao com nome de data)
        struct GalTab { char rotulo[64]; char pasta[MAX_PATH]; };
        static GalTab tabs[MAX_DATAS];
        int nTabs = 0;
        char ufPadrao[MAX_PATH];
        {
            DataGta tmpD;
            memset(&tmpD, 0, sizeof(tmpD));
            UserFilesDaData(tmpD, ufPadrao, sizeof(ufPadrao));
        }
        for (int i2 = 0; i2 < gNumDatas; i2++) {
            char uf[MAX_PATH];
            UserFilesDaData(gDatas[i2], uf, sizeof(uf));
            bool repetida = false;
            for (int t2 = 0; t2 < nTabs; t2++) if (_stricmp(tabs[t2].pasta, uf) == 0) { repetida = true; break; }
            if (repetida) continue;
            GalTab& g2 = tabs[nTabs++];
            strncpy(g2.pasta, uf, MAX_PATH - 1);
            g2.pasta[MAX_PATH - 1] = 0;
            if (_stricmp(uf, ufPadrao) == 0) strcpy(g2.rotulo, "User Files");
            else { strncpy(g2.rotulo, gDatas[i2].nome, sizeof(g2.rotulo) - 1); g2.rotulo[sizeof(g2.rotulo) - 1] = 0; }
        }
        if (nTabs == 0) { strcpy(tabs[0].rotulo, "User Files"); strncpy(tabs[0].pasta, ufPadrao, MAX_PATH - 1); nTabs = 1; }
        if (gGalData < 0 || gGalData >= nTabs) {
            char ufSel[MAX_PATH];
            UserFilesDaData(gDatas[gDataSel], ufSel, sizeof(ufSel));
            gGalData = 0;
            for (int t2 = 0; t2 < nTabs; t2++) if (_stricmp(tabs[t2].pasta, ufSel) == 0) { gGalData = t2; break; }
        }
        char ufRaiz[MAX_PATH], dirAtual[MAX_PATH];
        strncpy(ufRaiz, tabs[gGalData].pasta, sizeof(ufRaiz) - 1);
        ufRaiz[sizeof(ufRaiz) - 1] = 0;
        _snprintf(dirAtual, sizeof(dirAtual) - 1, "%s\\SAMP\\screens", ufRaiz);
        dirAtual[MAX_PATH - 1] = 0;
        if (_stricmp(dirAtual, gFotosDir) != 0) {
            strncpy(gFotosDir, dirAtual, MAX_PATH - 1);
            EscanearFotos();
        }
        ImGui::SetCursorScreenPos(ImVec2(116, 78));
        ImGui::BeginChild("##galpainel", ImVec2(ds.x - 148, ds.y - 130), false);
        ImDrawList* pl = ImGui::GetWindowDrawList();
        ImVec2 pa = ImGui::GetWindowPos(), pb = ImVec2(pa.x + ImGui::GetWindowSize().x, pa.y + ImGui::GetWindowSize().y);
        pl->AddRectFilled(pa, pb, IM_COL32(12, 12, 12, 226), 16);
        pl->AddRect(pa, pb, Cinza(40), 16, 0, 1);
        ImGui::SetCursorPos(ImVec2(24, 20));
        ImGui::PushFont(gFtBotao);
        ImGui::TextColored(ImColor(Cinza(245)), "GALERIA");
        ImGui::PopFont();
        // sub-abas: uma por User Files unico (mesmo estilo das abas de Servidores)
        if (nTabs > 1) {
            ImGui::PushFont(gFtBold);
            float tx2 = 200;
            for (int t2 = 0; t2 < nTabs; t2++) {
                ImVec2 rsz = ImGui::CalcTextSize(tabs[t2].rotulo);
                ImGui::SetCursorPos(ImVec2(tx2, 24));
                char idt[16]; sprintf(idt, "##galaba%d", t2);
                if (ImGui::InvisibleButton(idt, ImVec2(rsz.x + 8, 30))) gGalData = t2;
                ImVec2 ta = ImGui::GetItemRectMin();
                bool at = (gGalData == t2), hab = ImGui::IsItemHovered();
                pl->AddText(ImVec2(ta.x + 4, ta.y + 2), at ? Cinza(250) : (hab ? Cinza(210) : Cinza(140)), tabs[t2].rotulo);
                if (at) pl->AddRectFilled(ImVec2(ta.x + 4, ta.y + 26), ImVec2(ta.x + 4 + rsz.x, ta.y + 29), AC.cor, 2);
                tx2 += rsz.x + 26;
            }
            ImGui::PopFont();
        }
        ImGui::SetCursorPos(ImVec2(24, 52));
        ImGui::PushFont(gFtMini);
        char subt[160];
        sprintf(subt, "%d screenshots  -  %s", gNumFotos, tabs[gGalData].rotulo);
        ImGui::TextColored(ImColor(Cinza(140)), "%s", subt); // "%s": nome de data pode ter %
        ImGui::PopFont();
        {
            // icones no canto direito: abrir pasta + atualizar
            float bxI = pb.x - pa.x - 24 - 40;
            ImGui::SetCursorPos(ImVec2(bxI, 22));
            bool pastaCl = ImGui::InvisibleButton("##galpasta", ImVec2(40, 34));
            ImVec2 ia = ImGui::GetItemRectMin(), ib = ImGui::GetItemRectMax();
            bool pHov = ImGui::IsItemHovered();
            if (pHov) pl->AddRectFilled(ia, ib, Cinza(255, 18), 9);
            {
                float cx3 = (ia.x + ib.x) * 0.5f, cy3 = (ia.y + ib.y) * 0.5f;
                ImU32 icor = Cinza(pHov ? 250 : 175);
                Icone(pl, ImVec2(cx3, cy3), I_PASTA_ABRIR, icor, 17.0f);
            }
            Dica("Abrir a pasta das screenshots");
            if (pastaCl)
                ShellExecuteA(NULL, "open", GetFileAttributesA(gFotosDir) != INVALID_FILE_ATTRIBUTES ? gFotosDir : ufRaiz,
                              NULL, NULL, SW_SHOWNORMAL);
            ImGui::SetCursorPos(ImVec2(bxI - 48, 22));
            bool attCl = ImGui::InvisibleButton("##galatt", ImVec2(40, 34));
            ImVec2 ra = ImGui::GetItemRectMin(), rb = ImGui::GetItemRectMax();
            bool rHov = ImGui::IsItemHovered();
            if (rHov) pl->AddRectFilled(ra, rb, Cinza(255, 18), 9);
            Icone(pl, ImVec2((ra.x + rb.x) * 0.5f, (ra.y + rb.y) * 0.5f), I_ATUALIZAR,
                  Cinza(rHov ? 250 : 175), 17.0f);
            Dica("Atualizar a galeria (F5)");
            if (attCl) EscanearFotos();
        }
        ImGui::SetCursorPos(ImVec2(24, 82));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f);
        ImGui::BeginChild("##galgrid", ImVec2(pb.x - pa.x - 48, pb.y - pa.y - 102), false);
        if (gNumFotos == 0) {
            ImGui::SetCursorPos(ImVec2(6, 16));
            ImGui::PushFont(gFtBody);
            ImGui::TextColored(ImColor(Cinza(150)), "Nenhuma screenshot encontrada nesta data.");
            ImGui::PopFont();
            ImGui::SetCursorPos(ImVec2(6, 44));
            ImGui::PushFont(gFtMini);
            ImGui::TextColored(ImColor(Cinza(105)), "%s", gFotosDir);
            ImGui::TextColored(ImColor(Cinza(105)), "Tire fotos no jogo com F8 e clique em Atualizar.");
            ImGui::PopFont();
        } else if (gFotoVista < 0) { // com o visualizador aberto a grade descansa
            ImDrawList* gl2 = ImGui::GetWindowDrawList();
            const float GAP2 = 12;
            float wG = pb.x - pa.x - 48;
            int porL = (int)((wG + GAP2) / (205.0f + GAP2));
            if (porL < 1) porL = 1;
            float CW2 = (wG - (porL - 1) * GAP2 - 10) / porL; // -10 = folga da scrollbar
            float CH2 = CW2 * 9.0f / 16.0f;
            // streaming: so os itens perto da tela viram widgets/texturas; o resto e liberado
            float syG = ImGui::GetScrollY();
            float altG = ImGui::GetWindowSize().y;
            int linIni = (int)((syG - 320.0f) / (CH2 + GAP2)); if (linIni < 0) linIni = 0;
            int linFim = (int)((syG + altG + 320.0f) / (CH2 + GAP2)) + 1;
            int iIni = linIni * porL, iFim = linFim * porL;
            if (iFim > gNumFotos) iFim = gNumFotos;
            for (int i = 0; i < gNumFotos; i++)
                if ((i < iIni || i >= iFim) && gFotos[i].tex) { AdiarRelease(gFotos[i].tex); gFotos[i].tex = NULL; }
            for (int i = iIni; i < iFim; i++) {
                int col = i % porL, lin = i / porL;
                ImGui::SetCursorPos(ImVec2(col * (CW2 + GAP2), lin * (CH2 + GAP2)));
                char idf[16]; sprintf(idf, "##foto%d", i);
                bool clF = ImGui::InvisibleButton(idf, ImVec2(CW2, CH2));
                ImVec2 ca = ImGui::GetItemRectMin(), cb = ImGui::GetItemRectMax();
                bool chF = ImGui::IsItemHovered();
                bool visivel = ImGui::IsItemVisible();
                Foto& f = gFotos[i];
                if (visivel && !f.tex && !f.falhou) PedirImagem(f.caminho, 288, 0); // decodifica na thread, sem engasgo
                if (f.tex) {
                    ImagemCapa(gl2, f.tex, ca, cb, 10.0f, chF ? 0 : 34); // leve escurecida fora do hover
                } else {
                    gl2->AddRectFilled(ca, cb, Cinza(20), 10); // skeleton limpo, sem texto
                }
                gl2->AddRect(ca, cb, chF ? Cinza(150) : Cinza(44), 10, 0, chF ? 1.6f : 1.0f);
                if (clF) gFotoVista = i;
            }
            int linTot = (gNumFotos + porL - 1) / porL;
            ImGui::SetCursorPos(ImVec2(0, linTot * (CH2 + GAP2)));
            ImGui::Dummy(ImVec2(1, 1));
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }
    else if (gTela == 5) {
        // ---- MODS (posts do blog TrokMods via feed RSS) ----
        ImGui::SetCursorScreenPos(ImVec2(116, 78));
        ImGui::BeginChild("##mods", ImVec2(ds.x - 148, ds.y - 130), false);
        ImDrawList* pl = ImGui::GetWindowDrawList();
        ImVec2 pa = ImGui::GetWindowPos(), pb = ImVec2(pa.x + ImGui::GetWindowSize().x, pa.y + ImGui::GetWindowSize().y);
        pl->AddRectFilled(pa, pb, IM_COL32(12, 12, 12, 226), 16);
        pl->AddRect(pa, pb, Cinza(40), 16, 0, 1);
        ImGui::SetCursorPos(ImVec2(24, 20));
        ImGui::PushFont(gFtBotao);
        ImGui::TextColored(ImColor(Cinza(245)), "TROKMODS");
        ImGui::PopFont();
        ImGui::PushFont(gFtMini);
        ImGui::SetCursorPos(ImVec2(24, 48));
        ImGui::TextColored(ImColor(Cinza(140)), "Visite nosso blog TrokMods. Clique num post para abrir no navegador.");
        ImGui::PopFont();
        { // comunidade no Discord + blog, a esquerda do atualizar
            struct AtalhoMods { const char* gl; const char* url; const char* dica; };
            static const AtalhoMods ATALHOS[2] = {
                { I_COMUNIDADE, URL_DISCORD, "Entrar na comunidade do Discord" },
                { I_GLOBO,      URL_BLOG,    "Abrir o blog TrokMods" },
            };
            for (int k = 0; k < 2; k++) {
                ImGui::SetCursorPos(ImVec2(pb.x - pa.x - 24 - 36 * (3 - k), 18));
                char idA[16]; sprintf(idA, "##modsat%d", k);
                bool cl = ImGui::InvisibleButton(idA, ImVec2(36, 34));
                ImVec2 aa = ImGui::GetItemRectMin(), ab = ImGui::GetItemRectMax();
                bool hv = ImGui::IsItemHovered();
                if (hv) pl->AddRectFilled(aa, ab, Cinza(255, 16), 9);
                ImVec2 cIco((aa.x + ab.x) * 0.5f, (aa.y + ab.y) * 0.5f);
                ImU32 corIco = hv ? AC.hi : Cinza(170);
                if (k == 0) IconeDiscord(pl, cIco, corIco, 23.0f); // logo oficial, desenhado
                else if (gLogoBlogger) {                             // marca oficial do Blogger,
                    float r = 8.5f;                                  // tingida e do mesmo peso visual do Discord
                    pl->AddImage((ImTextureID)gLogoBlogger, ImVec2(cIco.x - r, cIco.y - r),
                                 ImVec2(cIco.x + r, cIco.y + r), ImVec2(0, 0), ImVec2(1, 1), corIco);
                } else Icone(pl, cIco, ATALHOS[k].gl, corIco, 17.0f);
                Dica(ATALHOS[k].dica);
                if (cl) ShellExecuteA(NULL, "open", ATALHOS[k].url, NULL, NULL, SW_SHOWNORMAL);
            }
        }
        { // atualizar por ICONE (seta circular, igual ao da galeria)
            ImGui::SetCursorPos(ImVec2(pb.x - pa.x - 24 - 36, 18));
            bool refCl = ImGui::InvisibleButton("##modsref", ImVec2(36, 34));
            ImVec2 ra = ImGui::GetItemRectMin(), rb = ImGui::GetItemRectMax();
            bool refHov = ImGui::IsItemHovered();
            if (refHov) pl->AddRectFilled(ra, rb, Cinza(255, 16), 9);
            Icone(pl, ImVec2((ra.x + rb.x) * 0.5f, (ra.y + rb.y) * 0.5f), I_ATUALIZAR,
                  Cinza(refHov ? 250 : 170), 17.0f);
            Dica("Atualizar os posts do blog");
            if (refCl && gModsEstado != 1) {
                for (int k = 0; k < MAX_MODS; k++) { AdiarRelease(gModsTex[k]); gModsTex[k] = NULL; }
                CreateThread(NULL, 0, ThreadMods, NULL, 0, NULL);
            }
        }
        ImGui::SetCursorPos(ImVec2(24, 84));
        ImGui::BeginChild("##modslista", ImVec2(pb.x - pa.x - 48, pb.y - pa.y - 104), false);
        ImDrawList* ml = ImGui::GetWindowDrawList();
        if (gNumMods == 0) {
            ImGui::SetCursorPos(ImVec2(6, 16));
            ImGui::PushFont(gFtBody);
            if (gModsEstado == 1)
                ImGui::TextColored(ImColor(Cinza(150)), "buscando posts do blog...");
            else
                ImGui::TextColored(ImColor(Cinza(150)), "O blog TrokMods está chegando.");
            ImGui::PopFont();
            ImGui::SetCursorPos(ImVec2(6, 44));
            ImGui::PushFont(gFtMini);
            ImGui::TextColored(ImColor(Cinza(105)), "Quando saírem posts novos, eles aparecem aqui sozinhos - com aviso na barra lateral.");
            ImGui::PopFont();
        } else {
            // grade de CARDS: miniatura do post do blog - fundo branco, titulo laranja,
            // data pequena, capa 16:9, uma linha de resumo e o botao em pilula
            const float GAPM = 20;
            float wGridM = pb.x - pa.x - 58;
            int porLinhaM = (int)((wGridM + GAPM) / (330.0f + GAPM));
            if (porLinhaM < 1) porLinhaM = 1;
            float CWM = (wGridM - (porLinhaM - 1) * GAPM) / porLinhaM;
            const float PADM = 16.0f;                     // respiro lateral, igual ao card do blog
            const float HBOT = 40.0f;                     // altura da pilula ABRIR O POST
            float imgH = (CWM - PADM * 2) * 9.0f / 16.0f;
            ImGui::PushFont(gFtPostTit);                  // faixa do titulo = maior titulo da lista
            float altT = ImGui::GetTextLineHeight();
            int maxLin = 1;
            for (int k = 0; k < gNumMods; k++) {
                char a1[224], a2[224];
                int nl = TextoDuasLinhas(a1, a2, sizeof(a1), gMods[k].titulo, CWM - PADM * 2 - 22.0f);
                if (nl > maxLin) maxLin = nl;
            }
            ImGui::PopFont();
            ImGui::PushFont(gFtBody);
            float altD = ImGui::GetTextLineHeight();
            ImGui::PopFont();
            float yImg = 14.0f + maxLin * altT + 3.0f + altD + 14.0f; // capa logo abaixo da data
            float yDesc = yImg + imgH + 15.0f;            // resumo em UMA linha
            float yBot = yDesc + 27.0f;
            float CHM = yBot + HBOT + PADM;
            int idxVisto = -1; // cards acima do marco da visita ganham bolinha de NOVO
            for (int k = 0; k < gNumMods; k++)
                if (gModsVistoAte[0] && _stricmp(gMods[k].url, gModsVistoAte) == 0) { idxVisto = k; break; }
            for (int i = 0; i < gNumMods; i++) {
                int col = i % porLinhaM, lin = i / porLinhaM;
                ImGui::SetCursorPos(ImVec2(col * (CWM + GAPM), lin * (CHM + GAPM)));
                char idm[16]; sprintf(idm, "##post%d", i);
                bool clP = ImGui::InvisibleButton(idm, ImVec2(CWM, yBot - 6)); // card (sem o botao)
                ImVec2 ca = ImGui::GetItemRectMin();
                bool hovP = ImGui::IsItemHovered();
                ImVec2 cb2(ca.x + CWM, ca.y + CHM);
                // fundo BRANCO como no blog; no hover so ganha uma sombra por baixo
                if (hovP) SombraSuave(ml, ImVec2(ca.x + 5, ca.y + 8), ImVec2(cb2.x - 5, cb2.y + 5), 16.0f, 40);
                ml->AddRectFilled(ca, cb2, IM_COL32(255, 255, 255, 255), 14.0f);
                bool novoP = gModsVistoAte[0] && (idxVisto < 0 || i < idxVisto);
                float recuo = novoP ? 22.0f : 0.0f; // abre espaco pra bolinha de post novo
                { // titulo encostado na data; a data fica sempre na mesma altura em todos os cards
                    char t1[224], t2[224];
                    float larg = CWM - PADM * 2 - recuo;
                    float cxT = ca.x + (CWM - recuo) * 0.5f;
                    ImGui::PushFont(gFtPostTit);
                    int nlin = TextoDuasLinhas(t1, t2, sizeof(t1), gMods[i].titulo, larg);
                    float ty = ca.y + 14.0f + (maxLin - nlin) * altT; // titulos curtos descem ate a data
                    if (nlin > 0) ml->AddText(ImVec2(cxT - ImGui::CalcTextSize(t1).x * 0.5f, ty), AC.cor, t1);
                    if (nlin > 1) ml->AddText(ImVec2(cxT - ImGui::CalcTextSize(t2).x * 0.5f, ty + altT), AC.cor, t2);
                    ImGui::PopFont();
                    ImGui::PushFont(gFtBody); // data por extenso, peso normal, cinza
                    TextoTruncadoCentro(ml, ca.x + CWM * 0.5f, ca.y + 14.0f + maxLin * altT + 3.0f,
                                        CWM - PADM * 2, IM_COL32(124, 124, 130, 255), gMods[i].data);
                    ImGui::PopFont();
                }
                if (novoP) {
                    float pulso2 = 0.55f + 0.45f * sinf((float)ImGui::GetTime() * 5.0f);
                    ml->AddCircleFilled(ImVec2(cb2.x - 18, ca.y + 25), 5.0f,
                                        ComAlpha(AC.cor, 0.45f + 0.55f * pulso2), 14);
                }
                // capa 16:9 abaixo dos textos (carregada preguicosamente do cache)
                if (!gModsTex[i] && gMods[i].imgCache[0] &&
                    GetFileAttributesA(gMods[i].imgCache) != INVALID_FILE_ATTRIBUTES)
                    gModsTex[i] = CarregarImagemMax(gDev, gMods[i].imgCache, 640);
                ImVec2 ia(ca.x + PADM, ca.y + yImg), ib(cb2.x - PADM, ca.y + yImg + imgH);
                ml->AddRectFilled(ia, ib, IM_COL32(233, 233, 236, 255), 8.0f); // enquanto a capa carrega
                if (gModsTex[i]) ImagemCapa(ml, gModsTex[i], ia, ib, 8.0f, hovP ? 0 : 16);
                ImGui::PushFont(gFtBody); // resumo em uma linha so, peso normal
                TextoTruncadoCentro(ml, ca.x + CWM * 0.5f, ca.y + yDesc, CWM - PADM * 2,
                                    IM_COL32(80, 80, 86, 255), gMods[i].resumo);
                ImGui::PopFont();
                if (clP && _strnicmp(gMods[i].url, "http", 4) == 0)
                    ShellExecuteA(NULL, "open", gMods[i].url, NULL, NULL, SW_SHOWNORMAL);
                // botao ABRIR O POST: pilula laranja centrada, do mesmo jeito que no blog
                ImGui::PushFont(gFtBotaoPost);
                float wBot = ImGui::CalcTextSize("ABRIR O POST").x + 58.0f;
                ImGui::PopFont();
                if (wBot > CWM - PADM * 2) wBot = CWM - PADM * 2;
                ImGui::SetCursorPos(ImVec2(col * (CWM + GAPM) + (CWM - wBot) * 0.5f,
                                           lin * (CHM + GAPM) + yBot));
                char idb[16]; sprintf(idb, "##baixa%d", i);
                bool clB = ImGui::InvisibleButton(idb, ImVec2(wBot, HBOT));
                ImVec2 ba2 = ImGui::GetItemRectMin(), bb2 = ImGui::GetItemRectMax();
                bool hovB = ImGui::IsItemHovered();
                ImU32 topoB = hovB ? LerpCor(AC.cor, IM_COL32(255, 255, 255, 255), 0.10f) : AC.cor;
                RectGradVertical(ml, ba2, bb2, topoB, LerpCor(topoB, AC.hi, 0.55f), 9.0f); // canto do blog
                ImGui::PushFont(gFtBotaoPost);
                ImVec2 bsz2 = ImGui::CalcTextSize("ABRIR O POST");
                ml->AddText(ImVec2((ba2.x + bb2.x - bsz2.x) * 0.5f, (ba2.y + bb2.y - bsz2.y) * 0.5f),
                            TextoSobreAccent(AC.cor), "ABRIR O POST");
                ImGui::PopFont();
                if (clB && _strnicmp(gMods[i].url, "http", 4) == 0)
                    ShellExecuteA(NULL, "open", gMods[i].url, NULL, NULL, SW_SHOWNORMAL);
            }
            int linhasM = (gNumMods + porLinhaM - 1) / porLinhaM;
            ImGui::SetCursorPos(ImVec2(0, linhasM * (CHM + GAPM)));
            ImGui::Dummy(ImVec2(1, 1)); // estende o scroll ate o fim da grade
        }
        ImGui::EndChild();
        ImGui::EndChild();
    }
    else if (gTela == 6) {
        // ---- INFORMACOES (projeto, contatos, cafe) ----
        ImGui::SetCursorScreenPos(ImVec2(116, 78));
        ImGui::BeginChild("##info", ImVec2(ds.x - 148, ds.y - 130), false);
        ImDrawList* pl = ImGui::GetWindowDrawList();
        ImVec2 pa = ImGui::GetWindowPos(), pb = ImVec2(pa.x + ImGui::GetWindowSize().x, pa.y + ImGui::GetWindowSize().y);
        pl->AddRectFilled(pa, pb, IM_COL32(12, 12, 12, 226), 16);
        pl->AddRect(pa, pb, Cinza(40), 16, 0, 1);
        ImGui::SetCursorPos(ImVec2(24, 20));
        ImGui::PushFont(gFtBotao);
        ImGui::TextColored(ImColor(Cinza(245)), "INFORMAÇÕES");
        ImGui::PopFont();
        ImGui::PushFont(gFtMini);
        ImGui::SetCursorPos(ImVec2(24, 48));
        ImGui::TextColored(ImColor(Cinza(140)), "Trok Launcher %s  -  feito pela equipe TrokMods", VERSAO);
        ImGui::PopFont();
        ImGui::SetCursorPos(ImVec2(24, 84));
        ImGui::PushFont(gFtBody);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (pb.x - pa.x) - 72);
        ImGui::TextColored(ImColor(Cinza(200)),
            "O Trok Launcher é um launcher moderno e gratuito de SA-MP: contas com avatar, "
            "várias instalações do jogo (datas), favoritos com capa, galeria das suas screenshots "
            "e atualização automática. Ele NÃO substitui nenhum arquivo do seu jogo - abre o "
            "samp.exe original da instalacao que voce escolher.");
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
        ImGui::SetCursorPos(ImVec2(24, 190));
        ImGui::PushFont(gFtMini);
        ImGui::TextColored(ImColor(Cinza(156)), "L I N K S");
        ImGui::PopFont();
        ImGui::PushFont(gFtBody);
        ImGui::SetCursorPos(ImVec2(24, 212));
        if (BotaoSec("Blog TrokMods", ImVec2(180, 40)))
            ShellExecuteA(NULL, "open", URL_BLOG, NULL, NULL, SW_SHOWNORMAL);
        ImGui::SameLine(0, 10);
        if (BotaoSec("Guia do launcher", ImVec2(180, 40))) // post de lancamento (tutorial completo)
            ShellExecuteA(NULL, "open", URL_POST_LAUNCHER, NULL, NULL, SW_SHOWNORMAL);
        ImGui::PopFont();
        ImGui::SetCursorPos(ImVec2(24, 274));
        ImGui::PushFont(gFtMini);
        ImGui::TextColored(ImColor(Cinza(156)), "G O S T O U ?");
        ImGui::PopFont();
        ImGui::SetCursorPos(ImVec2(24, 296));
        ImGui::PushFont(gFtBold);
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(AC.cor));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(AC.hi));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(AC.hi));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(TextoSobreAccent(AC.cor)));
        if (ImGui::Button("Me pague um café", ImVec2(196, 44)))
            ShellExecuteA(NULL, "open", URL_CAFE, NULL, NULL, SW_SHOWNORMAL);
        ImGui::PopStyleColor(4);
        ImGui::PopFont();
        ImGui::PushFont(gFtMini);
        ImGui::SetCursorPos(ImVec2(24, 348));
        ImGui::TextColored(ImColor(Cinza(110)), "O launcher é de graça e sempre vai ser. O café paga as madrugadas de mod.");
        ImGui::PopFont();
        ImGui::EndChild();
    }
    else if (gTela == 3) {
        // ---- CONFIGURACOES ---- (desenhada AQUI, junto das telas: se ficar depois dos
        // dropdowns, o child dela renderiza POR CIMA deles - foi o bug da sobreposicao)
        ImGui::SetCursorScreenPos(ImVec2(116, 78));
        ImGui::BeginChild("##cfg", ImVec2(ds.x - 148, ds.y - 130), false);
        ImDrawList* pl = ImGui::GetWindowDrawList();
        ImVec2 pa = ImGui::GetWindowPos(), pb = ImVec2(pa.x + ImGui::GetWindowSize().x, pa.y + ImGui::GetWindowSize().y);
        pl->AddRectFilled(pa, pb, IM_COL32(12, 12, 12, 226), 16);
        pl->AddRect(pa, pb, Cinza(40), 16, 0, 1);
        ImGui::SetCursorPos(ImVec2(26, 20));
        ImGui::PushFont(gFtBotao);
        ImGui::TextColored(ImColor(Cinza(245)), "CONFIGURAÇÕES");
        ImGui::PopFont();
        // nick saiu daqui: mora nas CONTAS (avatar/nick do topo); a conta 0 e a do samp padrão
        ImGui::PushFont(gFtMini);
        ImGui::SetCursorPos(ImVec2(26, 66)); ImGui::TextColored(ImColor(Cinza(156)), "C O R   D E   D E S T A Q U E");
        ImGui::PopFont();
        for (int i = 0; i < N_ACCENTS; i++) {
            ImGui::SetCursorPos(ImVec2(26.0f + i * 52.0f, 88.0f));
            char id[12]; sprintf(id, "##sw%d", i);
            if (ImGui::InvisibleButton(id, ImVec2(40, 40))) { gAccent = i; SalvarConfig(); }
            ImVec2 sa = ImGui::GetItemRectMin();
            ImVec2 c(sa.x + 20, sa.y + 20);
            float r = ImGui::IsItemHovered() ? 19.0f : 17.0f;
            ImDrawList* wl = ImGui::GetWindowDrawList();
            wl->AddCircleFilled(c, r, ACCENTS[i].cor, 32);
            if (i == gAccent) {
                wl->AddCircle(c, r + 3.5f, Cinza(245), 32, 2.0f);
            }
        }
        // ultimo circulo: COR PERSONALIZADA (anel arco-iris + miolo na cor escolhida)
        {
            ImGui::SetCursorPos(ImVec2(26.0f + N_ACCENTS * 52.0f, 88.0f));
            if (ImGui::InvisibleButton("##swcustom", ImVec2(40, 40))) {
                gAccent = N_ACCENTS; // clicar ja aplica a cor personalizada guardada
                SalvarConfig();
                ImGui::OpenPopup("##corcustom");
            }
            ImVec2 sa = ImGui::GetItemRectMin();
            ImVec2 c(sa.x + 20, sa.y + 20);
            float r = ImGui::IsItemHovered() ? 19.0f : 17.0f;
            ImDrawList* wl = ImGui::GetWindowDrawList();
            static const ImU32 ARCO[6] = {
                IM_COL32(235, 80, 70, 255), IM_COL32(238, 190, 70, 255), IM_COL32(110, 200, 90, 255),
                IM_COL32(70, 180, 230, 255), IM_COL32(120, 90, 230, 255), IM_COL32(225, 90, 200, 255)
            };
            for (int s2 = 0; s2 < 6; s2++) { // anel em 6 fatias sobrepostas (sem rachadura na emenda)
                float a0 = s2 * 1.0472f - 1.5708f, a1 = a0 + 1.0472f;
                wl->PathArcTo(c, r - 1.5f, a0 - 0.06f, a1 + 0.06f, 12);
                wl->PathStroke(ARCO[s2], 0, 3.2f);
            }
            wl->AddCircleFilled(c, r - 5.0f, gAccentCustom.cor, 32);
            if (gAccent >= N_ACCENTS) wl->AddCircle(c, r + 3.5f, Cinza(245), 32, 2.0f);
            Dica("Cor personalizada");
            if (ImGui::BeginPopup("##corcustom")) {
                static float corSel[3] = { 0, 0, 0 };
                if (ImGui::IsWindowAppearing()) {
                    corSel[0] = (gAccentCustom.cor & 255) / 255.0f;
                    corSel[1] = ((gAccentCustom.cor >> 8) & 255) / 255.0f;
                    corSel[2] = ((gAccentCustom.cor >> 16) & 255) / 255.0f;
                }
                ImGui::SetNextItemWidth(230);
                if (ImGui::ColorPicker3("##pick", corSel,
                        ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_DisplayHex)) {
                    int r2 = (int)(corSel[0] * 255.0f + 0.5f);
                    int g2 = (int)(corSel[1] * 255.0f + 0.5f);
                    int b2 = (int)(corSel[2] * 255.0f + 0.5f);
                    gAccentCustom.cor = IM_COL32(r2, g2, b2, 255);
                    gAccentCustom.hi = IM_COL32(r2 + (255 - r2) * 28 / 100, g2 + (255 - g2) * 28 / 100, b2 + (255 - b2) * 28 / 100, 255);
                    gAccent = N_ACCENTS; // aplica AO VIVO enquanto arrasta
                    SalvarConfig();
                }
                ImGui::EndPopup();
            }
        }
        // opcoes em LINHAS de lista (hover + divisoria), switch a direita no destaque
        {
            float wLin = pb.x - pa.x - 48;
            ImGui::PushFont(gFtBody);
            ImGui::SetCursorPos(ImVec2(24, 150));
            if (LinhaOpcao("Fechar o launcher ao entrar no jogo", &gFecharAoJogar, wLin)) SalvarConfig();
            Dica("Ao clicar em Jogar, o launcher fecha (ou vai pra bandeja, se a opção abaixo estiver ligada)");
            ImGui::SetCursorPos(ImVec2(24, 199));
            if (LinhaOpcao("Fechar para a bandeja em vez de sair", &gFecharBandeja, wLin)) SalvarConfig();
            Dica("O X esconde o launcher perto do relógio em vez de encerrar de vez");
            ImGui::SetCursorPos(ImVec2(24, 248));
            if (LinhaOpcao("Iniciar minimizado na bandeja", &gIniciarMin, wLin)) SalvarConfig();
            Dica("O launcher abre já escondido, só o ícone perto do relógio");
            ImGui::SetCursorPos(ImVec2(24, 297));
            static bool iniciarWin = IniciarComWindowsAtivo(); // lido do registro uma vez
            if (LinhaOpcao("Iniciar com o Windows", &iniciarWin, wLin)) DefinirIniciarComWindows(iniciarWin);
            Dica("Abre sozinho quando o computador liga");
            ImGui::SetCursorPos(ImVec2(24, 346));
            if (LinhaOpcao("Discord Rich Presence", &gDiscordRP, wLin)) SalvarConfig();
            Dica("Mostra no seu perfil do Discord o servidor em que você está jogando");
            ImGui::SetCursorPos(ImVec2(24, 395));
            if (LinhaOpcao("Lembrar o último servidor selecionado", &gLembrarUlt, wLin)) SalvarConfig();
            Dica("Ao abrir, a Home já vem no servidor em que você parou");
            ImGui::SetCursorPos(ImVec2(24, 444));
            if (LinhaOpcao("Mover o último jogado para o início dos favoritos", &gUltimoPrimeiro, wLin)) SalvarConfig();
            Dica("Ao entrar num servidor, o card dele vai pra frente da fila dos favoritos");
            // opções do SA-MP ORIGINAL (mesmo registro: mudar aqui muda la, e vice-versa)
            ImGui::SetCursorPos(ImVec2(24, 493));
            if (LinhaOpcao("Salvar senhas de servidor automaticamente", &gSalvarSenhaServ, wLin))
                GravarOpcaoSampRegistro("SaveServPasses", gSalvarSenhaServ);
            Dica("Opcao do proprio SA-MP: a senha digitada fica guardada (texto puro) no USERDATA.DAT");
            ImGui::SetCursorPos(ImVec2(24, 542));
            if (LinhaOpcao("Salvar senhas de RCON", &gSalvarSenhaRcon, wLin))
                GravarOpcaoSampRegistro("SaveRconPasses", gSalvarSenhaRcon);
            Dica("Opção do próprio SA-MP, usada pelas ferramentas RCON do browser original");
            ImGui::SetCursorPos(ImVec2(26, 602));
            ImGui::PushFont(gFtMini);
            ImGui::TextColored(ImColor(Cinza(156)), "B A C K U P");
            ImGui::PopFont();
            ImGui::SetCursorPos(ImVec2(24, 624));
            if (BotaoSec("     Exportar configurações...", ImVec2(232, 38))) gPedirExportCfg = true;
            Dica("Gera um arquivo único com contas, favoritos, opções e as imagens que você subiu");
            { // icone lucide: exportar (upload)
                ImVec2 ea = ImGui::GetItemRectMin(), eb = ImGui::GetItemRectMax();
                Icone(ImGui::GetWindowDrawList(), ImVec2(ea.x + 24, (ea.y + eb.y) * 0.5f), I_SUBIR,
                      Cinza(ImGui::IsItemHovered() ? 230 : 170), 16.0f);
            }
            ImGui::SameLine(0, 10);
            if (BotaoSec("     Importar configurações...", ImVec2(232, 38))) gPedirImportCfg = true;
            Dica("Restaura um backup exportado em outro PC (o launcher reabre sozinho)");
            { // icone lucide: importar (download)
                ImVec2 ea = ImGui::GetItemRectMin(), eb = ImGui::GetItemRectMax();
                Icone(ImGui::GetWindowDrawList(), ImVec2(ea.x + 24, (ea.y + eb.y) * 0.5f), I_BAIXAR,
                      Cinza(ImGui::IsItemHovered() ? 230 : 170), 16.0f);
            }
            ImGui::PopFont();
        }
        ImGui::EndChild();
    }

    // ===== painel "Editar data" (nivel raiz - popup dentro de child nao abre direito) =====
    {
        if (gRenomear >= 0 && !ImGui::IsPopupOpen("Editar data##trok")) ImGui::OpenPopup("Editar data##trok");
        ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        // vestido com o design do app (o modal cru do imgui vem com barra azul)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26, 24));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(IM_COL32(14, 14, 14, 252)));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(Cinza(58)));
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.01f, 0.01f, 0.01f, 0.72f));
        if (ImGui::BeginPopupModal("Editar data##trok", NULL,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove)) {
            int i = gRenomear;
            ImGui::PushFont(gFtBotao);
            ImGui::TextColored(ImColor(Cinza(245)), "EDITAR DATA");
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(1, 8));
            ImGui::PushFont(gFtMini);
            ImGui::TextColored(ImColor(Cinza(156)), "N O M E");
            ImGui::PopFont();
            ImGui::PushFont(gFtBody);
            ImGui::PushItemWidth(384);
            ImGui::InputText("##ednome", gEditNome, sizeof(gEditNome));
            ImGui::Dummy(ImVec2(1, 6));
            ImGui::PushFont(gFtMini);
            ImGui::TextColored(ImColor(Cinza(156)), "D E S C R I C A O");
            ImGui::PopFont();
            ImGui::InputText("##eddesc", gEditDesc, sizeof(gEditDesc));
            ImGui::PopItemWidth();
            ImGui::Dummy(ImVec2(1, 12));
            if (BotaoSec("Trocar imagem...", ImVec2(188, 38))) {
                SalvarEdicaoData(i);
                gPickImagem = i; gRenomear = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine(0, 8);
            if (BotaoSec("Trocar caminho...", ImVec2(188, 38))) {
                SalvarEdicaoData(i);
                gPickCaminho = i; gRenomear = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::Dummy(ImVec2(1, 2));
            if (BotaoSec("Trocar User Files...", ImVec2(188, 38))) {
                SalvarEdicaoData(i);
                gPickUserFiles = i; gRenomear = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine(0, 8);
            if (BotaoSec("Abrir User Files", ImVec2(188, 38))) {
                char uf[MAX_PATH];
                UserFilesDaData(gDatas[i], uf, sizeof(uf));
                ShellExecuteA(NULL, "open", uf, NULL, NULL, SW_SHOWNORMAL);
            }
            ImGui::PushFont(gFtMini);
            ImGui::TextColored(ImColor(Cinza(133)), "User Files vazio = a pasta padrão em Documentos. A galeria lê as screens dela.");
            ImGui::PopFont();
            // fileira de capas PRONTAS - previa ja em monocromatico, como fica no card de data
            GarantirCapasUI();
            if (gNumCapasUI > 0) {
                ImGui::Dummy(ImVec2(1, 6));
                ImGui::PushFont(gFtMini);
                ImGui::TextColored(ImColor(Cinza(156)), "C A P A S   D O   L A U N C H E R");
                ImGui::PopFont();
                ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f);
                ImGui::BeginChild("##capasdt", ImVec2(384, 70), false, ImGuiWindowFlags_HorizontalScrollbar);
                ImDrawList* cl3 = ImGui::GetWindowDrawList();
                float cxu2 = 0;
                for (int k = 0; k < gNumCapasUI; k++) {
                    ImGui::SetCursorPos(ImVec2(cxu2, 2));
                    char idc2[20]; sprintf(idc2, "##capadt%d", k);
                    bool clc2 = ImGui::InvisibleButton(idc2, ImVec2(96, 54));
                    ImVec2 ka2 = ImGui::GetItemRectMin(), kb2 = ImGui::GetItemRectMax();
                    bool khov2 = ImGui::IsItemHovered();
                    IDirect3DTexture9* tMono = gCapasTexUIMono[k] ? gCapasTexUIMono[k] : gCapasTexUI[k];
                    if (tMono) ImagemCapa(cl3, tMono, ka2, kb2, 7.0f, khov2 ? 0 : 40);
                    else cl3->AddRectFilled(ka2, kb2, Cinza(20), 7.0f);
                    bool atual2 = _stricmp(gDatas[i].img, gCapasArqUI[k]) == 0;
                    if (atual2 || khov2) cl3->AddRect(ka2, kb2, atual2 ? AC.cor : Cinza(150), 7.0f, 0, atual2 ? 2.0f : 1.0f);
                    if (clc2) { // aplica na hora (escolha manual: pode repetir a vontade)
                        char velho[MAX_PATH];
                        strncpy(velho, gDatas[i].img, sizeof(velho) - 1); velho[sizeof(velho) - 1] = 0;
                        strncpy(gDatas[i].img, gCapasArqUI[k], sizeof(gDatas[i].img) - 1);
                        AdiarRelease(gDatas[i].tex);
                        gDatas[i].tex = CarregarImagemMax(gDev, gDatas[i].img, 800);
                        EscurecerMonocromatico(gDatas[i].tex);
                        SalvarDatas();
                        DescartarImagemImportada(velho);
                    }
                    cxu2 += 102;
                }
                ImGui::SetCursorPos(ImVec2(cxu2 > 0 ? cxu2 - 6 : 0, 2));
                ImGui::Dummy(ImVec2(1, 1));
                if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f)
                    ImGui::SetScrollX(ImGui::GetScrollX() - ImGui::GetIO().MouseWheel * 64.0f);
                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
            }
            if (gDatas[i].img[0]) {
                ImGui::Dummy(ImVec2(1, 2));
                if (BotaoSec("Sortear outra capa##dt", ImVec2(384, 30))) {
                    // nunca fica sem imagem: sorteia outra (mono) respeitando as regras
                    char velho[MAX_PATH];
                    strncpy(velho, gDatas[i].img, sizeof(velho) - 1); velho[sizeof(velho) - 1] = 0;
                    gDatas[i].img[0] = 0; // sai da conta de "em uso" antes do sorteio
                    SortearCapaPadrao(gDatas[i].img, sizeof(gDatas[i].img));
                    AdiarRelease(gDatas[i].tex);
                    gDatas[i].tex = NULL;
                    if (gDatas[i].img[0]) {
                        gDatas[i].tex = CarregarImagemMax(gDev, gDatas[i].img, 800);
                        if (strstr(gDatas[i].img, "\\capas\\")) EscurecerMonocromatico(gDatas[i].tex);
                    }
                    SalvarDatas();
                    DescartarImagemImportada(velho);
                }
            }
            ImGui::Dummy(ImVec2(1, 4));
            bool podeRemover = gNumDatas > 1;
            if (!podeRemover) ImGui::BeginDisabled();
            static float tConfRemD = 0; // idem: 2 cliques
            if (tConfRemD > 0) tConfRemD -= dt;
            bool remDataCl = BotaoSec(tConfRemD > 0 ? "Clique de novo para remover##rmd" : "Remover esta data##rmd",
                    ImVec2(384, 34), podeRemover ? IM_COL32(240, 120, 116, 255) : Cinza(95)) && podeRemover;
            if (remDataCl && tConfRemD <= 0) tConfRemD = 3.0f;
            else if (remDataCl) {
                tConfRemD = 0;
                if (gDatas[i].tex) { gDatas[i].tex->Release(); gDatas[i].tex = NULL; }
                for (int j = i; j < gNumDatas - 1; j++) gDatas[j] = gDatas[j + 1];
                gNumDatas--;
                if (gDataSel >= gNumDatas) gDataSel = gNumDatas - 1;
                if (gDataSel < 0) gDataSel = 0;
                strncpy(gPastaGta, gDatas[gDataSel].caminho, sizeof(gPastaGta) - 1);
                SalvarDatas(); SalvarConfig();
                gRenomear = -1;
                ImGui::CloseCurrentPopup();
            }
            if (!podeRemover) ImGui::EndDisabled();
            ImGui::Dummy(ImVec2(1, 12));
            ImGui::PushFont(gFtBold);
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(AC.cor));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(AC.hi));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(AC.hi));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(TextoSobreAccent(AC.cor)));
            if (ImGui::Button("Salvar", ImVec2(188, 40))) {
                SalvarEdicaoData(i);
                gRenomear = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(4);
            ImGui::SameLine(0, 8);
            if (BotaoSec("Cancelar", ImVec2(188, 40))) { gRenomear = -1; ImGui::CloseCurrentPopup(); }
            ImGui::PopFont();
            ImGui::PopFont();
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }

    // ===== painel "Editar servidor" (favoritos: apelido, imagem, remover) =====
    {
        if (gEditSrv >= 0 && !ImGui::IsPopupOpen("Editar servidor##trok")) {
            gAddLink = false;
            CompactarLinks(gSrv[gEditSrv]); // slots sem buraco = indice do slot vira a ordem
            ImGui::OpenPopup("Editar servidor##trok");
        }
        ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26, 24));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(IM_COL32(14, 14, 14, 252)));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(Cinza(58)));
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.01f, 0.01f, 0.01f, 0.72f));
        if (ImGui::BeginPopupModal("Editar servidor##trok", NULL,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove)) {
            int i = gEditSrv;
            ImGui::PushFont(gFtBotao);
            ImGui::TextColored(ImColor(Cinza(245)), "EDITAR SERVIDOR");
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(1, 8));
            ImGui::PushFont(gFtMini);
            ImGui::TextColored(ImColor(Cinza(156)), "N O M E   D E   E X I B I C A O");
            ImGui::PopFont();
            ImGui::PushFont(gFtBody);
            ImGui::PushItemWidth(384);
            ImGui::InputTextWithHint("##edapelido", "vazio = nome real do servidor", gEditApelido, sizeof(gEditApelido));
            ImGui::PopItemWidth();
            ImGui::Dummy(ImVec2(1, 8));
            ImGui::PushFont(gFtMini);
            ImGui::TextColored(ImColor(Cinza(156)), "C O N T A   A O   J O G A R");
            ImGui::PopFont();
            {
                // capsula mostra AVATAR + NICK da conta escolhida; clique abre a lista
                int selPerfil = -1;
                for (int q = 0; q < gNumPerfis; q++)
                    if (gSrv[i].contaPref[0] && _stricmp(gPerfis[q].nick, gSrv[i].contaPref) == 0) { selPerfil = q; break; }
                if (ImGui::InvisibleButton("##ctpref", ImVec2(384, 36))) ImGui::OpenPopup("##popconta");
                {
                    ImVec2 ba = ImGui::GetItemRectMin(), bb = ImGui::GetItemRectMax();
                    bool bh = ImGui::IsItemHovered();
                    ImDrawList* dcp = ImGui::GetWindowDrawList();
                    if (bh) dcp->AddRectFilled(ba, bb, Cinza(255, 12), 9.0f);
                    dcp->AddRect(ba, bb, bh ? Cinza(200) : Cinza(120), 9.0f, 0, 1.5f);
                    const char* rotC = (selPerfil >= 0) ? gPerfis[selPerfil].nick : "Conta atual (não trocar)";
                    ImGui::PushFont(gFtBold);
                    ImVec2 tszC = ImGui::CalcTextSize(rotC);
                    float larg = 24 + 8 + tszC.x;
                    float x0 = (ba.x + bb.x - larg) * 0.5f, ymid = (ba.y + bb.y) * 0.5f;
                    ImVec2 qa(x0, ymid - 12), qb(x0 + 24, ymid + 12);
                    if (selPerfil >= 0)
                        DesenhaAvatar(dcp, qa, qb, gPerfis[selPerfil].avatar, gPerfis[selPerfil].cor, 7.0f);
                    else
                        dcp->AddRect(qa, qb, Cinza(110), 7.0f, 0, 1.4f); // sem troca: quadrado vazado
                    dcp->AddText(ImVec2(x0 + 32, ymid - tszC.y * 0.5f), Cinza(bh ? 240 : 205), rotC);
                    ImGui::PopFont();
                }
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
                if (ImGui::BeginPopup("##popconta")) {
                    ImDrawList* cl2 = ImGui::GetWindowDrawList();
                    for (int q = -1; q < gNumPerfis; q++) { // -1 = "conta atual (nao trocar)"
                        char idq[16]; sprintf(idq, "##pc%d", q + 1);
                        bool selQ = (q < 0) ? !gSrv[i].contaPref[0]
                                            : _stricmp(gPerfis[q].nick, gSrv[i].contaPref) == 0;
                        bool clq = ImGui::InvisibleButton(idq, ImVec2(360, 44));
                        ImVec2 la2 = ImGui::GetItemRectMin(), lb2 = ImGui::GetItemRectMax();
                        bool hv = ImGui::IsItemHovered();
                        if (hv) cl2->AddRectFilled(la2, lb2, Cinza(255, 14), 9);
                        if (selQ) cl2->AddRect(la2, lb2, ComAlpha(AC.cor, 0.85f), 9, 0, 1.5f);
                        ImVec2 qa(la2.x + 7, la2.y + 7), qb(la2.x + 37, la2.y + 37);
                        if (q < 0) cl2->AddRect(qa, qb, Cinza(110), 8, 0, 1.5f); // sem troca: vazado
                        else DesenhaAvatar(cl2, qa, qb, gPerfis[q].avatar, gPerfis[q].cor, 8.0f);
                        ImGui::PushFont(gFtBold);
                        cl2->AddText(ImVec2(la2.x + 48, la2.y + 12), Cinza(238),
                                     q < 0 ? "Conta atual (não trocar)" : gPerfis[q].nick);
                        ImGui::PopFont();
                        if (clq) {
                            if (q < 0) gSrv[i].contaPref[0] = 0;
                            else strncpy(gSrv[i].contaPref, gPerfis[q].nick, sizeof(gSrv[0].contaPref) - 1);
                            SalvarServidores();
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar();
            }
            ImGui::Dummy(ImVec2(1, 8));
            ImGui::PushFont(gFtMini);
            ImGui::TextColored(ImColor(Cinza(156)), "L I N K S   O F I C I A I S");
            ImGui::PopFont();
            {
                // lista livre: cada link tem nome e url proprios (viram botoes na Home)
                for (int q = 0; q < 4; q++) {
                    if (!gSrv[i].sites[q][0]) continue;
                    char rotL[24], urlL[150];
                    SepararLink(gSrv[i].sites[q], q, rotL, sizeof(rotL), urlL, sizeof(urlL));
                    ImVec2 lp = ImGui::GetCursorScreenPos();
                    ImDrawList* mlk = ImGui::GetWindowDrawList();
                    // linha arrastavel (area fora do Remover) p/ reordenar os links
                    char idrow[16]; sprintf(idrow, "##lrow%d", q);
                    ImGui::InvisibleButton(idrow, ImVec2(296, 38));
                    bool rowHov = ImGui::IsItemHovered();
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
                        ImGui::SetDragDropPayload("TROK_LNK", &q, sizeof(int));
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* pay = ImGui::AcceptDragDropPayload("TROK_LNK",
                                ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
                            float pulso = 0.55f + 0.45f * sinf((float)ImGui::GetTime() * 7.0f);
                            mlk->AddRectFilled(ImVec2(lp.x, lp.y - 3), ImVec2(lp.x + 384, lp.y - 1), ComAlpha(AC.cor, pulso), 2);
                            if (pay->IsDelivery()) { MoverLink(gSrv[i], *(const int*)pay->Data, q); SalvarServidores(); }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if (rowHov) mlk->AddRectFilled(lp, ImVec2(lp.x + 296, lp.y + 38), Cinza(255, 10), 8);
                    for (int g = 0; g < 3; g++) // grip: da a dica de que arrasta
                        mlk->AddCircleFilled(ImVec2(lp.x + 6, lp.y + 12 + g * 7.0f), 1.4f, Cinza(rowHov ? 175 : 90));
                    ImGui::PushFont(gFtBold);
                    mlk->AddText(ImVec2(lp.x + 18, lp.y + 2), Cinza(235), rotL);
                    ImGui::PopFont();
                    ImGui::PushFont(gFtMini);
                    ImGui::PushClipRect(lp, ImVec2(lp.x + 296, lp.y + 40), true);
                    mlk->AddText(ImVec2(lp.x + 18, lp.y + 22), Cinza(120), urlL);
                    ImGui::PopClipRect();
                    ImGui::PopFont();
                    ImGui::SetCursorScreenPos(ImVec2(lp.x + 384 - 78, lp.y + 4));
                    char idr[24]; sprintf(idr, "Remover##lk%d", q);
                    if (BotaoSec(idr, ImVec2(78, 26), IM_COL32(240, 120, 116, 255))) {
                        gSrv[i].sites[q][0] = 0;
                        CompactarLinks(gSrv[i]);
                        SalvarServidores();
                    }
                    ImGui::SetCursorScreenPos(ImVec2(lp.x, lp.y + 42));
                }
                bool temVaga = false;
                for (int q = 0; q < 4; q++) if (!gSrv[i].sites[q][0]) temVaga = true;
                if (temVaga && !gAddLink) {
                    if (BotaoSec("+  Adicionar link", ImVec2(384, 30))) { gAddLink = true; gNovoRot[0] = gNovoUrl[0] = 0; }
                } else if (temVaga) {
                    ImGui::PushItemWidth(120);
                    ImGui::InputTextWithHint("##nrot", "nome", gNovoRot, sizeof(gNovoRot));
                    ImGui::PopItemWidth();
                    ImGui::SameLine(0, 6);
                    ImGui::PushItemWidth(178);
                    ImGui::InputTextWithHint("##nurl", "https://...", gNovoUrl, sizeof(gNovoUrl));
                    ImGui::PopItemWidth();
                    ImGui::SameLine(0, 6);
                    if (BotaoSec("Ok##nlk", ImVec2(74, 30)) && gNovoUrl[0]) {
                        for (char* c = gNovoRot; *c; c++) if (*c == '|' || *c == '>') *c = ' ';
                        for (char* c = gNovoUrl; *c; c++) if (*c == '|') *c = ' ';
                        for (int q = 0; q < 4; q++) {
                            if (gSrv[i].sites[q][0]) continue;
                            sprintf(gSrv[i].sites[q], "%.22s>%.125s", gNovoRot[0] ? gNovoRot : "Link", gNovoUrl);
                            break;
                        }
                        SalvarServidores();
                        gAddLink = false;
                    }
                }
            }
            ImGui::Dummy(ImVec2(1, 12));
            if (BotaoSec("Trocar imagem de fundo...", ImVec2(188, 38))) {
                for (char* c = gEditApelido; *c; c++) if (*c == '|') *c = '/';
                strncpy(gSrv[i].apelido, gEditApelido, sizeof(gSrv[0].apelido) - 1);
                SalvarServidores();
                gPickImgSrv = i; gEditSrv = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine(0, 8);
            if (BotaoSec("Trocar logo...", ImVec2(188, 38))) {
                for (char* c = gEditApelido; *c; c++) if (*c == '|') *c = '/';
                strncpy(gSrv[i].apelido, gEditApelido, sizeof(gSrv[0].apelido) - 1);
                SalvarServidores();
                gPickLogoSrv = i; gEditSrv = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PushFont(gFtMini);
            ImGui::TextColored(ImColor(Cinza(133)), "Fundo aparece na Home; a logo troca o nome grande.");
            ImGui::PopFont();
            // fileira de capas PRONTAS do launcher: clica e aplica na hora
            GarantirCapasUI();
            if (gNumCapasUI > 0) {
                ImGui::Dummy(ImVec2(1, 6));
                ImGui::PushFont(gFtMini);
                ImGui::TextColored(ImColor(Cinza(156)), "C A P A S   D O   L A U N C H E R");
                ImGui::PopFont();
                ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f);
                ImGui::BeginChild("##capasui", ImVec2(384, 70), false, ImGuiWindowFlags_HorizontalScrollbar);
                ImDrawList* cl2 = ImGui::GetWindowDrawList();
                float cxu = 0;
                for (int k = 0; k < gNumCapasUI; k++) {
                    ImGui::SetCursorPos(ImVec2(cxu, 2));
                    char idc[16]; sprintf(idc, "##capaui%d", k);
                    bool clc = ImGui::InvisibleButton(idc, ImVec2(96, 54));
                    ImVec2 ka = ImGui::GetItemRectMin(), kb = ImGui::GetItemRectMax();
                    bool khov = ImGui::IsItemHovered();
                    if (gCapasTexUI[k]) ImagemCapa(cl2, gCapasTexUI[k], ka, kb, 7.0f, khov ? 0 : 40);
                    else cl2->AddRectFilled(ka, kb, Cinza(20), 7.0f);
                    bool atual = _stricmp(gSrv[i].img, gCapasArqUI[k]) == 0;
                    if (atual || khov) cl2->AddRect(ka, kb, atual ? AC.cor : Cinza(150), 7.0f, 0, atual ? 2.0f : 1.0f);
                    if (clc) { // aplica esta capa (escolha manual: pode repetir a vontade)
                        char velho[MAX_PATH];
                        strncpy(velho, gSrv[i].img, sizeof(velho) - 1); velho[sizeof(velho) - 1] = 0;
                        strncpy(gSrv[i].img, gCapasArqUI[k], sizeof(gSrv[i].img) - 1);
                        if (gSrv[i].tex) {
                            if (gSrv[i].tex == gFundoAtual) gFundoAtual = NULL;
                            if (gSrv[i].tex == gFundoAnt) gFundoAnt = NULL;
                            AdiarRelease(gSrv[i].tex);
                        }
                        gSrv[i].tex = CarregarImagemMax(gDev, gSrv[i].img, 1600);
                        SalvarServidores();
                        DescartarImagemImportada(velho);
                    }
                    cxu += 102;
                }
                ImGui::SetCursorPos(ImVec2(cxu > 0 ? cxu - 6 : 0, 2));
                ImGui::Dummy(ImVec2(1, 1));
                // roda do mouse sobre a fileira = rolar as capas na horizontal
                if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f)
                    ImGui::SetScrollX(ImGui::GetScrollX() - ImGui::GetIO().MouseWheel * 64.0f);
                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
            }
            if (gSrv[i].img[0] || gSrv[i].logo[0]) {
                ImGui::Dummy(ImVec2(1, 2));
                if (gSrv[i].img[0]) {
                    if (BotaoSec("Sortear outra capa", ImVec2(188, 30))) {
                        // "remover" nunca deixa sem imagem: sorteia outra respeitando as regras
                        char velho[MAX_PATH];
                        strncpy(velho, gSrv[i].img, sizeof(velho) - 1); velho[sizeof(velho) - 1] = 0;
                        gSrv[i].img[0] = 0; // sai da conta de "capas em uso" antes do sorteio
                        SortearCapaPadrao(gSrv[i].img, sizeof(gSrv[i].img));
                        if (gSrv[i].tex) {
                            if (gSrv[i].tex == gFundoAtual) gFundoAtual = NULL;
                            if (gSrv[i].tex == gFundoAnt) gFundoAnt = NULL;
                            AdiarRelease(gSrv[i].tex);
                        }
                        gSrv[i].tex = gSrv[i].img[0] ? CarregarImagemMax(gDev, gSrv[i].img, 1600) : NULL;
                        SalvarServidores();
                        DescartarImagemImportada(velho);
                    }
                    if (gSrv[i].logo[0]) ImGui::SameLine(0, 8);
                }
                if (gSrv[i].logo[0]) {
                    if (BotaoSec("Remover logo", ImVec2(188, 30))) {
                        char velho[MAX_PATH];
                        strncpy(velho, gSrv[i].logo, sizeof(velho) - 1); velho[sizeof(velho) - 1] = 0;
                        gSrv[i].logo[0] = 0;
                        AdiarRelease(gSrv[i].texLogo);
                        gSrv[i].texLogo = NULL;
                        SalvarServidores();
                        DescartarImagemImportada(velho);
                    }
                }
            }
            ImGui::Dummy(ImVec2(1, 4));
            static float tConfRemF = 0; // remover e destrutivo: pede confirmacao (2 cliques)
            if (tConfRemF > 0) tConfRemF -= dt;
            bool remFavCl = BotaoSec(tConfRemF > 0 ? "Clique de novo para remover##rmf" : "Remover dos favoritos##rmf",
                                     ImVec2(384, 34), IM_COL32(240, 120, 116, 255));
            if (remFavCl && tConfRemF <= 0) tConfRemF = 3.0f;
            else if (remFavCl) {
                tConfRemF = 0;
                EnterCriticalSection(&gLock);
                if (gSrv[i].tex) {
                    if (gSrv[i].tex == gFundoAtual) gFundoAtual = NULL;
                    if (gSrv[i].tex == gFundoAnt) gFundoAnt = NULL;
                    gSrv[i].tex->Release(); gSrv[i].tex = NULL;
                }
                if (gSrv[i].texLogo) { gSrv[i].texLogo->Release(); gSrv[i].texLogo = NULL; }
                for (int j = i; j < gNumSrv - 1; j++) gSrv[j] = gSrv[j + 1];
                gNumSrv--;
                if (gNumSrv < 1) { // nunca fica vazio
                    memset(&gSrv[0], 0, sizeof(gSrv[0]));
                    strcpy(gSrv[0].nome, "Adicione servidores na aba Internet");
                    strcpy(gSrv[0].ip, "127.0.0.1:7777");
                    gSrv[0].ping = -1;
                    gNumSrv = 1;
                }
                if (gSel >= gNumSrv) gSel = gNumSrv - 1;
                LeaveCriticalSection(&gLock);
                SalvarServidores();
                gEditSrv = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::Dummy(ImVec2(1, 12));
            ImGui::PushFont(gFtBold);
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(AC.cor));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(AC.hi));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(AC.hi));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(TextoSobreAccent(AC.cor)));
            if (ImGui::Button("Salvar", ImVec2(188, 40))) {
                for (char* c = gEditApelido; *c; c++) if (*c == '|') *c = '/';
                strncpy(gSrv[i].apelido, gEditApelido, sizeof(gSrv[0].apelido) - 1);
                SalvarServidores();
                gEditSrv = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(4);
            ImGui::SameLine(0, 8);
            if (BotaoSec("Cancelar", ImVec2(188, 40))) { gEditSrv = -1; ImGui::CloseCurrentPopup(); }
            ImGui::PopFont();
            ImGui::PopFont();
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }

    // ===== sidebar (desenhada por ULTIMO: expandida cobre o conteudo, estilo Rockstar) =====
    {
        float alvoS = gSideAberta ? 1.0f : 0.0f;
        float velS = dt * 8.0f;
        if (velS > 1.0f) velS = 1.0f;
        gSideAnim += (alvoS - gSideAnim) * velS; // aproximacao exponencial: entra e sai macio
        if (fabsf(gSideAnim - alvoS) < 0.004f) gSideAnim = alvoS;
        float eS = gSideAnim * gSideAnim * (3.0f - 2.0f * gSideAnim); // smoothstep
        float wSide = 76.0f + 154.0f * eS;
        ImGui::SetCursorScreenPos(ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::BeginChild("##sidebar", ImVec2(wSide, ds.y), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImDrawList* sl = ImGui::GetWindowDrawList();
        sl->AddRectFilled(ImVec2(0, 0), ImVec2(wSide, ds.y), IM_COL32(6, 6, 6, 252)); // preto Rockstar
        if (eS > 0.01f) { // sombrinha na borda quando cobre o conteudo
            sl->AddRectFilledMultiColor(ImVec2(wSide, 0), ImVec2(wSide + 18.0f * eS, ds.y),
                Cinza(0, (int)(90 * eS)), Cinza(0, 0), Cinza(0, 0), Cinza(0, (int)(90 * eS)));
        }
        // logo: o ALVO do TrokMods na cor de destaque (o .ico do exe segue sendo a loira)
        if (gLogoTrok)
            sl->AddImage((ImTextureID)gLogoTrok, ImVec2(23, 23), ImVec2(53, 53),
                         ImVec2(0, 0), ImVec2(1, 1), AC.cor);
        else
            sl->AddRectFilled(ImVec2(23, 23), ImVec2(53, 53), AC.cor, 9.0f);
        static const int NAVS[6] = { 0, 1, 2, 4, 5, 3 }; // sem a aba Informacoes
        static const char* NOMES_NAV[7] = { "Início", "Servidores", "Datas", "Configurações",
                                            "Galeria", "TrokMods", "Informações" };
        int alfaTxt = (int)(255.0f * eS);
        for (int p = 0; p < 6; p++) {
            int i = NAVS[p];
            float y = 96.0f + p * 56.0f;
            ImGui::SetCursorPos(ImVec2(14, y));
            char id[8]; sprintf(id, "##nav%d", i);
            if (ImGui::InvisibleButton(id, ImVec2(wSide - 28.0f, 46))) {
                gTela = i;
                if (i == 5) { // abriu a aba mods: a bolinha do icone some; os cards NOVOS
                    gModsNovo = false; // desta visita ainda mostram a bolinha deles
                    strncpy(gModsVistoAte, gModsUltimo, sizeof(gModsVistoAte) - 1);
                    gModsVistoAte[sizeof(gModsVistoAte) - 1] = 0;
                    if (gNumMods > 0 && _stricmp(gModsUltimo, gMods[0].url) != 0) {
                        strncpy(gModsUltimo, gMods[0].url, sizeof(gModsUltimo) - 1);
                        gModsUltimo[sizeof(gModsUltimo) - 1] = 0;
                        SalvarConfig();
                    }
                    if (gModsEstado == 0) CreateThread(NULL, 0, ThreadMods, NULL, 0, NULL);
                }
            }
            bool hov = ImGui::IsItemHovered();
            if (hov && gTela != i)
                sl->AddRectFilled(ImVec2(14, y), ImVec2(wSide - 14.0f, y + 46), Cinza(22), 12);
            if (gTela == i)
                sl->AddRectFilled(ImVec2(0, y + 10), ImVec2(4, y + 36), AC.cor, 2);
            IconeNav(sl, ImVec2(38, y + 23), i, gTela == i ? Cinza(250) : (hov ? Cinza(215) : Cinza(125)));
            if (alfaTxt > 8) { // nome da aba (aparece junto com a expansao)
                ImGui::PushFont(gFtBold);
                ImVec2 tsz = ImGui::CalcTextSize(NOMES_NAV[i]);
                sl->AddText(ImVec2(66, y + (46 - tsz.y) * 0.5f),
                            Cinza(gTela == i ? 245 : (hov ? 220 : 150), alfaTxt), NOMES_NAV[i]);
                ImGui::PopFont();
            }
            if (i == 5 && gModsNovo && gTela != 5) { // post novo no blog: bolinha pulsando
                float pulso = 0.55f + 0.45f * sinf((float)ImGui::GetTime() * 5.0f);
                sl->AddCircleFilled(ImVec2(54, y + 12), 4.0f, ComAlpha(AC.cor, 0.35f + 0.65f * pulso), 12);
            }
        }
        // botao de expandir/recolher (sutil: so um chevron duplo, sem caixa)
        {
            ImGui::SetCursorPos(ImVec2(14, ds.y - 64));
            bool clS = ImGui::InvisibleButton("##sidetoggle", ImVec2(48, 30));
            bool hovS = ImGui::IsItemHovered();
            if (clS) gSideAberta = !gSideAberta;
            // ícone lucide do painel (o mesmo conceito do launcher da Rockstar)
            ImU32 cS = Cinza(hovS ? 225 : 115);
            Icone(sl, ImVec2(38.0f, ds.y - 49.0f), gSideAberta ? I_PAINEL_FECHA : I_PAINEL_ABRE, cS, 19.0f);
            if (hovS) ImGui::SetTooltip(gSideAberta ? "Recolher menu" : "Expandir menu");
        }
        ImGui::PushFont(gFtMini);
        sl->AddText(ImVec2(28, ds.y - 26), Cinza(95), VERSAO);
        ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    // ===== modal: nova atualizacao (novidades + baixar) =====
    {
        if (gAttBaixa == 3) { // baixou (modal aberto ou nao): roda o setup --atualizar e fecha
            gAttBaixa = 4; // dispara UMA vez
            ShellExecuteA(NULL, "open", gAttArquivo, "--atualizar", NULL, SW_SHOWNORMAL);
            gRodando = false;
        }
        if (gAttPopup && !ImGui::IsPopupOpen("Atualização##trok")) ImGui::OpenPopup("Atualização##trok");
        ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26, 24));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(IM_COL32(14, 14, 14, 252)));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(Cinza(58)));
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.01f, 0.01f, 0.01f, 0.72f));
        if (ImGui::BeginPopupModal("Atualização##trok", NULL,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove)) {
            ImGui::PushFont(gFtBotao);
            ImGui::TextColored(ImColor(Cinza(245)), "NOVA ATUALIZAÇÃO");
            ImGui::PopFont();
            ImGui::PushFont(gFtMini);
            char vtxt[80];
            sprintf(vtxt, "%s disponível  -  você está na %s", gAttVersao, VERSAO);
            ImGui::TextColored(ImColor(Cinza(140)), "%s", vtxt); // "%s": conteudo vem da internet
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(1, 10));
            ImGui::PushFont(gFtMini);
            ImGui::TextColored(ImColor(Cinza(156)), "N O V I D A D E S");
            ImGui::PopFont();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(IM_COL32(10, 10, 10, 255)));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f);
            ImGui::BeginChild("##attnotas", ImVec2(384, 170), false);
            ImGui::SetCursorPos(ImVec2(12, 10));
            ImGui::PushFont(gFtBody);
            ImGui::PushTextWrapPos(370);
            ImGui::TextColored(ImColor(Cinza(205)), "%s", gAttNotas[0] ? gAttNotas : "Melhorias e correções.");
            ImGui::PopTextWrapPos();
            ImGui::PopFont();
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(1, 12));
            if (gAttBaixa == 1 || gAttBaixa >= 3) {
                // baixando: barra de progresso no lugar dos botoes
                ImDrawList* al = ImGui::GetWindowDrawList();
                ImVec2 bp = ImGui::GetCursorScreenPos();
                ImGui::Dummy(ImVec2(384, 40));
                ImVec2 b0(bp.x, bp.y + 10), b1(bp.x + 384, bp.y + 18);
                al->AddRectFilled(b0, b1, Cinza(34), 4);
                int pctD = gAttPct;
                if (pctD < 0) { // tamanho desconhecido: vai-e-vem
                    float tA = (float)fmod(ImGui::GetTime() * 0.9, 1.0);
                    float wS = 100.0f, xS = b0.x + (384.0f - wS) * (0.5f - 0.5f * cosf(tA * 6.2831f));
                    al->AddRectFilled(ImVec2(xS, b0.y), ImVec2(xS + wS, b1.y), AC.cor, 4);
                } else if (pctD > 0) {
                    al->AddRectFilled(b0, ImVec2(b0.x + 384.0f * pctD / 100.0f, b1.y), AC.cor, 4);
                }
                ImGui::PushFont(gFtMini);
                char ptx[48];
                if (pctD >= 0) sprintf(ptx, "Baixando a atualização...  %d%%", pctD);
                else strcpy(ptx, "Baixando a atualização...");
                al->AddText(ImVec2(bp.x, bp.y + 24), Cinza(150), ptx);
                ImGui::PopFont();
            } else {
                ImGui::PushFont(gFtBold);
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(AC.cor));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(AC.hi));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(AC.hi));
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(TextoSobreAccent(AC.cor)));
                const char* pex = strrchr(gAttUrl, '.');
                bool linkExe = pex && _stricmp(pex, ".exe") == 0;
                if (ImGui::Button("Atualizar agora", ImVec2(188, 40))) {
                    if (linkExe) { // baixa e instala sozinho, sem sair do app
                        gAttBaixa = 1;
                        CreateThread(NULL, 0, ThreadBaixarUpdate, NULL, 0, NULL);
                    } else if (gAttUrl[0]) { // link nao e um exe: cai pro navegador
                        ShellExecuteA(NULL, "open", gAttUrl, NULL, NULL, SW_SHOWNORMAL);
                        gAttPopup = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::PopStyleColor(4);
                ImGui::SameLine(0, 8);
                if (BotaoSec("Depois", ImVec2(188, 40))) { gAttPopup = false; ImGui::CloseCurrentPopup(); }
                ImGui::PopFont();
                if (gAttBaixa == 2) { // download falhou: avisa e oferece o navegador
                    ImGui::Dummy(ImVec2(1, 6));
                    ImGui::PushFont(gFtMini);
                    ImGui::TextColored(ImColor(IM_COL32(240, 120, 116, 255)), "Não consegui baixar sozinho.");
                    ImGui::SameLine(0, 10);
                    if (ImGui::SmallButton("Abrir no navegador")) { // manda pro post do blog, nao pro GitHub
                        ShellExecuteA(NULL, "open", URL_POST_LAUNCHER, NULL, NULL, SW_SHOWNORMAL);
                    }
                    ImGui::PopFont();
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }

    // ===== modal: boas-vindas (sem SA-MP ele NAO vai embora: e o porteiro do app) =====
    {
        if (gBoasVindas && gSampOk) gBoasVindas = false; // achou samp valido: liberado
        if (gBoasVindas && !ImGui::IsPopupOpen("BemVindo##trok")) ImGui::OpenPopup("BemVindo##trok");
        ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26, 24));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(IM_COL32(14, 14, 14, 252)));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(Cinza(58)));
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.01f, 0.01f, 0.01f, 0.72f));
        if (ImGui::BeginPopupModal("BemVindo##trok", NULL,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove)) {
            ImGui::PushFont(gFtBotao);
            ImGui::TextColored(ImColor(Cinza(245)), "BEM-VINDO AO TROK LAUNCHER");
            ImGui::PopFont();
            ImGui::PushFont(gFtBody);
            ImGui::PushTextWrapPos(400);
            ImGui::TextColored(ImColor(Cinza(190)),
                "Não encontrei o seu GTA San Andreas com SA-MP neste computador. "
                "Aponte o gta_sa.exe da sua instalacao e o resto o launcher resolve.");
            ImGui::PopTextWrapPos();
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(1, 12));
            ImGui::PushFont(gFtBold);
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(AC.cor));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(AC.hi));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(AC.hi));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(TextoSobreAccent(AC.cor)));
            if (ImGui::Button("Escolher o gta_sa.exe...", ImVec2(400, 42))) { // largura toda do modal
                gPickCaminho = gDataSel; // o picker roda fora do frame e atualiza a data em uso
                ImGui::CloseCurrentPopup(); // sem SA-MP valido o popup VOLTA no proximo frame
            }
            ImGui::PopStyleColor(4);
            ImGui::PopFont();
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }

    // ===== modal: senha do servidor (aparece sozinho ao conectar em servidor trancado) =====
    {
        if (gPedirSenha && !ImGui::IsPopupOpen("Senha##trok")) ImGui::OpenPopup("Senha##trok");
        ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26, 24));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(IM_COL32(14, 14, 14, 252)));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(Cinza(58)));
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.01f, 0.01f, 0.01f, 0.72f));
        if (ImGui::BeginPopupModal("Senha##trok", NULL,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove)) {
            ImGui::PushFont(gFtBotao);
            ImGui::TextColored(ImColor(Cinza(245)), "SERVIDOR COM SENHA");
            ImGui::PopFont();
            ImGui::PushFont(gFtMini);
            ImGui::TextColored(ImColor(Cinza(140)), "%s", gPendNome); // "%s": nome vem do servidor (não e format string!)
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(1, 10));
            ImGui::PushItemWidth(384);
            ImGui::PushFont(gFtBold);
            if (!ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) ImGui::SetKeyboardFocusHere();
            bool entrou = ImGui::InputText("##srvsenha", gConnSenha, sizeof(gConnSenha),
                                           ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopFont();
            ImGui::PopItemWidth();
            ImGui::Dummy(ImVec2(1, 12));
            ImGui::PushFont(gFtBold);
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(AC.cor));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(AC.hi));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(AC.hi));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(TextoSobreAccent(AC.cor)));
            bool conectar = ImGui::Button("Conectar", ImVec2(188, 40));
            ImGui::PopStyleColor(4);
            ImGui::SameLine(0, 8);
            if (BotaoSec("Cancelar", ImVec2(188, 40))) {
                gPedirSenha = false;
                gConnSenha[0] = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopFont();
            if ((conectar || entrou) && gConnSenha[0]) {
                gPedirSenha = false;
                ImGui::CloseCurrentPopup();
                if (gSalvarSenhaServ) { // opcao do SA-MP ligada: guarda no USERDATA.DAT
                    char sv[32];
                    strncpy(sv, gConnSenha, sizeof(sv) - 1);
                    sv[sizeof(sv) - 1] = 0;
                    for (char* c = sv; *c; c++) if (*c == '"') *c = '\''; // igual ao que vai pro samp
                    GravarSenhaFavorito(gPendIp, gPendNome, sv);
                }
                IniciarConexaoReal(gPendNome, gPendIp);
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }

    // ===== visualizador da galeria (modal em tela cheia: setas, teclado, abrir pasta) =====
    {
        if (gFotoVista >= 0 && !ImGui::IsPopupOpen("##fotover")) ImGui::OpenPopup("##fotover");
        if (gFotoVista >= 0) {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ds);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.012f, 0.012f, 0.012f, 0.97f));
            ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0.5f));
            if (ImGui::BeginPopupModal("##fotover", NULL, ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
                ImDrawList* vl = ImGui::GetWindowDrawList();
                bool fechar = false;
                Foto& f = gFotos[gFotoVista];
                FGPodar(); // solta full-res que nao e mais a atual nem vizinha
                FGGarantir(f.caminho); // a atual PRIMEIRO (garante slot pra ela)
                if (gFotoVista + 1 < gNumFotos) FGGarantir(gFotos[gFotoVista + 1].caminho);
                if (gFotoVista > 0) FGGarantir(gFotos[gFotoVista - 1].caminho);
                FGSlot* sAtu = FGSlotDe(f.caminho);
                IDirect3DTexture9* texCheia = sAtu ? sAtu->tex : NULL;
                if (gFGFadeIdx != gFotoVista) { // vizinha pre-carregada abre ja nitida, sem fade
                    gFGFadeIdx = gFotoVista;
                    gFGFade = texCheia ? 1.0f : 0.0f;
                }
                if (texCheia && gFGFade < 1.0f) {
                    gFGFade += dt * 7.0f;
                    if (gFGFade > 1.0f) gFGFade = 1.0f;
                }
                IDirect3DTexture9* texV = texCheia ? texCheia : f.tex; // thumb segura a tela
                ImVec2 imgA(0, 0), imgB(0, 0); // retangulo da foto (clicar FORA dele fecha)
                if (texV) {
                    D3DSURFACE_DESC td2;
                    if (SUCCEEDED(texV->GetLevelDesc(0, &td2)) && td2.Height) {
                        float maxW = ds.x - 170, maxH = ds.y - 130;
                        float esc = maxW / td2.Width;
                        if (td2.Height * esc > maxH) esc = maxH / td2.Height;
                        float w2 = td2.Width * esc, h2 = td2.Height * esc;
                        imgA = ImVec2((ds.x - w2) * 0.5f, (ds.y - h2) * 0.5f - 12);
                        imgB = ImVec2(imgA.x + w2, imgA.y + h2);
                        if (texCheia) { // thumb por baixo + full-res com alpha = cross-fade suave
                            if (gFGFade < 1.0f && f.tex)
                                vl->AddImage((ImTextureID)f.tex, imgA, imgB);
                            vl->AddImage((ImTextureID)texCheia, imgA, imgB, ImVec2(0, 0), ImVec2(1, 1),
                                         IM_COL32(255, 255, 255, (int)(gFGFade * 255.0f + 0.5f)));
                        } else {
                            vl->AddImage((ImTextureID)texV, imgA, imgB);
                        }
                        vl->AddRect(imgA, imgB, Cinza(52), 0, 0, 1.0f);
                    }
                }
                // fechar (X)
                ImGui::SetCursorPos(ImVec2(ds.x - 56, 16));
                if (ImGui::InvisibleButton("##fvx", ImVec2(38, 32))) fechar = true;
                {
                    ImVec2 xa = ImGui::GetItemRectMin(), xb = ImGui::GetItemRectMax();
                    if (ImGui::IsItemHovered()) vl->AddRectFilled(xa, xb, Cinza(255, 24), 8);
                    Icone(vl, ImVec2((xa.x + xb.x) * 0.5f, (xa.y + xb.y) * 0.5f), I_FECHAR, Cinza(220), 22.0f);
                }
                // setas
                if (gFotoVista > 0) {
                    ImGui::SetCursorPos(ImVec2(16, ds.y * 0.5f - 70));
                    if (ImGui::InvisibleButton("##fvant", ImVec2(54, 140))) gFotoVista--;
                    ImVec2 aa = ImGui::GetItemRectMin();
                    Icone(vl, ImVec2(aa.x + 27, aa.y + 70), I_SETA_ESQ,
                          Cinza(ImGui::IsItemHovered() ? 255 : 150), 34.0f);
                }
                if (gFotoVista < gNumFotos - 1) {
                    ImGui::SetCursorPos(ImVec2(ds.x - 70, ds.y * 0.5f - 70));
                    if (ImGui::InvisibleButton("##fvpro", ImVec2(54, 140))) gFotoVista++;
                    ImVec2 aa = ImGui::GetItemRectMin();
                    Icone(vl, ImVec2(aa.x + 27, aa.y + 70), I_SETA_DIR,
                          Cinza(ImGui::IsItemHovered() ? 255 : 150), 34.0f);
                }
                // rodape: nome do arquivo + contagem
                {
                    const char* nomeArq = strrchr(f.caminho, '\\');
                    nomeArq = nomeArq ? nomeArq + 1 : f.caminho;
                    char rodape[320];
                    sprintf(rodape, "%s   (%d de %d)", nomeArq, gFotoVista + 1, gNumFotos);
                    ImGui::PushFont(gFtMini);
                    ImVec2 rsz = ImGui::CalcTextSize(rodape);
                    vl->AddText(ImVec2((ds.x - rsz.x) * 0.5f, ds.y - 42), Cinza(165), rodape);
                    ImGui::PopFont();
                }
                // acoes por ICONE: lixeira (2 cliques, vai para a Lixeira) + copiar imagem
                {
                    static float tConfEx = 0, tCop2 = 0;
                    if (tConfEx > 0) tConfEx -= dt;
                    if (tCop2 > 0) tCop2 -= dt;
                    // --- lixeira ---
                    ImGui::SetCursorPos(ImVec2(24, ds.y - 58));
                    bool exCl = ImGui::InvisibleButton("##fvlixo", ImVec2(40, 36));
                    ImVec2 la2 = ImGui::GetItemRectMin(), lb2 = ImGui::GetItemRectMax();
                    bool exHov = ImGui::IsItemHovered();
                    bool conf = tConfEx > 0;
                    ImU32 exCor = conf ? IM_COL32(240, 96, 92, 255) : Cinza(exHov ? 250 : 175);
                    if (exHov || conf) vl->AddRectFilled(la2, lb2, conf ? IM_COL32(240, 96, 92, 34) : Cinza(255, 18), 9);
                    Icone(vl, ImVec2((la2.x + lb2.x) * 0.5f, (la2.y + lb2.y) * 0.5f), I_LIXEIRA, exCor, 18.0f);
                    Dica(conf ? "Clique de novo para excluir" : "Excluir (vai para a Lixeira do Windows)");
                    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) exCl = true;
                    if (exCl) {
                        if (conf) {
                            if (ExcluirParaLixeira(gFotos[gFotoVista].caminho)) {
                                AdiarRelease(gFotos[gFotoVista].tex);
                                FGLimpar(); // os indices mudaram; repede no proximo frame
                                for (int j = gFotoVista; j < gNumFotos - 1; j++) gFotos[j] = gFotos[j + 1];
                                gNumFotos--;
                                if (gFotoVista >= gNumFotos) gFotoVista = gNumFotos - 1;
                                if (gFotoVista < 0) fechar = true;
                            }
                            tConfEx = 0;
                        } else tConfEx = 3.0f;
                    }
                    // --- copiar imagem ---
                    ImGui::SetCursorPos(ImVec2(70, ds.y - 58));
                    bool cpCl = ImGui::InvisibleButton("##fvcopy", ImVec2(40, 36));
                    ImVec2 ca2 = ImGui::GetItemRectMin(), cb2v = ImGui::GetItemRectMax();
                    bool cpHov = ImGui::IsItemHovered();
                    ImU32 cpCor = Cinza(cpHov ? 250 : 175);
                    if (cpHov) vl->AddRectFilled(ca2, cb2v, Cinza(255, 18), 9);
                    Icone(vl, ImVec2((ca2.x + cb2v.x) * 0.5f, (ca2.y + cb2v.y) * 0.5f), I_COPIAR, cpCor, 17.0f);
                    Dica("Copiar imagem");
                    if (cpCl && !fechar && CopiarImagemClipboard(gHwnd, gFotos[gFotoVista >= 0 ? gFotoVista : 0].caminho)) tCop2 = 1.3f;
                    // avisos ao lado dos icones
                    ImGui::PushFont(gFtMini);
                    if (conf) vl->AddText(ImVec2(cb2v.x + 10, la2.y + 11), IM_COL32(240, 120, 116, 255), "clique de novo para excluir");
                    else if (tCop2 > 0) vl->AddText(ImVec2(cb2v.x + 10, la2.y + 11), Cinza(220), "Copiado!");
                    ImGui::PopFont();
                }
                // botao direito na foto: menu com "Copiar imagem" (vai em resolucao original)
                if (ImGui::IsMouseClicked(1)) ImGui::OpenPopup("##ctxfoto");
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
                ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(IM_COL32(16, 16, 16, 252)));
                if (ImGui::BeginPopup("##ctxfoto")) {
                    ImDrawList* cx2 = ImGui::GetWindowDrawList();
                    static float tCop = 0;
                    if (tCop > 0) tCop -= dt;
                    const char* rotCop = tCop > 0 ? "Copiado!" : "Copiar imagem";
                    ImGui::PushFont(gFtBody);
                    ImVec2 csz2 = ImGui::CalcTextSize(rotCop);
                    if (ImGui::InvisibleButton("##cpimg", ImVec2(csz2.x + 60, 32))) {
                        if (CopiarImagemClipboard(gHwnd, f.caminho)) tCop = 1.2f;
                    }
                    ImVec2 ba2 = ImGui::GetItemRectMin(), bb2 = ImGui::GetItemRectMax();
                    if (ImGui::IsItemHovered()) cx2->AddRectFilled(ba2, bb2, Cinza(255, 16), 8);
                    cx2->AddText(ImVec2(ba2.x + 10, (ba2.y + bb2.y - csz2.y) * 0.5f), Cinza(230), rotCop);
                    ImGui::PopFont();
                    ImGui::EndPopup();
                }
                ImGui::PopStyleColor();
                ImGui::PopStyleVar(2);
                // clicar no preto (fora da foto e de qualquer botao) fecha o visualizador
                if (!ImGui::IsPopupOpen("##ctxfoto") && ImGui::IsMouseClicked(0) &&
                    !ImGui::IsAnyItemHovered() &&
                    !(io.MousePos.x >= imgA.x && io.MousePos.x <= imgB.x &&
                      io.MousePos.y >= imgA.y && io.MousePos.y <= imgB.y))
                    fechar = true;
                // teclado: setas navegam, ESC fecha
                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && gFotoVista > 0) gFotoVista--;
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && gFotoVista < gNumFotos - 1) gFotoVista++;
                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) fechar = true;
                if (fechar) {
                    gFotoVista = -1;
                    FGLimpar(); // full-res nao fica na memoria com o viewer fechado
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(3);
        }
    }

    // ===== dropdown CONTAS colapsavel (ancorado no controle nick+avatar do topo) =====
    {
        gContasAnim = gEscolherAvatar ? 1.0f : 0.0f; // abre/fecha direto, sem animacao (pedido)
        if (gContasAnim > 0.01f) {
            const float PW = 360.0f;
            int itensAv = gNumAvatares > 0 ? gNumAvatares : N_ACCENTS;
            int linhasAv = (itensAv + 4) / 5;
            float HL = (gContaEdit == -1)
                ? 36.0f + gNumPerfis * 58.0f + (gNumPerfis < MAX_PERFIS ? 58.0f : 12.0f)
                : 122.0f + linhasAv * 68.0f + 58.0f + ((gContaEdit >= 0 && gNumPerfis > 1) ? 42.0f : 0.0f);
            if (HL > ds.y - 80.0f) HL = ds.y - 80.0f;
            float PH = HL * gContasAnim;
            gContasA = ImVec2(ds.x - 34.0f - PW, 60.0f);
            gContasB = ImVec2(gContasA.x + PW, 60.0f + PH);
            ImGui::SetCursorScreenPos(gContasA);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(IM_COL32(14, 14, 14, 252)));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f);
            ImGuiWindowFlags fDrop = (gContasAnim < 0.999f) ? (ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse) : 0;
            ImGui::BeginChild("##dropcontas", ImVec2(PW, PH), false, fDrop);
            ImDrawList* ml = ImGui::GetWindowDrawList();
            if (gContaEdit == -1) {
                // ---- lista de contas ----
                ImGui::SetCursorPos(ImVec2(16, 12));
                ImGui::PushFont(gFtMini);
                ImGui::TextColored(ImColor(Cinza(156)), "C O N T A S");
                ImGui::PopFont();
                float y2 = 36;
                for (int i = 0; i < gNumPerfis; i++) {
                    ImGui::SetCursorPos(ImVec2(14, y2));
                    char idc[16]; sprintf(idc, "##conta%d", i);
                    bool cl = ImGui::InvisibleButton(idc, ImVec2(332, 54));
                    ImVec2 la = ImGui::GetItemRectMin(), lb = ImGui::GetItemRectMax();
                    bool hovL = ImGui::IsItemHovered();
                    ImVec2 ma(lb.x - 34, la.y + 16), mb(lb.x - 8, la.y + 38);
                    bool mh = ImGui::IsMouseHoveringRect(ma, mb);
                    if (hovL && !mh) ml->AddRectFilled(la, lb, Cinza(255, 14), 10);
                    if (i == gPerfilSel) ml->AddRect(la, lb, ComAlpha(AC.cor, 0.85f), 10, 0, 1.6f);
                    DesenhaAvatar(ml, ImVec2(la.x + 8, la.y + 8), ImVec2(la.x + 46, la.y + 46),
                                  gPerfis[i].avatar, gPerfis[i].cor, 10.0f);
                    ImGui::PushFont(gFtBold);
                    ml->AddText(ImVec2(la.x + 60, la.y + 9), Cinza(240), gPerfis[i].nick);
                    ImGui::PopFont();
                    ImGui::PushFont(gFtMini);
                    ml->AddText(ImVec2(la.x + 60, la.y + 31), (i == gPerfilSel) ? AC.hi : Cinza(120),
                                (i == gPerfilSel) ? "EM USO" : "clique para usar");
                    ImGui::PopFont();
                    // "..." da conta: editar nick/avatar/remover
                    DesenhaReticencias(ml, ma, mb, mh);
                    if (mh && ImGui::IsMouseClicked(0)) {
                        gContaEdit = i;
                        strncpy(gEditNick, gPerfis[i].nick, sizeof(gEditNick) - 1);
                        gEditNick[sizeof(gEditNick) - 1] = 0;
                        gEditCor = gPerfis[i].cor % N_ACCENTS;
                        strncpy(gEditAvatar, gPerfis[i].avatar, sizeof(gEditAvatar) - 1);
                        gEditAvatar[sizeof(gEditAvatar) - 1] = 0;
                    } else if (cl && !mh) {
                        AplicarPerfil(i, true); // troca MANUAL: vence a pre-selecao dos servidores
                        gEscolherAvatar = false;
                    }
                    y2 += 58;
                }
                if (gNumPerfis < MAX_PERFIS) {
                    ImGui::SetCursorPos(ImVec2(14, y2 + 4));
                    ImGui::PushFont(gFtBold);
                    if (BotaoSec("+  Adicionar conta", ImVec2(332, 42))) {
                        gContaEdit = -2; // modo CRIACAO: so vira conta de verdade no Salvar
                        strcpy(gEditNick, "Novo_Nick");
                        gEditCor = gNumPerfis % N_ACCENTS;
                        gEditAvatar[0] = 0;
                        if (gNumAvatares > 0) { // nasce com um retrato aleatorio ja marcado
                            int r = (int)(GetTickCount() % (DWORD)gNumAvatares);
                            strncpy(gEditAvatar, gAvatares[r].arquivo, sizeof(gEditAvatar) - 1);
                            gEditAvatar[sizeof(gEditAvatar) - 1] = 0;
                        }
                    }
                    ImGui::PopFont();
                }
            } else {
                // ---- edicao de uma conta ----
                int i = gContaEdit;
                ImGui::SetCursorPos(ImVec2(16, 12));
                ImGui::PushFont(gFtMini);
                ImGui::TextColored(ImColor(Cinza(156)), gContaEdit == -2 ? "N O V A   C O N T A" : "E D I T A R   C O N T A");
                ImGui::PopFont();
                ImGui::SetCursorPos(ImVec2(16, 40));
                ImGui::PushFont(gFtMini);
                ImGui::TextColored(ImColor(Cinza(120)), "N I C K");
                ImGui::PopFont();
                ImGui::SetCursorPos(ImVec2(14, 58));
                ImGui::PushItemWidth(332);
                ImGui::PushFont(gFtBold);
                ImGui::InputText("##edconta", gEditNick, sizeof(gEditNick));
                ImGui::PopFont();
                ImGui::PopItemWidth();
                ImGui::SetCursorPos(ImVec2(16, 100));
                ImGui::PushFont(gFtMini);
                ImGui::TextColored(ImColor(Cinza(120)), "A V A T A R");
                ImGui::PopFont();
                if (gNumAvatares > 0) { // grade de IMAGENS da pasta de avatares
                    for (int c = 0; c < gNumAvatares; c++) {
                        ImGui::SetCursorPos(ImVec2(16.0f + (c % 5) * 68.0f, 122.0f + (c / 5) * 68.0f));
                        char idav[12]; sprintf(idav, "##av%d", c);
                        if (ImGui::InvisibleButton(idav, ImVec2(56, 56))) {
                            strncpy(gEditAvatar, gAvatares[c].arquivo, sizeof(gEditAvatar) - 1);
                            gEditAvatar[sizeof(gEditAvatar) - 1] = 0;
                        }
                        ImVec2 aa = ImGui::GetItemRectMin(), ab = ImGui::GetItemRectMax();
                        if (gAvatares[c].tex)
                            ml->AddImageRounded((ImTextureID)gAvatares[c].tex, aa, ab,
                                                ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 14.0f);
                        else
                            ml->AddRectFilled(aa, ab, Cinza(30), 14);
                        bool selAv = _stricmp(gEditAvatar, gAvatares[c].arquivo) == 0;
                        if (selAv || ImGui::IsItemHovered())
                            ml->AddRect(aa, ab, selAv ? AC.cor : Cinza(250), 14, 0, 2.0f);
                    }
                } else { // sem imagens: cores (fallback)
                    for (int c = 0; c < N_ACCENTS; c++) {
                        ImGui::SetCursorPos(ImVec2(16.0f + (c % 5) * 68.0f, 122.0f + (c / 5) * 68.0f));
                        char idav[12]; sprintf(idav, "##av%d", c);
                        if (ImGui::InvisibleButton(idav, ImVec2(56, 56))) gEditCor = c;
                        ImVec2 aa = ImGui::GetItemRectMin(), ab = ImGui::GetItemRectMax();
                        RectGradVertical(ml, aa, ab, ACCENTS[c].hi, ACCENTS[c].cor, 14.0f);
                        if (c == gEditCor || ImGui::IsItemHovered())
                            ml->AddRect(aa, ab, Cinza(250), 14, 0, 2.0f);
                    }
                }
                float yb = 122.0f + linhasAv * 68.0f + 4.0f;
                if (gContaEdit >= 0 && gNumPerfis > 1) {
                    ImGui::SetCursorPos(ImVec2(14, yb));
                    if (BotaoSec("Remover conta", ImVec2(332, 32), IM_COL32(240, 120, 116, 255))) {
                        for (int j = i; j < gNumPerfis - 1; j++) gPerfis[j] = gPerfis[j + 1];
                        gNumPerfis--;
                        if (gPerfilSel > i) gPerfilSel--;
                        else if (gPerfilSel == i) {
                            if (gPerfilSel >= gNumPerfis) gPerfilSel = gNumPerfis - 1;
                            AplicarPerfil(gPerfilSel, true); // a conta em uso foi removida: assume a vizinha
                        }
                        SalvarPerfis();
                        gContaEdit = -1;
                    }
                    yb += 42;
                }
                ImGui::SetCursorPos(ImVec2(14, yb));
                ImGui::PushFont(gFtBold);
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(AC.cor));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(AC.hi));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(AC.hi));
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(TextoSobreAccent(AC.cor)));
                if (ImGui::Button("Salvar", ImVec2(162, 40)) && gContaEdit != -1) {
                    for (char* c = gEditNick; *c; c++) if (*c == ' ' || *c == '|') *c = '_'; // nick SA-MP nao tem espaco
                    if (!gEditNick[0]) strcpy(gEditNick, "Nick_Sobrenome");
                    if (gContaEdit == -2) { // criacao: a conta so nasce AQUI
                        if (gNumPerfis < MAX_PERFIS) {
                            Perfil& np = gPerfis[gNumPerfis];
                            memset(&np, 0, sizeof(np));
                            strncpy(np.nick, gEditNick, sizeof(np.nick) - 1);
                            np.cor = gEditCor;
                            strncpy(np.avatar, gEditAvatar, sizeof(np.avatar) - 1);
                            gNumPerfis++;
                        }
                    } else {
                        strncpy(gPerfis[i].nick, gEditNick, sizeof(gPerfis[0].nick) - 1);
                        gPerfis[i].cor = gEditCor;
                        strncpy(gPerfis[i].avatar, gEditAvatar, sizeof(gPerfis[0].avatar) - 1);
                        if (i == gPerfilSel) { // editou a conta em uso: aplica na hora
                            strncpy(gNick, gEditNick, sizeof(gNick) - 1);
                            gAvatarCor = gEditCor;
                            GravarNickRegistro();
                            SalvarConfig();
                        }
                    }
                    SalvarPerfis();
                    gContaEdit = -1;
                }
                ImGui::PopStyleColor(4);
                ImGui::SameLine(0, 8);
                if (BotaoSec("Voltar", ImVec2(162, 40))) gContaEdit = -1;
                ImGui::PopFont();
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
            dl->AddRect(gContasA, gContasB, Cinza(58), 14, 0, 1.0f);
            // clique fora recolhe (clicar no controle do topo ja alterna sozinho)
            if (gEscolherAvatar && ImGui::IsMouseClicked(0) &&
                !ImGui::IsMouseHoveringRect(gContasA, gContasB) &&
                !ImGui::IsMouseHoveringRect(gCtTopA, gCtTopB))
                gEscolherAvatar = false;
        } else {
            gContasA = gContasB = ImVec2(0, 0);
        }
    }
    // ===== card fantasma completo seguindo o mouse durante o arrasto =====
    {
        const ImGuiPayload* payG = ImGui::GetDragDropPayload();
        if (payG && payG->IsDataType("TROK_FAV")) {
            int gi = *(const int*)payG->Data;
            if (gi >= 0 && gi < gNumSrv) {
                EnterCriticalSection(&gLock);
                Servidor sg = gSrv[gi];
                LeaveCriticalSection(&gLock);
                ImDrawList* fg = ImGui::GetForegroundDrawList();
                ImVec2 ga(io.MousePos.x - 46, io.MousePos.y - 20);
                ImVec2 gb2(ga.x + 274, ga.y + 144);
                fg->AddRectFilled(ImVec2(ga.x + 5, ga.y + 9), ImVec2(gb2.x + 5, gb2.y + 9), IM_COL32(0, 0, 0, 130), 13);
                fg->AddRectFilled(ga, gb2, IM_COL32(22, 22, 22, 250), 13);
                if (sg.tex) ImagemCapa(fg, sg.tex, ga, gb2, 13.0f, 194, 1.35f); // mesma capa do card real
                fg->AddRect(ga, gb2, AC.cor, 13, 0, 2.0f);
                fg->PushClipRect(ga, ImVec2(gb2.x - 12, gb2.y), true);
                ImGui::PushFont(gFtBold);
                fg->AddText(ImVec2(ga.x + 15, ga.y + 13), Cinza(240), NomeExib(sg));
                ImGui::PopFont();
                if (!sg.texLogo) {
                    ImGui::PushFont(gFtMini);
                    fg->AddText(ImVec2(ga.x + 15, ga.y + 33), Cinza(150), sg.modoQ[0] ? sg.modoQ : sg.modo);
                    ImGui::PopFont();
                }
                fg->PopClipRect();
                if (sg.texLogo) { // mesmo layout do card real
                    D3DSURFACE_DESC ld3;
                    if (SUCCEEDED(sg.texLogo->GetLevelDesc(0, &ld3)) && ld3.Height) {
                        float lh3 = 40.0f, lw3 = lh3 * (float)ld3.Width / (float)ld3.Height;
                        if (lw3 > 160.0f) { lw3 = 160.0f; lh3 = lw3 * (float)ld3.Height / (float)ld3.Width; }
                        float topoL = ga.y + 36, baseL = gb2.y - 28;
                        float lx3 = (ga.x + gb2.x - lw3) * 0.5f, ly3 = topoL + (baseL - topoL - lh3) * 0.5f;
                        fg->AddImage((ImTextureID)sg.texLogo, ImVec2(lx3, ly3), ImVec2(lx3 + lw3, ly3 + lh3));
                    }
                }
                ImGui::PushFont(gFtMonoS);
                char gpi[48], gpt[24];
                if (sg.ping >= 0) { sprintf(gpi, "%d/%d", sg.online, sg.maxp); sprintf(gpt, "%d ms", sg.ping); }
                else { strcpy(gpi, "--/--"); strcpy(gpt, "-- ms"); }
                fg->AddText(ImVec2(ga.x + 15, gb2.y - 28), Cinza(158), gpi);
                ImVec2 gsz2 = ImGui::CalcTextSize(gpt);
                fg->AddText(ImVec2(gb2.x - 15 - gsz2.x, gb2.y - 28), CorPing(sg.ping), gpt);
                ImGui::PopFont();
            }
        }
    }

    // ===== overlay conectando (foreground: cobre TUDO, ate os childs, que renderizam
    // por cima do drawlist da janela-mae - mesma causa do bug do fade do rail) =====
    if (gConectando) {
        gConnT += dt;
        ImDrawList* ov = ImGui::GetForegroundDrawList();
        ov->AddRectFilled(ImVec2(0, 0), ds, IM_COL32(6, 6, 6, 222));
        char nomeUp[96];
        int ni = 0;
        for (const char* pc = gConnNome; *pc && ni < 94; pc++) nomeUp[ni++] = (*pc >= 'a' && *pc <= 'z') ? *pc - 32 : *pc;
        nomeUp[ni] = 0;
        ImGui::PushFont(gFtMini);
        ImVec2 e1 = ImGui::CalcTextSize("C O N E C T A N D O");
        ov->AddText(ImVec2((ds.x - e1.x) * 0.5f, ds.y * 0.38f), AccentAtual().hi, "C O N E C T A N D O");
        ImGui::PopFont();
        ImGui::PushFont(gFtDisplay);
        ImVec2 e2 = ImGui::CalcTextSize(nomeUp);
        ov->AddText(ImVec2((ds.x - e2.x) * 0.5f, ds.y * 0.42f), Cinza(245), nomeUp);
        ImGui::PopFont();
        float frac = gConnT / 1.6f; if (frac > 1) frac = 1;
        float bx0 = ds.x * 0.5f - 150, bx1 = ds.x * 0.5f + 150, by = ds.y * 0.56f;
        ov->AddRectFilled(ImVec2(bx0, by), ImVec2(bx1, by + 5), Cinza(34), 3);
        ov->AddRectFilledMultiColor(ImVec2(bx0, by), ImVec2(bx0 + 300 * frac, by + 5),
            AccentAtual().cor, AccentAtual().hi, AccentAtual().hi, AccentAtual().cor);
        ImGui::PushFont(gFtMono);
        const char* etapa = gConnT < 0.6f ? "gravando seu nick..." : (gConnT < 1.2f ? "abrindo o samp.exe..." : "bom jogo!");
        ImVec2 e3 = ImGui::CalcTextSize(etapa);
        ov->AddText(ImVec2((ds.x - e3.x) * 0.5f, by + 18), Cinza(150), etapa);
        ImGui::PopFont();
        if (gConnT >= 1.6f) { gConectando = false; Jogar(); }
    }

    // ===== aviso (toast) - foreground: aparece por cima de tudo =====
    if (gAvisoT > 0) {
        gAvisoT -= dt;
        ImDrawList* tv = ImGui::GetForegroundDrawList();
        ImGui::PushFont(gFtBold);
        ImVec2 tsz2 = ImGui::CalcTextSize(gAviso);
        float alfa = gAvisoT > 4.0f ? (4.5f - gAvisoT) * 2.0f : (gAvisoT < 0.5f ? gAvisoT * 2.0f : 1.0f);
        if (alfa > 1) alfa = 1;
        if (alfa < 0) alfa = 0;
        ImVec2 ta2((ds.x - tsz2.x) * 0.5f - 18, 66), tb2((ds.x + tsz2.x) * 0.5f + 18, 66 + tsz2.y + 20);
        tv->AddRectFilled(ta2, tb2, IM_COL32(22, 14, 14, (int)(246 * alfa)), 10);
        tv->AddRect(ta2, tb2, ComAlpha(IM_COL32(240, 110, 106, 255), alfa), 10, 0, 1.4f);
        tv->AddText(ImVec2(ta2.x + 18, ta2.y + 10), ComAlpha(Cinza(240), alfa), gAviso);
        ImGui::PopFont();
    }

    // arrastar a janela pela faixa do topo (fora de widgets)
    if (ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive() && io.MousePos.y < 56 && io.MousePos.x > 76) {
        ReleaseCapture();
        SendMessageA(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }

    // borda externa da janela
    dl->AddRect(ImVec2(0.5f, 0.5f), ImVec2(ds.x - 0.5f, ds.y - 0.5f), Cinza(52), 0, 0, 1);
    ImGui::End();
    ImGui::PopStyleVar(2);
}

// ===================== main =====================

static LPDIRECT3D9 gD3D;
static D3DPRESENT_PARAMETERS gPP;

// icone na bandeja (relogio): clique abre, botao direito da Abrir/Sair
#define WM_TROK_TRAY (WM_USER + 7)
static NOTIFYICONDATAA gNid;
static void CriarTray(HWND h) {
    memset(&gNid, 0, sizeof(gNid));
    gNid.cbSize = sizeof(gNid);
    gNid.hWnd = h;
    gNid.uID = 1;
    gNid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    gNid.uCallbackMessage = WM_TROK_TRAY;
    gNid.hIcon = LoadIconA(GetModuleHandleA(NULL), MAKEINTRESOURCEA(1)); // icone do launcher
    if (!gNid.hIcon) gNid.hIcon = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
    strcpy(gNid.szTip, "Trok Launcher");
    Shell_NotifyIconA(NIM_ADD, &gNid);
}

// Carrega uma fonte da pasta de fontes do Windows. Se o arquivo nao existir, devolve
// NULL em vez de derrubar o app: o AddFontFromFileTTF do ImGui dispara assert nesse caso.
// Usa GetWindowsDirectory, entao funciona tambem com o Windows instalado fora do C:.
static ImFont* FonteDoWindows(ImGuiIO& io, const char* arquivo, float tam, const ImWchar* rango) {
    char cam[MAX_PATH];
    UINT n = GetWindowsDirectoryA(cam, MAX_PATH);
    if (!n || n + 32 >= MAX_PATH) return NULL;
    _snprintf(cam + n, MAX_PATH - n - 1, "%sFonts%s%s", (cam[n-1] == '\\') ? "" : "\\", "\\", arquivo);
    cam[MAX_PATH - 1] = 0;
    if (GetFileAttributesA(cam) == INVALID_FILE_ATTRIBUTES) return NULL;
    return io.Fonts->AddFontFromFileTTF(cam, tam, NULL, rango);
}

static LRESULT WINAPI WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    // qualquer input acorda o render preguicoso
    if ((m >= WM_MOUSEFIRST && m <= WM_MOUSELAST) || (m >= WM_KEYFIRST && m <= WM_KEYLAST) ||
        m == WM_SETFOCUS || m == WM_TROK_TRAY)
        gUltimaAtividade = GetTickCount();
    // o imgui trabalha em coordenadas LOGICAS (1420x800): converte o mouse fisico
    if (m == WM_MOUSEMOVE && gEscala != 1.0f) {
        int mx = (int)((short)LOWORD(l) / gEscala);
        int my = (int)((short)HIWORD(l) / gEscala);
        l = MAKELPARAM(mx, my);
    }
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 1;
    switch (m) {
    case WM_SIZE:
        if (gDev && w != SIZE_MINIMIZED) {
            gPP.BackBufferWidth = LOWORD(l); gPP.BackBufferHeight = HIWORD(l);
            ImGui_ImplDX9_InvalidateDeviceObjects();
            gDev->Reset(&gPP);
            ImGui_ImplDX9_CreateDeviceObjects();
        }
        return 0;
    case WM_TROK_TRAY:
        if (l == WM_LBUTTONUP || l == WM_LBUTTONDBLCLK) {
            ShowWindow(h, SW_SHOW);
            SetForegroundWindow(h);
        } else if (l == WM_RBUTTONUP) {
            HMENU menu = CreatePopupMenu();
            AppendMenuA(menu, MF_STRING, 1, "Abrir o Trok Launcher");
            AppendMenuA(menu, MF_STRING, 2, "Sair");
            POINT pt; GetCursorPos(&pt);
            SetForegroundWindow(h);
            int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, h, NULL);
            DestroyMenu(menu);
            if (cmd == 1) { ShowWindow(h, SW_SHOW); SetForegroundWindow(h); }
            else if (cmd == 2) gRodando = false;
        }
        return 0;
    case WM_DESTROY: gRodando = false; PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); // WIC (imagens das datas)
    InitializeCriticalSection(&gLock);

    GetModuleFileNameA(NULL, gIniPath, MAX_PATH);
    char* barra = strrchr(gIniPath, '\\');
#if defined(TROK_TESTE_SEM_SAMP) // exes de amostra usam ini proprio (nao tocam na config real)
    if (barra) strcpy(barra + 1, "teste-sem-samp.ini"); else strcpy(gIniPath, "teste-sem-samp.ini");
#elif defined(TROK_TESTE_UPDATE)
    if (barra) strcpy(barra + 1, "teste-update.ini"); else strcpy(gIniPath, "teste-update.ini");
#else
    if (barra) strcpy(barra + 1, "Trok Launcher.ini"); else strcpy(gIniPath, "Trok Launcher.ini");
#endif
    LerConfig();
    if (gTela == 6) gTela = 0; // a aba Informacoes saiu da barra
#if defined(TROK_TESTE_SEM_SAMP) || defined(TROK_TESTE_UPDATE)
    gFecharBandeja = false; // exe de amostra fecha DE VERDADE no X (sem pegadinha da bandeja)
    gIniciarMin = false;
#endif
    srand(GetTickCount()); // ANTES do primeiro sorteio de capas (CarregarImagensDatas)
    { // 1a execucao em PC sem SA-MP: mostra as boas-vindas guiando a configuracao
        char chk[MAX_PATH];
        _snprintf(chk, sizeof(chk) - 1, "%s\\samp.exe", gPastaGta);
        chk[sizeof(chk) - 1] = 0;
        if (GetFileAttributesA(chk) == INVALID_FILE_ATTRIBUTES) gBoasVindas = true;
#ifdef TROK_TESTE_SEM_SAMP
        gBoasVindas = true; // amostra: sempre simula PC sem SA-MP
#endif
        gSampOk = !gBoasVindas; // sem samp.exe, os botoes Jogar somem ate apontar um
    }

    // nitido em qualquer escala do Windows (sem isso o DWM estica o app e borra)
    {
        typedef BOOL(WINAPI* FnCtx)(HANDLE);
        typedef BOOL(WINAPI* FnDpi)();
        HMODULE u32 = GetModuleHandleA("user32.dll");
        FnCtx fc = (FnCtx)GetProcAddress(u32, "SetProcessDpiAwarenessContext");
        if (fc) fc((HANDLE)-4 /*PER_MONITOR_AWARE_V2*/);
        else {
            FnDpi fd = (FnDpi)GetProcAddress(u32, "SetProcessDPIAware");
            if (fd) fd();
        }
    }

    HICON hIco = LoadIconA(hInst, MAKEINTRESOURCEA(1)); // icone do .rc (NULL se compilado sem ele)
    WNDCLASSEXA wc = { sizeof(wc), CS_CLASSDC, WndProc, 0, 0, hInst, hIco, LoadCursorA(NULL, (LPCSTR)IDC_ARROW), NULL, NULL, "TrokLauncher", hIco };
    RegisterClassExA(&wc);
    // janela PROPORCIONAL ao monitor: ~74% da largura, proporcao 1420:800, com limites
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    int jw = (int)(sw * 0.74f);
    if (jw < 1180) jw = 1180;
    if (jw > 1600) jw = 1600; // teto = tamanho do launcher do Valorant (1780 ficou grande no 2K)
    int jh = jw * JANELA_H / JANELA_W;
    if (jh > (int)(sh * 0.88f)) { // tela baixa (1366x768 etc): a altura manda
        jh = (int)(sh * 0.88f);
        jw = jh * JANELA_W / JANELA_H;
    }
    // escala AMORTECIDA: o conteudo cresce metade do que a janela cresce
    // (1:1 ficou pequeno no 2K, escala cheia ficou grande demais - o meio-termo agrada)
    {
        float razao = jw / (float)JANELA_W;
        gEscala = 1.0f + (razao - 1.0f) * 0.5f;
        if (gEscala < 0.85f) gEscala = 0.85f;
        if (gEscala > 1.25f) gEscala = 1.25f;
    }
    HWND hwnd = CreateWindowA("TrokLauncher", "Trok Launcher", WS_POPUP,
        (sw - jw) / 2, (sh - jh) / 2, jw, jh, NULL, NULL, hInst, NULL);

    // cantos arredondados no Windows 11 (no Win10 e ignorado sem erro)
    HMODULE dwm = LoadLibraryA("dwmapi.dll");
    if (dwm) {
        typedef HRESULT(WINAPI* FnAttr)(HWND, DWORD, LPCVOID, DWORD);
        FnAttr SetAttr = (FnAttr)GetProcAddress(dwm, "DwmSetWindowAttribute");
        if (SetAttr) { DWORD pref = 2 /*DWMWCP_ROUND*/; SetAttr(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &pref, sizeof(pref)); }
    }

    gD3D = Direct3DCreate9(D3D_SDK_VERSION);
    ZeroMemory(&gPP, sizeof(gPP));
    gPP.Windowed = TRUE; gPP.SwapEffect = D3DSWAPEFFECT_DISCARD;
    gPP.BackBufferFormat = D3DFMT_UNKNOWN; gPP.EnableAutoDepthStencil = TRUE;
    gPP.AutoDepthStencilFormat = D3DFMT_D16;
    gPP.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    // MULTITHREADED: a thread de imagens cria texturas direto (custo minusculo no nosso caso)
    if (FAILED(gD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &gPP, &gDev))) {
        gD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &gPP, &gDev);
    }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(gDev);
    CarregarImagensDatas(gDev);
    CarregarAvatares(gDev); // pasta avatars\ ao lado do exe (criada se nao existir)
    gLogoTrok = CarregarImagemRecurso(gDev, 10, 80); // alvo da sidebar (pre-escalado = nitido)
    gLogoBlogger = CarregarImagemRecurso(gDev, 12, 64); // marca do Blogger (atalho do blog)

    // fontes do sistema, rasterizadas no tamanho FISICO (nitidas) e medidas no logico
    static const ImWchar RANGO_TXT[] = { 0x0020, 0x00FF, 0x2013, 0x2014, 0x2018, 0x201D, 0x2022, 0x2022, 0x2026, 0x2026, 0 }; // latin + travessao/aspas curvas/reticencias (titulos do blog)
    gFtBody    = FonteDoWindows(io, "segoeui.ttf", 16.0f * gEscala, RANGO_TXT);
    gFtBold    = FonteDoWindows(io, "segoeuib.ttf", 16.0f * gEscala, RANGO_TXT);
    gFtMono    = FonteDoWindows(io, "consola.ttf", 14.5f * gEscala, RANGO_TXT);
    gFtDisplay = FonteDoWindows(io, "seguibl.ttf", 52.0f * gEscala, RANGO_TXT);
    gFtBotao   = FonteDoWindows(io, "seguibl.ttf", 23.0f * gEscala, RANGO_TXT);
    // piso tipografico: nada menor que ~13.5 (o menor texto da Rockstar e a regua)
    gFtMini    = FonteDoWindows(io, "segoeuib.ttf", 13.5f * gEscala, RANGO_TXT);
    gFtMonoS   = FonteDoWindows(io, "consola.ttf", 14.0f * gEscala, RANGO_TXT);
    gFtMiniLeve = FonteDoWindows(io, "segoeui.ttf", 13.5f * gEscala, RANGO_TXT);
    gFtCardNome = FonteDoWindows(io, "segoeuib.ttf", 17.5f * gEscala, RANGO_TXT);
    gFtPostTit  = FonteDoWindows(io, "segoeuib.ttf", 22.0f * gEscala, RANGO_TXT);
    gFtBotaoPost = FonteDoWindows(io, "seguibl.ttf", 16.0f * gEscala, RANGO_TXT);
    gFtCardDesc = FonteDoWindows(io, "segoeuib.ttf", 13.5f * gEscala, RANGO_TXT);
    { // fonte de icones LUCIDE embutida no exe (recurso 11) - so os glifos usados (atlas leve)
        HRSRC rL = FindResourceA(NULL, MAKEINTRESOURCEA(11), (LPCSTR)RT_RCDATA);
        if (rL) {
            HGLOBAL hL = LoadResource(NULL, rL);
            void* pL = LockResource(hL);
            DWORD tamL = SizeofResource(NULL, rL);
            static const ImWchar RANGO_ICO[] = {
                0xE060,0xE060, 0xE06C,0xE06F, 0xE09E,0xE09E, 0xE0B2,0xE0B2, 0xE0D7,0xE0D7,
                0xE0DC,0xE0DC, 0xE0E8,0xE0E8, 0xE0F5,0xE0F6, 0xE0F9,0xE0F9, 0xE11C,0xE11C, 0xE129,0xE129,
                0xE145,0xE145, 0xE153,0xE154, 0xE18E,0xE18E, 0xE19E,0xE19E, 0xE1B2,0xE1B2,
                0xE21C,0xE21D, 0xE247,0xE247, 0xE40D,0xE40D, 0xE5C4,0xE5C4, 0 };
            static ImFontConfig cfgL;
            cfgL.FontDataOwnedByAtlas = false; // o ttf mora no recurso do exe, o atlas nao da free
            if (pL && tamL) {
                gFtIco  = io.Fonts->AddFontFromMemoryTTF(pL, (int)tamL, 19.0f * gEscala, &cfgL, RANGO_ICO);
                gFtIcoG = io.Fonts->AddFontFromMemoryTTF(pL, (int)tamL, 26.0f * gEscala, &cfgL, RANGO_ICO);
            }
        }
    }
    if (!gFtBody) { gFtBody = gFtBold = gFtMono = gFtDisplay = gFtBotao = gFtMini = gFtMonoS = io.Fonts->AddFontDefault(); }
    // fontes novas caem numa ja carregada: nunca na default do ImGui (que sai minuscula)
    if (!gFtBotaoPost) gFtBotaoPost = gFtBold;
    if (!gFtMiniLeve)  gFtMiniLeve  = gFtBody;
    if (!gFtPostTit)   gFtPostTit   = gFtBold;
    if (!gFtCardNome)  gFtCardNome  = gFtBold;
    if (!gFtCardDesc)  gFtCardDesc  = gFtBody;
    io.FontGlobalScale = 1.0f / gEscala; // layout logico; o vertice escala de volta no render

    ImGuiStyle& st = ImGui::GetStyle();
    st.FrameRounding = 9; st.FramePadding = ImVec2(12, 9);
    st.ScrollbarSize = 8; st.ScrollbarRounding = 4;
    st.Colors[ImGuiCol_ScrollbarBg]          = ImColor(0, 0, 0, 0);
    st.Colors[ImGuiCol_ScrollbarGrab]        = ImColor(56, 56, 56, 255);
    st.Colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(84, 84, 84, 255);
    st.Colors[ImGuiCol_ScrollbarGrabActive]  = ImColor(110, 110, 110, 255);
    st.Colors[ImGuiCol_FrameBg]        = ImColor(20, 20, 20, 255);
    st.Colors[ImGuiCol_FrameBgHovered] = ImColor(28, 28, 28, 255);
    st.Colors[ImGuiCol_FrameBgActive]  = ImColor(32, 32, 32, 255);
    st.Colors[ImGuiCol_Text]           = ImColor(240, 240, 240, 255);
    st.Colors[ImGuiCol_Button]         = ImColor(28, 28, 28, 255);
    st.Colors[ImGuiCol_ButtonHovered]  = ImColor(46, 46, 46, 255);
    st.Colors[ImGuiCol_ButtonActive]   = ImColor(60, 60, 60, 255);
    st.Colors[ImGuiCol_CheckMark]      = ImColor(240, 240, 240, 255);
    st.Colors[ImGuiCol_DragDropTarget] = ImColor(0, 0, 0, 0); // indicador de drop e desenhado a mao
    st.Colors[ImGuiCol_PopupBg] = ImColor(18, 18, 20, 250);   // tooltips no visual do app
    st.Colors[ImGuiCol_Border]  = ImColor(58, 58, 58, 255);
    st.PopupRounding = 8;
    st.PopupBorderSize = 1;

    gHwnd = hwnd;
    CriarTray(hwnd);
    if (gIniciarMin) {
        ShowWindow(hwnd, SW_HIDE); // nasce quietinho na bandeja
    } else {
        ShowWindow(hwnd, SW_SHOWDEFAULT);
        UpdateWindow(hwnd);
    }

    HANDLE hq = CreateThread(NULL, 0, ThreadQuery, NULL, 0, NULL);
    CreateThread(NULL, 0, ThreadAtualizacao, NULL, 0, NULL); // checa versao nova (silencioso)
    CreateThread(NULL, 0, ThreadMods, NULL, 0, NULL);        // posts do blog (bolinha se tiver novo)
    CreateThread(NULL, 0, ThreadDiscord, NULL, 0, NULL);     // rich presence (se houver app id)
    InitializeCriticalSection(&gLockImg);
    gEvImg = CreateEventA(NULL, FALSE, FALSE, NULL);
    HANDLE hImg[2]; // guardadas p/ esperar no shutdown (senao usam gDev ja liberado)
    hImg[0] = CreateThread(NULL, 0, ThreadImagens, NULL, 0, NULL); // decodificacao fora da UI
    hImg[1] = CreateThread(NULL, 0, ThreadImagens, NULL, 0, NULL); // 2 operarias: carga 2x mais rapida
    gUltimaAtividade = GetTickCount();

    while (gRodando) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            if (msg.message == WM_QUIT) gRodando = false;
        }
        if (!gRodando) break;
        // post novo no blog acende a bolinha SOZINHO: rechecagem periodica do feed, mesmo
        // com o launcher aberto ha horas ou dormindo na bandeja (por isso vem antes do
        // "escondido: não renderiza" - so a UI para, o feed nao)
        {
            static DWORD ultimoFeed = GetTickCount();
            const DWORD INTERVALO = 20u * 60u * 1000u; // 20 min
            if (gModsEstado != 1 && (GetTickCount() - ultimoFeed) > INTERVALO) {
                ultimoFeed = GetTickCount();
                CreateThread(NULL, 0, ThreadMods, NULL, 0, NULL);
            }
        }
        if (!IsWindowVisible(hwnd)) { Sleep(60); continue; } // escondido na bandeja: nao renderiza
        // render preguicoso: parado (sem input ha 1,5s e sem animacao correndo), cai p/ ~12 fps
        // e economiza bateria; o primeiro input acorda instantaneo (timestamp no WndProc)
        {
            bool animando = gConectando || gAvisoT > 0 || gPedirSenha || gFundoFade < 0.999f ||
                            gNumJobsImg > 0 || gNumResImg > 0 || // thumbs chegando: fica esperto
                            (gFotoVista >= 0 && gFGFade < 1.0f) || // cross-fade do visualizador
                            gAttBaixa == 1 || // barra de download da atualizacao
                            (gModsNovo && gTela != 5) || // bolinha pulsando na sidebar
                            gHomeSlide < 1.0f || // slide de entrada da Home
                            fabsf(gSideAnim - (gSideAberta ? 1.0f : 0.0f)) > 0.001f; // menu expandindo
            if (!animando && (GetTickCount() - gUltimaAtividade) > 1500) Sleep(70);
        }
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        io.DisplaySize = ImVec2(jw / gEscala, jh / gEscala); // a UI enxerga o tamanho LOGICO
        ImGui::NewFrame();
        DesenhaUI(hwnd);
        ImGui::EndFrame();
        gDev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(10, 10, 10, 255), 1.0f, 0);
        if (gDev->BeginScene() >= 0) {
            ImGui::Render();
            ImDrawData* dd = ImGui::GetDrawData();
            if (dd && gEscala != 1.0f) { // estica os vertices logicos p/ a janela fisica
                dd->DisplaySize.x *= gEscala;
                dd->DisplaySize.y *= gEscala;
                for (int n = 0; n < dd->CmdListsCount; n++) {
                    ImDrawList* cl = dd->CmdLists[n];
                    for (int v = 0; v < cl->VtxBuffer.Size; v++) {
                        cl->VtxBuffer[v].pos.x *= gEscala;
                        cl->VtxBuffer[v].pos.y *= gEscala;
                    }
                    for (int c = 0; c < cl->CmdBuffer.Size; c++) {
                        cl->CmdBuffer[c].ClipRect.x *= gEscala;
                        cl->CmdBuffer[c].ClipRect.y *= gEscala;
                        cl->CmdBuffer[c].ClipRect.z *= gEscala;
                        cl->CmdBuffer[c].ClipRect.w *= gEscala;
                    }
                }
            }
            ImGui_ImplDX9_RenderDrawData(dd);
            gDev->EndScene();
        }
        if (gDev->Present(NULL, NULL, NULL, NULL) == D3DERR_DEVICELOST &&
            gDev->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
            ImGui_ImplDX9_InvalidateDeviceObjects();
            gDev->Reset(&gPP);
            ImGui_ImplDX9_CreateDeviceObjects();
        }
        ProcessarPedidosDatas(); // dialogos nativos (trocar imagem/caminho, nova data) fora do frame
        for (int li = 0; li < gNumLixo; li++) gLixoTex[li]->Release(); // texturas removidas na UI
        gNumLixo = 0;
    }

    gRodando = false;
    Shell_NotifyIconA(NIM_DELETE, &gNid);
    WaitForSingleObject(hq, 2500);
    if (gEvImg) { SetEvent(gEvImg); SetEvent(gEvImg); } // acorda as operarias p/ verem gRodando=false
    WaitForMultipleObjects(2, hImg, TRUE, 3000); // decodes em voo terminam ANTES do device morrer
    if (!gPulaSalvarSaida) SalvarConfig(); // acabou de importar: o ini novo fica como veio
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    for (int i = 0; i < gNumDatas; i++) if (gDatas[i].tex) gDatas[i].tex->Release();
    for (int i = 0; i < gNumSrv; i++) {
        if (gSrv[i].tex) gSrv[i].tex->Release();
        if (gSrv[i].texLogo) gSrv[i].texLogo->Release();
    }
    if (gDev) gDev->Release();
    if (gD3D) gD3D->Release();
    DestroyWindow(hwnd);
    UnregisterClassA("TrokLauncher", hInst);
    CoUninitialize();
    WSACleanup();
    return 0;
}
