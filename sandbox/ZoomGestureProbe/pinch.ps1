# Касание двумя пальцами в точке экрана -- жест масштабирования для любого
# окна: пальцы расходятся (или сходятся) за кадры по 16 мс.
# InjectSyntheticPointerInput, как в main.cpp этой пробы. Пример:
#   pwsh -File pinch.ps1 -X 700 -Y 500 -From 100 -To 220 -Frames 25
param(
    [int]$X, [int]$Y,
    [int]$From = 100, [int]$To = 400, [int]$Frames = 30
)

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class Touch {
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Explicit, Size = 152)]
    public struct PointerTypeInfo {
        [FieldOffset(0)] public int type;                 // PT_TOUCH = 2
        // POINTER_TOUCH_INFO с 8: POINTER_INFO (96) и сам touch
        [FieldOffset(8)] public int pointerType;
        [FieldOffset(12)] public uint pointerId;
        [FieldOffset(16)] public uint frameId;
        [FieldOffset(20)] public uint pointerFlags;
        [FieldOffset(40)] public POINT ptPixelLocation;
        [FieldOffset(104)] public uint touchFlags;
        [FieldOffset(108)] public uint touchMask;
        [FieldOffset(112)] public RECT rcContact;
        [FieldOffset(144)] public uint orientation;
        [FieldOffset(148)] public uint pressure;
    }
    [DllImport("user32.dll", SetLastError = true)] public static extern IntPtr CreateSyntheticPointerDevice(int type, uint maxCount, int feedback);
    [DllImport("user32.dll", SetLastError = true)] public static extern bool InjectSyntheticPointerInput(IntPtr device, PointerTypeInfo[] info, uint count);
    [DllImport("user32.dll")] public static extern void DestroySyntheticPointerDevice(IntPtr device);
}
'@

$DOWN = 0x00010000 -bor 0x00000002 -bor 0x00000004   # DOWN | INRANGE | INCONTACT
$UPDATE = 0x00020000 -bor 0x00000002 -bor 0x00000004 # UPDATE | INRANGE | INCONTACT
$UP = 0x00040000                                     # UP

$device = [Touch]::CreateSyntheticPointerDevice(2, 2, 1)
if ($device -eq [IntPtr]::Zero) { throw "CreateSyntheticPointerDevice failed" }

function Frame([int]$apart, [uint32]$flags) {
    $contacts = New-Object 'Touch+PointerTypeInfo[]' 2
    for ($i = 0; $i -lt 2; $i++) {
        $c = New-Object Touch+PointerTypeInfo
        $c.type = 2; $c.pointerType = 2; $c.pointerId = $i; $c.pointerFlags = $flags
        $p = New-Object Touch+POINT
        $p.X = if ($i -eq 0) { $X - [int]($apart / 2) } else { $X + [int]($apart / 2) }
        $p.Y = $Y
        $c.ptPixelLocation = $p
        $c.touchMask = 7   # CONTACTAREA | ORIENTATION | PRESSURE
        $r = New-Object Touch+RECT
        $r.L = $p.X - 2; $r.T = $p.Y - 2; $r.R = $p.X + 2; $r.B = $p.Y + 2
        $c.rcContact = $r
        $c.orientation = 90; $c.pressure = 32000
        $contacts[$i] = $c
    }
    if (-not [Touch]::InjectSyntheticPointerInput($device, $contacts, 2)) {
        throw "InjectSyntheticPointerInput failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
}

Frame $From $DOWN
for ($f = 1; $f -le $Frames; $f++) {
    Start-Sleep -Milliseconds 16
    Frame ($From + [int](($To - $From) * $f / $Frames)) $UPDATE
}
Start-Sleep -Milliseconds 16
Frame $To $UP
[Touch]::DestroySyntheticPointerDevice($device)
"pinch $From -> $To at $X,$Y"
