rule EnvironmentMingw {
    meta:
        category = "Environment"
        name = "MinGW"

    strings:
        $mingw_runtime = "Mingw runtime failure" ascii
        $mingw64_runtime = "Mingw-w64 runtime failure:" ascii fullword
        $msys2 = "Built by MSYS2 project" ascii

    condition:
        2 of them
}

rule EnvironmentWin32 {
    meta:
        category = "Environment"
        name = "Win32"

    strings:
        $kernel32 = "KERNEL32.dll" ascii
        $user32 = "USER32.dll" ascii
        $advapi32 = "ADVAPI32.dll" ascii
        $ole32 = "OLE32.dll" ascii
        $oleaut32 = "OLEAUT32.dll" ascii
        $shell32 = "SHELL32.dll" ascii
        $shlwapi = "SHLWAPI.dll" ascii
        $comctl32 = "COMCTL32.dll" ascii
        $comdlg32 = "COMDLG32.dll" ascii
        $gdi32 = "GDI32.dll" ascii
        $imm32 = "IMM32.dll" ascii
        $msvcrt = "MSVCRT.dll" ascii

    condition:
        4 of them
}