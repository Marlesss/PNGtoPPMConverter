#include "return_codes.h"
#include <malloc.h>
#include <stdio.h>

int nameEquals(const char *first, const char *second)
{
	if (!first || !second)
	{
		return 0;
	}
	for (size_t i = 0; i < 4; i++)
	{
		if (first[i] != second[i])
		{
			return 0;
		}
	}
	return 1;
}

void handle_scanline(const unsigned char *line, unsigned char *decoded, unsigned char *buf, size_t width, size_t height, int bpp)
{
	int filter = line[0];
	for (size_t j = 0; j < width * bpp; j++)
	{
		unsigned char elem = line[j + 1];
		unsigned char result = 0;
		unsigned char left = j >= bpp ? buf[j - bpp] : 0, above = decoded[j], upperleft = (j >= bpp) ? decoded[j - bpp] : 0;
		if (filter == 0)
		{
			// None
			result = elem;
		}
		else if (filter == 1)
		{
			// Sub
			result = elem + left;
		}
		else if (filter == 2)
		{
			// Up
			result = elem + above;
		}
		else if (filter == 3)
		{
			// Average
			result = elem + ((int)left + (int)above) / 2;
		}
		else if (filter == 4)
		{
			// Paeth
			int p = (int)left + (int)above - (int)upperleft;
			int pleft = p - (int)left > 0 ? p - (int)left : -(p - (int)left);
			int pabove = p - (int)above > 0 ? p - (int)above : -(p - (int)above);
			int pupperleft = p - (int)upperleft > 0 ? p - (int)upperleft : -(p - (int)upperleft);
			if (pleft <= pabove && pleft <= pupperleft)
			{
				result = elem + left;
			}
			else if (pabove <= pupperleft)
			{
				result = elem + above;
			}
			else
			{
				result = elem + upperleft;
			}
		}
		buf[j] = result;
	}
	for (size_t j = 0; j < width * bpp; j++)
	{
		decoded[j] = buf[j];
	}
}

#if defined(ZLIB)
	#include <zlib.h>

