/* see LICENSE for licensing details */
#version 460 core

out vec4 colour;

uniform uvec2 u_screen_dim;

vec2 map_mandelbrot(vec2 v)
{
	float x = v.x * 2.47 - 2;
	float y = v.y * 2.14 - 1.12;
	return vec2(x, y);
}

void main()
{
	vec2 xy0 = map_mandelbrot(gl_FragCoord.xy / u_screen_dim.xy);
	colour = vec4(0, 0, 0.1, 1);

	int i;
	float xx = 0, yy = 0;
	vec2 xy  = xy0;
	for (i = 0; i < 256 && xx + yy < 4.0; i++) {
		xx = xy.x * xy.x;
		yy = xy.y * xy.y;
		xy = vec2(xx - yy + xy0.x, 2 * xy.x * xy.y + xy0.y);
	}
	if (i < 256) {
		float r = (i >> 0) / 255.0;
		float g = (i >> 2) / 255.0;
		float b = (i >> 3) / 255.0;
		colour = vec4(r, g, b, 1.0);
	}
}
