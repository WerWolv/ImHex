use proc_macro::TokenStream;
use quote::quote;
use syn::punctuated::Punctuated;
use syn::parse::{Parse, ParseStream};
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
    let args = syn::parse_macro_input!(attr as AttrList).0.into_iter().collect::<Vec<_>>();
    let plugin_name = &args[0];
    let author_name = &args[1];
    let description = &args[2];

    let function = syn::parse_macro_input!(item as syn::ItemFn);

    let pkg_name = std::env::var("CARGO_PKG_NAME").unwrap();
    let plugin_name_export = format!(
        "\u{1}_ZN3hex6plugin{}{}8internal13getPluginNameEv",
        pkg_name.len(),
        pkg_name
    );

    let plugin_author_export = format!(
        "\u{1}_ZN3hex6plugin{}{}8internal15getPluginAuthorEv",
        pkg_name.len(),
        pkg_name
    );

    let plugin_desc_export = format!(
        "\u{1}_ZN3hex6plugin{}{}8internal20getPluginDescriptionEv",
        pkg_name.len(),
        pkg_name
    );

    let plugin_init_export = format!(
        "\u{1}_ZN3hex6plugin{}{}8internal16initializePluginEv",
        pkg_name.len(),
        pkg_name
    );

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

        #[export_name = #plugin_init_export]
        pub extern "C" #function
    ).into()
}