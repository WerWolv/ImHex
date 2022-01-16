/// Wraps u32 that represents a packed RGBA color. Mostly used by types in the
/// low level custom drawing API, such as [`DrawListMut`](crate::DrawListMut).
///
/// The bits of a color are in "`0xAABBGGRR`" format (e.g. RGBA as little endian
/// bytes). For clarity: we don't support an equivalent to the
/// `IMGUI_USE_BGRA_PACKED_COLOR` define.
///
/// This used to be named `ImColor32`, but was renamed to avoid confusion with
/// the type with that name in the C++ API (which uses 32 bits per channel).
///
/// While it doesn't provide methods to access the fields, they can be accessed
/// via the `Deref`/`DerefMut` impls it provides targeting
/// [`imgui::color::ImColor32Fields`](crate::color::ImColor32Fields), which has
/// no other meaningful uses.
///
/// # Example
/// ```
/// let mut c = imgui::ImColor32::from_rgba(0x80, 0xc0, 0x40, 0xff);
/// assert_eq!(c.to_bits(), 0xff_40_c0_80); // Note: 0xAA_BB_GG_RR
/// // Field access
/// assert_eq!(c.r, 0x80);
/// assert_eq!(c.g, 0xc0);
/// assert_eq!(c.b, 0x40);
/// assert_eq!(c.a, 0xff);
/// c.b = 0xbb;
/// assert_eq!(c.to_bits(), 0xff_bb_c0_80);
/// ```
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[repr(transparent)]
pub struct ImColor32(u32); // TBH maybe the wrapped field should be `pub`.

impl ImColor32 {
    /// Convenience constant for solid black.
    pub const BLACK: Self = Self(0xff_00_00_00);

    /// Convenience constant for solid white.
    pub const WHITE: Self = Self(0xff_ff_ff_ff);

    /// Convenience constant for full transparency.
    pub const TRANSPARENT: Self = Self(0);

    /// Construct a color from 4 single-byte `u8` channel values, which should
    /// be between 0 and 255.
    #[inline]
    pub const fn from_rgba(r: u8, g: u8, b: u8, a: u8) -> Self {
        Self(
            ((a as u32) << Self::A_SHIFT)
                | ((r as u32) << Self::R_SHIFT)
                | ((g as u32) << Self::G_SHIFT)
                | ((b as u32) << Self::B_SHIFT),
        )
    }

    /// Construct a fully opaque color from 3 single-byte `u8` channel values.
    /// Same as [`Self::from_rgba`] with a == 255
    #[inline]
    pub const fn from_rgb(r: u8, g: u8, b: u8) -> Self {
        Self::from_rgba(r, g, b, 0xff)
    }

    /// Construct a fully opaque color from 4 `f32` channel values in the range
    /// `0.0 ..= 1.0` (values outside this range are clamped to it, with NaN
    /// mapped to 0.0).
    ///
    /// Note: No alpha premultiplication is done, so your input should be have
    /// premultiplied alpha if needed.
    #[inline]
    // not const fn because no float math in const eval yet 😩
    pub fn from_rgba_f32s(r: f32, g: f32, b: f32, a: f32) -> Self {
        Self::from_rgba(
            f32_to_u8_sat(r),
            f32_to_u8_sat(g),
            f32_to_u8_sat(b),
            f32_to_u8_sat(a),
        )
    }

    /// Return the channels as an array of f32 in `[r, g, b, a]` order.
    #[inline]
    pub fn to_rgba_f32s(self) -> [f32; 4] {
        let &ImColor32Fields { r, g, b, a } = &*self;
        [
            u8_to_f32_sat(r),
            u8_to_f32_sat(g),
            u8_to_f32_sat(b),
            u8_to_f32_sat(a),
        ]
    }

    /// Return the channels as an array of u8 in `[r, g, b, a]` order.
    #[inline]
    pub fn to_rgba(self) -> [u8; 4] {
        let &ImColor32Fields { r, g, b, a } = &*self;
        [r, g, b, a]
    }

    /// Equivalent to [`Self::from_rgba_f32s`], but with an alpha of 1.0 (e.g.
    /// opaque).
    #[inline]
    pub fn from_rgb_f32s(r: f32, g: f32, b: f32) -> Self {
        Self::from_rgba(f32_to_u8_sat(r), f32_to_u8_sat(g), f32_to_u8_sat(b), 0xff)
    }

    /// Construct a color from the `u32` that makes up the bits in `0xAABBGGRR`
    /// format.
    ///
    /// Specifically, this takes the RGBA values as a little-endian u32 with 8
    /// bits per channel.
    ///
    /// Note that [`ImColor32::from_rgba`] may be a bit easier to use.
    #[inline]
    pub const fn from_bits(u: u32) -> Self {
        Self(u)
    }

