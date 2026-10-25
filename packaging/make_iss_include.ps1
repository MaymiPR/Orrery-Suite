# Writes packaging/plugins.iss.inc (Inno Setup [Components], [Files], [Icons], [UninstallDelete]) for every plug-in in
# packaging/plugins.txt.   Usage: pwsh packaging/make_iss_include.ps1 <build dir>
# Artefacts are read from <build>/<Target>_artefacts/Release.
param([string]$BuildDir = "build")
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = (Resolve-Path $BuildDir).Path
$plugins = Get-Content "$here/plugins.txt" | Where-Object { $_.Trim() -and -not $_.StartsWith('#') } | ForEach-Object {
    $parts = $_.Split([char]0x7C); @{ Target = $parts[0].Trim(); Product = $parts[1].Trim() } }
$c = @('[Components]',
       'Name: "vst3"; Description: "VST3 plug-ins (C:\Program Files\Common Files\VST3\Newton Suite)"; Types: full all custom',
       'Name: "clap"; Description: "CLAP plug-ins (C:\Program Files\Common Files\CLAP\Newton Suite)"; Types: full all custom',
       'Name: "app"; Description: "Standalone apps (C:\Program Files\Newton Suite)"; Types: all')
$f = @('[Files]')
$i = @('[Icons]')
$u = @('[UninstallDelete]')
foreach ($pl in $plugins) {
    $id = $pl.Target.ToLower()
    $a = Join-Path $build "$($pl.Target)_artefacts\Release"
    $c += "Name: ""vst3\$id""; Description: ""$($pl.Product)""; Types: full all custom"
    $c += "Name: ""clap\$id""; Description: ""$($pl.Product)""; Types: full all custom"
    $c += "Name: ""app\$id""; Description: ""$($pl.Product)""; Types: all"
    $f += "Source: ""$a\VST3\$($pl.Product).vst3\*""; DestDir: ""{#VstDir}\$($pl.Product).vst3""; Components: vst3\$id; Flags: ignoreversion recursesubdirs createallsubdirs"
    $f += "Source: ""$a\CLAP\$($pl.Product).clap""; DestDir: ""{#ClapDir}""; Components: clap\$id; Flags: ignoreversion"
    $f += "Source: ""$a\Standalone\$($pl.Product).exe""; DestDir: ""{app}""; Components: app\$id; Flags: ignoreversion"
    $i += "Name: ""{group}\$($pl.Product)""; Filename: ""{app}\$($pl.Product).exe""; Components: app\$id"
    $u += "Type: filesandordirs; Name: ""{#VstDir}\$($pl.Product).vst3"""
    $u += "Type: files; Name: ""{#ClapDir}\$($pl.Product).clap"""
}
($c + '' + $f + '' + $i + '' + $u) | Set-Content -Encoding utf8 "$here/plugins.iss.inc"
Get-Content "$here/plugins.iss.inc"
