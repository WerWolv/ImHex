#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>

namespace hex::plugin::windows {

    void registerLanguageZhCN() {
        ContentRegistry::Language::addLocalizations("zh-CN", {
            { "hex.windows.view.tty_console.name", "TTY控制台" },
                { "hex.windows.view.tty_console.config", "配置"},
                    { "hex.windows.view.tty_console.port", "端口" },
                    { "hex.windows.view.tty_console.reload", "刷新" },
                    { "hex.windows.view.tty_console.baud", "波特率" },
                    { "hex.windows.view.tty_console.num_bits", "数据位" },
                    { "hex.windows.view.tty_console.stop_bits", "终止位" },
                    { "hex.windows.view.tty_console.parity_bits", "奇偶校验位" },
                    { "hex.windows.view.tty_console.cts", "使用CTS流控制" },
                    { "hex.windows.view.tty_console.connect", "连接" },
                    { "hex.windows.view.tty_console.disconnect", "断开" },
                    { "hex.windows.view.tty_console.connect_error", "无法连接到COM端口！" },
                    { "hex.windows.view.tty_console.no_available_port", "未选择有效的COM端口或无可用COM端口！" },
                    { "hex.windows.view.tty_console.clear", "清除" },
                    { "hex.windows.view.tty_console.auto_scroll", "自动滚动" },
                { "hex.windows.view.tty_console.console", "控制台" },
                    { "hex.windows.view.tty_console.send_etx", "发送ETX" },
                    { "hex.windows.view.tty_console.send_eot", "发送EOT" },
                    { "hex.windows.view.tty_console.send_sub", "发送SUB" },

                    { "hex.builtin.setting.general.context_menu_entry", "窗口上下文菜单项" },
        });
    }

}
