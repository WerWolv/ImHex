class ImHexTypeMeta(type):
    def __new__(cls, name, bases, dct):
        return super().__new__(cls, name, bases, dct)

    def __getitem__(self, value):
        return array(self, value)

class ImHexType(metaclass=ImHexTypeMeta):
    pass

class u8(ImHexType):
    pass
class u16(ImHexType):
    pass
class u32(ImHexType):
    pass
class u64(ImHexType):
    pass
class u128(ImHexType):
    pass

class s8(ImHexType):
    pass
class s16(ImHexType):
    pass
class s32(ImHexType):
    pass
class s64(ImHexType):
    pass
class s128(ImHexType):
    pass

class float(ImHexType):
    pass
class double(ImHexType):
    pass

class array(ImHexType):
    def __init__(self, array_type, size):
        self.array_type = array_type()
        self.size = size
    
    array_type : type
    size : int