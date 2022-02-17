use proc_macro::TokenStream;
use quote::quote;
use syn::parse::{Parse, ParseStream};
use syn::punctuated::Punctuated;
use syn::LitStr;
use syn::Token;

struct AttrList(Punctuated<LitStr, Token![,]>);

impl Parse for AttrList {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        Punctuated::parse_terminated(input).map(Self)
    }
}

#[proc_macro_attribute]
pub fn plugin_setup(attr: TokenStream, item: TokenStream) -> TokenStream {
    let args = syn::parse_macro_input!(attr as AttrList)
        .0
        .into_iter()
        .collect::<Vec<_>>();
    let plugin_name = &args[0];
    let author_name = &args[1];
    let description = &args[2];

    let function = syn::parse_macro_input!(item as syn::ItemFn);

    let plugin_name_export = "getPluginName";
    let plugin_author_export = "getPluginAuthor";
    let plugin_desc_export = "getPluginDescription";
    let plugin_version_export = "getCompatibleVersion";
    let plugin_init_export = "initializePlugin";
    let plugin_set_imgui_ctx_export = "setImGuiContext";

    let imhex_version = std::env::var("IMHEX_VERSION").unwrap();

    quote!(
        #[export_name = #plugin_name_export]
        pub extern "C" fn plugin_name() -> *const u8 {
            concat!(#plugin_name, "\0").as_ptr()
        }

        #[export_name = #plugin_author_export]
        pub extern "C" fn plugin_author() -> *const u8 {
            concat!(#author_name, "\0").as_ptr()
        }

        #[export_name = #plugin_desc_export]
        pub extern "C" fn plugin_description() -> *const u8 {
            concat!(#description, "\0").as_ptr()
        }

        #[export_name = #plugin_set_imgui_ctx_export]
        pub unsafe extern "C" fn set_imgui_context(context: *mut ::hex::imgui::sys::ImGuiContext) {
            ::hex::imgui::sys::igSetCurrentContext(context);
        }

        #[export_name = #plugin_version_export]
        pub unsafe extern "C" fn plugin_version() -> *const u8 {
            concat!(#imhex_version, "\0").as_ptr()
        }

        #[export_name = #plugin_init_export]
        pub extern "C" #function
    )
    .into()
}
