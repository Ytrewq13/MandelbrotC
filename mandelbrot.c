# include "png_maker.h"
# include <complex.h>

# define MAX_ITER 128
# define MY_INFINITY 16

# define IMG_WIDTH 1280
# define IMG_HEIGHT 800
# define X_MIN -2.5
# define X_MAX 1.5

# define HUE_OFFSET 0

pixel_t * get_color(double x, double y) {
	// calculate the color of a pixel with complex value x, y
	double complex number = 0;
	int iterations = 0;
	while (pow(creal(number), 2) + pow(cimag(number), 2) < MY_INFINITY && iterations < MAX_ITER) {
		iterations++;
		//number = cpow(number, 2) + x + y * I;
        number = cpow(number, 2) + CMPLX(x,y);
	}

	// normalize between 0 and 360 for hue.
	double hue = 360 * (double) iterations / (double) MAX_ITER;
	struct HSV hsv = {hue, 1.0, 1.0};
	struct RGB rgb = HSVToRGB(hsv);

	pixel_t * pixel = (pixel_t*)malloc(sizeof(pixel_t));
	pixel->red = (int) rgb.R;
	pixel->green = (int) rgb.G;
	pixel->blue = (int) rgb.B;
	if (iterations == MAX_ITER) {
		pixel->red = pixel->green = pixel->blue = 0;
	}
	return pixel;
}

int main(int argc, char ** argv) {
	int width = IMG_WIDTH;
	int height = IMG_HEIGHT;
	double min_x = X_MIN;
	double max_x = X_MAX;
	double min_y = (max_x - min_x) * -0.5 * height/width;
	double max_y = (max_x - min_x) * 0.5 * height/width;
	// Create the png.
	bitmap_t image;
	int x;
	int y;
	image.width = width;
	image.height = height;

	image.pixels = calloc(image.width * image.height, sizeof(pixel_t));

	if (! image.pixels) {
		return -1;
	}

	for (y = 0; y < image.height; y++) {
		if (y % 20 == 0) printf("Rendering row %d...\n", y);
		for (x = 0; x < image.width; x++) {
			pixel_t * pixel = pixel_at(& image, x, y);
			double x_coord = ((double) x / (double) image.width) * (max_x - min_x) + min_x;
			double y_coord = ((double) y / (double) image.height) * (max_y - min_y) + min_y;
			pixel_t * color = get_color(x_coord, y_coord);
			*pixel = *color; // This is probably wrong, but it works.
			free(color);
		}
	}

	save_png_to_file(&image, "image.png");
	free(image.pixels);
	return 0;
}



























// Whitespace.
