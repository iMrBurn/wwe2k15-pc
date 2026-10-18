Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path $root 'wwe2k15.exe'
$settingsFile = Join-Path $root 'launcher-settings.json'
$logDir = Join-Path $root 'logs'
$userData = Join-Path $env:LOCALAPPDATA 'WWE2K15'

$savedGame = ''
if (Test-Path -LiteralPath $settingsFile) {
    try { $savedGame = (Get-Content -LiteralPath $settingsFile -Raw | ConvertFrom-Json).gameDataRoot } catch {}
}
if (-not $savedGame) {
    foreach ($candidate in @((Join-Path $root 'assets'), (Join-Path $root 'game'), (Join-Path $env:USERPROFILE 'Desktop\WWE2K15'))) {
        if (Test-Path -LiteralPath (Join-Path $candidate 'default.xex')) { $savedGame = $candidate; break }
    }
}

$form = New-Object System.Windows.Forms.Form
$form.Text = 'WWE 2K15 PC - Launcher'
$form.StartPosition = 'CenterScreen'
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.MinimizeBox = $true
$form.ClientSize = New-Object System.Drawing.Size(620, 350)
$form.BackColor = [System.Drawing.Color]::FromArgb(18, 18, 20)
$form.ForeColor = [System.Drawing.Color]::WhiteSmoke
$form.Font = New-Object System.Drawing.Font('Segoe UI', 10)

$title = New-Object System.Windows.Forms.Label
$title.Text = 'WWE 2K15  |  PC RECOMP'
$title.Font = New-Object System.Drawing.Font('Segoe UI', 21, [System.Drawing.FontStyle]::Bold)
$title.ForeColor = [System.Drawing.Color]::FromArgb(230, 35, 50)
$title.Location = New-Object System.Drawing.Point(28, 24)
$title.Size = New-Object System.Drawing.Size(560, 42)
$form.Controls.Add($title)

$subtitle = New-Object System.Windows.Forms.Label
$subtitle.Text = 'Lanceur de la version PC experimentale'
$subtitle.Location = New-Object System.Drawing.Point(31, 69)
$subtitle.Size = New-Object System.Drawing.Size(560, 25)
$subtitle.ForeColor = [System.Drawing.Color]::Silver
$form.Controls.Add($subtitle)

$pathLabel = New-Object System.Windows.Forms.Label
$pathLabel.Text = 'Dossier du jeu (doit contenir default.xex)'
$pathLabel.Location = New-Object System.Drawing.Point(31, 116)
$pathLabel.Size = New-Object System.Drawing.Size(540, 24)
$form.Controls.Add($pathLabel)

$pathBox = New-Object System.Windows.Forms.TextBox
$pathBox.Text = $savedGame
$pathBox.Location = New-Object System.Drawing.Point(31, 145)
$pathBox.Size = New-Object System.Drawing.Size(458, 29)
$pathBox.BackColor = [System.Drawing.Color]::FromArgb(38, 38, 42)
$pathBox.ForeColor = [System.Drawing.Color]::WhiteSmoke
$pathBox.BorderStyle = 'FixedSingle'
$form.Controls.Add($pathBox)

$browse = New-Object System.Windows.Forms.Button
$browse.Text = 'Parcourir...'
$browse.Location = New-Object System.Drawing.Point(499, 143)
$browse.Size = New-Object System.Drawing.Size(90, 32)
$browse.FlatStyle = 'Flat'
$browse.BackColor = [System.Drawing.Color]::FromArgb(54, 54, 58)
$browse.ForeColor = [System.Drawing.Color]::White
$form.Controls.Add($browse)
$browse.Add_Click({
    $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $dialog.Description = 'Selectionne le dossier du jeu contenant default.xex'
    if (Test-Path -LiteralPath $pathBox.Text) { $dialog.SelectedPath = $pathBox.Text }
    if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { $pathBox.Text = $dialog.SelectedPath }
})

