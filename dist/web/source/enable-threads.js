// NOTE: This file creates a service worker that cross-origin-isolates the page (read more here: https://web.dev/coop-coep/) which allows us to use wasm threads.
// Normally you would set the COOP and COEP headers on the server to do this, but Github Pages doesn't allow this, so this is a hack to do that.

/* Edited version of: coi-serviceworker v0.1.6 - Guido Zuidhof, licensed under MIT */
// From here: https://github.com/gzuidhof/coi-serviceworker
if(typeof window === 'undefined') {
    self.addEventListener("install", () => self.skipWaiting());
    self.addEventListener("activate", e => e.waitUntil(self.clients.claim()));

    async function handleFetch(request) {
        if(request.cache === "only-if-cached" && request.mode !== "same-origin") {
            return;
        }

        if(request.mode === "no-cors") { // We need to set `credentials` to "omit" for no-cors requests, per this comment: https://bugs.chromium.org/p/chromium/issues/detail?id=1309901#c7
            request = new Request(request.url, {
                cache: request.cache,
                credentials: "omit",
                headers: request.headers,
                integrity: request.integrity,
                destination: request.destination,
                keepalive: request.keepalive,
                method: request.method,
                mode: request.mode,
                redirect: request.redirect,
                referrer: request.referrer,
                referrerPolicy: request.referrerPolicy,
                signal: request.signal,
            });
        }

        let r = await fetch(request).catch(e => console.error(e));

        if(r.status === 0) {
            return r;
        }

        const headers = new Headers(r.headers);
        headers.set("Cross-Origin-Embedder-Policy", "require-corp"); // or: require-corp
        headers.set("Cross-Origin-Opener-Policy", "same-origin");

        return new Response(r.body, { status: r.status, statusText: r.statusText, headers });
    }

    self.addEventListener("fetch", function(e) {
        e.respondWith(handleFetch(e.request)); // respondWith must be executed synchonously (but can be passed a Promise)
    });

} else {
    (async function () {
        if (window.crossOriginIsolated !== false) return;

        if (!("serviceWorker" in navigator)) {
            console.warn("Service workers are not supported.");
            return;
        }

        const wasControlled = !!navigator.serviceWorker.controller;

        navigator.serviceWorker.addEventListener(
            "controllerchange",
            () => {
                if (!wasControlled) {
                    location.reload();
                }
            },
            { once: true }
        );

        try {
            const registration =
                await navigator.serviceWorker.register(document.currentScript.src);

            console.log(
                "COOP/COEP Service Worker registered",
                registration.scope
            );
        } catch (e) {
            console.error(
                "COOP/COEP Service Worker failed to register:",
                e
            );
        }
    })();
}

// Code to deregister:
// let registrations = await navigator.serviceWorker.getRegistrations();
// for(let registration of registrations) {
//   await registration.unregister();
// }