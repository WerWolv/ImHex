//! Structs to create a Drag and Drop sequence. Almost all structs are re-exported
//! and can be accessed from the crate root; some additional utilities can be found in here.
//!
//! A DragDrop is a UI mechanism where users can appear to "drag"
//! some data from one [source](DragDropSource) to one [target](DragDropTarget).
//! A source and a target must both have some `name` identifier, which is declared when they
//! are created. If these names are equal, then a `payload` of some kind
//! will be given to the target caller when the user releases their mouse button over
//! the target (additionally, the UI will reflect that the payload *can* be deposited
//! in the target).
//!
//! The complexity of this implementation is primarily in managing this payload. Users
//! can provide three different kinds of payloads:
//!
//! 1.  Users can give an [empty payload](DragDropPayloadEmpty) with [begin](DragDropSource::begin).
//!     This payload type is essentially just a notification system, but using some shared state,
//!     this can be reasonably powerful, and is the safest way to transfer non-Copy data offered
//!     right now.
//! 2.  Users can give a [simple Copy payload](DragDropPayloadPod) with [begin](DragDropSource::begin_payload).
//!     This allows users to copy data to Dear ImGui, which will take ownership over it, and then be given
//!     it back to the Target. Please note: users are of course free to not drop any drag (cancel a drag),
//!     so this data could easily be lost forever. Our `'static + Copy` bound is intended to keep users
//!     to simplistic types.
//! 3.  An unsafe implementation is provided which allows for any data to be unsafely copied. Note that once
//!     you use this method, the safe implementations in #1 and #2 can create memory unsafety problems; notably,
//!     they both assume that a payload has certain header information within it.
//!
//! For examples of each payload type, see [DragDropSource].
use std::{any, ffi, marker::PhantomData};

use crate::{sys, Condition, Ui};
use bitflags::bitflags;

bitflags!(
    /// Flags for igBeginDragDropSource(), igAcceptDragDropPayload()
    #[repr(transparent)]
    pub struct DragDropFlags: u32 {
        /// By default, a successful call to igBeginDragDropSource opens a tooltip so you can
        /// display a preview or description of the source contents. This flag disable this
        /// behavior.
        const SOURCE_NO_PREVIEW_TOOLTIP = sys::ImGuiDragDropFlags_SourceNoPreviewTooltip;
        /// By default, when dragging we clear data so that igIsItemHovered() will return false, to
        /// avoid subsequent user code submitting tooltips. This flag disable this behavior so you
        /// can still call igIsItemHovered() on the source item.
        const SOURCE_NO_DISABLE_HOVER = sys::ImGuiDragDropFlags_SourceNoDisableHover;
        /// Disable the behavior that allows to open tree nodes and collapsing header by holding
        /// over them while dragging a source item.
        const SOURCE_NO_HOLD_TO_OPEN_OTHERS = sys::ImGuiDragDropFlags_SourceNoHoldToOpenOthers;
        /// Allow items such as igText(), igImage() that have no unique identifier to be used as
        /// drag source, by manufacturing a temporary identifier based on their window-relative
        /// position. This is extremely unusual within the dear imgui ecosystem and so we made it
        /// explicit.
        const SOURCE_ALLOW_NULL_ID = sys::ImGuiDragDropFlags_SourceAllowNullID;
        /// External source (from outside of imgui), won't attempt to read current item/window
        /// info. Will always return true. Only one Extern source can be active simultaneously.
        const SOURCE_EXTERN = sys::ImGuiDragDropFlags_SourceExtern;
        /// Automatically expire the payload if the source ceases to be submitted (otherwise
        /// payloads are persisting while being dragged)
        const SOURCE_AUTO_EXPIRE_PAYLOAD = sys::ImGuiDragDropFlags_SourceAutoExpirePayload;
        /// igAcceptDragDropPayload() will returns true even before the mouse button is released.
        /// You can then call igIsDelivery() to test if the payload needs to be delivered.
        const ACCEPT_BEFORE_DELIVERY = sys::ImGuiDragDropFlags_AcceptBeforeDelivery;
        /// Do not draw the default highlight rectangle when hovering over target.
        const ACCEPT_NO_DRAW_DEFAULT_RECT = sys::ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
        /// Request hiding the igBeginDragDropSource tooltip from the igBeginDragDropTarget site.
        const ACCEPT_NO_PREVIEW_TOOLTIP = sys::ImGuiDragDropFlags_AcceptNoPreviewTooltip;
        /// For peeking ahead and inspecting the payload before delivery. This is just a convenience
        /// flag for the intersection of `ACCEPT_BEFORE_DELIVERY` and `ACCEPT_NO_DRAW_DEFAULT_RECT`
        const ACCEPT_PEEK_ONLY = sys::ImGuiDragDropFlags_AcceptPeekOnly;
    }
);

