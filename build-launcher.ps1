# Compila o Trok Launcher (EXE win32 + ImGui/DX9, CRT estatico, 32 bits).
# Uso:  & .\build-launcher.ps1        (na pasta _dev\native)
param([switch]$Simbolos, [string]$Nome = 'Trok Launcher.exe', [string]$Define = '')
$ErrorActionPreference = 'Stop'
$vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat'
if (-not (Test-Path $vcvars)) { throw "vcvars32.bat nao encontrado" }
$dir = $PSScriptRoot
$obj = Join-Path $dir 'build\launcher'
New-Item -ItemType Directory -Force $obj | Out-Null
# /GS LIGADO (protecao de pilha): seguranca real e menos falso positivo de antivirus
$flags = if ($Simbolos) { '/Od /Zi' } else { '/O2' }
if ($Define) { $flags += " /D$Define" } # ex.: TROK_TESTE_SEM_SAMP (exes de amostra)
$fontes = @(
  "$dir\TrokLauncher.cpp",
  "$dir\imgui\imgui.cpp", "$dir\imgui\imgui_draw.cpp",
  "$dir\imgui\imgui_tables.cpp", "$dir\imgui\imgui_widgets.cpp",
  "$dir\imgui\backends\imgui_impl_dx9.cpp", "$dir\imgui\backends\imgui_impl_win32.cpp"
)
$lista = ($fontes | ForEach-Object { '"' + $_ + '"' }) -join ' '
$saida = Join-Path $dir $Nome
# recursos (icone + versao): compila o .rc se ele e o .ico existirem
$res = ''
if ((Test-Path "$dir\TrokLauncher.rc") -and (Test-Path "$dir\TrokLauncher.ico")) {
    $res = "`"$obj\TrokLauncher.res`""
}
$cmd = @"
call "$vcvars" >nul 2>&1
if errorlevel 1 exit /b 1
$(if ($res) { "rc /nologo /fo $res `"$dir\TrokLauncher.rc`"`r`nif errorlevel 1 exit /b 1" })
cl /nologo /utf-8 /MT $flags /guard:cf /EHsc /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /I "$dir" /I "$dir\imgui" /Fo"$obj\\" /Fe"$saida" $lista $res /link /MACHINE:X86 /SUBSYSTEM:WINDOWS /GUARD:CF /DYNAMICBASE /NXCOMPAT /SAFESEH /OPT:REF /OPT:ICF /MANIFEST:EMBED /MANIFESTINPUT:"$dir\TrokLauncher.manifest" /MANIFESTUAC:"level='asInvoker' uiAccess='false'"
"@
$bat = Join-Path $env:TEMP ("build-launcher-" + [guid]::NewGuid().ToString('N') + '.bat')
Set-Content -Path $bat -Value $cmd -Encoding ASCII
try { & cmd.exe /c $bat 2>&1 | ForEach-Object { Write-Output $_ }; $codigo = $LASTEXITCODE }
finally { Remove-Item $bat -Force -ErrorAction SilentlyContinue }
if ($codigo -ne 0 -or -not (Test-Path $saida)) { throw "Compilacao FALHOU (codigo $codigo)" }
$bytes = [IO.File]::ReadAllBytes($saida)
$peOff = [BitConverter]::ToInt32($bytes, 0x3C)
if ([BitConverter]::ToUInt16($bytes, $peOff + 4) -ne 0x14C) { throw "saida nao e x86!" }
Write-Output ("OK: {0} ({1:N0} bytes, PE32 x86)" -f $saida, (Get-Item $saida).Length)