    /// Return the bits of the color as a u32. These are in "`0xAABBGGRR`" format, that
    /// is, little-endian RGBA with 8 bits per channel.
    #[inline]
    pub const fn to_bits(self) -> u32 {
        self.0
    }

    // These are public in C++ ImGui, should they be public here?
    /// The number of bits to shift the byte of the red channel. Always 0.
    const R_SHIFT: u32 = 0;
    /// The number of bits to shift the byte of the green channel. Always 8.
    const G_SHIFT: u32 = 8;
    /// The number of bits to shift the byte of the blue channel. Always 16.
    const B_SHIFT: u32 = 16;
    /// The number of bits to shift the byte of the alpha channel. Always 24.
    const A_SHIFT: u32 = 24;
}

impl Default for ImColor32 {
    #[inline]
    fn default() -> Self {
        Self::TRANSPARENT
    }
}

impl std::fmt::Debug for ImColor32 {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("ImColor32")
            .field("r", &self.r)
            .field("g", &self.g)
            .field("b", &self.b)
            .field("a", &self.a)
            .finish()
    }
}

/// A struct that exists to allow field access to [`ImColor32`]. It essentially
/// exists to be a `Deref`/`DerefMut` target and provide field access.
///
/// Note that while this is repr(C), be aware that on big-endian machines
/// (`cfg(target_endian = "big")`) the order of the fields is reversed, as this
/// is a view into a packed u32.
///
/// Generally should not be used, except as the target of the `Deref` impl of
/// [`ImColor32`].
#[derive(Copy, Clone, Debug)]
#[repr(C, align(4))]
// Should this be #[non_exhaustive] to discourage direct use?
#[rustfmt::skip]
pub struct ImColor32Fields {
    #[cfg(target_endian = "little")] pub r: u8,
    #[cfg(target_endian = "little")] pub g: u8,
    #[cfg(target_endian = "little")] pub b: u8,
    #[cfg(target_endian = "little")] pub a: u8,
    // TODO(someday): i guess we should have BE tests, but for now I verified
    // this locally.
    #[cfg(target_endian = "big")] pub a: u8,
    #[cfg(target_endian = "big")] pub b: u8,
    #[cfg(target_endian = "big")] pub g: u8,
    #[cfg(target_endian = "big")] pub r: u8,
}

// We assume that big and little are the only endiannesses, and that exactly one
// is set. That is, PDP endian is not in use, and the we aren't using a
// completely broken custom target json or something.
#[cfg(any(
    all(target_endian = "little", target_endian = "big"),
    all(not(target_endian = "little"), not(target_endian = "big")),
))]
compile_error!("`cfg(target_endian = \"little\")` must be `cfg(not(target_endian = \"big\")`");

// static assert sizes match
const _: [(); core::mem::size_of::<ImColor32>()] = [(); core::mem::size_of::<ImColor32Fields>()];
const _: [(); core::mem::align_of::<ImColor32>()] = [(); core::mem::align_of::<ImColor32Fields>()];

impl core::ops::Deref for ImColor32 {
    type Target = ImColor32Fields;
    #[inline]
    fn deref(&self) -> &ImColor32Fields {
        // Safety: we statically assert the size and align match, and neither
        // type has any special invariants.
        unsafe { &*(self as *const Self as *const ImColor32Fields) }
    }
}
impl core::ops::DerefMut for ImColor32 {
    #[inline]
    fn deref_mut(&mut self) -> &mut ImColor32Fields {
        // Safety: we statically assert the size and align match, and neither
        // type has any special invariants.
        unsafe { &mut *(self as *mut Self as *mut ImColor32Fields) }
    }
}

impl From<ImColor32> for u32 {
    #[inline]
    fn from(color: ImColor32) -> Self {
        color.0
    }
}

impl From<u32> for ImColor32 {
    #[inline]
    fn from(color: u32) -> Self {
        ImColor32(color)
    }
}

impl From<[f32; 4]> for ImColor32 {
    #[inline]
    fn from(v: [f32; 4]) -> Self {
        Self::from_rgba_f32s(v[0], v[1], v[2], v[3])
    }
}

impl From<(f32, f32, f32, f32)> for ImColor32 {
    #[inline]
    fn from(v: (f32, f32, f32, f32)) -> Self {
        Self::from_rgba_f32s(v.0, v.1, v.2, v.3)
    }
}

impl From<[f32; 3]> for ImColor32 {
    #[inline]
    fn from(v: [f32; 3]) -> Self {
        Self::from_rgb_f32s(v[0], v[1], v[2])
    }
}

impl From<(f32, f32, f32)> for ImColor32 {
    fn from(v: (f32, f32, f32)) -> Self {
        Self::from_rgb_f32s(v.0, v.1, v.2)
    }
}

