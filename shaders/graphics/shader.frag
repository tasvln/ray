#version 450

layout(set = 0, binding = 0) uniform sampler2D rayOutput;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(rayOutput, 0));
    outColor = texture(rayOutput, uv);
}