$status = New-Object System.Windows.Forms.Label
$status.Location = New-Object System.Drawing.Point(32, 184)
$status.Size = New-Object System.Drawing.Size(555, 28)
$form.Controls.Add($status)
$refreshStatus = {
    if (-not (Test-Path -LiteralPath $exe)) {
        $status.Text = 'Fichiers du port absents : wwe2k15.exe est introuvable.'
        $status.ForeColor = [System.Drawing.Color]::OrangeRed
    } elseif (Test-Path -LiteralPath (Join-Path $pathBox.Text 'default.xex')) {
        $status.Text = 'Pret a jouer - Vulkan et manette SDL seront utilises.'
        $status.ForeColor = [System.Drawing.Color]::LightGreen
    } else {
        $status.Text = 'Choisis le dossier extrait de ton jeu Xbox 360.'
        $status.ForeColor = [System.Drawing.Color]::Khaki
    }
}
$pathBox.Add_TextChanged($refreshStatus)
$form.Add_Shown($refreshStatus)

$launch = New-Object System.Windows.Forms.Button
$launch.Text = 'LANCER WWE 2K15'
$launch.Location = New-Object System.Drawing.Point(31, 232)
$launch.Size = New-Object System.Drawing.Size(270, 54)
$launch.FlatStyle = 'Flat'
$launch.FlatAppearance.BorderSize = 0
$launch.BackColor = [System.Drawing.Color]::FromArgb(190, 24, 42)
$launch.ForeColor = [System.Drawing.Color]::White
$launch.Font = New-Object System.Drawing.Font('Segoe UI', 12, [System.Drawing.FontStyle]::Bold)
$form.Controls.Add($launch)
$launch.Add_Click({
    $gameRoot = $pathBox.Text.Trim()
    if (-not (Test-Path -LiteralPath $exe)) {
        [System.Windows.Forms.MessageBox]::Show('wwe2k15.exe est introuvable. Rebuild le projet ou place le lanceur a cote du jeu.', 'WWE 2K15 PC', 'OK', 'Error') | Out-Null
        return
    }
    if (-not (Test-Path -LiteralPath (Join-Path $gameRoot 'default.xex'))) {
        [System.Windows.Forms.MessageBox]::Show('Le dossier choisi ne contient pas default.xex.', 'Dossier du jeu invalide', 'OK', 'Warning') | Out-Null
        return
    }
    try {
        @{ gameDataRoot = (Resolve-Path -LiteralPath $gameRoot).Path } | ConvertTo-Json | Set-Content -LiteralPath $settingsFile -Encoding UTF8
        New-Item -ItemType Directory -Force -Path $userData | Out-Null
        $quotedGame = '"' + (Resolve-Path -LiteralPath $gameRoot).Path.Replace('"', '\"') + '"'
        $quotedUser = '"' + $userData.Replace('"', '\"') + '"'
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $exe
        $psi.WorkingDirectory = $root
        $psi.Arguments = "--game_data_root=$quotedGame --user_data_root=$quotedUser --gpu_plugin=xenos --gpu_backend=vulkan --input_backend=sdl"
        [System.Diagnostics.Process]::Start($psi) | Out-Null
        $form.Close()
    } catch {
        [System.Windows.Forms.MessageBox]::Show($_.Exception.Message, 'Impossible de lancer le jeu', 'OK', 'Error') | Out-Null
    }
})

$logsButton = New-Object System.Windows.Forms.Button
$logsButton.Text = 'Ouvrir les journaux'
$logsButton.Location = New-Object System.Drawing.Point(319, 243)
$logsButton.Size = New-Object System.Drawing.Size(160, 38)
$logsButton.FlatStyle = 'Flat'
$logsButton.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 49)
$logsButton.ForeColor = [System.Drawing.Color]::WhiteSmoke
$form.Controls.Add($logsButton)
$logsButton.Add_Click({
    if (-not (Test-Path -LiteralPath $logDir)) { New-Item -ItemType Directory -Force -Path $logDir | Out-Null }
    Start-Process explorer.exe -ArgumentList ('"' + $logDir + '"')
})

$form.ShowDialog() | Out-Null
