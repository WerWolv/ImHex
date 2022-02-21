use crate::ffi::hex::ImHexApi::Common;

/// Close ImHex, optionally prompting the user if they'd like to quit
pub fn close_imhex(without_question: bool) {
    Common::closeImHex(without_question)
}

/// Close and reopen ImHex
pub fn restart_imhex() {
    Common::restartImHex()
}
