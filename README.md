# Trok Launcher

Launcher moderno de **SA-MP**, feito em C++ nativo com Win32, Direct3D 9 e Dear ImGui. Ele não substitui nenhum arquivo do seu jogo: abre o `samp.exe` original da instalação que você escolher.

## Download

➡️ **[Baixar no blog TrokMods](https://trokmods.blogspot.com/2026/09/trok-launcher.html)**

Instalador único, sem dependências. Windows 10/11, com GTA San Andreas e SA-MP instalados (R1/R3/R5).

## O que ele faz

- **Contas** — vários nicks, cada um com avatar; troca em 2 cliques e o jogo abre com o nick certo
- **Datas** — várias instalações do GTA/SA-MP, cada uma com seu próprio User Files
- **Servidores** — favoritos com capa, logo e links oficiais; lista da internet com filtros; senha automática em servidor trancado
- **Galeria** — suas screenshots (F8) com visualizador, cópia para a área de transferência e exclusão segura
- **TrokMods** — os posts do blog aparecem dentro do launcher, com aviso quando sai novidade
- **Extras** — cor de destaque personalizável, bandeja do Windows, iniciar com o Windows, atualização automática e Discord Rich Presence

Na primeira abertura, o launcher importa sozinho seu nick e seus favoritos do SA-MP original.

## Compilar

Precisa do Visual Studio Build Tools (MSVC x86). Da pasta do projeto:

```powershell
.\build-launcher.ps1     # gera "Trok Launcher.exe"
.\empacotar.ps1          # gera release\TrokLauncher-Setup.exe
```

`empacotar.ps1 -Testes` também gera os executáveis de amostra usados para testar a tela de atualização e o cenário sem SA-MP.

## Sobre os arquivos que não estão aqui

As artes `artes/A.png`…`artes/U.png` ficam fora do repositório: são os loadscreens do GTA San Andreas em tamanho cheio (15 MB), que servem só de fonte para gerar as capas. As capas prontas (`capas/`) e os avatares (`avatars/`) estão aqui porque o instalador os empacota.

Todo o material do GTA San Andreas é da **Rockstar Games**. Este projeto não tem ligação com a Rockstar nem com o time do SA-MP.

`Trok Launcher.ini`, `imagens/` e `datas/` também ficam fora: são a configuração e o conteúdo de quem usa.

---

**Equipe TrokMods** · [blog](https://trokmods.blogspot.com) · [Discord](https://discord.gg/2uHzexhg6J)