impl From<ImColor32> for [f32; 4] {
    #[inline]
    fn from(v: ImColor32) -> Self {
        v.to_rgba_f32s()
    }
}
impl From<ImColor32> for (f32, f32, f32, f32) {
    #[inline]
    fn from(color: ImColor32) -> Self {
        let [r, g, b, a]: [f32; 4] = color.into();
        (r, g, b, a)
    }
}

// These utilities might be worth making `pub` as free functions in
// `crate::color` so user code can ensure their numeric handling is
// consistent...

/// Clamp `v` to between 0.0 and 1.0, always returning a value between those.
///
/// Never returns NaN, or -0.0 — instead returns +0.0 for these (We differ from
/// C++ Dear ImGUI here which probably is just ignoring values like these).
#[inline]
pub(crate) fn saturate(v: f32) -> f32 {
    // Note: written strangely so that special values (NaN/-0.0) are handled
    // automatically with no extra checks.
    if v > 0.0 {
        if v <= 1.0 {
            v
        } else {
            1.0
        }
    } else {
        0.0
    }
}

/// Quantize a value in `0.0..=1.0` to `0..=u8::MAX`. Input outside 0.0..=1.0 is
/// clamped. Uses a bias of 0.5, because we assume centered quantization is used
/// (and because C++ imgui does it too). See:
/// - https://github.com/ocornut/imgui/blob/e28b51786eae60f32c18214658c15952639085a2/imgui_internal.h#L218
/// - https://cbloomrants.blogspot.com/2020/09/topics-in-quantization-for-games.html
///   (see `quantize_centered`)
#[inline]
pub(crate) fn f32_to_u8_sat(f: f32) -> u8 {
    let f = saturate(f) * 255.0 + 0.5;
    // Safety: `saturate`'s result is between 0.0 and 1.0 (never NaN even for
    // NaN input), and so for all inputs, `saturate(f) * 255.0 + 0.5` is inside
    // `0.5 ..= 255.5`.
    //
    // This is verified for all f32 in `test_f32_to_u8_sat_exhaustive`.
    //
    // Also note that LLVM doesn't bother trying to figure this out so the
    // unchecked does actually help. (That said, this likely doesn't matter
    // for imgui-rs, but I had this code in another project and it felt
    // silly to needlessly pessimize it).
    unsafe { f.to_int_unchecked() }
}

/// Opposite of `f32_to_u8_sat`. Since we assume centered quantization, this is
/// equivalent to dividing by 255 (or, multiplying by 1.0/255.0)
#[inline]
pub(crate) fn u8_to_f32_sat(u: u8) -> f32 {
    (u as f32) * (1.0 / 255.0)
}

#[test]
fn check_sat() {
    assert_eq!(saturate(1.0), 1.0);
    assert_eq!(saturate(0.5), 0.5);
    assert_eq!(saturate(0.0), 0.0);
    assert_eq!(saturate(-1.0), 0.0);
    // next float from 1.0
    assert_eq!(saturate(1.0 + f32::EPSILON), 1.0);
    // prev float from 0.0 (Well, from -0.0)
    assert_eq!(saturate(-f32::MIN_POSITIVE), 0.0);
    // some NaNs.
    assert_eq!(saturate(f32::NAN), 0.0);
    assert_eq!(saturate(-f32::NAN), 0.0);
    // neg zero comes through as +0
    assert_eq!(saturate(-0.0).to_bits(), 0.0f32.to_bits());
}

// Check that the unsafe in `f32_to_u8_sat` is fine for all f32 (and that the
// comments I wrote about `saturate` are actually true). This is way too slow in
// debug mode, but finishes in ~15s on my machine for release (just this test).
// This is tested in CI, but will only run if invoked manually with something
// like: `cargo test -p imgui --release -- --ignored`.
#[test]
#[ignore]
fn test_f32_to_u8_sat_exhaustive() {
    for f in (0..=u32::MAX).map(f32::from_bits) {
        let v = saturate(f);
        assert!(
            (0.0..=1.0).contains(&v) && (v.to_bits() != (-0.0f32).to_bits()),
            "sat({} [e.g. {:#x}]) => {} [e.g {:#x}]",
            f,
            f.to_bits(),
            v,
            v.to_bits(),
        );
        let sat = v * 255.0 + 0.5;
        // Note: This checks what's required by is the safety predicate for
        // `f32::to_int_unchecked`:
        // https://doc.rust-lang.org/std/primitive.f32.html#method.to_int_unchecked
        assert!(
            sat.trunc() >= 0.0 && sat.trunc() <= (u8::MAX as f32) && sat.is_finite(),
            "f32_to_u8_sat({} [e.g. {:#x}]) would be UB!",
            f,
            f.to_bits(),
        );
    }
}

#[test]
fn test_saturate_all_u8s() {
    for u in 0..=u8::MAX {
        let v = f32_to_u8_sat(u8_to_f32_sat(u));
        assert_eq!(u, v);
    }
}
