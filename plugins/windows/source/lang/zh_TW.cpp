#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>

namespace hex::plugin::windows {

    void registerLanguageZhTW() {
        ContentRegistry::Language::addLocalizations("zh-TW", {
            { "hex.windows.title_bar_button.feedback", "意見回饋" },
            { "hex.windows.title_bar_button.debug_build", "除錯組建"},

            { "hex.windows.view.tty_console.name", "TTY 終端機" },
                { "hex.windows.view.tty_console.config", "設定"},
                    { "hex.windows.view.tty_console.port", "連接埠" },
                    { "hex.windows.view.tty_console.reload", "重新載入" },
                    { "hex.windows.view.tty_console.baud", "鮑率" },
                    { "hex.windows.view.tty_console.num_bits", "資料位元" },
                    { "hex.windows.view.tty_console.stop_bits", "停止位元" },
                    { "hex.windows.view.tty_console.parity_bits", "同位位元" },
                    { "hex.windows.view.tty_console.cts", "使用 CTS 流量控制" },
                    { "hex.windows.view.tty_console.connect", "連接" },
                    { "hex.windows.view.tty_console.disconnect", "斷開連接" },
                    { "hex.windows.view.tty_console.connect_error", "無法連接至 COM 連接埠！" },
                    { "hex.windows.view.tty_console.no_available_port", "未選取有效的 COM 連接埠或無可用的 COM 連接埠！" },
                    { "hex.windows.view.tty_console.clear", "清除" },
                    { "hex.windows.view.tty_console.auto_scroll", "自動捲動" },
                { "hex.windows.view.tty_console.console", "終端機" },
                    { "hex.windows.view.tty_console.send_etx", "傳送 ETX" },
                    { "hex.windows.view.tty_console.send_eot", "傳送 EOT" },
                    { "hex.windows.view.tty_console.send_sub", "傳送 SUB" },

                    { "hex.builtin.setting.general.context_menu_entry", "視窗內容功能表項目" },
        });
    }

}