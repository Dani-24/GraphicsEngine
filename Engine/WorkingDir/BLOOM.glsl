#ifdef BLOOM

#if defined(VERTEX) ///////////////////////////////////////////////////



#elif defined(FRAGMENT) ///////////////////////////////////////////////////

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D scene;
uniform float brightnessThreshold;
uniform float offset;
uniform vec3 weights[5];

vec3 brightPass(vec3 color) {
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722)); // Luminance formula
    if (brightness > brightnessThreshold)
        return color;
    else
        return vec3(0.0);
}

vec3 gaussianBlur(sampler2D image, vec2 uv, float offset, vec3 weights[5]) {
    vec3 result = texture(image, uv).rgb * weights[0];
    for (int i = 1; i < 5; ++i)
    {
        result += texture(image, uv + vec2(offset * i, 0.0)).rgb * weights[i];
        result += texture(image, uv - vec2(offset * i, 0.0)).rgb * weights[i];
        result += texture(image, uv + vec2(0.0, offset * i)).rgb * weights[i];
        result += texture(image, uv - vec2(0.0, offset * i)).rgb * weights[i];
    }
    return result;
}

void main()
{
    // Apply bright-pass filter
    vec3 color = texture(scene, TexCoords).rgb;
    vec3 brightColor = brightPass(color);

    // Apply Gaussian blur
    vec3 blurredColor = gaussianBlur(scene, TexCoords, offset, weights);

    // Combine original scene with blurred bright areas
    vec3 bloomColor = blurredColor;
    vec3 sceneColor = color;

    FragColor = vec4(sceneColor + bloomColor, 1.0); // Additive blending
}

#endif
#endif