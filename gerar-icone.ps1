# Identidade do Trok Launcher:
#   TrokLauncher.ico  = simbolo ALVO na cor de destaque (laranja) sobre fundo escuro arredondado
#   TrokSetup.ico     = arte da logo (loira) com pontas arredondadas  -> icone do instalador
#   release\discord-logo-512.png = loira arredondada (icone do app no Discord)
# ICOs no formato PNG-em-ICO (Vista+). Uso:  & .\gerar-icone.ps1
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$dir = $PSScriptRoot
New-Item -ItemType Directory -Force (Join-Path $dir 'release') | Out-Null

# cor de destaque padrao do app (Laranja da paleta)
$corR = 242; $corG = 97; $corB = 29
$fundoEscuro = [System.Drawing.Color]::FromArgb(255, 13, 13, 15)

function CaminhoArredondado([int]$tam) {
    $r = [float]($tam * 0.22)
    $cam = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $cam.AddArc(0, 0, $d, $d, 180, 90)
    $cam.AddArc($tam - $d, 0, $d, $d, 270, 90)
    $cam.AddArc($tam - $d, $tam - $d, $d, $d, 0, 90)
    $cam.AddArc(0, $tam - $d, $d, $d, 90, 90)
    $cam.CloseFigure()
    return $cam
}

# --- face do LAUNCHER: alvo tingido de laranja sobre fundo escuro arredondado ---
function FaceAlvo([int]$tam) {
    $bmp = New-Object System.Drawing.Bitmap($tam, $tam)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'; $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'
    $cam = CaminhoArredondado $tam
    $pincel = New-Object System.Drawing.SolidBrush($fundoEscuro)
    $g.FillPath($pincel, $cam)
    $pincel.Dispose(); $cam.Dispose()
    # alvo branco -> laranja via ColorMatrix (escala os canais, preserva o alpha)
    $mx = New-Object System.Drawing.Imaging.ColorMatrix
    $mx.Matrix00 = $script:corR / 255.0
    $mx.Matrix11 = $script:corG / 255.0
    $mx.Matrix22 = $script:corB / 255.0
    $mx.Matrix33 = 1.0; $mx.Matrix44 = 1.0
    $atr = New-Object System.Drawing.Imaging.ImageAttributes
    $atr.SetColorMatrix($mx)
    $marg = [int]($tam * 0.18) # respiro do simbolo dentro do quadrado
    $lado = $tam - 2 * $marg
    $destR = New-Object System.Drawing.Rectangle($marg, $marg, $lado, $lado)
    $g.DrawImage($script:alvo, $destR, 0, 0, $script:alvo.Width, $script:alvo.Height,
                 [System.Drawing.GraphicsUnit]::Pixel, $atr)
    $atr.Dispose(); $g.Dispose()
    return $bmp
}

# --- face da LOGO (loira) com pontas arredondadas ---
function FaceLogo([int]$tam) {
    $bmp = New-Object System.Drawing.Bitmap($tam, $tam)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'; $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'
    $cam = CaminhoArredondado $tam
    $g.SetClip($cam)
    $g.DrawImage($script:logo, 0, 0, $tam, $tam)
    $g.Dispose(); $cam.Dispose()
    return $bmp
}

function GravarIco([string]$saida, [scriptblock]$face) {
    $tamanhos = 256, 128, 64, 48, 32, 16
    $pngs = @()
    foreach ($t in $tamanhos) {
        $bmp = & $face $t
        $ms = New-Object IO.MemoryStream
        $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        $pngs += ,@($t, $ms.ToArray())
        $ms.Dispose()
    }
    $fs = New-Object IO.FileStream($saida, [IO.FileMode]::Create)
    $bw = New-Object IO.BinaryWriter($fs)
    $bw.Write([uint16]0); $bw.Write([uint16]1); $bw.Write([uint16]$pngs.Count)
    $off = 6 + 16 * $pngs.Count
    foreach ($p in $pngs) {
        $t = $p[0]; $dados = $p[1]
        $bw.Write([byte]($(if ($t -ge 256) { 0 } else { $t })))
        $bw.Write([byte]($(if ($t -ge 256) { 0 } else { $t })))
        $bw.Write([byte]0); $bw.Write([byte]0)
        $bw.Write([uint16]1); $bw.Write([uint16]32)
        $bw.Write([uint32]$dados.Length)
        $bw.Write([uint32]$off)
        $off += $dados.Length
    }
    foreach ($p in $pngs) { $bw.Write($p[1]) }
    $bw.Close()
    Write-Output ("icone: {0} ({1:N0} bytes)" -f $saida, (Get-Item $saida).Length)
}

$alvoArq = Join-Path $dir 'artes\alvo-512.png' # versao otimizada; original fica de fonte
if (-not (Test-Path $alvoArq)) { $alvoArq = Join-Path $dir 'artes\alvo.png' }
if (-not (Test-Path $alvoArq)) { throw 'falta artes\alvo.png' }
$alvo = [System.Drawing.Image]::FromFile($alvoArq)
$logoArq = Join-Path $dir 'artes\logo-trokmods-hd-rosa.png'
if (-not (Test-Path $logoArq)) { $logoArq = Join-Path $dir 'artes\logo-trokmods-hd.png' }
if (-not (Test-Path $logoArq)) { $logoArq = Join-Path $dir 'artes\logo-trokmods.png' }
$logo = [System.Drawing.Image]::FromFile($logoArq)

GravarIco (Join-Path $dir 'TrokLauncher.ico') ${function:FaceLogo}   # launcher = loira (o alvo vive DENTRO do app)
GravarIco (Join-Path $dir 'TrokSetup.ico')    ${function:FaceLogo}   # instalador = loira
$d512 = FaceLogo 512
$d512.Save((Join-Path $dir 'release\discord-logo-512.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$d512.Dispose()
Write-Output 'discord: release\discord-logo-512.png (loira)'
$alvo.Dispose(); $logo.Dispose()
