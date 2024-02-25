rule CompilerMSVC {
    meta:
        category = "Compiler"
        name = "MSVC"

    strings:
        $iostreams_mangled_name = "$basic_iostream@DU" ascii
        $std_namespace = "@@std@@" ascii

    condition:
        any of them
}

rule CompilerGCC {
    meta:
        category = "Compiler"
        name = "GCC"

    strings:
        $iostreams_mangled_name = "_ZSt4cout" ascii
        $std_namespace = "_ZSt" ascii
        $gcc_version = "GCC: (GNU) " ascii

    condition:
        2 of them
}

rule CompilerClang {
    meta:
        category = "Compiler"
        name = "Clang"

    strings:
        $iostreams_mangled_name = "_ZSt4cout" ascii
        $std_namespace = "_ZSt" ascii
        $clang_version = "clang version " ascii

    condition:
        2 of them
}