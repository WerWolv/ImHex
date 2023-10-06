function glfwSetCursorCustom(wnd, shape) {
    let body = document.getElementsByTagName("body")[0]
    switch (shape) {
        case 0x00036001: // GLFW_ARROW_CURSOR
            body.style.cursor = "default";
            break;
        case 0x00036002: // GLFW_IBEAM_CURSOR
            body.style.cursor = "text";
            break;
        case 0x00036003: // GLFW_CROSSHAIR_CURSOR
            body.style.cursor = "crosshair";
            break;
        case 0x00036004: // GLFW_HAND_CURSOR
            body.style.cursor = "pointer";
            break;
        case 0x00036005: // GLFW_HRESIZE_CURSOR
            body.style.cursor = "ew-resize";
            break;
        case 0x00036006: // GLFW_VRESIZE_CURSOR
            body.style.cursor = "ns-resize";
            break;
        default:
            body.style.cursor = "default";
            break;
    }

}

function glfwCreateStandardCursorCustom(shape) {
    return shape
}

var Module = {
    preRun:  [],
    postRun: [],
    onRuntimeInitialized: function() {
        // Triggered when the wasm module is loaded and ready to use.
        document.getElementById("loading_text").style.display = "none"
        document.getElementById("canvas").style.display = "initial"
    },
    print:   (function() { })(),
    printErr: function(text) { },
    canvas: (function() {
        let canvas = document.getElementById('canvas');
        // As a default initial behavior, pop up an alert when webgl context is lost. To make your
        // application robust, you may want to override this behavior before shipping!
        // See http://www.khronos.org/registry/webgl/specs/latest/1.0/#5.15.2
        canvas.addEventListener("webglcontextlost", function(e) { alert('WebGL context lost. You will need to reload the page.'); e.preventDefault(); }, false);

        return canvas;
    })(),
    setStatus: function(text) { },
    totalDependencies: 0,
    monitorRunDependencies: function(left) { },
    instantiateWasm: function(imports, successCallback) {
        imports.env.glfwSetCursor = glfwSetCursorCustom
        imports.env.glfwCreateStandardCursor = glfwCreateStandardCursorCustom
        instantiateAsync(wasmBinary, wasmBinaryFile, imports, (result) => successCallback(result.instance, result.module));
    }
};


window.addEventListener('resize', js_resizeCanvas, false);
function js_resizeCanvas() {
    let canvas = document.getElementById('canvas');
    canvas.width  = Math.min(document.documentElement.clientWidth, window.innerWidth || 0);
    canvas.height = Math.min(document.documentElement.clientHeight, window.innerHeight || 0);

    canvas.classList.add("canvas_full_screen")
}