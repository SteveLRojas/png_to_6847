#include <stdio.h>
#include <stdlib.h>
#include "stdint.h"
#include <math.h>
#include "lodepng.h"

#define PI 3.14159265358979323846

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static char cg3_string[] = ".cg3";

uint8_t CG3_PALETTE[] =
{
	0x00, 0xff, 0x00,	// GREEN
	0xff, 0xff, 0x00,	// YELLOW
	0x00, 0x00, 0xff,	// BLUE
	0xff, 0x00, 0x00	// RED
	//0xff, 0xff, 0xff,	// BUFF
	//0x00, 0xff, 0xff,	// CYAN
	//0xff, 0x00, 0xff,	// MAGENTA
	//0xff, 0x80, 0x00,	// ORANGE
};

typedef struct CG3_ELEMENT
{
	unsigned int rms_error[4];
	unsigned int min_rms_error;
	uint8_t source_offset_y;
	uint8_t source_offset_x;
} cg3_element;

typedef struct CG3_HEAP
{
	cg3_element* elements;
	unsigned int size;
} cg3_heap;

typedef struct DFT_IMAGE
{
	double** real_part_red;
	double** real_part_green;
	double** real_part_blue;
	double** imaginary_part_red;
	double** imaginary_part_green;
	double** imaginary_part_blue;
	unsigned int width;
	unsigned int height;
} dft_image;

typedef struct PIXEL_IMAGE
{
	uint8_t** pixels_red;
	uint8_t** pixels_green;
	uint8_t** pixels_blue;
	unsigned int width;
	unsigned int height;
} pixel_image;

void create_dft_image(dft_image* DFTImage, unsigned int height, unsigned int width)
{
	DFTImage->real_part_red = (double**)malloc(height * sizeof(double*));
	DFTImage->real_part_green = (double**)malloc(height * sizeof(double*));
	DFTImage->real_part_blue = (double**)malloc(height * sizeof(double*));
	DFTImage->imaginary_part_red = (double**)malloc(height * sizeof(double*));
	DFTImage->imaginary_part_green = (double**)malloc(height * sizeof(double*));
	DFTImage->imaginary_part_blue = (double**)malloc(height * sizeof(double*));
	for(unsigned int y = 0; y < height; ++y)
	{
		DFTImage->real_part_red[y] = (double*)malloc(width * sizeof(double));
		DFTImage->real_part_green[y] = (double*)malloc(width * sizeof(double));
		DFTImage->real_part_blue[y] = (double*)malloc(width * sizeof(double));
		DFTImage->imaginary_part_red[y] = (double*)malloc(width * sizeof(double));
		DFTImage->imaginary_part_green[y] = (double*)malloc(width * sizeof(double));
		DFTImage->imaginary_part_blue[y] = (double*)malloc(width * sizeof(double));
		for(unsigned int x = 0; x < width; ++x)
		{
			DFTImage->real_part_red[y][x] = 0.0;
			DFTImage->real_part_green[y][x] = 0.0;
			DFTImage->real_part_blue[y][x] = 0.0;
			DFTImage->imaginary_part_red[y][x] = 0.0;
			DFTImage->imaginary_part_green[y][x] = 0.0;
			DFTImage->imaginary_part_blue[y][x] = 0.0;
		}
	}
	DFTImage->width = width;
	DFTImage->height = height;
}

void delete_dft_image(dft_image* DFTImage)
{
	for(unsigned int y = 0; y < DFTImage->height; ++y)
	{
		free(DFTImage->real_part_red[y]);
		free(DFTImage->real_part_green[y]);
		free(DFTImage->real_part_blue[y]);
		free(DFTImage->imaginary_part_red[y]);
		free(DFTImage->imaginary_part_green[y]);
		free(DFTImage->imaginary_part_blue[y]);
	}
	free(DFTImage->real_part_red);
	free(DFTImage->real_part_green);
	free(DFTImage->real_part_blue);
	free(DFTImage->imaginary_part_red);
	free(DFTImage->imaginary_part_green);
	free(DFTImage->imaginary_part_blue);
}

