rule LanguageCpp {
    meta:
        category = "Programming Language"
        name = "C++"

    strings:
        $exception_windows = "_CxxThrowException" ascii fullword
        $iostreams = "iostream" ascii
        
    condition:
        any of them
}

rule LanguageC {
    meta:
        category = "Programming Language"
        name = "C++"

    strings:
        $printf = "printf" ascii
        $scanf = "scanf" ascii
        $malloc = "malloc" ascii
        $calloc = "calloc" ascii
        $realloc = "realloc" ascii
        $free = "free" ascii

    condition:
        any of them and not LanguageCpp
}

rule LanguageRust {
    meta:
        category = "Programming Language"
        name = "Rust"

    strings:
        $option_unwrap = "called `Option::unwrap()` on a `None`" ascii
        $result_unwrap = "called `Result::unwrap()` on an `Err`" ascii
        $panic_1 = "panicked at" ascii
        $panic_2 = "thread '' panicked at" ascii
        $panic_3 = "thread panicked while processing panic. aborting." ascii
        $panicking_file = "panicking.rs" ascii fullword

    condition:
        any of them
}

rule LanguageGo {
    meta:
        category = "Programming Language"
        name = "Go"

    strings:
        $max_procs = "runtime.GOMAXPROCS" ascii fullword
        $panic = "runtime.gopanic" ascii fullword
        $go_root = "runtime.GOROOT" ascii fullword

    condition:
        any of them

}