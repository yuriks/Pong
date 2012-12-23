#version 330

in vec2 vf_tex_coord;

layout(location = 0) out vec4 out_color;

uniform sampler2D u_texture;

void main() {
	out_color = texture(u_texture, vf_tex_coord);
}