void resize_dft_image(dft_image* DFTImage, unsigned int new_height, unsigned int new_width)
{
	//find data size to copy
	int data_height = (new_height < DFTImage->height) ? new_height : DFTImage->height;
	int data_width = (new_width < DFTImage->width) ? new_width : DFTImage->width;
	//create new image
	dft_image new_dft_image;
	create_dft_image(&new_dft_image, new_height, new_width);
	//copy data to new image
	int y_effective;
	int x_effective;
	int x_new_offset = new_width / 2;
	int y_new_offset = new_height / 2;
	int x_old_offset = (DFTImage->width) / 2;
	int y_old_offset = (DFTImage->height) / 2;
	int y_new;
	int y_old;
	int x_new;
	int x_old;
	for(int y = 0; y < data_height; ++y)
	{
		y_effective = y - (data_height / 2);
		y_new = y_effective + y_new_offset;
		y_old = y_effective + y_old_offset;
		for(int x = 0; x < data_width; ++x)
		{
			x_effective = x - (data_width / 2);
			x_new = x_effective + x_new_offset;
			x_old = x_effective + x_old_offset;

			new_dft_image.real_part_red[y_new][x_new] = DFTImage->real_part_red[y_old][x_old];
			new_dft_image.real_part_green[y_new][x_new] = DFTImage->real_part_green[y_old][x_old];
			new_dft_image.real_part_blue[y_new][x_new] = DFTImage->real_part_blue[y_old][x_old];
			new_dft_image.imaginary_part_red[y_new][x_new] = DFTImage->imaginary_part_red[y_old][x_old];
			new_dft_image.imaginary_part_green[y_new][x_new] = DFTImage->imaginary_part_green[y_old][x_old];
			new_dft_image.imaginary_part_blue[y_new][x_new] = DFTImage->imaginary_part_blue[y_old][x_old];
		}
	}
	//copy new data to old image
	delete_dft_image(DFTImage);
	DFTImage->real_part_red = new_dft_image.real_part_red;
	DFTImage->real_part_green = new_dft_image.real_part_green;
	DFTImage->real_part_blue = new_dft_image.real_part_blue;
	DFTImage->imaginary_part_red = new_dft_image.imaginary_part_red;
	DFTImage->imaginary_part_green = new_dft_image.imaginary_part_green;
	DFTImage->imaginary_part_blue = new_dft_image.imaginary_part_blue;
	DFTImage->width = new_width;
	DFTImage->height = new_height;
	return;
}

void create_pixel_image(pixel_image* input_image, unsigned int height, unsigned int width)
{
	input_image->pixels_red = (uint8_t**)malloc(height * sizeof(uint8_t*));
	input_image->pixels_green = (uint8_t**)malloc(height * sizeof(uint8_t*));
	input_image->pixels_blue = (uint8_t**)malloc(height * sizeof(uint8_t*));
	for(unsigned int y = 0; y < height; ++y)
	{
		input_image->pixels_red[y] = (uint8_t*)malloc(width * sizeof(uint8_t));
		input_image->pixels_green[y] = (uint8_t*)malloc(width * sizeof(uint8_t));
		input_image->pixels_blue[y] = (uint8_t*)malloc(width * sizeof(uint8_t));
		for(unsigned int x = 0; x < width; ++x)
		{
			input_image->pixels_red[y][x] = 0;
			input_image->pixels_green[y][x] = 0;
			input_image->pixels_blue[y][x] = 0;
		}
	}
	input_image->width = width;
	input_image->height = height;
	return;
}

void delete_pixel_image(pixel_image* input_image)
{
	for(unsigned int y = 0; y < input_image->height; ++y)
	{
		free(input_image->pixels_red[y]);
		free(input_image->pixels_green[y]);
		free(input_image->pixels_blue[y]);
	}
	free(input_image->pixels_red);
	free(input_image->pixels_green);
	free(input_image->pixels_blue);
	return;
}

