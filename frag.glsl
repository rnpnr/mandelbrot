/* see LICENSE for licensing details */
#version 460 core

out vec4 colour;

uniform uvec2 u_screen_dim;
uniform vec2  u_top_left;
uniform vec2  u_bottom_right;

vec3 wavelength2rgb(float lambda)
{
	vec3 rgb = vec3(0);
	float t;
	/* red */
	if        (lambda >= 400 && lambda < 410) {
		t = (lambda - 400) / 10;
		rgb.x =        0.33 * t - (0.20 * t * t);
	} else if (lambda >= 410 && lambda < 475) {
		t = (lambda - 410) / 65;
		rgb.x = 0.14            - (0.13 * t * t);
	} else if (lambda >= 545 && lambda < 595) {
		t = (lambda - 545) / 50;
		rgb.x =        1.98 * t - (      t * t);
	} else if (lambda >= 595 && lambda < 650) {
		t = (lambda - 595) / 65;
		rgb.x = 0.98 + 0.06 * t - (0.40 * t * t);
	} else if (lambda >= 650 && lambda < 700) {
		t = (lambda - 650) / 50;
		rgb.x = 0.65 - 0.84 * t + (0.20 * t * t);
	}

	/* green */
	if        (lambda >= 415 && lambda < 475) {
		t = (lambda - 415) / 60;
		rgb.y =                 + (0.80 * t * t);
	} else if (lambda >= 475 && lambda < 585) {
		t = (lambda - 475) / 115;
		rgb.y = 0.80 + 0.76 * t - (0.80 * t * t);
	} else if (lambda >= 585 && lambda < 639) {
		t = (lambda - 585) / 54;
		rgb.y = 0.84 - 0.84 * t                 ;
	}

	/* blue */
	if        (lambda >= 400 && lambda < 475) {
		t = (lambda - 400) / 75;
		rgb.z =        2.20 * t - (1.50 * t * t);
	} else if (lambda >= 475 && lambda < 560) {
		t = (lambda - 475) / 85;
		rgb.z = 0.70 +        t + (0.30 * t * t);
	}
	return rgb;
}

vec2 map_mandelbrot(vec2 v)
{
	vec2 scale = abs(u_top_left - u_bottom_right);
	v *= scale;
	return vec2(u_top_left.x, u_bottom_right.y) + v;
}

void main()
{
	float aspect = u_screen_dim.x / u_screen_dim.y;
	vec2 xy0 = map_mandelbrot(gl_FragCoord.xy / u_screen_dim.xy);

	int i;
	float xx = 0, yy = 0;
	vec2 xy  = xy0;
	for (i = 0; i < 300 && xx + yy < 10.0; i++) {
		xx = xy.x * xy.x;
		yy = xy.y * xy.y;
		xy = vec2(xx - yy + xy0.x, 2 * xy.x * xy.y + xy0.y);
	}

	/* extra iterations to reduce error in escape calculation */
	for (int j = 0; j < 2; j++) {
		xx = xy.x * xy.x;
		yy = xy.y * xy.y;
		xy = vec2(xx - yy + xy0.x, 2 * xy.x * xy.y + xy0.y);
	}

	float mu = i - log(log(sqrt(xx + yy))) / log(2.0);
	mu = clamp(mu, 0, 300);
	//float q = pow(mu / 300, 0.2);
	float q = mu / 300;
	colour = vec4(wavelength2rgb(400.0 + 300 * q).xyz, 1.0);
}
