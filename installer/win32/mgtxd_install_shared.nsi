

!macro CHECK_ADMIN
UserInfo::GetAccountType
pop $0
${If} $0 != "admin"
    MessageBox mb_iconstop "Please run this setup using administrator rights."
    SetErrorLevel 740
    Quit
${EndIf}
!macroend

!macro INCLUDE_FORMATS dir
File "${dir}\a8.magf"
File "${dir}\v8u8.magf"
!macroend

!macro SHARED_INSTALL_DATA
setOutPath $INSTDIR
File /r "..\..\releasefiles\*"
setOutPath $INSTDIR\resources
File /r "..\..\resources\*"
setOutPath $INSTDIR\data
File /r "..\..\data\*"
setOutPath $INSTDIR\languages
File /r "..\..\languages\*"
!macroend

!macro REG_INIT
# Access the proper registry view.
${If} ${RunningX64}
    SetRegView 64
${Else}
    SetRegView 32
${EndIf}
!macroend

# Information for add/remove programs.
!macro REGISTER_INSTALL
WriteRegStr HKLM ${INST_REG_KEY} "DisplayName" "Magic.TXD"
WriteRegStr HKLM ${INST_REG_KEY} "DisplayIcon" "$INSTDIR\magictxd.exe"
WriteRegStr HKLM ${INST_REG_KEY} "DisplayVersion" "1.0"
WriteRegDWORD HKLM ${INST_REG_KEY} "NoModify" 1
WriteRegDWORD HKLM ${INST_REG_KEY} "NoRepair" 1
WriteRegStr HKLM ${INST_REG_KEY} "Publisher" "GTA community"
WriteRegStr HKLM ${INST_REG_KEY} "UninstallString" "$\"$INSTDIR\uinst.exe$\""
!macroend

!macro SHARED_SHORTCUT_SECT
Section /o "Startmenu Shortcuts" STARTMEN_SEC_ID
    !insertmacro MUI_STARTMENU_WRITE_BEGIN SM_PAGE_ID
        createDirectory "${SM_PATH}"
        createShortcut "${SM_PATH}\Magic.TXD.lnk" "$INSTDIR\magictxd.exe"
        createShortcut "${SM_PATH}\Remove Magic.TXD.lnk" "$INSTDIR\uinst.exe"
    !insertmacro MUI_STARTMENU_WRITE_END
SectionEnd
!macroend

!macro SHARED_ASSOCIATE_SECT
!define TXD_ASSOC_KEY ".txd"
!define MGTXD_TXD_ASSOC "${APPNAME}.txd"

Section "Associate .txd files" ASSOC_TXD_ID
    # KILL previous UserChoice config.
    DeleteRegKey HKCU SOFTWARE\Microsoft\Windows\Roaming\OpenWith\FileExts\.TXD
    DeleteRegKey HKCU Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.TXD

    # Write some registry settings for .TXD files.
    WriteRegStr HKCR "${TXD_ASSOC_KEY}" "" "${MGTXD_TXD_ASSOC}"
    WriteRegStr HKCR "${TXD_ASSOC_KEY}" "Content Type" "image/dict"
    WriteRegStr HKCR "${TXD_ASSOC_KEY}" "PerceivedType" "image"
    WriteRegStr HKCR "${TXD_ASSOC_KEY}\OpenWithProgids" "${MGTXD_TXD_ASSOC}" ""
    
    # Now write Magic.TXD association information.
    WriteRegStr HKCR "${MGTXD_TXD_ASSOC}\DefaultIcon" "" "$INSTDIR\magictxd.exe"
    WriteRegStr HKCR "${MGTXD_TXD_ASSOC}\shell\open\command" "" "$\"$INSTDIR\magictxd.exe$\" $\"%1$\""
SectionEnd
!macroend

