rule CppExecutable {
    meta:
        category = "Programming Language"
        name = "C++"

    strings:
        $exception_windows = "_CxxThrowException" ascii fullword
        $iostreams = "iostream" ascii
        
    condition:
        any of them
}

rule CppMSVC {
    meta:
        category = "Compiler"
        name = "MSVC"

    strings:
        $iostreams_mangled_name = "$basic_iostream@DU" ascii
        $std_namespace = "@@std@@" ascii

    condition:
        any of them and CppExecutable
}