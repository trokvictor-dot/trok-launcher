# Empacota o Trok Launcher para distribuicao:
#   release\TrokLauncher-Setup.exe
# Uso:  & .\empacotar.ps1
param([switch]$Testes)   # -Testes tambem gera os exes de amostra
$ErrorActionPreference = 'Stop'
$dir = $PSScriptRoot
$versao = '1.0'
$rel = Join-Path $dir 'release'
New-Item -ItemType Directory -Force $rel | Out-Null

if (-not (Test-Path "$dir\Trok Launcher.exe")) { throw 'Compile antes: & .\build-launcher.ps1' }

# --- setup: instalador PROPRIO (TrokInstaller, mesma cara do launcher) ---
& (Join-Path $dir 'build-instalador.ps1')

# hash do setup: cole como linha "sha256=<hash>" no versao.txt e o launcher confere o que baixou
$setup = Join-Path $rel 'TrokLauncher-Setup.exe'
if (Test-Path $setup) { Write-Output ('sha256 do setup (para o versao.txt): ' + (Get-FileHash $setup -Algorithm SHA256).Hash.ToLower()) }

# --- exes de AMOSTRA (cenarios de teste) - so com -Testes, pra pasta ficar limpa ---
if ($Testes) {
    & (Join-Path $dir 'build-launcher.ps1') -Nome 'Trok Launcher TESTE-UPDATE.exe' -Define 'TROK_TESTE_UPDATE'
    & (Join-Path $dir 'build-launcher.ps1') -Nome 'Trok Launcher TESTE-SEM-SAMP.exe' -Define 'TROK_TESTE_SEM_SAMP'
}
