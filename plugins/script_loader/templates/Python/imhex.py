import ctypes

from enum import Enum
from abc import ABC, abstractmethod

_script_loader = ctypes.CDLL("Script Loader", ctypes.DEFAULT_MODE, int(__script_loader__))
_callback_refs = []


class Color:
    def __init__(self, r: int, g: int, b: int, a: int):
        self.r = r
        self.g = g
        self.b = b
        self.a = a

    def to_int(self):
        return (self.a << 24) | (self.b << 16) | (self.g << 8) | self.r


class UI:
    @staticmethod
    def show_message_box(message: str):
        _script_loader.showMessageBoxV1(ctypes.create_string_buffer(message.encode("utf-8")))

    @staticmethod
    def show_input_text_box(title: str, message: str, buffer_size: int = 256):
        buffer = ctypes.create_string_buffer(buffer_size)
        _script_loader.showInputTextBoxV1(ctypes.create_string_buffer(title.encode("utf-8")),
                                          ctypes.create_string_buffer(message.encode("utf-8")),
                                          buffer, buffer_size)
        return buffer.value.decode("utf-8")

    @staticmethod
    def show_yes_no_question_box(title: str, message: str):
        result = ctypes.c_bool()
        _script_loader.showYesNoQuestionBoxV1(ctypes.create_string_buffer(title.encode("utf-8")),
                                             ctypes.create_string_buffer(message.encode("utf-8")),
                                             ctypes.byref(result))

    class ToastType(Enum):
        INFO    = 0
        WARNING = 1
        ERROR   = 2

    @staticmethod
    def show_toast(message: str, toast_type: ToastType):
        _script_loader.showToastV1(ctypes.create_string_buffer(message.encode("utf-8")), toast_type.value)

    @staticmethod
    def get_imgui_context():
        return _script_loader.getImGuiContextV1()

    @staticmethod
    def register_view(icon: str, name: str, draw_callback):
        draw_function = ctypes.CFUNCTYPE(None)

        global _callback_refs
        _callback_refs.append(draw_function(draw_callback))

        _script_loader.registerViewV1(ctypes.create_string_buffer(icon.encode("utf-8")),
                                     ctypes.create_string_buffer(name.encode("utf-8")),
                                     draw_function(draw_callback))

    @staticmethod
    def add_menu_item(icon: str, menu_name: str, item_name: str, callback):
        callback_function = ctypes.CFUNCTYPE(None)

        global _callback_refs
        _callback_refs.append(callback_function(callback))

        _script_loader.addMenuItemV1(ctypes.create_string_buffer(icon.encode("utf-8")),
                                     ctypes.create_string_buffer(menu_name.encode("utf-8")),
                                     ctypes.create_string_buffer(item_name.encode("utf-8")),
                                     _callback_refs[-1])

class Bookmarks:
    @staticmethod
    def create_bookmark(address: int, size: int, color: Color, name: str, description: str = ""):
        _script_loader.addBookmarkV1(address, size,
                                     color.to_int(),
                                     ctypes.create_string_buffer(name.encode("utf-8")),
                                     ctypes.create_string_buffer(description.encode("utf-8")))

class Logger:
    @staticmethod
    def print(message: str):
        _script_loader.logPrintV1(ctypes.create_string_buffer(message.encode("utf-8")))

    @staticmethod
    def println(message: str):
        _script_loader.logPrintlnV1(ctypes.create_string_buffer(message.encode("utf-8")))

    @staticmethod
    def debug(message: str):
        _script_loader.logDebugV1(ctypes.create_string_buffer(message.encode("utf-8")))

    @staticmethod
    def info(message: str):
        _script_loader.logInfoV1(ctypes.create_string_buffer(message.encode("utf-8")))

    @staticmethod
    def warn(message: str):
        _script_loader.logWarnV1(ctypes.create_string_buffer(message.encode("utf-8")))

    @staticmethod
    def error(message: str):
        _script_loader.logErrorV1(ctypes.create_string_buffer(message.encode("utf-8")))

    @staticmethod
    def fatal(message: str):
        _script_loader.logFatalV1(ctypes.create_string_buffer(message.encode("utf-8")))

class Memory:
    @staticmethod
    def read(address: int, size: int):
        buffer = ctypes.create_string_buffer(size)
        _script_loader.readMemoryV1(address, buffer, size)
        return buffer.raw

    @staticmethod
    def write(address: int, data: bytes):
        _script_loader.writeMemoryV1(address, data, len(data))

    @staticmethod
    def get_base_address():
        return _script_loader.getBaseAddressV1()

    @staticmethod
    def get_data_size():
        return _script_loader.getDataSizeV1()

    @staticmethod
    def get_selection():
        start = ctypes.c_uint64()
        end = ctypes.c_uint64()

        if not _script_loader.getSelectionV1(ctypes.byref(start), ctypes.byref(end)):
            return None, None
        else:
            return start.value, end.value

    class Provider(ABC):
        def __init__(self, type_name, name):
            self.type_name = type_name
            self.name = name

        @abstractmethod
        def read(self, address: int, size: int):
            pass

        @abstractmethod
        def write(self, address: int, data: bytes):
            pass

        @abstractmethod
        def get_size(self):
            pass

    @staticmethod
    def register_provider(provider):
        provider_read_function = ctypes.CFUNCTYPE(None, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_uint64)
        provider_write_function = ctypes.CFUNCTYPE(None, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_uint64)
        provider_get_size_function = ctypes.CFUNCTYPE(ctypes.c_uint64)

        global _callback_refs
        _callback_refs.append(provider_read_function(provider.read))
        _callback_refs.append(provider_write_function(provider.write))
        _callback_refs.append(provider_get_size_function(provider.get_size))

        _script_loader.registerMemoryProviderV1(ctypes.create_string_buffer(provider.type_name.encode("utf-8")),
                                                ctypes.create_string_buffer(provider.name.encode("utf-8")),
                                                _callback_refs[-3],
                                                _callback_refs[-2],
                                                _callback_refs[-1])