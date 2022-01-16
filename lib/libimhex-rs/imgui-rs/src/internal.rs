//! Internal raw utilities (don't use unless you know what you're doing!)

use std::slice;

/// A generic version of the raw imgui-sys ImVector struct types
#[repr(C)]
pub struct ImVector<T> {
    size: i32,
    capacity: i32,
    pub(crate) data: *mut T,
}

impl<T> ImVector<T> {
    #[inline]
    pub fn as_slice(&self) -> &[T] {
        unsafe { slice::from_raw_parts(self.data, self.size as usize) }
    }
}

#[test]
#[cfg(test)]
fn test_imvector_memory_layout() {
    use std::mem;
    assert_eq!(
        mem::size_of::<ImVector<u8>>(),
        mem::size_of::<sys::ImVector_char>()
    );
    assert_eq!(
        mem::align_of::<ImVector<u8>>(),
        mem::align_of::<sys::ImVector_char>()
    );
    use sys::ImVector_char;
    type VectorChar = ImVector<u8>;
    macro_rules! assert_field_offset {
        ($l:ident, $r:ident) => {
            assert_eq!(
                memoffset::offset_of!(VectorChar, $l),
                memoffset::offset_of!(ImVector_char, $r)
            );
        };
    }
    assert_field_offset!(size, Size);
    assert_field_offset!(capacity, Capacity);
    assert_field_offset!(data, Data);
}

/// Marks a type as a transparent wrapper over a raw type
pub trait RawWrapper {
    /// Wrapped raw type
    type Raw;
    /// Returns an immutable reference to the wrapped raw value
    ///
    /// # Safety
    ///
    /// It is up to the caller to use the returned raw reference without causing undefined
    /// behaviour or breaking safety rules.
    unsafe fn raw(&self) -> &Self::Raw;
    /// Returns a mutable reference to the wrapped raw value
    ///
    /// # Safety
    ///
    /// It is up to the caller to use the returned mutable raw reference without causing undefined
    /// behaviour or breaking safety rules.
    unsafe fn raw_mut(&mut self) -> &mut Self::Raw;
}

/// Casting from/to a raw type that has the same layout and alignment as the target type
pub unsafe trait RawCast<T>: Sized {
    /// Casts an immutable reference from the raw type
    ///
    /// # Safety
    ///
    /// It is up to the caller to guarantee the cast is valid.
    #[inline]
    unsafe fn from_raw(raw: &T) -> &Self {
        &*(raw as *const _ as *const Self)
    }
    /// Casts a mutable reference from the raw type
    ///
    /// # Safety
    ///
    /// It is up to the caller to guarantee the cast is valid.
    #[inline]
    unsafe fn from_raw_mut(raw: &mut T) -> &mut Self {
        &mut *(raw as *mut _ as *mut Self)
    }
    /// Casts an immutable reference to the raw type
    ///
    /// # Safety
    ///
    /// It is up to the caller to guarantee the cast is valid.
    #[inline]
    unsafe fn raw(&self) -> &T {
        &*(self as *const _ as *const T)
    }
    /// Casts a mutable reference to the raw type
    ///
    /// # Safety
    ///
    /// It is up to the caller to guarantee the cast is valid.
    #[inline]
    unsafe fn raw_mut(&mut self) -> &mut T {
        &mut *(self as *mut _ as *mut T)
    }
}

/// A primary data type
#[repr(u32)]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum DataType {
    I8 = sys::ImGuiDataType_S8,
    U8 = sys::ImGuiDataType_U8,
    I16 = sys::ImGuiDataType_S16,
    U16 = sys::ImGuiDataType_U16,
    I32 = sys::ImGuiDataType_S32,
    U32 = sys::ImGuiDataType_U32,
    I64 = sys::ImGuiDataType_S64,
    U64 = sys::ImGuiDataType_U64,
    F32 = sys::ImGuiDataType_Float,
    F64 = sys::ImGuiDataType_Double,
}

/// Primitive type marker.
///
/// If this trait is implemented for a type, it is assumed to have *exactly* the same
/// representation in memory as the primitive value described by the associated `KIND` constant.
pub unsafe trait DataTypeKind: Copy {
    const KIND: DataType;
}
unsafe impl DataTypeKind for i8 {
    const KIND: DataType = DataType::I8;
}
unsafe impl DataTypeKind for u8 {
    const KIND: DataType = DataType::U8;
}
unsafe impl DataTypeKind for i16 {
    const KIND: DataType = DataType::I16;
}
unsafe impl DataTypeKind for u16 {
    const KIND: DataType = DataType::U16;
}
unsafe impl DataTypeKind for i32 {
    const KIND: DataType = DataType::I32;
}
unsafe impl DataTypeKind for u32 {
    const KIND: DataType = DataType::U32;
}
unsafe impl DataTypeKind for i64 {
    const KIND: DataType = DataType::I64;
}
unsafe impl DataTypeKind for u64 {
    const KIND: DataType = DataType::U64;
}
unsafe impl DataTypeKind for f32 {
    const KIND: DataType = DataType::F32;
}
unsafe impl DataTypeKind for f64 {
    const KIND: DataType = DataType::F64;
}