!macro SHARED_INIT
Function .onInit
    !insertmacro CHECK_ADMIN
    !insertmacro REG_INIT
    !insertmacro UPDATE_INSTDIR
    
    # Check through the registry whether Magic.TXD is installed already.
    # We do not want to install twice.
    StrCpy $0 ""
    ReadRegStr $0 HKLM ${COMPONENT_REG_PATH} "InstallDir"
    
    ${If} $0 != ""
        # We want to check for a corrupted Magic.TXD installation because this actually happened.
        # Anti-Viri can go rogue too, so lets make sure.
        IfFileExists "$0\uinst.exe" continueOkay 0
        
        MessageBox MB_ICONEXCLAMATION|MB_YESNO "Setup has detected a corrupted Magic.TXD installation (uninstaller is missing). Would you like to force installation?" /SD IDYES IDNO continueOkay
        goto forceInstall

continueOkay:
        MessageBox MB_ICONINFORMATION|MB_YESNO "Setup has detected through the registry that Magic.TXD has already been installed at $\"$0$\".$\nSetup cannot continue unless the old version is uninstalled.$\n$\nWould you like to uninstall now?" /SD IDYES IDNO quitInstall
        
        # The user agreed to uninstall, so we run the uninstaller.
        # If the uninstaller suceeded, we continue setup.
        # Otherwise we show an error and quit.
        StrCpy $1 "$0\uinst.exe"
        
        ExecWait "$1 _?=$0" $1
        
        ${If} $1 != 0
            MessageBox MB_ICONEXCLAMATION|MB_YESNO "Failed to uninstall Magic.TXD. This can have many reasons, one of which a corrupted Magic.TXD installation.$\nWould you like to force installation?" /SD IDYES IDNO quitInstall
            
            # The user wants to continue the installation process.
            # Let's pray that everything goes okay.
        ${EndIf}
        
forceInstall:
        # Clean up after the uninstaller.
        # It could not complete a few steps.
        Delete "$0\uinst.exe"
        RMDIR "$0"
        
        # We were successful, so continue installation :)
        goto proceedInstall
quitInstall:
        Quit
proceedInstall:
    ${EndIf}
    
    StrCpy $DoStartMenu "false"
FunctionEnd

Function un.onInit
    !insertmacro CHECK_ADMIN
    !insertmacro REG_INIT

    # Try to fetch installation directory.
    StrCpy $0 ""
    ReadRegStr $0 HKLM ${COMPONENT_REG_PATH} "InstallDir"
    
    ${If} $0 != ""
        StrCpy $INSTDIR $0
    ${EndIf}
FunctionEnd
!macroend

!macro UNREG_FILEASSOC
; Unregister file association if present.
DeleteRegKey HKCR "${MGTXD_TXD_ASSOC}"
DeleteRegValue HKCR "${TXD_ASSOC_KEY}\OpenWithProgids" "${MGTXD_TXD_ASSOC}"
!macroend

!macro MAIN_UNINSTALL_LOGIC
RMDIR /r $INSTDIR\resources
RMDIR /r $INSTDIR\data
RMDIR /r $INSTDIR\licenses
RMDIR /r $INSTDIR\languages
RMDIR /r /REBOOTOK $INSTDIR\formats
RMDIR /r /REBOOTOK $INSTDIR\formats_x64
RMDIR /r /REBOOTOK $INSTDIR\formats_legacy
Delete /REBOOTOK $INSTDIR\PVRTexLib.dll
Delete $INSTDIR\magictxd.exe
Delete $INSTDIR\uinst.exe
RMDIR $INSTDIR

!insertmacro MUI_STARTMENU_GETFOLDER SM_PAGE_ID $StartMenuFolder

${If} $StartMenuFolder != ""
    ; Delete shortcut stuff.
    Delete "${SM_PATH}\Magic.TXD.lnk"
    Delete "${SM_PATH}\Remove Magic.TXD.lnk"
    RMDIR "${SM_PATH}"
${EndIf}

# Remove the registry key that is responsible for shell stuff.
DeleteRegKey /ifempty HKCR "${TXD_ASSOC_KEY}"
    
DeleteRegKey HKLM ${INST_REG_KEY}
DeleteRegKey HKLM ${COMPONENT_REG_PATH}
!macroend