/// Creates a source for drag drop data out of the last ID created.
///
/// ```no_run
/// # use imgui::*;
/// fn show_ui(ui: &Ui<'_>) {
///     ui.button("Hello, I am a drag source!");
///     
///     // Creates an empty DragSource with no tooltip
///     DragDropSource::new("BUTTON_DRAG").begin(ui);
/// }
/// ```
///
/// Notice especially the `"BUTTON_DRAG"` name -- this is the identifier of this
/// DragDropSource; [DragDropTarget]'s will specify an identifier to *receive*, and these
/// names must match up. A single item should only have one [DragDropSource], though
/// a target may have multiple different targets.
///
/// DropDropSources don't do anything until you use one of the three `begin_` methods
/// on this struct. Each of these methods describes how you handle the Payload which ImGui
/// will manage, and then give to a [DragDropTarget], which will received the payload. The
/// simplest and safest Payload is the empty payload, created with [begin](Self::begin).
#[derive(Debug)]
pub struct DragDropSource<T> {
    name: T,
    flags: DragDropFlags,
    cond: Condition,
}

impl<T: AsRef<str>> DragDropSource<T> {
    /// Creates a new [DragDropSource] with no flags and the `Condition::Always` with the given name.
    /// ImGui refers to this `name` field as a `type`, but really it's just an identifier to match up
    /// Source/Target for DragDrop.
    pub fn new(name: T) -> Self {
        Self {
            name,
            flags: DragDropFlags::empty(),
            cond: Condition::Always,
        }
    }

    /// Sets the flags on the [DragDropSource]. Only the flags `SOURCE_NO_PREVIEW_TOOLTIP`,
    /// `SOURCE_NO_DISABLE_HOVER`, `SOURCE_NO_HOLD_TO_OPEN_OTHERS`, `SOURCE_ALLOW_NULL_ID`,
    /// `SOURCE_EXTERN`, `SOURCE_AUTO_EXPIRE_PAYLOAD` make semantic sense, but any other flags will
    /// be accepted without panic.
    #[inline]
    pub fn flags(mut self, flags: DragDropFlags) -> Self {
        self.flags = flags;
        self
    }

    /// Sets the condition on the [DragDropSource]. Defaults to [Always](Condition::Always).
    #[inline]
    pub fn condition(mut self, cond: Condition) -> Self {
        self.cond = cond;
        self
    }

