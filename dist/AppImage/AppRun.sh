#!/bin/bash

BASE_DIR="$(dirname "$(readlink -f "${0}")")"

source "${BASE_DIR}/AppRun.env"

# GTK 3 does not consistently map the portal color scheme to a dark theme.
# Set it before GLFW initializes libdecor, unless the user already chose one.
if [[ -z "${GTK_THEME:-}" ]]; then
    color_scheme=""
    if command -v dbus-send >/dev/null 2>&1; then
        portal_setting=$(dbus-send --session --print-reply --reply-timeout=1000 \
            --dest=org.freedesktop.portal.Desktop \
            /org/freedesktop/portal/desktop \
            org.freedesktop.portal.Settings.Read \
            string:org.freedesktop.appearance string:color-scheme 2>/dev/null)

        case "${portal_setting}" in
            *"uint32 1"*) color_scheme="prefer-dark" ;;
            *"uint32 2"*) color_scheme="prefer-light" ;;
        esac
    fi

    if [[ -z "${color_scheme}" ]] && command -v gsettings >/dev/null 2>&1; then
        color_scheme=$(gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null)
    fi

    if [[ "${color_scheme}" = *"prefer-dark"* ]]; then
        gtk_theme="Adwaita"
        if command -v gsettings >/dev/null 2>&1; then
            configured_theme=$(gsettings get org.gnome.desktop.interface gtk-theme 2>/dev/null)
            configured_theme=${configured_theme#\'}
            configured_theme=${configured_theme%\'}
            [[ -n "${configured_theme}" ]] && gtk_theme="${configured_theme}"
        fi

        case "${gtk_theme}" in
            *[Dd]ark*|*:dark) ;;
            *) gtk_theme="${gtk_theme}:dark" ;;
        esac
        export GTK_THEME="${gtk_theme}"
    fi
fi

exec "${BASE_DIR}/usr/bin/imhex" "$@"
