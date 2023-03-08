#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <complex.h>
#include "lodepng.h"

#define PI 3.14159265358979323846

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

const char cg3_string[] = ".cg3";
const char png_string[] = ".png";
const char source_string[] = "-SOURCE";
const char out_string[] = "-OUT";
const char scaled_string[] = "-SCALED";
const char magnitude_string[] = "-MAGNITUDE";
const char preview_string[] = "-PREVIEW";
const char debug_string[] = "-DEBUG";

uint8_t source_index = 0;
uint8_t out_index = 0;
uint8_t scaled_index = 0;
uint8_t magnitude_index = 0;
uint8_t preview_index = 0;
uint8_t debug_enable;

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

typedef struct PIXEL_IMAGE
{
	uint8_t** pixels_red;
	uint8_t** pixels_green;
	uint8_t** pixels_blue;
	unsigned int width;
	unsigned int height;
} pixel_image;

int str_comp_partial(const char* str1, const char* str2)
{
	for(int i = 0; str1[i] && str2[i]; ++i)
	{
		if(str1[i] != str2[i])
			return 0;
	}
	return 1;
}

void to_caps(char* str)
{
	unsigned int d = 0;
	char prev_char = 0x00;
	while(str[d])
	{
		//do not modifiy substrings that appear in quotes
		if(str[d] == 0x22)	//double quotes
		{
			while(1)
			{
				++d;
				if(!str[d])
					return;
				if(str[d] == 0x22 && prev_char != 0x5C)
					break;
				prev_char = str[d];
			}
		}
		if(str[d] == 0x27)	//single quotes
		{
			while(1)
			{
				++d;
				if(!str[d])
					return;
				if(str[d] == 0x27 && prev_char != 0x5C)
					break;
				prev_char = str[d];
			}
		}
		if(str[d] == ';')	//skip comments
			return;
		if((str[d] > 0x60) & (str[d] < 0x7B))
			str[d] = str[d] - 0x20;
		prev_char = str[d];
		++d;
	}
	return;
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
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y][x]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y + 1][x]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y + 1][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
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
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y][x]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y + 1][x]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y + 1][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
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

void split_image(uint8_t* input, unsigned int height, unsigned int width, uint8_t* red, uint8_t* green, uint8_t* blue, uint8_t* alpha)
{
	if(alpha)
	{
		for(unsigned int d = 0; d < height * width; ++d)
		{
			unsigned int pixel = d << 2;
			red[d] = input[pixel];
			green[d] = input[pixel + 1];
			blue[d] = input[pixel + 2];
			alpha[d] = input[pixel + 3];
		}
	}
	else
	{
		for(unsigned int d = 0; d < height * width; ++d)
		{
			unsigned int pixel = d << 2;
			red[d] = input[pixel];
			green[d] = input[pixel + 1];
			blue[d] = input[pixel + 2];
		}
	}
	return;
}

void merge_image(uint8_t* output, unsigned int height, unsigned int width, uint8_t* red, uint8_t* green, uint8_t* blue, uint8_t* alpha)
{
	if(alpha)
	{
		for(unsigned int d = 0; d < height * width; ++d)
		{
			unsigned int pixel = d << 2;
			output[pixel + 0] = red[d];
			output[pixel + 1] = green[d];
			output[pixel + 2] = blue[d];
			output[pixel + 3] = alpha[d];
		}
	}
	else
	{
		for(unsigned int d = 0; d < height * width; ++d)
		{
			unsigned int pixel = d << 2;
			output[pixel + 0] = red[d];
			output[pixel + 1] = green[d];
			output[pixel + 2] = blue[d];
			output[pixel + 3] = 255;
		}
	}
	return;
}

void image_to_complex(uint8_t* input, unsigned int input_height, unsigned int input_width, float complex* output, unsigned int output_height, unsigned int output_width)
{
	unsigned int out_index;
	unsigned int in_index;
	for(unsigned int d = 0; d < input_height; ++d)
	{
		for(unsigned int i = 0; i < input_width; ++i)
		{
			out_index = (output_width * d) + i;
			in_index = (input_width * d) + i;
			output[out_index] = input[in_index];
		}
		for(unsigned int i = input_width; i < output_width; ++i)
		{
			out_index = (output_width * d) + i;
			output[out_index] = 0.0;
		}
	}
	for(unsigned int d = input_height; d < output_height; ++d)
	{
		for(unsigned int i = 0; i < output_width; ++i)
		{
			out_index = (output_width * d) + i;
			output[out_index] = 0.0;
		}
	}
	return;
}

/*void complex_to_image(uint8_t* output, unsigned int output_height, unsigned int output_width, float complex* input, unsigned int input_height, unsigned int input_width)
{
	unsigned int out_index;
	unsigned int in_index;
	for(unsigned int d = 0; d < output_height; ++d)
	{
		for(unsigned int i = 0; i < output_width; ++i)
		{
			out_index = (output_width * d) + i;
			in_index = (input_width * d) + i;
			output[out_index] = (uint8_t)(MIN(255.0, MAX(0.0, (0.5 + crealf(input[in_index])))));
		}
	}
	return;
}*/

void complex_to_pixel_image(pixel_image output, float complex* input_red, float complex* input_green, float complex* input_blue)
{
	unsigned int in_index;
	for(unsigned int y = 0; y < output.height; ++y)
	{
		for(unsigned int x = 0; x < output.width; ++x)
		{
			in_index = (output.width * y) + x;
			output.pixels_red[y][x] = (uint8_t)(MIN(255.0, MAX(0.0, (0.5 + crealf(input_red[in_index])))));
			output.pixels_green[y][x] = (uint8_t)(MIN(255.0, MAX(0.0, (0.5 + crealf(input_green[in_index])))));
			output.pixels_blue[y][x] = (uint8_t)(MIN(255.0, MAX(0.0, (0.5 + crealf(input_blue[in_index])))));
		}
	}
	return;
}

void complex_to_magnitude_image(uint8_t* output, unsigned int height, unsigned int width, float complex* input)
{
	for(unsigned int d = 0; d < height; ++d)
	{
		for(unsigned int i = 0; i < width; ++i)
		{
			unsigned int y_index = (d + (height / 2)) % height;
			unsigned int x_index = (i + (width / 2)) % width;
			unsigned int in_index = width * d + i;
			unsigned int out_index = width * y_index + x_index;
			float magnitude = 128.0 + 23.0 * log(cabsf(input[in_index])/* / (float)(width * height)*/);
			output[out_index] = (uint8_t)(MIN(255.0, MAX(0.0, (0.5 + magnitude))));
		}
	}
	return;
}

void dft(float complex* input, unsigned int num_points, float complex* output)
{
	for(unsigned int freq = 0; freq < num_points; ++freq)
	{
		float complex temp = 0.0;
		for(unsigned int point = 0; point < num_points; ++point)
		{
			float exp = 2.0 * PI * (float)freq * ((float)point / (float)num_points);
			temp = temp + input[point] * (cos(exp) - I * sin(exp));
		}
		output[freq] = temp / (float)num_points;
	}
}

void idft(float complex* input, unsigned int num_points, float complex* output)
{
	for(unsigned int point = 0; point < num_points; ++point)
	{
		float complex temp = 0.0;
		for(unsigned int freq = 0; freq < num_points; ++freq)
		{
			float exp = 2.0 * PI * (float)freq * ((float)point / (float)num_points);
			temp = temp + input[freq] * (cos(exp) + I * sin(exp));
		}
		output[point] = temp;// / (float)num_points;
	}
}

void dft_2d(float complex* input, unsigned int input_height, unsigned int input_width, float complex* output)
{
	float complex* transformed = (float complex*)malloc(sizeof(float complex) * input_width * input_height);

	//transform the rows
	printf("DFT: Transforming rows\n");
	for(unsigned int d = 0; d < input_height; ++d)
	{
		unsigned int offset = input_width * d;
		dft(input + offset, input_width, transformed + offset);
	}

	//transpose the array
	printf("DFT: Transposing array\n");
	float complex* transposed = (float complex*)malloc(sizeof(float complex) * input_width * input_height);
	for(unsigned int d = 0; d < input_height; ++d)
	{
		for(unsigned int i = 0; i < input_width; ++i)
		{
			transposed[input_height * i + d] = transformed[input_width * d + i];
		}
	}

	//transform the columns
	printf("DFT: Transforming columns\n");
	for(unsigned int d = 0; d < input_width; ++d)
	{
		unsigned int offset = input_height * d;
		dft(transposed + offset, input_height, transformed + offset);
	}

	//transpose again
	printf("DFT: Transposing output\n");
	for(unsigned int d = 0; d < input_width; ++d)
	{
		for(unsigned int i = 0; i < input_height; ++i)
		{
			output[input_width * i + d] = transformed[input_height * d + i];
		}
	}

	free(transposed);
	free(transformed);
	return;
}

void idft_2d(float complex* input, unsigned int input_height, unsigned int input_width, float complex* output)
{
	float complex* transformed = (float complex*)malloc(sizeof(float complex) * input_width * input_height);

	//transform the rows
	printf("IDFT: Transforming rows\n");
	for(unsigned int d = 0; d < input_height; ++d)
	{
		unsigned int offset = input_width * d;
		idft(input + offset, input_width, transformed + offset);
	}

	//transpose the array
	printf("IDFT: Transposing array\n");
	float complex* transposed = (float complex*)malloc(sizeof(float complex) * input_width * input_height);
	for(unsigned int d = 0; d < input_height; ++d)
	{
		for(unsigned int i = 0; i < input_width; ++i)
		{
			transposed[input_height * i + d] = transformed[input_width * d + i];
		}
	}

	//transform the columns
	printf("IDFT: Transforming columns\n");
	for(unsigned int d = 0; d < input_width; ++d)
	{
		unsigned int offset = input_height * d;
		idft(transposed + offset, input_height, transformed + offset);
	}

	//transpose again
	printf("IDFT: Transposing output\n");
	for(unsigned int d = 0; d < input_width; ++d)
	{
		for(unsigned int i = 0; i < input_height; ++i)
		{
			output[input_width * i + d] = transformed[input_height * d + i];
		}
	}

	free(transposed);
	free(transformed);
	return;
}

void resize_dft_image(float complex* input, unsigned int input_height, unsigned int input_width, float complex* output, unsigned int output_height, unsigned int output_width)
{
	//find data size to copy
	unsigned int data_height = (output_height < input_height) ? output_height : input_height;
	unsigned int data_width = (output_width < input_width) ? output_width : input_width;

	unsigned int y_in;
	unsigned int y_out;
	unsigned int x_in;
	unsigned int x_out;

	//clear output
	for(unsigned int d = 0; d < (output_width * output_height); ++d)
	{
		output[d] = 0.0;
	}

	for(unsigned int y = 0; y < data_height / 2; ++y)
	{
		for(unsigned int x = 0; x < data_width / 2; ++x)
		{
			output[output_width * y + x] = input[input_width * y + x];
		}

		x_in = (data_width / 2) + (input_width - data_width);
		x_out = (data_width / 2) + (output_width - data_width);

		while(x_in < input_width)
		{
			output[output_width * y + x_out] = input[input_width * y + x_in];
			++x_in;
			++x_out;
		}
	}

	y_in = (data_height / 2) + (input_height - data_height);
	y_out = (data_height / 2) + (output_height - data_height);

	while(y_in < input_height)
	{
		for(unsigned int x = 0; x < data_width / 2; ++x)
		{
			output[output_width * y_out + x] = input[input_width * y_in + x];
		}

		x_in = (data_width / 2) + (input_width - data_width);
		x_out = (data_width / 2) + (output_width - data_width);

		while(x_in < input_width)
		{
			output[output_width * y_out + x_out] = input[input_width * y_in + x_in];
			++x_in;
			++x_out;
		}

		++y_in;
		++y_out;
	}

	return;
}

int main(int argc, char** argv)
{
	//Parse program arguments
	debug_enable = 0;
	unsigned int arg = 1;
	if(argc == 1)
	{
		printf("Usage: -SOURCE <source file> -OUT <output binary> -SCLAED <output scaled image> -MAGNITUDE <output magnitude image> -PREVIEW <output preview image> -DEBUG\n");
		printf("-SCALED -MAGNITUDE, -PREVIEW and -DEBUG are optional\n");
		exit(1);
	}
	while(arg < (unsigned int)argc)
	{
		if(argv[arg][0] == '-')
		{
			to_caps(argv[arg]);
			if(str_comp_partial(source_string, argv[arg]))
			{
				source_index = ++arg;
			}
			else if(str_comp_partial(out_string, argv[arg]))
			{
				out_index = ++arg;
			}
			else if(str_comp_partial(scaled_string, argv[arg]))
			{
				scaled_index = ++arg;
			}
			else if(str_comp_partial(magnitude_string, argv[arg]))
			{
				magnitude_index = ++arg;
			}
			else if(str_comp_partial(preview_string, argv[arg]))
			{
				preview_index = ++arg;
			}
			else if(str_comp_partial(debug_string, argv[arg]))
			{
				debug_enable = 0xFF;
			}
			++arg;
		}
		else
		{
			source_index = arg++;
		}
	}
	if(arg > (unsigned int)argc)
	{
		printf("Invalid arguments!\n");
		exit(1);
	}
	if(source_index == 0)
	{
		printf("No source file specified!\n");
		exit(1);
	}
	if(out_index == 0)
	{
		printf("No output file specified!\n");
		exit(1);
	}

	unsigned char* image;
	unsigned int width, height;

	unsigned error = lodepng_decode32_file(&image, &width, &height, argv[source_index]);
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

	uint8_t* input_red;
	uint8_t* input_green;
	uint8_t* input_blue;

	input_red = (uint8_t*)malloc(sizeof(uint8_t) * width * height);
	input_green = (uint8_t*)malloc(sizeof(uint8_t) * width * height);
	input_blue = (uint8_t*)malloc(sizeof(uint8_t) * width * height);

	split_image((uint8_t*)image, height, width, input_red, input_green, input_blue, NULL);
	free(image);

	float complex* cplx_source_red;
	float complex* cplx_source_green;
	float complex* cplx_source_blue;

	cplx_source_red = (float complex*)malloc(sizeof(float complex) * width * height);
	cplx_source_green = (float complex*)malloc(sizeof(float complex) * width * height);
	cplx_source_blue = (float complex*)malloc(sizeof(float complex) * width * height);
	printf("Created complex image\n");

	image_to_complex(input_red, height, width, cplx_source_red, height, width);
	image_to_complex(input_green, height, width, cplx_source_green, height, width);
	image_to_complex(input_blue, height, width, cplx_source_blue, height, width);
	free(input_red);
	free(input_green);
	free(input_blue);
	printf("Copied data to complex image\n");

	float complex* dft_red;
	float complex* dft_green;
	float complex* dft_blue;

	dft_red = (float complex*)malloc(sizeof(float complex) * width * height);
	dft_green = (float complex*)malloc(sizeof(float complex) * width * height);
	dft_blue = (float complex*)malloc(sizeof(float complex) * width * height);
	printf("Created blank DFT image\n");

	dft_2d(cplx_source_red, height, width, dft_red);
	dft_2d(cplx_source_green, height, width, dft_green);
	dft_2d(cplx_source_blue, height, width, dft_blue);
	free(cplx_source_red);
	free(cplx_source_green);
	free(cplx_source_blue);
	printf("Filled DFT image\n");

	if(magnitude_index)
	{
		uint8_t* magnitude_red;
		uint8_t* magnitude_green;
		uint8_t* magnitude_blue;

		magnitude_red = (uint8_t*)malloc(sizeof(uint8_t) * width * height);
		magnitude_green = (uint8_t*)malloc(sizeof(uint8_t) * width * height);
		magnitude_blue = (uint8_t*)malloc(sizeof(uint8_t) * width * height);

		complex_to_magnitude_image(magnitude_red, height, width, dft_red);
		complex_to_magnitude_image(magnitude_green, height, width, dft_green);
		complex_to_magnitude_image(magnitude_blue, height, width, dft_blue);
		printf("Created magnitude plot\n");

		uint8_t* magnitude_image;
		magnitude_image = (uint8_t*)malloc(sizeof(uint8_t) * height * width * 4);

		merge_image(magnitude_image, height, width, magnitude_red, magnitude_green, magnitude_blue, NULL);
		free(magnitude_red);
		free(magnitude_green);
		free(magnitude_blue);
		printf("Converted magnitude plot to RGBA\n");

		//TODO: enforce PNG file extension
		error = lodepng_encode32_file(argv[magnitude_index], magnitude_image, width, height);
		if(error)
		{
			printf("error %u: %s\n", error, lodepng_error_text(error));
			return 1;
		}
		printf("Wrote magnitude image\n");
		free(magnitude_image);
	}

	float complex* resized_dft_red;
	float complex* resized_dft_green;
	float complex* resized_dft_blue;

	unsigned int new_height = 192;
	unsigned int new_width = 256;

	resized_dft_red = (float complex*)malloc(sizeof(float complex) * new_width * new_height);
	resized_dft_green = (float complex*)malloc(sizeof(float complex) * new_width * new_height);
	resized_dft_blue = (float complex*)malloc(sizeof(float complex) * new_width * new_height);
	printf("Created new DFT image\n");

	resize_dft_image(dft_red, height, width, resized_dft_red, new_height, new_width);
	resize_dft_image(dft_green, height, width, resized_dft_green, new_height, new_width);
	resize_dft_image(dft_blue, height, width, resized_dft_blue, new_height, new_width);
	free(dft_red);
	free(dft_green);
	free(dft_blue);
	printf("Resized DFT image\n");

	float complex* ift_red;
	float complex* ift_green;
	float complex* ift_blue;

	ift_red = (float complex*)malloc(sizeof(float complex) * new_width * new_height);
	ift_green = (float complex*)malloc(sizeof(float complex) * new_width * new_height);
	ift_blue = (float complex*)malloc(sizeof(float complex) * new_width * new_height);
	printf("Created IFT image\n");

	idft_2d(resized_dft_red, new_height, new_width, ift_red);
	idft_2d(resized_dft_green, new_height, new_width, ift_green);
	idft_2d(resized_dft_blue, new_height, new_width, ift_blue);
	free(resized_dft_red);
	free(resized_dft_green);
	free(resized_dft_blue);
	printf("Filled IFT image\n");

	uint8_t* output_red;
	uint8_t* output_green;
	uint8_t* output_blue;

	//create scaled RGB image
	pixel_image scaled_image;
	create_pixel_image(&scaled_image, new_height, new_width);
	printf("Created new RGB image\n");

	complex_to_pixel_image(scaled_image, ift_red, ift_green, ift_blue);
	free(ift_red);
	free(ift_green);
	free(ift_blue);
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
	replace_file_extension(cg3_string, argv[out_index], new_name);
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

	unsigned int image_size;
	if(preview_index)
	{
		//create cg3 preview
		image_size = 4 * 192 * 256;
		unsigned char* rgba_cg3_preview = (unsigned char*)malloc(image_size * sizeof(unsigned char));
		cg3_to_rgba(rgba_cg3_preview, cg3_image);
		printf("Created CG3 preview\n");
		//TODO: enforce PNG file extension
		error = lodepng_encode32_file(argv[preview_index], rgba_cg3_preview, 256, 192);
		if(error)
		{
			printf("error %u: %s\n", error, lodepng_error_text(error));
			return 1;
		}
		free(rgba_cg3_preview);
		printf("Wrote CG3 preview\n");
	}

	if(scaled_index)
	{
		//convert RGB image to RGBA image
		image_size = 4 * scaled_image.height * scaled_image.width;
		unsigned char* output_image = (unsigned char*)malloc(image_size * sizeof(unsigned char));
		fill_RGBA_image(output_image, &scaled_image);
		delete_pixel_image(&scaled_image);
		printf("Converted RGB image to RGBA\n");

		//Write scaled image to file
		//TODO: enforce PNG file extension
		error = lodepng_encode32_file(argv[scaled_index], output_image, scaled_image.width, scaled_image.height);
		if(error)
		{
			printf("error %u: %s\n", error, lodepng_error_text(error));
			return 1;
		}
		free(output_image);
		printf("Wrote scaled image\n");
		return 0;
	}
}
