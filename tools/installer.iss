; ============================================================
; LW Vision Windows 安装包脚本 (Inno Setup 6)
; ------------------------------------------------------------
; 用法:
;   1. 先 build.bat release + tools\deploy.bat 生成 release_pkg\LWVision\
;   2. 安装 Inno Setup 6 (https://jrsoftware.org/isinfo.php, 免费)
;   3. 用 ISCC 编译本脚本 (或右键 Compile): 产物 LWVision_Setup_版本.exe
; 特性: 桌面/开始菜单快捷方式、可选开机自启、卸载保留用户数据
;       (config/ templates/ data/ images/ 检测记录库不会被卸载删除)
; ============================================================

#define MyAppName "LW Vision"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "LW Vision"
#define MyAppExeName "LWVision.exe"

[Setup]
AppId={{8E5F7A2C-4B3D-4E89-9C1A-LWVISION2026}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\LWVision
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
; 工控机常无最新运行库, 关闭下载; 便携包已自包含全部 DLL
OutputDir=..\release_pkg
OutputBaseFilename=LWVision_Setup_{#MyAppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; 中文支持: 需 Inno Setup 自带 ChineseSimplified.isl (6.x 已内置)
ShowLanguageDialog=auto
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "startupicon"; Description: "开机自动启动"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; 整个便携包目录 (deploy.bat 产物) 原样装入
Source: "..\release_pkg\LWVision\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\卸载 {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userstartup}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: startupicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; 卸载只删程序本体; 用户数据目录保留 (检测记录/模板/授权/图片)
; config\ templates\ data\ images\ 位于 {app} 下, 用 UninstallFilesDir 之外的排除不可行,
; 因此在 [Code] 里由用户选择; 默认保留以下目录内容:
Type: files; Name: "{app}\*.dll"
Type: files; Name: "{app}\{#MyAppExeName}"

[Code]
// 卸载时询问是否同时删除检测记录/模板等用户数据 (默认保留)
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    if MsgBox('是否同时删除检测记录、模板与个人配置? (选择"否"则保留数据目录)',
              mbConfirmation, MB_YESNO) = IDYES then
    begin
      DelTree(ExpandConstant('{app}\data'), True, True, True);
      DelTree(ExpandConstant('{app}\config'), True, True, True);
      DelTree(ExpandConstant('{app}\templates'), True, True, True);
      DelTree(ExpandConstant('{app}\images'), True, True, True);
    end;
  end;
end;