int zlib_decode(unsigned char *source, size_t source_size, FILE *out, size_t width, size_t height, int bpp)
{
	size_t line_size = width * bpp + 1;
	unsigned char *line = malloc(sizeof(char) * line_size);
	if (line == NULL)
	{
		fprintf(stderr, "Not enough memory\n");
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	unsigned char *previous_decoded = malloc(sizeof(char) * width * bpp);
	if (previous_decoded == NULL)
	{
		fprintf(stderr, "Not enough memory\n");
		free(line);
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	for (size_t j = 0; j < width * bpp; j++)
		previous_decoded[j] = 0;
	unsigned char *buf = malloc(sizeof(char) * width * bpp);
	if (buf == NULL)
	{
		fprintf(stderr, "Not enough memory\n");
		free(line);
		free(previous_decoded);
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	int ret;
	z_stream strm;
	strm.zalloc = 0;
	strm.zfree = 0;
	strm.opaque = 0;
	strm.avail_in = 0;
	strm.next_in = 0;
	ret = inflateInit(&strm);
	if (ret != Z_OK)
	{
		free(line);
		free(previous_decoded);
		free(buf);
		return -1;
	}
	strm.avail_in = source_size;
	strm.next_in = source;
	for (size_t i = 0; i < height; i++)
	{
		// decode scanline
		strm.avail_out = line_size;
		strm.next_out = line;
		ret = inflate(&strm, Z_NO_FLUSH);
		if (strm.avail_out != 0)
		{
			ret = ERROR_INVALID_DATA;
		}
		// handle scanline
		handle_scanline(line, previous_decoded, buf, width, height, bpp);
		fwrite(previous_decoded, 1, width * bpp, out);
	}
	free(line);
	free(previous_decoded);
	free(buf);
	(void)inflateEnd(&strm);
	return ret == Z_STREAM_END ? 0 : ret;
}

int (*decode)(unsigned char *, size_t, FILE *, size_t, size_t, int) = zlib_decode;
#elif defined(LIBDEFLATE)
	#include <libdeflate.h>
int libdeflate_decode(unsigned char *source, size_t source_size, FILE *out, size_t width, size_t height, int bpp)
{
	unsigned char *previous_decoded = malloc(sizeof(char) * width * bpp);
	if (previous_decoded == NULL)
	{
		fprintf(stderr, "Not enough memory\n");
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	for (size_t j = 0; j < width * bpp; j++)
		previous_decoded[j] = 0;
	unsigned char *buf = malloc(sizeof(char) * width * bpp);
	if (buf == NULL)
	{
		fprintf(stderr, "Not enough memory\n");
		free(previous_decoded);
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	int ret;
	struct libdeflate_decompressor *d = libdeflate_alloc_decompressor();
	if (d == NULL)
	{
		fprintf(stderr, "Not enough memory\n");
		free(previous_decoded);
		free(buf);
		return -1;
	}
	size_t line_size = width * bpp + 1;
	size_t total_size = line_size * height;
	unsigned char *total = malloc(sizeof(char) * total_size);
	if (total == NULL)
	{
		fprintf(stderr, "Not enough memory\n");
		free(previous_decoded);
		free(buf);
		libdeflate_free_decompressor(d);
		return -1;
	}
	ret = libdeflate_zlib_decompress(d, source, source_size, total, total_size, NULL);
	for (size_t i = 0; i < height; i++)
	{
		handle_scanline(total + (line_size * i), previous_decoded, buf, width, height, bpp);
		fwrite(previous_decoded, 1, width * bpp, out);
	}
	free(previous_decoded);
	free(buf);
	free(total);
	libdeflate_free_decompressor(d);
	return 0;
}
int (*decode)(unsigned char *, size_t, FILE *, size_t, size_t, int) = libdeflate_decode;
#elif defined(ISAL)
	#include <include/igzip_lib.h>
int isal_decode(unsigned char *source, size_t source_size, FILE *out, size_t width, size_t height, int bpp)
{
	unsigned char *previous_decoded = malloc(sizeof(char) * width * bpp);
	if (previous_decoded == NULL)
	{
		fprintf(stderr, "Not enough memory\n");
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	for (size_t j = 0; j < width * bpp; j++)
		previous_decoded[j] = 0;
	unsigned char *buf = malloc(sizeof(char) * width * bpp);
	if (buf == NULL)
	{
		fprintf(stderr, "Not enough memory\n");
		free(previous_decoded);
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	struct inflate_state state;
	isal_inflate_init(&state);
	state.crc_flag = ISAL_GZIP_NO_HDR_VER;
	state.next_in = source;
	state.avail_in = source_size;
	size_t line_size = width * bpp + 1;
	size_t total_size = line_size * height;
	unsigned char *total = malloc(sizeof(char) * total_size);
	if (total == NULL)
	{
		fprintf(stderr, "Not enough memory\n");
		free(previous_decoded);
		free(buf);
		return -1;
	}
	state.avail_out = total_size;
	state.next_out = total;
	isal_inflate(&state);
	for (size_t i = 0; i < height; i++)
	{
		handle_scanline(total + (line_size * i), previous_decoded, buf, width, height, bpp);
		fwrite(previous_decoded, 1, width * bpp, out);
	}
	free(previous_decoded);
	free(buf);
	free(total);
	return 0;
}
int (*decode)(unsigned char *, size_t, FILE *, size_t, size_t, int) = isal_decode;
#else
	#error "You have to compile this program with one of the keys: ZLIB, LIBDEFLATE, ISAL"
#endif

unsigned char PNG_HEADER[] = { 137, 80, 78, 71, 13, 10, 26, 10 };

size_t byteToDec(const char *num, size_t from, size_t to)
{
	size_t dec = 0;
	for (size_t i = from; i < to; i++)
	{
		dec = dec * 256 + ((unsigned char)num[i]);
	}
	return dec;
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "Invalid arguments\nYou must run with arguments inputFileName outputFileName\n");
		return ERROR_INVALID_PARAMETER;
	}
	FILE *in = fopen(argv[1], "rb");
	if (in == NULL)
	{
		fprintf(stderr, "Can't open the input file\n");
		return ERROR_FILE_NOT_FOUND;
	}

	for (int i = 0; i < 8; i++)
	{
		if (getc(in) != PNG_HEADER[i])
		{
			fprintf(stderr, "Wrong type of file. You have to give the correct png file.\n");
			fclose(in);
			return ERROR_INVALID_DATA;
		}
	}
	size_t length;
	char name[5];
	char buf[4];
	fread(buf, 1, 4, in);
	if (byteToDec(buf, 0, 4) != 13)
	{
		fprintf(stderr, "IHDR length is not 13\n");
		fclose(in);
		return ERROR_INVALID_DATA;
	}
	fread(name, 1, 4, in);
	name[4] = 0;
	if (!nameEquals(name, "IHDR"))
	{
		fprintf(stderr, "Wrong type of file. You have to give the correct png file.\n");
		fclose(in);
		return ERROR_INVALID_DATA;
	}
	fread(buf, 1, 4, in);
	size_t width = byteToDec(buf, 0, 4);
	fread(buf, 1, 4, in);
	size_t height = byteToDec(buf, 0, 4);
	unsigned int bit_depth = getc(in);
	unsigned int type_of_color = getc(in);
	unsigned int compression = getc(in);
	unsigned int filter = getc(in);
	unsigned int interlace = getc(in);
	int bpp = (type_of_color == 0 ? 1 : 3);
	size_t count_of_bytes = width * height * bpp;
	fseek(in, 4, SEEK_CUR);
	if (!(type_of_color == 0 || type_of_color == 2) || bit_depth != 8 || compression || filter || interlace)
	{
		fprintf(stderr, "Unsupported file type\n");
		fclose(in);
		return ERROR_INVALID_DATA;
	}
	int wasIDAT = 0, IDATended = 0;
	size_t raw_data_offset = 0;
	size_t raw_data_size = count_of_bytes;
	unsigned char *raw_data = malloc(sizeof(char) * raw_data_size);
	if (raw_data == NULL)
	{
		fprintf(stderr, "Not enough memory\n");
		fclose(in);
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	do
	{
		fread(buf, 1, 4, in);
		length = byteToDec(buf, 0, 4);
		fread(name, 1, 4, in);
		name[4] = 0;
		if (nameEquals(name, "IDAT"))
		{
			if (IDATended)
			{
				fprintf(stderr, "Got another chunk in the sequence of IDAT\n");
				fclose(in);
				free(raw_data);
				return ERROR_INVALID_DATA;
			}
			wasIDAT = 1;
			if (raw_data_offset + length >= raw_data_size)
			{
				size_t new_size = raw_data_offset + length;
				unsigned char *new_data = realloc(raw_data, new_size);
				if (new_data == NULL)
				{
					fprintf(stderr, "Not enough memory\n");
					fclose(in);
					free(raw_data);
					return ERROR_NOT_ENOUGH_MEMORY;
				}
				raw_data = new_data;
			}
			if (fread(raw_data + raw_data_offset, 1, length, in) != length)
			{
				fprintf(stderr, "Can't read IDAT data\n");
				fclose(in);
				free(raw_data);
				return ERROR_INVALID_DATA;
			}
			raw_data_offset += length;
		}
		else
		{
			IDATended = wasIDAT;
			if (!nameEquals(name, "IEND") && 65 <= name[0] && name[0] <= 90)
			{
				fprintf(stderr, "Chunk %s is not supported\n", name);
				fclose(in);
				free(raw_data);
				return ERROR_INVALID_DATA;
			}
			fseek(in, length, SEEK_CUR);
		}
		fseek(in, 4, SEEK_CUR);
	} while (!nameEquals(name, "IEND") && !feof(in));
	fclose(in);
	if (!nameEquals(name, "IEND"))
	{
		fprintf(stderr, "Waited for IEND-chunk, got EOF\n");
		free(raw_data);
		return ERROR_INVALID_DATA;
	}
	if (length != 0)
	{
		fprintf(stderr, "Length of IEND is not equal to 0\n");
		free(raw_data);
		return ERROR_INVALID_DATA;
	}
	if (!wasIDAT || !raw_data_offset)
	{
		fprintf(stderr, "No IDAT data\n");
		free(raw_data);
		return ERROR_INVALID_DATA;
	}

	FILE *out = fopen(argv[2], "wb");
	if (out == NULL)
	{
		fprintf(stderr, "Can't open the output file\n");
		free(raw_data);
		return ERROR_FILE_NOT_FOUND;
	}
	fprintf(out, "P%d\n%d %d\n255\n", type_of_color == 0 ? 5 : 6, width, height);
	// decode deflate_data
	int ret = decode(raw_data, raw_data_offset, out, width, height, bpp);
	fclose(out);
	free(raw_data);
	if (ret != 0)
	{
		fprintf(stderr, "Error while decoding\n");
		return ERROR_INVALID_DATA;
	}
	return ERROR_SUCCESS;
}

/*
 * isa-l хочет знать размер декодированных данных, поэтому частично не получится
 * handle_scanline без буфера
 * добавить i в handle_scanline и избавиться от заполнения previous_decode
 */