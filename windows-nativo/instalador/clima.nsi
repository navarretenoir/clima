; Instalador de Clima para Windows (95 → 11) — desarrollado por Javiera Navarrete.
; ANSI a propósito: el mismo instalador corre en Windows 95/98/Me y en 2000/XP/Vista/7/8/10/11.
Unicode false
SetCompressor /SOLID lzma
!include "MUI2.nsh"
!include "WinVer.nsh"

!define NOMBRE "Clima"
!define VERSION "1.0.2"
!define AUTORA "Javiera Navarrete"
!define CLAVE_DESINST "Software\Microsoft\Windows\CurrentVersion\Uninstall\Clima"

Name "${NOMBRE} ${VERSION}"
OutFile "..\dist\Clima-${VERSION}-instalador.exe"
InstallDir "$PROGRAMFILES\${NOMBRE}"
InstallDirRegKey HKLM "Software\${AUTORA}\${NOMBRE}" "Carpeta"
RequestExecutionLevel admin
BrandingText "${NOMBRE} · desarrollado por ${AUTORA}"
VIProductVersion "1.0.2.0"
VIAddVersionKey /LANG=1034 "ProductName" "${NOMBRE}"
VIAddVersionKey /LANG=1034 "CompanyName" "${AUTORA}"
VIAddVersionKey /LANG=1034 "LegalCopyright" "(c) 2026 ${AUTORA}"
VIAddVersionKey /LANG=1034 "FileDescription" "Instalador de ${NOMBRE}"
VIAddVersionKey /LANG=1034 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1034 "ProductVersion" "${VERSION}"

!define MUI_ICON "..\src\clima.ico"
!define MUI_UNICON "..\src\clima.ico"
!define MUI_WELCOMEPAGE_TITLE "Bienvenida a ${NOMBRE}"
!define MUI_WELCOMEPAGE_TEXT "Clima es una app del tiempo centrada en Chile. Muestra el tiempo de cualquiera de las 346 comunas de Chile, con un cielo animado que cambia según la condición y la hora del día.$\r$\n$\r$\nCaracterísticas:$\r$\n  • Temperatura actual, sensación, humedad, viento, UV, presión y salida/puesta de sol$\r$\n  • Pronóstico por hora (24 h) y para 10 días$\r$\n  • Buscador de comunas: escribe el nombre y elige$\r$\n  • Funciona sin conexión con el último pronóstico guardado$\r$\n  • Sin publicidad ni rastreo$\r$\n$\r$\nCompatible con Windows 95, 98, Me, 2000, XP, Vista, 7, 8, 10 y 11.$\r$\n$\r$\nDesarrollado por ${AUTORA}."
!define MUI_LICENSEPAGE_TEXT_TOP "Lee el acuerdo de uso antes de instalar."
!define MUI_FINISHPAGE_RUN "$INSTDIR\Clima.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Abrir Clima ahora"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "ansi\ACUERDO-licencia.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Spanish"

Section "Clima" SEC_APP
  SectionIn RO
  SetOutPath "$INSTDIR"
  ; Windows 95/98/Me reciben la compilación ANSI; 2000 → 11 la UNICODE
  ${If} ${IsNT}
    File "/oname=Clima.exe" "..\dist\Clima.exe"
  ${Else}
    File "/oname=Clima.exe" "..\dist\Clima9x.exe"
  ${EndIf}
  File "ansi\ACUERDO.txt"
  File "ansi\LEEME.txt"
  WriteUninstaller "$INSTDIR\Desinstalar.exe"
  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\${NOMBRE}"
  CreateShortCut "$SMPROGRAMS\${NOMBRE}\${NOMBRE}.lnk" "$INSTDIR\Clima.exe" "" "$INSTDIR\Clima.exe" 0 "" "" "El tiempo para las comunas de Chile"
  CreateShortCut "$SMPROGRAMS\${NOMBRE}\Desinstalar ${NOMBRE}.lnk" "$INSTDIR\Desinstalar.exe"
  CreateShortCut "$DESKTOP\${NOMBRE}.lnk" "$INSTDIR\Clima.exe"
  WriteRegStr HKLM "Software\${AUTORA}\${NOMBRE}" "Carpeta" "$INSTDIR"
  WriteRegStr HKLM "${CLAVE_DESINST}" "DisplayName" "${NOMBRE}"
  WriteRegStr HKLM "${CLAVE_DESINST}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${CLAVE_DESINST}" "Publisher" "${AUTORA}"
  WriteRegStr HKLM "${CLAVE_DESINST}" "DisplayIcon" "$INSTDIR\Clima.exe"
  WriteRegStr HKLM "${CLAVE_DESINST}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${CLAVE_DESINST}" "UninstallString" '"$INSTDIR\Desinstalar.exe"'
  WriteRegStr HKLM "${CLAVE_DESINST}" "Comments" "Desarrollado por ${AUTORA}"
  WriteRegDWORD HKLM "${CLAVE_DESINST}" "NoModify" 1
  WriteRegDWORD HKLM "${CLAVE_DESINST}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  SetShellVarContext all
  Delete "$INSTDIR\Clima.exe"
  Delete "$INSTDIR\ACUERDO.txt"
  Delete "$INSTDIR\LEEME.txt"
  Delete "$INSTDIR\Desinstalar.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\${NOMBRE}\${NOMBRE}.lnk"
  Delete "$SMPROGRAMS\${NOMBRE}\Desinstalar ${NOMBRE}.lnk"
  RMDir "$SMPROGRAMS\${NOMBRE}"
  Delete "$DESKTOP\${NOMBRE}.lnk"
  DeleteRegKey HKLM "${CLAVE_DESINST}"
  DeleteRegKey HKLM "Software\${AUTORA}\${NOMBRE}"
  DeleteRegKey HKCU "Software\${AUTORA}\${NOMBRE}"
SectionEnd