    /// Creates the source of a drag and returns a handle on the tooltip.
    /// This handle can be immediately dropped without binding it, in which case a default empty
    /// circle will be used for the "blank" tooltip as this item is being dragged around.
    ///
    /// Otherwise, use this tooltip to add data which will display as this item is dragged.
    /// If `SOURCE_NO_PREVIEW_TOOLTIP` is enabled, however, no preview will be displayed
    /// and this returned token does nothing. Additionally, a given target may use the flag
    /// `ACCEPT_NO_PREVIEW_TOOLTIP`, which will also prevent this tooltip from being shown.
    ///
    /// This drag has no payload, but is still probably the most useful way in imgui-rs to handle payloads.
    /// Using `once_cell` or some shared data, this pattern can be very powerful:
    ///
    /// ```no_run
    /// # use imgui::*;
    /// fn show_ui(ui: &Ui<'_>, drop_message: &mut Option<String>) {
    ///     ui.button("Drag me!");
    ///
    ///     let drag_drop_name = "Test Drag";
    ///     
    ///     // drag drop SOURCE
    ///     if DragDropSource::new(drag_drop_name).begin(ui).is_some() {
    ///         // warning -- this would allocate every frame if `DragDropSource` has
    ///         // condition `Always`, which it does by default. We're okay with that for
    ///         // this example, but real code probably wouldn't want to allocate so much.
    ///         *drop_message = Some("Test Payload".to_string());
    ///     }
    ///
    ///     ui.button("Target me!");
    ///
    ///     // drag drop TARGET
    ///     if let Some(target) = imgui::DragDropTarget::new(ui) {
    ///         if target
    ///             .accept_payload_empty(drag_drop_name, DragDropFlags::empty())
    ///             .is_some()
    ///         {
    ///             let msg = drop_message.take().unwrap();
    ///             assert_eq!(msg, "Test Payload");
    ///         }
    ///
    ///         target.pop();
    ///     }
    /// }
    /// ```
    ///
    /// In the above, you'll see how the payload is really just a message passing service.
    /// If you want to pass a simple integer or other "plain old data", take a look at
    /// [begin_payload](Self::begin_payload).
    #[inline]
    pub fn begin<'ui>(self, ui: &Ui<'ui>) -> Option<DragDropSourceToolTip<'ui>> {
        self.begin_payload(ui, ())
    }

    /// Creates the source of a drag and returns a handle on the tooltip.
    /// This handle can be immediately dropped without binding it, in which case a default empty
    /// circle will be used for the "blank" tooltip as this item is being dragged around.
    ///
    /// Otherwise, use this tooltip to add data which will display as this item is dragged.
    /// If `SOURCE_NO_PREVIEW_TOOLTIP` is enabled, however, no preview will be displayed
    /// and this returned token does nothing. Additionally, a given target may use the flag
    /// `ACCEPT_NO_PREVIEW_TOOLTIP`, which will also prevent this tooltip from being shown.
    ///
    /// This function also takes a payload in the form of `T: Copy + 'static`. ImGui will
    /// memcpy this data immediately to an internally held buffer, and will return the data
    /// to [DragDropTarget].
    ///
    /// ```no_run
    /// # use imgui::*;
    /// fn show_ui(ui: &Ui<'_>) {
    ///     ui.button("Drag me!");
    ///
    ///     let drag_drop_name = "Test Drag";
    ///     let msg_to_send = "hello there sailor";
    ///     
    ///     // drag drop SOURCE
    ///     if let Some(tooltip) = DragDropSource::new(drag_drop_name).begin_payload(ui, msg_to_send) {
    ///         ui.text("Sending message!");
    ///         tooltip.end();
    ///     }
    ///
    ///     ui.button("Target me!");
    ///
    ///     // drag drop TARGET
    ///     if let Some(target) = imgui::DragDropTarget::new(ui) {
    ///         if let Some(Ok(payload_data)) = target
    ///             .accept_payload::<&'static str, _>(drag_drop_name, DragDropFlags::empty())
    ///         {
    ///             let msg = payload_data.data;
    ///             assert_eq!(msg, msg_to_send);
    ///         }
    ///
    ///         target.pop();
    ///     }
    /// }
    /// ```
    #[inline]
    pub fn begin_payload<'ui, P: Copy + 'static>(
        self,
        ui: &Ui<'ui>,
        payload: P,
    ) -> Option<DragDropSourceToolTip<'ui>> {
        unsafe {
            let payload = TypedPayload::new(payload);
            self.begin_payload_unchecked(
                ui,
                &payload as *const _ as *const ffi::c_void,
                std::mem::size_of::<TypedPayload<P>>(),
            )
        }
    }

    /// Creates the source of a drag and returns a handle on the tooltip.
    /// This handle can be immediately dropped without binding it, in which case a default empty
    /// circle will be used for the "blank" tooltip as this item is being dragged around.
    ///
    /// Otherwise, use this tooltip to add data which will display as this item is dragged.
    /// If `SOURCE_NO_PREVIEW_TOOLTIP` is enabled, however, no preview will be displayed
    /// and this returned token does nothing. Additionally, a given target may use the flag
    /// `ACCEPT_NO_PREVIEW_TOOLTIP`, which will also prevent this tooltip from being shown.
    ///
    /// This function also takes a payload of any `*const T`. ImGui will
    /// memcpy this data immediately to an internally held buffer, and will return the data
    /// to [DragDropTarget].
    ///
    /// ## Safety
    /// This function itself will not cause a panic, but using it directly opts you into
    /// managing the lifetime of the pointer provided yourself. Dear ImGui will execute a memcpy on
    /// the data passed in with the size (in bytes) given, but this is, of course, just a copy,
    /// so if you pass in an `&String`, for example, the underlying String data will not be cloned,
    /// and could easily dangle if the `String` is dropped.
    ///
    /// Moreover, if `Condition::Always` is set (as it is by default), you will be copying in your data
    /// every time this function is ran in your update loop, which if it involves an allocating and then
    /// handing the allocation to ImGui, would result in a significant amount of data created.
    ///
    /// Overall, users should be very sure that this function is needed before they reach for it, and instead
    /// should consider either [begin](Self::begin) or [begin_payload](Self::begin_payload).
    #[inline]
    pub unsafe fn begin_payload_unchecked<'ui>(
        &self,
        ui: &Ui<'ui>,
        ptr: *const ffi::c_void,
        size: usize,
    ) -> Option<DragDropSourceToolTip<'ui>> {
        let should_begin = sys::igBeginDragDropSource(self.flags.bits() as i32);

        if should_begin {
            sys::igSetDragDropPayload(ui.scratch_txt(&self.name), ptr, size, self.cond as i32);

            Some(DragDropSourceToolTip::push())
        } else {
            None
        }
    }
}

