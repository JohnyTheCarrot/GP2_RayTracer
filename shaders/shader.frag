#version 450

layout (location = 0) in vec2 outUV;
layout (location = 0) out vec4 fragColor;

layout (set = 0, binding = 0) uniform sampler2D noisyTxt;

layout (push_constant) uniform shaderInformation {
    float aspectRatio;
}
pushc;

void main() {
    vec2 uv = outUV;
    float gamma = 2. / 2.2;
    fragColor = pow(texture(noisyTxt, uv).rgba, vec4(gamma));
    //    fragColor = vec4(1.f, 0.f, 0.f, 1.f);
}
