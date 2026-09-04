# Compila o INSTALADOR proprio do Trok Launcher (TrokInstaller.cpp, mesma cara do app).
# Empacota dentro dele: Trok Launcher.exe + avatars\* (formato TROKPAK1 como recurso).
# Arte do painel esquerdo: artes\instalador-grande.png (se existir; senao a mira placeholder).
# Uso:  & .\build-instalador.ps1
$ErrorActionPreference = 'Stop'
$vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat'
if (-not (Test-Path $vcvars)) { throw "vcvars32.bat nao encontrado" }
$dir = $PSScriptRoot
$obj = Join-Path $dir 'build\instalador'
New-Item -ItemType Directory -Force $obj | Out-Null
New-Item -ItemType Directory -Force (Join-Path $dir 'release') | Out-Null

if (-not (Test-Path "$dir\Trok Launcher.exe")) { throw 'Compile o launcher antes: & .\build-launcher.ps1' }

# --- 1. payload TROKPAK1: magic + int32 n + tabela(u16 len, nome, u32 tam) + blobs ---
$itens = New-Object System.Collections.ArrayList
[void]$itens.Add(@{ rel = 'Trok Launcher.exe'; full = (Join-Path $dir 'Trok Launcher.exe') })
if (Test-Path "$dir\avatars") {
    Get-ChildItem "$dir\avatars" -File | Sort-Object Name | ForEach-Object {
        [void]$itens.Add(@{ rel = ('avatars\' + $_.Name); full = $_.FullName })
    }
}
if (Test-Path "$dir\capas") { # capas padrao dos favoritos
    Get-ChildItem "$dir\capas" -File | Sort-Object Name | ForEach-Object {
        [void]$itens.Add(@{ rel = ('capas\' + $_.Name); full = $_.FullName })
    }
}
$pak = Join-Path $obj 'payload.trokpak'
$fs = New-Object IO.FileStream($pak, [IO.FileMode]::Create)
$bw = New-Object IO.BinaryWriter($fs)
$bw.Write([Text.Encoding]::ASCII.GetBytes('TROKPAK1'))
$bw.Write([int]$itens.Count)
foreach ($i in $itens) {
    $nb = [Text.Encoding]::Default.GetBytes([string]$i.rel)
    $bw.Write([uint16]$nb.Length)
    $bw.Write($nb)
    $bw.Write([uint32](Get-Item $i.full).Length)
}
foreach ($i in $itens) { $bw.Write([IO.File]::ReadAllBytes($i.full)) }
$bw.Close()
Write-Output ("payload: {0} arquivos, {1:N0} KB" -f $itens.Count, ((Get-Item $pak).Length / 1KB))

# --- 2. recursos: icone + versao + payload (+ arte, se existir) ---
$arte = Join-Path $dir 'artes\instalador-grande.png'
$temArte = Test-Path $arte
$rc = @"
#include <winver.h>
1 ICON "$($dir -replace '\\','\\\\')\\TrokSetup.ico"
2 RCDATA "$($pak -replace '\\','\\\\')"
$(if ($temArte) { "3 RCDATA `"$($arte -replace '\\','\\\\')`"" })
1 VERSIONINFO
FILEVERSION     1,0,0,0
PRODUCTVERSION  1,0,0,0
FILEOS          VOS_NT_WINDOWS32
FILETYPE        VFT_APP
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "041604B0"
    BEGIN
      VALUE "CompanyName",      "TrokMods"
      VALUE "FileDescription",  "Instalador do Trok Launcher"
      VALUE "FileVersion",      "1.0.0.0"
      VALUE "ProductName",      "Trok Launcher"
      VALUE "ProductVersion",   "1.0"
      VALUE "LegalCopyright",   "TrokMods"
      VALUE "OriginalFilename", "TrokLauncher-Setup.exe"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x0416, 1200
  END
END
"@
$rcArq = Join-Path $obj 'TrokInstaller.rc'
Set-Content -Path $rcArq -Value $rc -Encoding ASCII
Write-Output ("arte: " + $(if ($temArte) { 'artes\instalador-grande.png' } else { 'placeholder (mira desenhada)' }))

# --- 3. compilar ---
$fontes = @(
  "$dir\TrokInstaller.cpp",
  "$dir\imgui\imgui.cpp", "$dir\imgui\imgui_draw.cpp",
  "$dir\imgui\imgui_tables.cpp", "$dir\imgui\imgui_widgets.cpp",
  "$dir\imgui\backends\imgui_impl_dx9.cpp", "$dir\imgui\backends\imgui_impl_win32.cpp"
)
$lista = ($fontes | ForEach-Object { '"' + $_ + '"' }) -join ' '
$saida = Join-Path $dir 'release\TrokLauncher-Setup.exe'
$cmd = @"
call "$vcvars" >nul 2>&1
if errorlevel 1 exit /b 1
rc /nologo /fo "$obj\TrokInstaller.res" "$rcArq"
if errorlevel 1 exit /b 1
cl /nologo /utf-8 /MT /O2 /EHsc /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /I "$dir" /I "$dir\imgui" /Fo"$obj\\" /Fe"$saida" $lista "$obj\TrokInstaller.res" /link /MACHINE:X86 /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTUAC:"level='asInvoker' uiAccess='false'"
"@
$bat = Join-Path $env:TEMP ("build-instalador-" + [guid]::NewGuid().ToString('N') + '.bat')
Set-Content -Path $bat -Value $cmd -Encoding ASCII
try { & cmd.exe /c $bat 2>&1 | ForEach-Object { Write-Output $_ }; $codigo = $LASTEXITCODE }
finally { Remove-Item $bat -Force -ErrorAction SilentlyContinue }
if ($codigo -ne 0 -or -not (Test-Path $saida)) { throw "Compilacao do instalador FALHOU (codigo $codigo)" }
Write-Output ("OK: {0} ({1:N0} KB)" -f $saida, ((Get-Item $saida).Length / 1KB))