/// A helper struct for RAII drag-drop support.
pub struct DragDropSourceToolTip<'ui>(PhantomData<Ui<'ui>>);

impl DragDropSourceToolTip<'_> {
    /// Creates a new tooltip internally.
    #[inline]
    fn push() -> Self {
        Self(PhantomData)
    }

    /// Ends the tooltip directly. You could choose to simply allow this to drop
    /// by not calling this, which will also be fine.
    #[inline]
    pub fn end(self) {
        // left empty to invoke drop...
    }
}

impl Drop for DragDropSourceToolTip<'_> {
    fn drop(&mut self) {
        unsafe { sys::igEndDragDropSource() }
    }
}

/// Creates a target for drag drop data out of the last ID created.
///
/// ```no_run
/// # use imgui::*;
/// fn show_ui(ui: &Ui<'_>) {
///     // Drop something on this button please!
///     ui.button("Hello, I am a drag Target!");
///     
///     if let Some(target) = DragDropTarget::new(ui) {
///         // accepting an empty payload (which is really just raising an event)
///         if let Some(_payload_data) = target.accept_payload_empty("BUTTON_DRAG", DragDropFlags::empty()) {
///             println!("Nice job getting on the payload!");
///         }
///    
///         // and we can accept multiple, different types of payloads with one drop target.
///         // this is a good pattern for handling different kinds of drag/drop situations with
///         // different kinds of data in them.
///         if let Some(Ok(payload_data)) = target.accept_payload::<usize, _>("BUTTON_ID", DragDropFlags::empty()) {
///             println!("Our payload's data was {}", payload_data.data);
///         }
///     }
/// }
/// ```
///
/// Notice especially the `"BUTTON_DRAG"` and `"BUTTON_ID"` name -- this is the identifier of this
/// DragDropTarget; [DragDropSource]s will specify an identifier when they send a payload, and these
/// names must match up. Notice how a target can have multiple acceptances on them -- this is a good
/// pattern to handle multiple kinds of data which could be passed around.
///
/// DropDropTargets don't do anything until you use one of the three `accept_` methods
/// on this struct. Each of these methods will spit out a _Payload struct with an increasing
/// amount of information on the Payload. The absolute safest solution is [accept_payload_empty](Self::accept_payload_empty).
#[derive(Debug)]
pub struct DragDropTarget<'ui>(&'ui Ui<'ui>);

impl<'ui> DragDropTarget<'ui> {
    /// Creates a new DragDropTarget, holding the [Ui]'s lifetime for the duration
    /// of its existence. This is required since this struct runs some code on its Drop
    /// to end the DragDropTarget code.
    #[doc(alias = "BeginDragDropTarget")]
    pub fn new(ui: &'ui Ui<'ui>) -> Option<Self> {
        let should_begin = unsafe { sys::igBeginDragDropTarget() };
        if should_begin {
            Some(Self(ui))
        } else {
            None
        }
    }

