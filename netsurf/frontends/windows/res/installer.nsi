# This installs JesusSaves executables and resources, creates a start menu shortcut, builds an uninstaller, and
# adds uninstall information to the registry for Add/Remove Programs
 
# show up in a few places.
# All the other settings can be tweaked by editing the !defines at the top of this script

!define APPNAME "JesusSaves" ; JESUSSAVES-v1.1.1: own install dir / Start menu / uninstall key, never shared with a NetSurf install
!define COMPANYNAME "JesusSaves"
!define DESCRIPTION "Web Browser"

# The version values must be simple integers
!ifndef VERSIONMAJOR
  !define VERSIONMAJOR 1
!endif
!ifndef VERSIONMINOR
  !define VERSIONMINOR 1
!endif
!ifndef VERSIONBUILD
  !define VERSIONBUILD 1
!endif

# This is the size (in kB) of all the files copied into "Program Files"
!define INSTALLSIZE 9000

# output filename
!ifndef OUTFNAME
  !define OUTFNAME "jesussaves-installer.exe"
!endif

# path to resources
!ifndef RESDIR
  !define RESDIR "frontends/windows/res"
!endif


RequestExecutionLevel admin ;Require admin rights on NT6+ (When UAC is turned on)
 
# JESUSSAVES-v1.1.1: the 64-bit build installs into the native Program Files and writes the
# native registry view (a 32-bit installer would otherwise use "Program Files (x86)" and WOW6432Node)
!ifdef WIN64
  InstallDir "$PROGRAMFILES64\${APPNAME}"
!else
  InstallDir "$PROGRAMFILES\${APPNAME}"
!endif
 
# rtf or txt file - remember if it is txt, it must be in the DOS text format (\r\n)
LicenseData "COPYING"
# This will be in the installer/uninstaller's title bar
Name "${APPNAME} - ${DESCRIPTION}"
Icon "${RESDIR}\NetSurf.ico"
outFile "${OUTFNAME}"
BrandingText "${COMPANYNAME}"
 
!include LogicLib.nsh
 
# Just three pages - license agreement, install location, and installation
page license
page directory
Page instfiles
 
!macro VerifyUserIsAdmin
UserInfo::GetAccountType
pop $0
${If} $0 != "admin" ;Require admin rights on NT4+
        messageBox mb_iconstop "Administrator rights required!"
        setErrorLevel 740 ;ERROR_ELEVATION_REQUIRED
        quit
${EndIf}
!macroend
 
function .onInit
	!ifdef WIN64
		SetRegView 64
	!endif
	setShellVarContext all
	!insertmacro VerifyUserIsAdmin
functionEnd
 
section "install"
	# Files for the install directory

	# Default output path
	setOutPath $INSTDIR

	# Files added here should be removed by the uninstaller section
	file "JesusSaves.exe"
	file /oname=NetSurf.ico "${RESDIR}\NetSurf.ico"
	file /oname=default.css "${RESDIR}\default.css"
	file /oname=internal.css "${RESDIR}\internal.css"
	file /oname=adblock.css "${RESDIR}\adblock.css"
	file /oname=welcome.html "${RESDIR}\welcome.html"
	file /oname=credits.html "${RESDIR}\credits.html"
	file /oname=changelog.html "${RESDIR}\changelog.html"
	file /oname=licence.html "${RESDIR}\licence.html"
	file /oname=netsurf.png "${RESDIR}\netsurf.png"
	file /oname=messages "${OBJROOT}\messages-en"
	file /oname=ca-bundle.crt "${RESDIR}\ca-bundle.crt"
 
	# Uninstaller - See function un.onInit and section "uninstall" for configuration
	writeUninstaller "$INSTDIR\uninstall.exe"
 
	# Start Menu
		createShortCut "$SMPROGRAMS\${APPNAME}.lnk" "$INSTDIR\JesusSaves.exe" "" "$INSTDIR\NetSurf.ico"
 
	# Registry information for add/remove programs
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME} - ${DESCRIPTION}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "QuietUninstallString" "$\"$INSTDIR\uninstall.exe$\" /S"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "InstallLocation" "$\"$INSTDIR$\""
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayIcon" "$\"$INSTDIR\NetSurf.ico$\""
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "Publisher" "$\"${COMPANYNAME}$\""
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayVersion" "$\"${VERSIONMAJOR}.${VERSIONMINOR}.${VERSIONBUILD}$\""
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "VersionMajor" ${VERSIONMAJOR}
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "VersionMinor" ${VERSIONMINOR}
	# There is no option for modifying or repairing the install
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoModify" 1
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoRepair" 1
	# Set the INSTALLSIZE constant (!defined at the top of this script) so Add/Remove Programs can accurately report the size
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "EstimatedSize" ${INSTALLSIZE}
sectionEnd

# Uninstaller
 
function un.onInit
	!ifdef WIN64
		SetRegView 64
	!endif
	SetShellVarContext all
 
	#Verify the uninstaller - last chance to back out
	MessageBox MB_OKCANCEL "Permanently remove ${APPNAME}?" IDOK next
		Abort
	next:
	!insertmacro VerifyUserIsAdmin
functionEnd
 
section "uninstall"
 
	# Remove Start Menu launcher
	delete "$SMPROGRAMS\${APPNAME}.lnk"
 
	# Remove files
	delete $INSTDIR\JesusSaves.exe
	delete $INSTDIR\NetSurf.ico
	delete $INSTDIR\default.css
	delete $INSTDIR\internal.css
	delete $INSTDIR\adblock.css
	delete $INSTDIR\welcome.html
	delete $INSTDIR\credits.html
	delete $INSTDIR\changelog.html
	delete $INSTDIR\licence.html
	delete $INSTDIR\netsurf.png
	delete $INSTDIR\messages
	delete $INSTDIR\ca-bundle.crt

	# Always delete uninstaller as the last action
	delete $INSTDIR\uninstall.exe
 
	# Try to remove the install directory - this will only happen if it is empty
	rmDir $INSTDIR
 
	# Remove uninstaller information from the registry
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
sectionEnd