void dft_2d(pixel_image* inImage, dft_image* DFTImage)
{
	double ak_red;	//real part accumulator
	double ak_green;
	double ak_blue;
	double bk_red;	//imaginary part accumulator
	double bk_green;
	double bk_blue;
	double real_multiplier;
	double imaginary_multiplier;
	double exponent_x;
	double exponent_y;
	double scaler = (double)inImage->width * (double)inImage->height;
	double width = (double)inImage->width;
	double height = (double)inImage->height;

	for(int y_freq = 0; y_freq < (int)(inImage->height); ++y_freq)	//spatial frequency along y
	{
		printf("DFT: Y Freq %d\n", y_freq);
 		for(int x_freq = 0; x_freq < (int)(inImage->width); ++x_freq)	//spatial frequency along x
 		{
			ak_red = 0.0;
			ak_green = 0.0;
			ak_blue = 0.0; 
			bk_red = 0.0;
			bk_green = 0.0;
			bk_blue = 0.0;
			for(int y_coord = 0; y_coord < (int)(inImage->height); ++y_coord)	//spatial coordinate y
			{
				exponent_y = 2.0 * PI * (double)(y_freq - (int)((inImage->height) / 2)) * (double)y_coord / height;
				for(int x_coord = 0; x_coord < (int)(inImage->width); ++x_coord)	//spatial coordinate x
				{
					exponent_x = 2.0 * PI * (double)(x_freq - (int)((inImage->width) / 2)) * (double)x_coord / width;
					real_multiplier = cos(exponent_x + exponent_y);
					imaginary_multiplier = sin(exponent_x + exponent_y);
					ak_red += (double)inImage->pixels_red[y_coord][x_coord] * real_multiplier;
					ak_green += (double)inImage->pixels_green[y_coord][x_coord] * real_multiplier;
					ak_blue += (double)inImage->pixels_blue[y_coord][x_coord] * real_multiplier;
					bk_red += (double)inImage->pixels_red[y_coord][x_coord] * imaginary_multiplier;
					bk_green += (double)inImage->pixels_green[y_coord][x_coord] * imaginary_multiplier;
					bk_blue += (double)inImage->pixels_blue[y_coord][x_coord] * imaginary_multiplier;
				}
			}
 			DFTImage->real_part_red[y_freq][x_freq] = ak_red / scaler;
 			DFTImage->real_part_green[y_freq][x_freq] = ak_green / scaler;
 			DFTImage->real_part_blue[y_freq][x_freq] = ak_blue / scaler;
 			DFTImage->imaginary_part_red[y_freq][x_freq] = bk_red / scaler;
 			DFTImage->imaginary_part_green[y_freq][x_freq] = bk_green / scaler;
 			DFTImage->imaginary_part_blue[y_freq][x_freq] = bk_blue / scaler;
		}
	}
}

void inverse_dft_2d(pixel_image* outImage, dft_image* DFTImage)
{
	double ak_red;	//real part accumulator
	double ak_green;
	double ak_blue;
	double bk_red;	//imaginary part accumulator
	double bk_green;
	double bk_blue;
	double real_multiplier;
	double imaginary_multiplier;
	double exponent_x;
	double exponent_y;
	double width = (double)DFTImage->width;
	double height = (double)DFTImage->height;

	for(int y_coord = 0; y_coord < (int)(DFTImage->height); ++y_coord)
	{
		printf("Inverse DFT Y coordinate: %u\n", y_coord);
		for(int x_coord = 0; x_coord < (int)(DFTImage->width); ++x_coord)
		{
			ak_red = 0.0;
			ak_green = 0.0;
			ak_blue = 0.0; 
			bk_red = 0.0;
			bk_green = 0.0;
			bk_blue = 0.0;
			for(int y_freq = 0; y_freq < (int)(DFTImage->height); ++y_freq)
			{
				exponent_y = 2.0 * PI * (double)(y_freq - (int)((DFTImage->height) / 2)) * (double)y_coord / height;
				for(int x_freq = 0; x_freq < (int)(DFTImage->width); ++x_freq)
				{
					exponent_x = 2.0 * PI * (double)(x_freq - (int)((DFTImage->width) / 2)) * (double)x_coord / width;
					real_multiplier = cos(exponent_x + exponent_y);
					imaginary_multiplier = sin(exponent_x + exponent_y);
					ak_red += DFTImage->real_part_red[y_freq][x_freq] * real_multiplier;
					ak_green += DFTImage->real_part_green[y_freq][x_freq] * real_multiplier;
					ak_blue += DFTImage ->real_part_blue[y_freq][x_freq] * real_multiplier;
					bk_red += DFTImage->imaginary_part_red[y_freq][x_freq] * imaginary_multiplier;
					bk_green += DFTImage->imaginary_part_green[y_freq][x_freq] * imaginary_multiplier;
					bk_blue += DFTImage->imaginary_part_blue[y_freq][x_freq] * imaginary_multiplier;
				}
			}
			outImage->pixels_red[y_coord][x_coord] = (uint8_t)(MIN(255.0, MAX(0.0, (0.5 + (ak_red + bk_red)))));
			outImage->pixels_green[y_coord][x_coord] = (uint8_t)(MIN(255.0, MAX(0.0, (0.5 + (ak_green + bk_green)))));
			outImage->pixels_blue[y_coord][x_coord] = (uint8_t)(MIN(255.0, MAX(0.0, (0.5 + (ak_blue + bk_blue)))));
		}
	}
}

