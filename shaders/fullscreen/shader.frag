#version 450

layout(set = 0, binding = 0) uniform sampler2D rayResult;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = gl_FragCoord.xy / textureSize(rayResult, 0);
    vec3 hdr = texture(rayResult, uv).rgb;
    
    // Simple tonemapping
    vec3 ldr = hdr / (hdr + vec3(1.0));
    outColor = vec4(ldr, 1.0);
}
