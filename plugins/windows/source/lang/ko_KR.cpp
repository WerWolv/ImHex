#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>

namespace hex::plugin::windows {

    void registerLanguageKoKR() {
        ContentRegistry::Language::addLocalizations("ko-KR", {
            { "hex.windows.title_bar_button.feedback", "피드백 남기기" },
            { "hex.windows.title_bar_button.debug_build", "디버그 빌드"},

            { "hex.windows.view.tty_console.name", "TTY 콘솔" },
                { "hex.windows.view.tty_console.config", "설정"},
                    { "hex.windows.view.tty_console.port", "포트" },
                    { "hex.windows.view.tty_console.reload", "새로 고침" },
                    { "hex.windows.view.tty_console.baud", "Baud rate" },
                    { "hex.windows.view.tty_console.num_bits", "Data bits" },
                    { "hex.windows.view.tty_console.stop_bits", "Stop bits" },
                    { "hex.windows.view.tty_console.parity_bits", "Parity bit" },
                    { "hex.windows.view.tty_console.cts", "Use CTS flow control" },
                    { "hex.windows.view.tty_console.connect", "연결" },
                    { "hex.windows.view.tty_console.disconnect", "연결 끊기" },
                    { "hex.windows.view.tty_console.connect_error", "COM 포트 연결에 실패했습니다!" },
                    { "hex.windows.view.tty_console.no_available_port", "선택한 COM 포트가 올바르지 않거나 COM 포트가 존재하지 않습니다!" },
                    { "hex.windows.view.tty_console.clear", "지우기" },
                    { "hex.windows.view.tty_console.auto_scroll", "자동 스크롤" },
                { "hex.windows.view.tty_console.console", "콘솔" },
                    { "hex.windows.view.tty_console.send_etx", "ETX 보내기" },
                    { "hex.windows.view.tty_console.send_eot", "EOT 보내기" },
                    { "hex.windows.view.tty_console.send_sub", "SUB 보내기" },

                    { "hex.builtin.setting.general.context_menu_entry", "Windows 컨텍스트 메뉴 항목" },
        });
    }

}