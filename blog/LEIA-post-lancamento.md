# Post de lancamento v1.0 - estado

## Arquivos
- `post-lancamento.html` - o post inteiro, pronto pra colar na visualizacao HTML do editor do Blogger.
  2.443 palavras, 15 secoes h2. Sem <script>/<style>/<div>: so h2/h3/p/ul/ol/li/strong/em.
- `prints/` - PNG originais (1600x901), como saiu da tela.
- `prints/web/` - JPEG otimizados 1280px q80 (o que vai pro blog), ~755 KB no total.
  Os arquivos `ordem-01-*.jpg` ... `ordem-11-*.jpg` estao na ORDEM em que aparecem no post.

## Onde cada print entra
No `post-lancamento.html` os lugares estao marcados assim:
`<p style="text-align:center"><strong>[[ IMAGEM: ordem-03-home.jpg ]]</strong></p>`
Basta trocar cada marcador pela imagem correspondente.

| # | arquivo | assunto |
|---|---------|---------|
| 01 | ordem-01-setup.jpg | instalador aberto |
| 02 | ordem-02-sem-gta.jpg | popup BEM-VINDO (sem SA-MP na maquina) |
| 03 | ordem-03-home.jpg | Home com favoritos |
| 04 | ordem-04-servidores.jpg | tela Servidores |
| 05 | ordem-05-galeria.jpg | Galeria |
| 06 | ordem-06-datas.jpg | Datas |
| 07 | ordem-07-mods.jpg | aba Mods |
| 08 | ordem-08-config.jpg | Configuracoes |
| 09 | ordem-09-update-aviso.jpg | botao de atualizacao no topo |
| 10 | ordem-10-update.jpg | modal NOVA ATUALIZACAO |
| 11 | ordem-11-info.jpg | aba Informacoes |

## Estado: RASCUNHO PRONTO (03/09/2026)
Post criado no Blogger como rascunho, com as 11 imagens ja encaixadas.
Link: https://www.blogger.com/blog/post/edit/4949372959382121447/5739736843345010796
Titulo: Trok Launcher v1.0 - o seu SA-MP, moderno
Marcadores: Nossos, Em Destaque, Trok Launcher, Programas e Utilitarios, SAMP
Falta so clicar em PUBLICAR (deixei sem publicar de proposito).

LICOES, se for repetir:
- O uploader de imagem do Blogger SO funciona na Visualizacao Escrever. Na Visualizacao
  em HTML ele abre e morre sem erro nenhum. Foi o que travou o upload.
- O painel de navegador do Claude nao abre janela de arquivo do Windows e tem clipboard
  isolado: upload de imagem precisa ser feito no navegador de verdade.
- Copiar um .html aberto no navegador copia o texto RENDERIZADO, sem as tags. Pra colar
  no editor, abrir no Bloco de Notas (ou usar post-lancamento-COLAR.txt).

## Como os fatos do post foram checados
Os rotulos e frases sao os reais do app - conferidos em `frases-do-app.txt` e no fonte.
Um primeiro rascunho gerado por agentes foi DESCARTADO: a revisao adversarial achou de 12 a 29
erros por secao (ex: mandava rodar `Desinstalar.exe` com 2 cliques, o que REINSTALA em vez de
desinstalar - o modo remocao so liga com o argumento `--remover`). O texto atual foi escrito
a partir do codigo, nao do rascunho. Os rascunhos e as listas de problemas ficaram no scratchpad
da sessao (`scratchpad/secoes/*.PROBLEMAS.txt`), se algum dia quiser reaproveitar.

## Titulo/marcadores sugeridos pro post
- Titulo: **Trok Launcher v1.0 - o seu SA-MP, moderno**
- Marcadores: `Nossos`, `Trok Launcher`, `Programas e Utilitarios`, `SAMP`
  (nao levar `Mobile`: e programa de Windows)
