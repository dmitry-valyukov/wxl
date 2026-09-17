# Водит sample.titlebar-probe настоящей мышью: запускает с ключами, двигает и жмёт
# в клиентских координатах окна, снимает область экрана вокруг окна (так в кадр
# попадают системные подсказки и раскладки Windows 11) и нажимает кнопки тела
# окна через UI Automation. Мышь -- системная, поэтому на время прогона экран
# занят.
#
#   pwsh -File samples\TitleBarProbe\drive.ps1 -ProbeArgs "--buttons own --zoom 1.5" `
#        -Do "wait 4000; move 980 36; wait 2000; shot hover-max.png 60"
#
# Команды: wait <мс>; move <x> <y>; click|dblclick|rclick <x> <y>; down; up;
# drag <x> <y> <dx> <dy>; shot <файл> [поле]; invoke <имя>; tree; altspace; esc;
# ctrlkey <виртуальный код, hex> -- клавиша с Ctrl, например ctrlkey BB (плюс);
# restore; rect; alive.
param(
    [string]$Exe = 'build\x64\samples\TitleBarProbe\Debug\sample.titlebar-probe.exe',
    [string]$ProbeArgs = '',
    [string]$Do = 'wait 3000; shot start.png',
    [string]$ShotDir = '.',
    [switch]$Keep
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Drawing, System.Windows.Forms
Add-Type -Namespace Probe -Name Native -MemberDefinition @'
    public struct POINT { public int X; public int Y; }
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, uint d, IntPtr e);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
'@
[Probe.Native]::SetProcessDpiAwarenessContext([IntPtr]-4) | Out-Null
New-Item -ItemType Directory -Force $ShotDir | Out-Null

$exePath = (Resolve-Path $Exe).Path
$proc = if ($ProbeArgs) { Start-Process -FilePath $exePath -ArgumentList $ProbeArgs -PassThru } else { Start-Process -FilePath $exePath -PassThru }
$deadline = (Get-Date).AddSeconds(30)
while ((Get-Date) -lt $deadline) {
    $proc.Refresh()
    if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { break }
    Start-Sleep -Milliseconds 200
}
$hwnd = $proc.MainWindowHandle
if ($hwnd -eq [IntPtr]::Zero) { throw 'no window' }
[Probe.Native]::SetForegroundWindow($hwnd) | Out-Null

function ScreenPoint([int]$x, [int]$y) {
    $p = New-Object Probe.Native+POINT
    $p.X = $x; $p.Y = $y
    [Probe.Native]::ClientToScreen($hwnd, [ref]$p) | Out-Null
    return $p
}
function MoveTo([int]$x, [int]$y) { $p = ScreenPoint $x $y; [Probe.Native]::SetCursorPos($p.X, $p.Y) | Out-Null; [Probe.Native]::mouse_event(1, 0, 0, 0, [IntPtr]::Zero) }
function Down([int]$button) { if ($button -eq 2) { [Probe.Native]::mouse_event(8, 0, 0, 0, [IntPtr]::Zero) } else { [Probe.Native]::mouse_event(2, 0, 0, 0, [IntPtr]::Zero) } }
function Up([int]$button) { if ($button -eq 2) { [Probe.Native]::mouse_event(16, 0, 0, 0, [IntPtr]::Zero) } else { [Probe.Native]::mouse_event(4, 0, 0, 0, [IntPtr]::Zero) } }
function Shot([string]$file, [int]$pad = 0) {
    $r = New-Object Probe.Native+RECT
    [Probe.Native]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $x = [Math]::Max(0, $r.Left - $pad); $y = [Math]::Max(0, $r.Top - $pad)
    $w = ($r.Right - $r.Left) + 2 * $pad; $h = ($r.Bottom - $r.Top) + 2 * $pad
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($x, $y, 0, 0, $bmp.Size)
    $g.Dispose()
    $path = Join-Path $ShotDir $file
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    "shot $path ($x,$y ${w}x$h)"
}
function Invoke([string]$name) {
    $root = [System.Windows.Automation.AutomationElement]::FromHandle($hwnd)
    $cond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::NameProperty, $name)
    $el = $root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $cond)
    if (-not $el) { "no element '$name'"; return }
    $el.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
    "invoked $name"
}
function Tree {
    $root = [System.Windows.Automation.AutomationElement]::FromHandle($hwnd)
    $all = $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
    foreach ($el in $all) { $c = $el.Current; "{0,-12} {1,-30} {2} {3}" -f ($c.ControlType.ProgrammaticName -replace 'ControlType\.', ''), $c.Name, $c.ClassName, $c.BoundingRectangle }
}

