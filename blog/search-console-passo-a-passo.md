# Google Search Console: conectar o TrokMods (passo a passo)

O Search Console mostra como o Google enxerga o blog: quais buscas trazem gente, quais posts estão indexados e o que está com erro. Como o Blogger é do Google, a verificação de propriedade é automática quando você entra com a mesma conta do blog.

## 1. Abrir pelo Blogger (verificação automática)

1. Entre no Blogger com a conta dona do blog.
2. Menu da esquerda: **Configurações**.
3. Role até a seção **Rastreadores e indexação**.
4. Clique em **Google Search Console**. Abre o Search Console já com a propriedade `trokmods.blogspot.com` verificada.
   - Se abrir a tela "Bem-vindo ao Search Console" pedindo para adicionar propriedade: escolha **Prefixo do URL**, cole `https://trokmods.blogspot.com/` e clique em Continuar. A verificação passa sozinha (método "Blogger").

## 2. Enviar o sitemap (uma vez só)

1. No Search Console, menu da esquerda: **Sitemaps**.
2. Em "Adicionar um novo sitemap", digite `sitemap.xml` e clique em **Enviar**.
3. Repita com `sitemap-pages.xml` (páginas fixas, se você criar alguma).
4. O status fica "Êxito" em alguns minutos. Posts novos entram nele sozinhos; não precisa reenviar.

## 3. Pedir indexação do post do launcher (e de cada post novo importante)

1. Menu **Inspeção de URL** (ou a barra de busca no topo).
2. Cole `https://trokmods.blogspot.com/2026/09/trok-launcher.html` e dê Enter.
3. Clique em **Solicitar indexação**. Leva de horas a alguns dias.
4. Faça isso com cada post que você quer que apareça rápido (limite de ~10 pedidos por dia).

## 4. O que olhar depois (a partir de 2 a 7 dias)

- **Desempenho**: buscas que trouxeram cliques, posição média por post. Serve para ver quais títulos funcionam.
- **Páginas** (Indexação): quantos posts estão no Google e os motivos dos que ficaram de fora. "Descoberta, mas não indexada" no começo é normal.
- **Experiência > Core Web Vitals**: só aparece quando há tráfego suficiente.

## Já está pronto no tema

- `<meta name='description'>` por página (descrição de pesquisa de cada post).
- JSON-LD `Article` nos posts e `SoftwareApplication` no post do launcher.
- Tag `lang` na página, favicon, tema responsivo.
- `robots.txt` e sitemap são gerados pelo próprio Blogger (`/robots.txt` e `/sitemap.xml`).

## Dica de rotina

Ao publicar um post: preencha a **Descrição da pesquisa** (até 155 caracteres) e o **alt** das imagens, publique, e peça a indexação pela Inspeção de URL. É o suficiente.
