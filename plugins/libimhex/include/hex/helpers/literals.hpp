#pragma once

namespace hex::literals {

    /* Byte literals */

    unsigned long long operator ""_Bytes(unsigned long long bytes) {
        return bytes;
    }

    unsigned long long operator ""_kiB(unsigned long long kiB) {
        return operator ""_Bytes(kiB * 1024);
    }

    unsigned long long operator ""_MiB(unsigned long long MiB) {
        return operator ""_kiB(MiB * 1024);
    }

    unsigned long long operator ""_GiB(unsigned long long GiB) {
        return operator ""_MiB(GiB * 1024);
    }

}