void fill_magnitude_image(pixel_image* outImage, dft_image* DFTImage)
{
	double red_magnitude;
	double green_magnitude;
	double blue_magnitude;
	for(int y = 0; y < (int)(DFTImage->height); ++y)
	{
		for(int x = 0; x < (int)(DFTImage->width); ++x)
		{
			red_magnitude = 128.0 + 23.0 * log(sqrt(pow((DFTImage->real_part_red[y][x]), 2) + pow((DFTImage->imaginary_part_red[y][x]), 2)));	//logarithmic plot so we dont just get a black image...
			green_magnitude = 128.0 + 23.0 * log(sqrt(pow((DFTImage->real_part_green[y][x]), 2) + pow((DFTImage->imaginary_part_green[y][x]), 2)));
			blue_magnitude = 128.0 + 23.0 * log(sqrt(pow((DFTImage->real_part_blue[y][x]), 2) + pow((DFTImage->imaginary_part_blue[y][x]), 2)));
			outImage->pixels_red[y][x] = (uint8_t)(MIN(255.0, MAX(0.0, (0.5 + red_magnitude))));
			outImage->pixels_green[y][x] = (uint8_t)(MIN(255.0, MAX(0.0, (0.5 + green_magnitude))));
			outImage->pixels_blue[y][x] = (uint8_t)(MIN(255.0, MAX(0.0, (0.5 + blue_magnitude))));
		}
	}
}

void fill_RGBA_image(unsigned char* output_image, pixel_image* input_image)
{
	unsigned int image_size = 4 * input_image->height * input_image->width;
	unsigned int x;
	unsigned int y;
	for(unsigned int d = 0; d < image_size; d = d + 4)
	{
		x = (d >> 2) % input_image->width;
		y = (d >> 2) / input_image->width;
		output_image[d] = input_image->pixels_red[y][x];
		output_image[d + 1] = input_image->pixels_green[y][x];
		output_image[d + 2] = input_image->pixels_blue[y][x];
		output_image[d + 3] = 255;
	}
}

void create_cg3_elements(cg3_element* output_elements, pixel_image* input_image)
{
	unsigned int element_offset = 0;
	unsigned int rms_error;
	unsigned int min_rms_error;
	unsigned int diff_square;
	for(unsigned int y = 0; y < input_image->height; y = y + 2)
	{
		for(unsigned int x = 0; x < input_image->width; x = x + 2)
		{
			min_rms_error = 0xffffffff;
			for(unsigned int palette_index = 0; palette_index < 4; ++palette_index)
			{
				//compute total diff_square
				//red diff
				diff_square = (unsigned int)pow((double)((int)(input_image->pixels_red[y][x]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_red[y][x + 1]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_red[y + 1][x]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_red[y + 1][x + 1]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
				//green diff
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y][x]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y + 1][x]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y + 1][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
				//blue diff
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y][x]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y][x + 1]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y + 1][x]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y + 1][x + 1]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
				rms_error = (unsigned int)(sqrt((double)diff_square) + 0.5);
				output_elements[element_offset].rms_error[palette_index] = rms_error;
				if(rms_error < min_rms_error)
				{
					min_rms_error = rms_error;
				}
			}
			output_elements[element_offset].min_rms_error = min_rms_error;
			output_elements[element_offset].source_offset_y = y;
			output_elements[element_offset].source_offset_x = x;
			++element_offset;
		}
	}
}

