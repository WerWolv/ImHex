# Skip all std:: and __gnu_debug:: symbols
skip -rfu ^std::
skip -rfu ^__gnu_debug::

# Skip all ImGui:: symbols
skip -rfu ^ImGui::

# Trigger breakpoint when execution reaches triggerSafeShutdown()
break triggerSafeShutdown

# Print backtrace after execution jumped to an invalid address
define fixbt
    set $pc = *(void **)$rsp
    set $rsp = $rsp + 8
    bt
end