    /// Accepts an empty payload. This is the safest option for raising named events
    /// in the DragDrop API. See [DragDropSource::begin] for more information on how you
    /// might use this pattern.
    ///
    /// Note: If you began this operation with `begin_payload_unchecked` it always incorrect
    /// to use this function. Use `accept_payload_unchecked` instead
    pub fn accept_payload_empty(
        &self,
        name: impl AsRef<str>,
        flags: DragDropFlags,
    ) -> Option<DragDropPayloadEmpty> {
        self.accept_payload(name, flags)?
            .ok()
            .map(|payload_pod: DragDropPayloadPod<()>| DragDropPayloadEmpty {
                preview: payload_pod.preview,
                delivery: payload_pod.delivery,
            })
    }

    /// Accepts a payload with plain old data in it. This returns a Result, since you can specify any
    /// type. The sent type must match the return type (via TypeId) to receive an `Ok`.
    ///
    /// Note: If you began this operation with `begin_payload_unchecked` it always incorrect
    /// to use this function. Use `accept_payload_unchecked` instead
    pub fn accept_payload<T: 'static + Copy, Name: AsRef<str>>(
        &self,
        name: Name,
        flags: DragDropFlags,
    ) -> Option<Result<DragDropPayloadPod<T>, PayloadIsWrongType>> {
        let output = unsafe { self.accept_payload_unchecked(name, flags) };

        // convert the unsafe payload to our Result
        output.map(|unsafe_payload| {
            // sheering off the typeid...
            let received =
                unsafe { (unsafe_payload.data as *const TypedPayloadHeader).read_unaligned() };
            let expected = any::TypeId::of::<T>();

            if received.type_id == expected {
                let data =
                    unsafe { (unsafe_payload.data as *const TypedPayload<T>).read_unaligned() }
                        .data;
                Ok(DragDropPayloadPod {
                    data,
                    preview: unsafe_payload.preview,
                    delivery: unsafe_payload.delivery,
                })
            } else {
                Err(PayloadIsWrongType {
                    received,
                    expected: TypedPayloadHeader::new::<T>(),
                })
            }
        })
    }

    /// Accepts a drag and drop payload  which contains a raw pointer to [c_void](std::ffi::c_void)
    /// and a size in bytes. Users should generally avoid using this function
    /// if one of the safer variants is acceptable.
    ///
    /// ## Safety
    ///
    /// Because this pointer comes from ImGui, absolutely no promises can be made on its
    /// contents, alignment, or lifetime. Interacting with it is therefore extremely unsafe.
    /// **Important:** a special note needs to be made to the [ACCEPT_BEFORE_DELIVERY](DragDropFlags::ACCEPT_BEFORE_DELIVERY) flag --
    /// passing this flag will make this function return `Some(DragDropPayload)` **even before
    /// the user has actually "dropped" the payload by release their mouse button.**
    ///
    /// In safe functions, this works just fine, since the data can be freely copied
    /// (or doesn't exist at all!). However, if you are working with your own data, you must
    /// be extremely careful with this data, as you may, effectively, only have immutable access to it.
    ///
    /// Moreover, if the `DragDropSource` has also used `Condition::Once` or similar when they sent the data,
    /// ImGui will assume its data is still valid even after your preview, so corrupting that data could
    /// lead to all sorts of unsafe behavior on ImGui's side. In summary, using this function for any data
    /// which isn't truly `Copy` or "plain old data" is difficult, and requires substantial knowledge
    /// of the various edge cases.
    pub unsafe fn accept_payload_unchecked(
        &self,
        name: impl AsRef<str>,
        flags: DragDropFlags,
    ) -> Option<DragDropPayload> {
        let inner = sys::igAcceptDragDropPayload(self.0.scratch_txt(name), flags.bits() as i32);
        if inner.is_null() {
            None
        } else {
            let inner = *inner;

            // @fixme: there are actually other fields on `inner` which I have shorn -- they're
            // considered internal to imgui (such as id of who sent this), so i've left it for
            // now this way.
            Some(DragDropPayload {
                data: inner.Data,
                size: inner.DataSize as usize,
                preview: inner.Preview,
                delivery: inner.Delivery,
            })
        }
    }

    /// Ends the current target. Ironically, this doesn't really do anything in ImGui
    /// or in imgui-rs, but it might in the future.
    pub fn pop(self) {
        // omitted...exists just to run Drop.
    }
}

