#version 450
layout(location = 0) in flat uvec2 i_TexCoord;
layout(location = 1) in vec3 i_Normal;
layout(location = 2) in flat vec3 i_Color;
layout(location = 0) out vec4 o_Color;
layout(std140, binding = 2) uniform Data {
    vec4 colors[6];
}
d_Data;
void main() {
    uint i = i_TexCoord.x;
    float d = dot(normalize(i_Normal), normalize(vec3(0.5, 1.0, 0.25)));
    float light = 0.1 + 0.9 * clamp(d, 0.0, 1.0);
    o_Color = vec4(light * i_Color.rgb, 1.0);
    // o_Color = vec4(i_Position + vec3(0.5, 0.5, 0.5), 1.0);
}