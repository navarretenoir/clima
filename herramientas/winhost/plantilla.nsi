; Instalador de @NOMBRE@ para Windows 10/11 (y 7/8.1 con WebView2) — desarrollado por Javiera Navarrete.
Unicode true
SetCompressor /SOLID lzma
!include "MUI2.nsh"
!define NOMBRE "@NOMBRE@"
!define EXE "@EXE@"
!define VERSION "@VERSION@"
!define AUTORA "Javiera Navarrete"
!define CLAVE_DESINST "Software\Microsoft\Windows\CurrentVersion\Uninstall\@EXE@"
Name "${NOMBRE} ${VERSION}"
OutFile "..\dist\@EXE@-${VERSION}-instalador.exe"
InstallDir "$PROGRAMFILES\${NOMBRE}"
InstallDirRegKey HKLM "Software\${AUTORA}\${EXE}" "Carpeta"
RequestExecutionLevel admin
BrandingText "${NOMBRE} · desarrollado por ${AUTORA}"
VIProductVersion "@VERPUNTOS@"
VIAddVersionKey /LANG=1034 "ProductName" "${NOMBRE}"
VIAddVersionKey /LANG=1034 "CompanyName" "${AUTORA}"
VIAddVersionKey /LANG=1034 "LegalCopyright" "(c) 2026 ${AUTORA}"
VIAddVersionKey /LANG=1034 "FileDescription" "Instalador de ${NOMBRE}"
VIAddVersionKey /LANG=1034 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1034 "ProductVersion" "${VERSION}"
!define MUI_ICON "..\src\app.ico"
!define MUI_UNICON "..\src\app.ico"
!define MUI_WELCOMEPAGE_TITLE "Bienvenida a ${NOMBRE}"
!define MUI_WELCOMEPAGE_TEXT "@BIENVENIDA@$\r$\n$\r$\nDesarrollado por ${AUTORA}."
!define MUI_LICENSEPAGE_TEXT_TOP "Lee el acuerdo de uso antes de instalar."
!define MUI_FINISHPAGE_RUN "$INSTDIR\${EXE}.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Abrir ${NOMBRE} ahora"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "ACUERDO.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Spanish"
Section "${NOMBRE}"
  SectionIn RO
  SetOutPath "$INSTDIR"
  File "..\dist\${EXE}.exe"
  File "..\dist\WebView2Loader.dll"
  File "ACUERDO.txt"
@ARCHIVOS_EXTRA@
  WriteUninstaller "$INSTDIR\Desinstalar.exe"
  SetShellVarContext all
  CreateShortCut "$SMPROGRAMS\${NOMBRE}.lnk" "$INSTDIR\${EXE}.exe" "" "$INSTDIR\${EXE}.exe" 0 "" "" "@DESCRIPCION@"
  CreateShortCut "$DESKTOP\${NOMBRE}.lnk" "$INSTDIR\${EXE}.exe"
  WriteRegStr HKLM "Software\${AUTORA}\${EXE}" "Carpeta" "$INSTDIR"
  WriteRegStr HKLM "${CLAVE_DESINST}" "DisplayName" "${NOMBRE}"
  WriteRegStr HKLM "${CLAVE_DESINST}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${CLAVE_DESINST}" "Publisher" "${AUTORA}"
  WriteRegStr HKLM "${CLAVE_DESINST}" "DisplayIcon" "$INSTDIR\${EXE}.exe"
  WriteRegStr HKLM "${CLAVE_DESINST}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${CLAVE_DESINST}" "UninstallString" '"$INSTDIR\Desinstalar.exe"'
  WriteRegStr HKLM "${CLAVE_DESINST}" "Comments" "Desarrollado por ${AUTORA}"
  WriteRegDWORD HKLM "${CLAVE_DESINST}" "NoModify" 1
  WriteRegDWORD HKLM "${CLAVE_DESINST}" "NoRepair" 1
SectionEnd
Section "Uninstall"
  SetShellVarContext all
  RMDir /r "$INSTDIR"
  Delete "$SMPROGRAMS\${NOMBRE}.lnk"
  Delete "$DESKTOP\${NOMBRE}.lnk"
  DeleteRegKey HKLM "${CLAVE_DESINST}"
  DeleteRegKey HKLM "Software\${AUTORA}\${EXE}"
SectionEnd
