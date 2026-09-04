# TrokMods - categorias oficiais e regra do Mobile

## Regra de ouro dos marcadores
Todo post leva os marcadores (campo "Marcadores" do editor do Blogger) que se aplicarem.
**Se o mod roda no celular, adicionar TAMBEM o marcador `Mobile`.**
Os nomes devem ser EXATOS (sao eles que formam as paginas /search/label/... que a lateral aponta).

## Marcadores de secao
- `SAMP` - tudo de SA-MP
- `Horizonte RP` - conteudo do servidor Horizonte
- `Mobile` - roda no celular (ver criterio abaixo)
- `Nossos` - criacoes do Trok (aparece no menu laranja)
- `Tutoriais` - guias (aparece no menu laranja)

## Categorias (nomes diferenciados da MixMods de proposito)
Marcadores exatos (sem acento, URL de label limpa): Em Destaque, Nossos, Veiculos,
Armas, Skins, Animacoes, Objetos e Cenario, Graficos e Shaders, HUD e Interface,
Fixes e Melhorias, Scripts (exibido "Scripts (Lua/CLEO/ASI)"), Sons, Texturas,
Traducao, Vegetacao, Mods BR, Programas e Utilitarios, Tutoriais.

## Widget Label1 = ESVAZIADO de proposito (03/09/2026)
O widget de marcadores do Blogger (Label1, titulo "GTA San Andreas") tinha uma lista fixa
antiga (SAMP / Horizonte RP / Mods Mobile) e nao aparecia enquanto o blog nao tinha marcadores;
quando o post de lancamento ganhou marcadores, ele ACORDOU como um 3o bloco na lateral.
Fica no tema com o includable vazio (deletar o widget pede confirmacao extra no Blogger).
NAO preencher de novo: a lateral e o HTML fixo dos blocos PC / So Mobile.

## Lateral do blog = 3 blocos por COMPATIBILIDADE (04/09/2026)
Titulos: "PC e Mobile", "So PC", "So Mobile". NAO existe marcador "So PC" - o bloco usa
NEGACAO de marcador, que o Blogger SUPORTA (testado: label:SAMP+-label:Nossos excluiu o post
que tinha "Nossos"). Regra unica de publicacao: se roda no celular, poe a tag Mobile. So isso.

- **PC e Mobile** - link /search/?q=label:Mobile+label:<Cat>  (tem a tag Mobile)
  Veiculos, Armas, Skins, Animacoes, Objetos e Cenario, HUD e Interface, Texturas, Vegetacao
  + Ver tudo. Sao os tipos que rodam nos dois sem conversao (.dff/.txd/.ifp, data, .fxp/.fxt).
- **So PC** - link /search/?q=label:<Cat>+-label:Mobile  (NAO tem a tag Mobile)
  Horizonte RP, Em Destaque, Nossos Mods, Scripts, Programas e Utilitarios, Graficos e Shaders,
  Fixes e Melhorias, Sons, Mods BR, Tutoriais + Ver tudo (label SAMP).
- **So Mobile** - link /search/?q=label:Mobile+label:<Cat>
  Scripts, Traducao, Tutoriais + Ver tudo (label Mobile).

Scripts e Tutoriais aparecem em dois blocos de proposito (versao PC x versao convertida).
Marcador com espaco no meio precisa de aspas na URL: label:%22Programas%20e%20Utilitarios%22.

## Home = cards, nao o post inteiro (desde 03/09/2026)
Na home/listas o tema mostra so capa (data:post.firstImageUrl) + resumo (data:post.snippet)
+ botao ABRIR O POST; o corpo completo aparece so na pagina do post. Nao precisa de quebra
"leia mais" em post nenhum. NENHUM link visivel pra github.com no blog (decisao dele):
os botoes de download apontam pro post de lancamento do launcher.

## Paleta: blog e launcher usam o MESMO hex (desde 04/09/2026)
--laranja:#FC5E3A e --laranja-hi:#FF8055 no blog; o preset "Laranja" do launcher
(ACCENTS[2]) foi trocado para os mesmos valores, e a cor custom padrao tambem.
ATENCAO: os prints do launcher saem com a cor DESLOCADA pelo perfil de cor do monitor dele
(#F2611D virava #FC451B no PNG). Nao derivar cor de print - comparar sempre o HEX DA FONTE.
Fundos do blog copiam o app: --fundo:#0C0C0C, --topo:#060606, --header:#0A0A0A.
Cards e lateral continuam BRANCOS (pedido dele). Logo 96px desktop / 58px celular (.logo-img).

## Layout do topo (desde 03/09/2026)
barra-topo (Baixar + icones SVG Discord/YouTube) > CAPA (print Burger Shot,
base64 no CSS .capa, logo/header DENTRO dela ancorado embaixo com degrade) >
faixa laranja (Nossos Mods | Tutoriais | Trok Launcher) > conteudo.
OBS: tema-trokmods.xml local esta DESATUALIZADO - a fonte de verdade e o tema live
(editar via Blogger > Tema > Editar HTML). capa-blog.jpg nesta pasta = a capa embutida.

## Criterio Mobile (quando por a tag)
**Funciona no celular SEM conversao:**
- Qualquer `.dff`, `.txd`, `.ifp` (carro, objeto, arma, pessoa, animacao) - ha apps que instalam `.txd` de PC
- Qualquer arquivo da pasta `data` e subpastas (`.dat`, `.cfg`, `.ide`, `.ipl`, `.zon`, `.ped`, `.rrr`)
- Efeitos `.fxp` e textos `.fxt` (`.gxt` parcial); `.ini` carregados por mods

**Precisa de conversao (NAO levar a tag, ou avisar no post):**
- `.scm`, `.asi` e a maioria dos CLEO (`.cs`/`.cm`) - alguns CLEO simples funcionam
  ou sao faceis de adaptar (ex.: trocar/remover a tecla de ativacao)

## Sugestao de primeiro post de Tutoriais
Titulo: "Quais mods de PC funcionam no GTA SA do celular?"
Corpo: o criterio acima em texto corrido + marcadores `Tutoriais` e `Mobile`.
