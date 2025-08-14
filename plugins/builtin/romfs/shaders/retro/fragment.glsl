#version 330 core
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D screenTexture;
uniform vec2 Resolution;   // Output resolution (pixels)
uniform float Time;        // Time in seconds

// Number of color levels per channel
uniform int colorLevels = 16; // 4-bit look

// Enable dithering (0 = off, 1 = on)
uniform int useDither = 1;

// Bayer 4x4 matrix for ordered dithering
float bayerDither4x4(int x, int y) {
    int index = (x & 3) + ((y & 3) << 2);
    int ditherMatrix[16] = int[16](
         0,  8,  2, 10,
        12,  4, 14,  6,
         3, 11,  1,  9,
        15,  7, 13,  5
    );
    return float(ditherMatrix[index]) / 16.0;
}

void main() {
    vec2 retroResolution = Resolution / 1.5;
    // Calculate pixelated coordinates
    vec2 pixelSize = 1.0 / retroResolution;
    vec2 uv = floor(TexCoords * retroResolution) * pixelSize;

    // Sample the scene at pixelated coordinates
    vec3 color = texture(screenTexture, uv).rgb;

    // Optional ordered dithering
    if (useDither == 1) {
        ivec2 pixelCoord = ivec2(floor(TexCoords * Resolution));
        float ditherValue = bayerDither4x4(pixelCoord.x, pixelCoord.y);
        color += (ditherValue - 0.5) / float(colorLevels); // small shift
    }

    // Quantize colors to limited palette
    color = floor(color * float(colorLevels)) / float(colorLevels - 1);

    // Optional slight flicker for retro screens
    color *= 1.0 + 0.02 * sin(Time * 60.0 + TexCoords.y * 200.0);

    FragColor = vec4(color, 1.0);
}
