import ctypes

script_loader = ctypes.CDLL("Script Loader", ctypes.DEFAULT_MODE, int(__script_loader__))


class UI:
    @staticmethod
    def show_message_box(message: str):
        script_loader.showMessageBoxV1(ctypes.create_string_buffer(message.encode("utf-8")))
