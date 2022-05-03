OutFile setup_xp.exe
Unicode true
RequestExecutionLevel admin

!define APPNAME "Magic.TXD"
!define APPNAME_FRIENDLY "Magic TXD"

!define MUI_ICON "..\inst.ico"

!include "MUI2.nsh"
!include "Sections.nsh"
!include "x64.nsh"

!macro UPDATE_INSTDIR
StrCpy $INSTDIR "$PROGRAMFILES\${APPNAME_FRIENDLY}"
!macroend

!include "mgtxd_install_shared.nsi"

InstallDir "$PROGRAMFILES\${APPNAME_FRIENDLY}"
Name "${APPNAME}"

var StartMenuFolder

!define COMPONENT_REG_PATH  "Software\Magic.TXD"

!define MUI_STARTMENUPAGE_REGISTRY_ROOT HKLM
!define MUI_STARTMENUPAGE_REGISTRY_KEY ${COMPONENT_REG_PATH}
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "SMFolder"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!define MUI_PAGE_CUSTOMFUNCTION_PRE pre_startmenu
!insertmacro MUI_PAGE_STARTMENU SM_PAGE_ID $StartMenuFolder
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

!include LogicLib.nsh

!macro SHARED_INSTALL
!insertmacro SHARED_INSTALL_DATA
setOutPath $INSTDIR\formats_legacy
!insertmacro INCLUDE_FORMATS "..\..\output\formats_legacy"
!macroend

VAR DoStartMenu

!define INST_REG_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Magic.TXD"
!define SM_PATH "$SMPROGRAMS\$StartMenuFolder"

Section "-Magic.TXD core"
    # Install the main program.
    setOutPath $INSTDIR
    File /oname=magictxd.exe "..\..\output\magictxd_xp.exe"
    !insertmacro SHARED_INSTALL
    WriteUninstaller $INSTDIR\uinst.exe
    
    !insertmacro REGISTER_INSTALL

    # Write meta information.
    WriteRegStr HKLM ${COMPONENT_REG_PATH} "InstallDir" "$INSTDIR"
SectionEnd

!insertmacro SHARED_SHORTCUT_SECT
!insertmacro SHARED_ASSOCIATE_SECT

LangString DESC_SMShortcut ${LANG_ENGLISH} "Creates shortcuts to Magic.TXD in the startmenu."
LangString DESC_TXDAssoc ${LANG_ENGLISH} "Associates .txd files in Windows Explorer with Magic.TXD."
LangString DESC_ShellInt ${LANG_ENGLISH} "Provides thumbnails and context menu extensions for TXD files."
LangString DESC_PVRSupport ${LANG_ENGLISH} "Copies the PowerVR library for certain mobile device TXDs."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${STARTMEN_SEC_ID} $(DESC_SMShortcut)
    !insertmacro MUI_DESCRIPTION_TEXT ${ASSOC_TXD_ID} $(DESC_TXDAssoc)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

!insertmacro SHARED_INIT

Section un.defUninst
    !insertmacro MAIN_UNINSTALL_LOGIC
SectionEnd

Function pre_startmenu
    SectionGetFlags ${STARTMEN_SEC_ID} $R0
    IntOp $R0 $R0 & ${SF_SELECTED}

    ${If} $R0 != ${SF_SELECTED}
        abort
    ${EndIf}
FunctionEnd