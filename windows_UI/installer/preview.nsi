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

; Check every existing ancestor, not just the leaf. Exact filenames alone do
; not protect a recipient directory reached through a junction or symlink.
!macro ValidatePathFunction NAME
Function ${NAME}
  Exch $0
  Push $1
  Push $2
  path_loop:
    System::Call 'kernel32::GetFileAttributesW(w "$0") i .r1'
    IntCmp $1 -1 path_parent
    IntOp $1 $1 & 0x400
    IntCmp $1 0 path_parent
      MessageBox MB_ICONSTOP "A managed path is a junction or symbolic link; refusing to change files." /SD IDOK
      SetErrorLevel 2
      Abort
  path_parent:
    ${GetParent} "$0" $2
    StrCmp $2 "" path_done
    StrCmp $2 $0 path_done
    StrCpy $0 $2
    Goto path_loop
  path_done:
  Pop $2
  Pop $1
  Pop $0
FunctionEnd
!macroend
!insertmacro ValidatePathFunction ValidatePath
!insertmacro ValidatePathFunction un.ValidatePath

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
  StrCmp $R9 "test" init_done
  IfFileExists "$SMPROGRAMS\SpektraLab Preview\*.*" 0 init_done
    MessageBox MB_ICONSTOP "An existing Start menu folder belongs to another installation. Refusing to overwrite its shortcuts." /SD IDOK
    SetErrorLevel 2
    Abort
  init_done:
FunctionEnd

Section "SpektraLab Preview" SEC_APP
  ; Do not overlay another installation or an arbitrary directory. A fresh
  ; empty destination is required; uninstall first when updating this preview.
  IfFileExists "$INSTDIR\*.*" 0 destination_ok
    MessageBox MB_ICONSTOP "Destination is not empty. Uninstall the previous preview or select a new empty folder." /SD IDOK
    SetErrorLevel 2
    Abort
  destination_ok:
  Push $INSTDIR
  Call ValidatePath
  ClearErrors
  SetOutPath "$INSTDIR"
  IfErrors install_failed
  GetFullPathName $R3 "$INSTDIR"
  StrCmp $R3 "" install_failed
  StrCpy $INSTDIR $R3
  File /r "${PAYLOAD}\*.*"
  IfErrors install_failed
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  IfErrors install_failed
  FileOpen $R0 "$INSTDIR\spektralab-install.marker" w
  IfErrors install_failed
  FileWriteUTF16LE $R0 "SpektraLabWindowsPreview-${BUILD_LABEL}$\r$\n$INSTDIR"
  FileClose $R0
  IfErrors install_failed
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
  IfErrors install_failed
  skip_registration:
  Goto install_done
  install_failed:
    ; Never recursively roll back a destination into which a recipient may
    ; have added files. Leave an explicit failure, no launch and no ownership
    ; claim. The next installation must use a fresh empty directory.
    ReadRegStr $R0 HKCU "${REGKEY}" "InstallLocation"
    StrCmp $R0 $INSTDIR 0 +2
      DeleteRegKey HKCU "${REGKEY}"
    Delete "$INSTDIR\spektralab-install.marker"
    MessageBox MB_ICONSTOP "Installation failed. No application will be launched. Partial files may remain in the selected directory; preserve personal files and use a fresh empty directory to retry." /SD IDOK
    SetErrorLevel 3
    Abort
  install_done:
SectionEnd

Function un.onInit
  SetShellVarContext current
  GetFullPathName $R3 "$INSTDIR"
  StrCmp $R3 "" marker_invalid
  StrCpy $INSTDIR $R3
  Push $INSTDIR
  Call un.ValidatePath
  ClearErrors
  FileOpen $R0 "$INSTDIR\spektralab-install.marker" r
  IfErrors marker_invalid
  FileReadUTF16LE $R0 $R1
  FileReadUTF16LE $R0 $R2
  FileClose $R0
  StrCmp $R1 "SpektraLabWindowsPreview-${BUILD_LABEL}$\r$\n" 0 marker_invalid
  StrCmp $R2 $INSTDIR marker_valid
  marker_invalid:
    MessageBox MB_ICONSTOP "Installation marker is missing, invalid or belongs to another location; refusing to remove files." /SD IDOK
    SetErrorLevel 2
    Abort
  marker_valid:
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
