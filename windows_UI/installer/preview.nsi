Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"

Name "SpektraLab Windows Preview ${BUILD_LABEL}"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\SpektraLabPreview"
RequestExecutionLevel user
SetCompressor /SOLID lzma
SetCompressorDictSize 32
ShowInstDetails show
ShowUninstDetails show
!define REGKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpektraLabWindowsPreview"
!define MUI_ABORTWARNING
!define MUI_ICON "..\assets\film.ico"
!define MUI_UNICON "..\assets\film.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\SpektraLabQt.exe"
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\READ-ME-FIRST.txt"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${PAYLOAD}\licenses\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  SetShellVarContext current
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "This preview requires 64-bit Windows." /SD IDOK
    Abort
  ${EndIf}
  ; Test mode changes BOTH registry/shortcut behavior, never the user's entry.
  ${GetParameters} $R0
  ${GetOptions} $R0 "/TESTONLY" $R1
  IfErrors +2
    StrCpy $R9 "test"
  StrCmp $R9 "test" registration_ok
  ReadRegStr $R0 HKCU "${REGKEY}" "InstallLocation"
  StrCmp $R0 "" registration_ok
    MessageBox MB_ICONSTOP "A preview is already registered. Uninstall it before installing this build." /SD IDOK
    SetErrorLevel 2
    Abort
  registration_ok:
FunctionEnd

Section "SpektraLab Preview" SEC_APP
  ; Do not overlay another installation or an arbitrary directory. A fresh
  ; empty destination is required; uninstall first when updating this preview.
  IfFileExists "$INSTDIR\*.*" 0 destination_ok
    MessageBox MB_ICONSTOP "Destination is not empty. Uninstall the previous preview or select a new empty folder." /SD IDOK
    SetErrorLevel 2
    Abort
  destination_ok:
  SetOutPath "$INSTDIR"
  File /r "${PAYLOAD}\*.*"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  FileOpen $R0 "$INSTDIR\spektralab-install.marker" w
  FileWrite $R0 "SpektraLabWindowsPreview-${BUILD_LABEL}"
  FileClose $R0
  StrCmp $R9 "test" skip_registration
  CreateDirectory "$SMPROGRAMS\SpektraLab Preview"
  CreateShortcut "$SMPROGRAMS\SpektraLab Preview\SpektraLab.lnk" "$INSTDIR\SpektraLabQt.exe" "" "$INSTDIR\SpektraLabQt.exe" 0
  CreateShortcut "$SMPROGRAMS\SpektraLab Preview\Read me.lnk" "$INSTDIR\READ-ME-FIRST.txt"
  CreateShortcut "$SMPROGRAMS\SpektraLab Preview\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "${REGKEY}" "DisplayName" "SpektraLab Windows Preview"
  WriteRegStr HKCU "${REGKEY}" "DisplayVersion" "${BUILD_LABEL}"
  WriteRegStr HKCU "${REGKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${REGKEY}" "DisplayIcon" "$INSTDIR\SpektraLabQt.exe"
  WriteRegStr HKCU "${REGKEY}" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegStr HKCU "${REGKEY}" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
  WriteRegDWORD HKCU "${REGKEY}" "NoModify" 1
  WriteRegDWORD HKCU "${REGKEY}" "NoRepair" 1
  skip_registration:
SectionEnd

Function un.onInit
  SetShellVarContext current
  IfFileExists "$INSTDIR\spektralab-install.marker" +3
    MessageBox MB_ICONSTOP "Installation marker is missing; refusing to remove files." /SD IDOK
    Abort
FunctionEnd

Section "Uninstall"
  ; Generated exact file list, then non-recursive empty-directory removal.
  ; Never recursively remove $INSTDIR or the user's photo/edit directories.
  !include "${UNINSTALL_LIST}"
  Delete "$INSTDIR\spektralab-install.marker"
  Delete "$INSTDIR\Uninstall.exe"
  ReadRegStr $R0 HKCU "${REGKEY}" "InstallLocation"
  StrCmp $R0 $INSTDIR 0 +6
    DeleteRegKey HKCU "${REGKEY}"
    Delete "$SMPROGRAMS\SpektraLab Preview\SpektraLab.lnk"
    Delete "$SMPROGRAMS\SpektraLab Preview\Read me.lnk"
    Delete "$SMPROGRAMS\SpektraLab Preview\Uninstall.lnk"
    RMDir "$SMPROGRAMS\SpektraLab Preview"
  RMDir "$INSTDIR"
SectionEnd