void create_cg3_output(uint8_t* cg3_output, cg3_element* input_elements, pixel_image* input_image)
{
	unsigned int rms_error;
	unsigned int min_rms_error;
	unsigned int diff_square;
	unsigned int error_offset[4] = {0, 0, 0, 0};
	unsigned int x;
	unsigned int y;
	uint8_t best_match;
	uint8_t output_byte;
	uint8_t pair_offset;	//bit pair offset in output byte
	unsigned int output_offset;
	for(unsigned int element_offset = 0; element_offset < 12288; ++element_offset)
	{
		min_rms_error = (unsigned int)(-1);
		x = input_elements[element_offset].source_offset_x;
		y = input_elements[element_offset].source_offset_y;
		for(uint8_t palette_index = 0; palette_index < 4; ++palette_index)
		{
			//compute total diff_square
			//red diff
			diff_square = (unsigned int)pow((double)((int)(input_image->pixels_red[y][x]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_red[y][x + 1]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_red[y + 1][x]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_red[y + 1][x + 1]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
			//green diff
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y][x]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y + 1][x]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y + 1][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
			//blue diff
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y][x]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y][x + 1]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y + 1][x]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y + 1][x + 1]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
			rms_error = (unsigned int)(sqrt((double)diff_square) + 0.5);
			rms_error = rms_error + (error_offset[palette_index] >> 4);
			if(rms_error < min_rms_error)
			{
				min_rms_error = rms_error;
				best_match = palette_index;
			}
		}
		output_offset = (y >> 1) * 128 + (x >> 1);	//recover element index
		pair_offset = output_offset & 0x03;
		pair_offset = pair_offset ^ 0x03;
		pair_offset = pair_offset << 1;		//determine its position in the output byte
		output_offset = output_offset >> 2;	//determine the output byte offset
		output_byte = 0x03;
		output_byte = output_byte << pair_offset;
		output_byte = ~output_byte;		//create mask for output byte
		cg3_output[output_offset] = cg3_output[output_offset] & output_byte;	//mask off any existing data in corresponding position of output byte
		output_byte = best_match << pair_offset;	//align new data
		cg3_output[output_offset] = cg3_output[output_offset] | output_byte;	//write new data to corresponding output byte
		error_offset[best_match] = error_offset[best_match] + 1;	//tweak the increment for best results
		//low increments are better for images with very uniform color
	}
}

void cg3_heapify_down(cg3_heap* heap, unsigned int index)
{
	unsigned int left_index;
	unsigned int right_index;
	unsigned int largest;
	cg3_element temp;
	while(1)
	{
		left_index = index * 2 + 1;
		right_index = index * 2 + 2;
		largest = index;
		if((left_index < heap->size) && (heap->elements[left_index].min_rms_error > heap->elements[largest].min_rms_error))
			largest = left_index;
		if((right_index < heap->size) && (heap->elements[right_index].min_rms_error > heap->elements[largest].min_rms_error))
			largest = right_index;
		if(largest == index)
			return;
		temp = heap->elements[index];
		heap->elements[index] = heap->elements[largest];
		heap->elements[largest] = temp;
		index = largest;
	}
}

void cg3_heapify(cg3_heap* heap, cg3_element* elements, unsigned int size)
{
	heap->size = size;
	heap->elements = elements;
	for(unsigned int index = size - 1; index != (unsigned int)(-1); --index)
	{
		cg3_heapify_down(heap, index);
	}
	return;
}

