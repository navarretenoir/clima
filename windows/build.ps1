# build.ps1 — port a Windows x64 de Clima (Electron) desde Windows; equivalente a build.sh.
# Refresca app/ui + app/data desde el proyecto (la MISMA interfaz de Linux) y empaqueta con @electron/packager.
$ErrorActionPreference = 'Stop'
$R = Split-Path -Parent $MyInvocation.MyCommand.Path; $P = Split-Path -Parent $R
$env:Path = "C:\Program Files\nodejs;$env:Path"
New-Item -ItemType Directory -Force "$R\app\ui", "$R\app\data" | Out-Null
Copy-Item "$P\ui\*.html", "$P\ui\*.css", "$P\ui\*.js" "$R\app\ui\" -Force
Copy-Item "$P\data\comunas_cl.json", "$P\data\cities_cl.json" "$R\app\data\" -Force
Push-Location $R
try {
    npx -y @electron/packager . Clima --platform=win32 --arch=x64 --electron-version=44.5.1 --out=dist --overwrite --icon=app/clima.ico --ignore="^/dist" --ignore="^/build\.(sh|ps1)$"
    if ($LASTEXITCODE) { throw "falló @electron/packager" }
} finally { Pop-Location }
"Listo: $R\dist\Clima-win32-x64\Clima.exe"