impl Drop for DragDropTarget<'_> {
    fn drop(&mut self) {
        unsafe { sys::igEndDragDropTarget() }
    }
}

/// An empty DragDropPayload. It has no data in it, and just includes
/// two bools with status information.
#[derive(Debug, Clone, Copy)]
#[non_exhaustive]
pub struct DragDropPayloadEmpty {
    /// Set when [`accept_payload_empty`](DragDropTarget::accept_payload_empty) was called
    /// and mouse has been hovering the target item.
    pub preview: bool,

    /// Set when [`accept_payload_empty`](DragDropTarget::accept_payload_empty) was
    /// called and mouse button is released over the target item.
    pub delivery: bool,
}

/// A DragDropPayload with status information and some POD, or plain old data,
/// in it.
#[derive(Debug, Copy, Clone)]
#[non_exhaustive]
pub struct DragDropPayloadPod<T: 'static + Copy> {
    /// The kind data which was requested.
    pub data: T,

    /// Set when [`accept_payload`](DragDropTarget::accept_payload) was called
    /// and mouse has been hovering the target item.
    pub preview: bool,

    /// Set when [`accept_payload`](DragDropTarget::accept_payload) was
    /// called and mouse button is released over the target item.
    pub delivery: bool,
}

#[derive(Debug)]
#[non_exhaustive]
pub struct DragDropPayload {
    /// Data which is copied and owned by ImGui. If you have accepted the payload, you can
    /// take ownership of the data; otherwise, view it immutably. Interacting with `data` is
    /// very unsafe.
    pub data: *const ffi::c_void,

    /// The size of the data in bytes.
    pub size: usize,

    /// Set when [`accept_payload_unchecked`](DragDropTarget::accept_payload_unchecked) was called
    /// and mouse has been hovering the target item.
    pub preview: bool,

    /// Set when [`accept_payload_unchecked`](DragDropTarget::accept_payload_unchecked) was
    /// called and mouse button is released over the target item. If this is set to false, then you
    /// set DragDropFlags::ACCEPT_BEFORE_DELIVERY and shouldn't mutate `data`.
    pub delivery: bool,
}

/// A typed payload.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
struct TypedPayload<T> {
    header: TypedPayloadHeader,
    data: T,
}

impl<T: Copy + 'static> TypedPayload<T> {
    /// Creates a new typed payload which contains this data.
    fn new(data: T) -> Self {
        Self {
            header: TypedPayloadHeader::new::<T>(),
            data,
        }
    }
}

/// A header for a typed payload.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Ord, PartialOrd)]
#[repr(C)]
struct TypedPayloadHeader {
    type_id: any::TypeId,
    #[cfg(debug_assertions)]
    type_name: &'static str,
}

impl TypedPayloadHeader {
    #[cfg(debug_assertions)]
    fn new<T: 'static>() -> Self {
        Self {
            type_id: any::TypeId::of::<T>(),
            type_name: any::type_name::<T>(),
        }
    }

    #[cfg(not(debug_assertions))]
    fn new<T: 'static>() -> Self {
        Self {
            type_id: any::TypeId::of::<T>(),
        }
    }
}

/// Indicates that an incorrect payload type was received. It is opaque,
/// but you can view useful information with Debug formatting when
/// `debug_assertions` are enabled.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Ord, PartialOrd)]
pub struct PayloadIsWrongType {
    expected: TypedPayloadHeader,
    received: TypedPayloadHeader,
}

#[cfg(debug_assertions)]
impl std::fmt::Display for PayloadIsWrongType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "Payload is {} -- expected {}",
            self.received.type_name, self.expected.type_name
        )
    }
}

#[cfg(not(debug_assertions))]
impl std::fmt::Display for PayloadIsWrongType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.pad("Payload is wrong type")
    }
}

impl std::error::Error for PayloadIsWrongType {}