void cg3_heapsort(cg3_element* elements, unsigned int size)
{
	cg3_element temp;
	cg3_heap heap;
	cg3_heapify(&heap, elements, size);
	for(unsigned int end = size - 1; end != (unsigned int)(-1); --end)
	{
		temp = elements[end];
		elements[end] = elements[0];
		elements[0] = temp;
		--size;
		cg3_heapify(&heap, elements, size);
	}
	return;
}

void cg3_to_rgba(unsigned char* output_image, uint8_t* input_image)
{
	unsigned int cg3_offset;
	uint8_t pair_offset;
	uint8_t palette_index;
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	unsigned int rgba_offset = 0;
	for(unsigned int y = 0; y < 192; ++y)
	{
		for(unsigned int x = 0; x < 256; ++x)
		{
			cg3_offset = (y >> 1) * 128 + (x >> 1);	//determine element offset
			pair_offset = cg3_offset & 0x03;
			pair_offset = pair_offset ^ 0x03;
			pair_offset = pair_offset << 1;		//determine the element position in the cg3 byte
			cg3_offset = cg3_offset >> 2;	//determine byte offset
			palette_index = 0x03 & (input_image[cg3_offset] >> pair_offset);
			//get colors
			red = CG3_PALETTE[palette_index * 3];
			green = CG3_PALETTE[palette_index * 3 + 1];
			blue = CG3_PALETTE[palette_index * 3 + 2];
			output_image[rgba_offset] = red;
			output_image[rgba_offset + 1] = green;
			output_image[rgba_offset + 2] = blue;
			output_image[rgba_offset + 3] = 255;
			rgba_offset = rgba_offset + 4;
		}
	}
}

void replace_file_extension(char* new_ext, char* out_name, char* new_name)
{
	uint8_t count = 0;
	uint8_t count_copy;
	//copy string and get its length
	do
	{
		new_name[count] = out_name[count];
		++count;
	}while(out_name[count]);
	count_copy = count;
	//search for a '.' starting from the end
	while(count)
	{
		count = count - 1;
		if(new_name[count] == '.')	//replace it and what follows with .mif
		{
			for(unsigned int d = 0; d < 5; ++d)
			{
				new_name[count] = new_ext[d];
				count = count + 1;
			}
			break;
		}
	}
	//check if the file name had no '.'
	if(count == 0)
	{
		count = count_copy;
		for(unsigned int d = 0; d < 5; ++d)
		{
			new_name[count] = new_ext[d];
			count = count + 1;
		}
	}
}

