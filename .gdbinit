# Skip all std:: and __gnu_debug:: symbols
skip -rfu ^std::
skip -rfu ^__gnu_debug::

# Skip all ImGui:: symbols
skip -rfu ^ImGui::

# Trigger breakpoint when execution reaches triggerSafeShutdown()
break triggerSafeShutdown