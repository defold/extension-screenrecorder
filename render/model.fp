varying mediump vec4 var_position;
varying mediump vec3 var_normal;
varying mediump vec2 var_texcoord0;

uniform lowp sampler2D tex0;
uniform lowp vec4 light;

void main() {
    vec4 color = texture2D(tex0, var_texcoord0.xy);

    // Diffuse light calculations
    vec3 ambient_light = vec3(0.5);
    vec3 diff_light = vec3(normalize(light.xyz - var_position.xyz));
    diff_light = max(dot(var_normal, diff_light), 0.0) + ambient_light;
    diff_light = clamp(diff_light, 0.0, 1.0);

    gl_FragColor = vec4(color.rgb * diff_light, 1.0);
}
