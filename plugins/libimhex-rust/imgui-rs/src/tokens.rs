#[macro_export]
/// This is a macro used internally by imgui-rs to create StackTokens
/// representing various global state in DearImGui.
///
/// These tokens can either be allowed to drop or dropped manually
/// by called `end` on them. Preventing this token from dropping,
/// or moving this token out of the block it was made in can have
/// unintended side effects, including failed asserts in the DearImGui C++.
///
/// In general, if you're looking at this, don't overthink these -- just slap
/// a '_token` as their binding name and allow them to drop.
macro_rules! create_token {
    (
        $(#[$struct_meta:meta])*
        $v:vis struct $token_name:ident<'ui>;

        $(#[$end_meta:meta])*
        drop { $on_drop:expr }
    ) => {
        #[must_use]
        $(#[$struct_meta])*
        pub struct $token_name<'a>($crate::__core::marker::PhantomData<crate::Ui<'a>>);

        impl<'a> $token_name<'a> {
            /// Creates a new token type.
            pub(crate) fn new(_: &crate::Ui<'a>) -> Self {
                Self(std::marker::PhantomData)
            }

            $(#[$end_meta])*
            #[inline]
            pub fn end(self) {
                // left empty for drop
            }
        }

        impl Drop for $token_name<'_> {
            fn drop(&mut self) {
                unsafe { $on_drop }
            }
        }
    }
}