int main(int argc, char** argv)
{
	
	//Assert proper number of program arguments
	if(argc<3)
	{
		fprintf(stderr, "Specify input file and output file!\n");
		exit(1);
	}

	unsigned char* image;
	unsigned int width, height;

	unsigned error = lodepng_decode32_file(&image, &width, &height, argv[1]);
	if(error)
	{
		printf("error %u: %s\n", error, lodepng_error_text(error));
		return 1;
	}
	printf("Loaded image\n");
	printf("Width is: %u\n", width);
	printf("Height is: %u\n", height);
	printf("Image red channel at 0: %u\n", image[0]);
	printf("Image green channel at 0: %u\n", image[1]);
	printf("Image blue channel at 0: %u\n", image[2]);
	printf("Image alpha channel at 0: %d\n", image[3]);
	
	unsigned int image_size = 4 * width * height;

	//create RGB image
	pixel_image input_image;
	create_pixel_image(&input_image, height, width);
	printf("Created RGB image\n");

	//copy RGBA data to RGB image
	unsigned int x;
	unsigned int y;
	for(unsigned int d = 0; d < image_size; d = d + 4)
	{
		x = (d >> 2) % width;
		y = (d >> 2) / width;
		input_image.pixels_red[y][x] = image[d];
		input_image.pixels_green[y][x] = image[d + 1];
		input_image.pixels_blue[y][x] = image[d + 2];
	}
	free(image);
	printf("Copied data to RGB image\n");

	//create dft image
	dft_image DFTImage;
	create_dft_image(&DFTImage, height, width);
	printf("Created blank DFT image\n");

	//convert pixel image to dft image
	dft_2d(&input_image, &DFTImage);
	delete_pixel_image(&input_image);
	printf("Filled DFT image\n");

	if(argc > 3)
	{
		//create magnitude image
		pixel_image magnitude_image;
		create_pixel_image(&magnitude_image, DFTImage.height, DFTImage.width);
		fill_magnitude_image(&magnitude_image, &DFTImage);
		printf("Created magnitude plot\n");
		//convert RGB image to RGBA image
		image_size = 4 * magnitude_image.height * magnitude_image.width;
		unsigned char* RGBA_magnitude = (unsigned char*)malloc(image_size * sizeof(unsigned char));
		fill_RGBA_image(RGBA_magnitude, &magnitude_image);
		delete_pixel_image(&magnitude_image);
		printf("Converted magnitude plot to RGBA\n");
		//Write output image to file
		error = lodepng_encode32_file(argv[3], RGBA_magnitude, DFTImage.width, DFTImage.height);
		if(error)
		{
			printf("error %u: %s\n", error, lodepng_error_text(error));
			return 1;
		}
		free(RGBA_magnitude);
		printf("Wrote magnitude plot\n");
	}

	//scale dft image
	resize_dft_image(&DFTImage, 192, 256);
	printf("Resized DFT image\n");

	//create scaled RGB image
	pixel_image scaled_image;
	create_pixel_image(&scaled_image, DFTImage.height, DFTImage.width);
	printf("Created new RGB image\n");

	//reconstruct image from DFT
	inverse_dft_2d(&scaled_image, &DFTImage);
	delete_dft_image(&DFTImage);
	printf("Filled in new RGB image\n");

	//create cg3 elements
	cg3_element cg3_display_elements[12288];
	create_cg3_elements(cg3_display_elements, &scaled_image);
	cg3_heapsort(cg3_display_elements, 12288);
	printf("Created and sorted display elements\n");

	printf("Top 25 RMS errors:\n");
	for(unsigned int d = 0; d < 25; ++d)
	{
		printf("%u\n", cg3_display_elements[d].min_rms_error);
	}

	//create cg3 image
	uint8_t cg3_image[3072];
	create_cg3_output(cg3_image, cg3_display_elements, &scaled_image);
	printf("Created CG3 image\n");

	//write cg3 image
	char new_name[32];
	replace_file_extension(cg3_string, argv[2], new_name);
	size_t written = 0;
	FILE* f = fopen(new_name, "wb");
	while (written < 3072)
	{
		written += fwrite(cg3_image + written, sizeof(uint8_t), 3072 - written, f);
		if (written == 0) {
		    printf("Error writing output file!\n");
		}
	}
	fclose(f);
	printf("Wrote CG3 image\n");

	if(argc > 4)
	{
		//create cg3 preview
		image_size = 4 * 192 * 256;
		unsigned char* rgba_cg3_preview = (unsigned char*)malloc(image_size * sizeof(unsigned char));
		cg3_to_rgba(rgba_cg3_preview, cg3_image);
		printf("Created CG3 preview\n");
		error = lodepng_encode32_file(argv[4], rgba_cg3_preview, 256, 192);
		if(error)
		{
			printf("error %u: %s\n", error, lodepng_error_text(error));
			return 1;
		}
		free(rgba_cg3_preview);
		printf("Wrote CG3 preview\n");
	}

	//convert RGB image to RGBA image
	image_size = 4 * scaled_image.height * scaled_image.width;
	unsigned char* output_image = (unsigned char*)malloc(image_size * sizeof(unsigned char));
	fill_RGBA_image(output_image, &scaled_image);
	delete_pixel_image(&scaled_image);
	printf("Converted RGB image to RGBA\n");

	//Write output image to file
	error = lodepng_encode32_file(argv[2], output_image, scaled_image.width, scaled_image.height);
	if(error)
	{
		printf("error %u: %s\n", error, lodepng_error_text(error));
		return 1;
	}
	free(output_image);
	printf("Wrote output file\n");
	return 0;
}