try {
    foreach ($raw in $Do -split ';') {
        $cmd = $raw.Trim()
        if (-not $cmd) { continue }
        $parts = $cmd -split '\s+'
        switch ($parts[0]) {
            'wait' { Start-Sleep -Milliseconds ([int]$parts[1]) }
            'move' { MoveTo ([int]$parts[1]) ([int]$parts[2]) }
            'click' { MoveTo ([int]$parts[1]) ([int]$parts[2]); Start-Sleep -Milliseconds 80; Down 1; Start-Sleep -Milliseconds 60; Up 1 }
            'dblclick' { MoveTo ([int]$parts[1]) ([int]$parts[2]); Down 1; Up 1; Start-Sleep -Milliseconds 60; Down 1; Up 1 }
            'rclick' { MoveTo ([int]$parts[1]) ([int]$parts[2]); Start-Sleep -Milliseconds 80; Down 2; Start-Sleep -Milliseconds 60; Up 2 }
            'down' { Down 1 }
            'up' { Up 1 }
            'drag' { MoveTo ([int]$parts[1]) ([int]$parts[2]); Down 1; for ($i = 1; $i -le 10; $i++) { MoveTo ([int]$parts[1] + ([int]$parts[3] * $i / 10)) ([int]$parts[2] + ([int]$parts[4] * $i / 10)); Start-Sleep -Milliseconds 30 }; Up 1 }
            'shot' { Shot $parts[1] ([int]($parts[2] ?? 0)) }
            'invoke' { Invoke (($parts | Select-Object -Skip 1) -join ' ') }
            'tree' { Tree }
            'altspace' { [Probe.Native]::keybd_event(0x12, 0, 0, [IntPtr]::Zero); [Probe.Native]::keybd_event(0x20, 0, 0, [IntPtr]::Zero); [Probe.Native]::keybd_event(0x20, 0, 2, [IntPtr]::Zero); [Probe.Native]::keybd_event(0x12, 0, 2, [IntPtr]::Zero) }
            'esc' { [Probe.Native]::keybd_event(0x1B, 0, 0, [IntPtr]::Zero); [Probe.Native]::keybd_event(0x1B, 0, 2, [IntPtr]::Zero) }
            'ctrlkey' { $vk = [byte][Convert]::ToInt32($parts[1], 16); [Probe.Native]::keybd_event(0x11, 0, 0, [IntPtr]::Zero); [Probe.Native]::keybd_event($vk, 0, 0, [IntPtr]::Zero); [Probe.Native]::keybd_event($vk, 0, 2, [IntPtr]::Zero); [Probe.Native]::keybd_event(0x11, 0, 2, [IntPtr]::Zero) }
            'rect' { $r = New-Object Probe.Native+RECT; [Probe.Native]::GetWindowRect($hwnd, [ref]$r) | Out-Null; "window $($r.Left),$($r.Top) $($r.Right - $r.Left)x$($r.Bottom - $r.Top)" }
            'restore' { [Probe.Native]::PostMessageW($hwnd, 0x112, [IntPtr]0xF120, [IntPtr]::Zero) | Out-Null }
            'alive' { $proc.Refresh(); "alive=$(-not $proc.HasExited)" }
            default { "unknown command $cmd" }
        }
    }
} finally {

    if (-not $Keep) {
        $proc.Refresh()
        if (-not $proc.HasExited) { [Probe.Native]::PostMessageW($hwnd, 0x10, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 800 }
        $proc.Refresh()
        if (-not $proc.HasExited) { $proc.Kill() }
    }
    "exit=$($proc.ExitCode)"
}

