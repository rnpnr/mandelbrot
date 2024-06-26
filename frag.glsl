/* see LICENSE for licensing details */
#version 460 core

out vec4 colour;

uniform uvec2 u_screen_dim;
uniform vec2  u_top_left;
uniform vec2  u_bottom_right;

uniform vec2 u_z_n[300];
uniform bool u_use_approx = false;

const int   iterations    = 300;
const float escape_radius = 4.0;

/* input:  h [0,360] | s,v [0, 1] *
 * output: rgb [0,1]              */
vec3 hsv2rgb(vec3 hsv)
{
	vec3 k = mod(vec3(5, 3, 1) + hsv.x / 60, 6);
	k = max(min(min(k, 4 - k), 1), 0);
	return hsv.z - hsv.z * hsv.y * k;
}

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
	return vec2(u_top_left.x, u_bottom_right.y) + v * scale;
}

vec2 complex_mult(vec2 a, vec2 b)
{
	vec2 result;
	result.x = a.x * a.x - b.y * b.y;
	result.y = 2 * a.x * b.y;
	return result;
}

void main()
{
	vec2 vf = gl_FragCoord.xy / u_screen_dim.xy;
	vec2 d  = map_mandelbrot(vf);

	int i;
	vec2 z = u_use_approx ? u_z_n[0] : d;
	vec2 e = vec2(0);
	float xx = 0, yy = 0;

	if (u_use_approx) {
		for (i = 0; i < iterations && xx + yy < escape_radius; i++) {
			xx = z.x * z.x;
			yy = z.y * z.y;
			e = complex_mult(2 * u_z_n[i], e) + complex_mult(e, e) + d;
			z = u_z_n[i] + e;
		}
	} else {
		for (i = 0; i < iterations && xx + yy < escape_radius; i++) {
			xx = z.x * z.x;
			yy = z.y * z.y;
			z  = vec2(xx - yy + d.x, 2 * z.x * z.y + d.y);
		}
	}

	/* extra iterations to reduce error in escape calculation */
	/* fun value j = 5 */
	for (int j = 0; j < 2; j++) {
		xx = z.x * z.x;
		yy = z.y * z.y;
		z  = vec2(xx - yy + d.x, 2 * z.x * z.y + d.y);
	}

	float zmag = sqrt(xx + yy);
	float mu = i - log(log(zmag)) / log(2.0);
	mu = clamp(mu, 0, iterations);
	float q = pow(mu / iterations, 0.8);
	//float q = pow(mu / iterations, 0.69);
	//float q = mu / iterations;
	//colour = vec4(hsv2rgb(vec3(q, 0.8, 0.8)), 1.0);
	colour = vec4(wavelength2rgb(400.0 + 300 * q).xyz, 1.0);
}
