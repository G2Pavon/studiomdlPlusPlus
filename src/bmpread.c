#include <string.h>
#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
#include <STDIO.H>
#endif

#ifndef _WIN32
#include <stdio.h>
#include <stdlib.h>

// __attribute__((packed)) on non-Intel arch may cause some unexpected error, plz be informed.

typedef struct tagBITMAPFILEHEADER
{
	uint16_t bfType;	  // 2  /* Magic identifier */
	uint32_t bfSize;	  // 4  /* File size in bytes */
	uint16_t bfReserved1; // 2
	uint16_t bfReserved2; // 2
	uint32_t bfOffBits;	  // 4 /* Offset to image data, bytes */
} __attribute__((packed)) BITMAPFILEHEADER;

typedef enum
{
	BI_RGB = 0x0000,
	BI_RLE8 = 0x0001,
	BI_RLE4 = 0x0002,
	BI_BITFIELDS = 0x0003,
	BI_JPEG = 0x0004,
	BI_PNG = 0x0005,
	BI_CMYK = 0x000B,
	BI_CMYKRLE8 = 0x000C,
	BI_CMYKRLE4 = 0x000D
} Compression;

typedef struct tagBITMAPINFOHEADER
{
	uint32_t biSize;		 // 4 /* Header size in bytes */
	int32_t biWidth;		 // 4 /* Width of image */
	int32_t biHeight;		 // 4 /* Height of image */
	uint16_t biPlanes;		 // 2 /* Number of colour planes */
	uint16_t biBitCount;	 // 2 /* Bits per pixel */
	uint32_t biCompression;	 // 4 /* Compression type */
	uint32_t biSizeImage;	 // 4 /* Image size in bytes */
	int32_t biXPelsPerMeter; // 4
	int32_t biYPelsPerMeter; // 4 /* Pixels per meter */
	uint32_t biClrUsed;		 // 4 /* Number of colours */
	uint32_t biClrImportant; // 4 /* Important colours */
} __attribute__((packed)) BITMAPINFOHEADER;

typedef struct tagRGBQUAD
{
	uint8_t rgbBlue;
	uint8_t rgbGreen;
	uint8_t rgbRed;
	uint8_t rgbReserved;
} RGBQUAD;
// for biBitCount is 16/24/32, it may be useless

#endif

int LoadBMP(const char *szFile, uint8_t **ppbBits, uint8_t **ppbPalette, int *width, int *height)
{
	int i, rc = 0;
	FILE *pfile = NULL;
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER bmih;
	RGBQUAD rgrgbPalette[256];
	uint32_t cbBmpBits;
	uint8_t *pbBmpBits;
	uint8_t *pb, *pbPal = NULL;
	uint32_t cbPalBytes;
	uint32_t biTrueWidth;

	// Bogus parameter check
	if (!(ppbPalette != NULL && ppbBits != NULL))
	{
		fprintf(stderr, "invalid BMP file\n");
		rc = -1000;
		goto GetOut;
	}

	// File exists?
	if ((pfile = fopen(szFile, "rb")) == NULL)
	{
		fprintf(stderr, "unable to open BMP file\n");
		rc = -1;
		goto GetOut;
	}

	// Read file header
	if (fread(&bmfh, sizeof bmfh, 1 /*count*/, pfile) != 1)
	{
		rc = -2;
		goto GetOut;
	}

	// Bogus file header check
	if (!(bmfh.bfReserved1 == 0 && bmfh.bfReserved2 == 0))
	{
		rc = -2000;
		goto GetOut;
	}

	// Read info header
	if (fread(&bmih, sizeof bmih, 1 /*count*/, pfile) != 1)
	{
		rc = -3;
		goto GetOut;
	}

	// Bogus info header check
	if (!(bmih.biSize == sizeof bmih && bmih.biPlanes == 1))
	{
		fprintf(stderr, "invalid BMP file header\n");
		rc = -3000;
		goto GetOut;
	}

	// Bogus bit depth?  Only 8-bit supported.
	if (bmih.biBitCount != 8)
	{
		fprintf(stderr, "BMP file not 8 bit\n");
		rc = -4;
		goto GetOut;
	}

	// Bogus compression?  Only non-compressed supported.
	if (bmih.biCompression != BI_RGB)
	{
		fprintf(stderr, "invalid BMP compression type\n");
		rc = -5;
		goto GetOut;
	}

	// Figure out how many entires are actually in the table
	if (bmih.biClrUsed == 0)
	{
		bmih.biClrUsed = 256;
		cbPalBytes = (1 << bmih.biBitCount) * sizeof(RGBQUAD);
	}
	else
	{
		cbPalBytes = bmih.biClrUsed * sizeof(RGBQUAD);
	}

	// Read palette (bmih.biClrUsed entries)
	if (fread(rgrgbPalette, cbPalBytes, 1 /*count*/, pfile) != 1)
	{
		rc = -6;
		goto GetOut;
	}

	// convert to a packed 768 byte palette
	pbPal = malloc(768);
	if (pbPal == NULL)
	{
		rc = -7;
		goto GetOut;
	}

	pb = pbPal;

	// Copy over used entries
	for (i = 0; i < (int)bmih.biClrUsed; i++)
	{
		*pb++ = rgrgbPalette[i].rgbRed;
		*pb++ = rgrgbPalette[i].rgbGreen;
		*pb++ = rgrgbPalette[i].rgbBlue;
	}

	// Fill in unused entires will 0,0,0
	for (i = bmih.biClrUsed; i < 256; i++)
	{
		*pb++ = 0;
		*pb++ = 0;
		*pb++ = 0;
	}

	// Read bitmap bits (remainder of file)
	cbBmpBits = bmfh.bfSize - ftell(pfile);
	pb = malloc(cbBmpBits);
	if (fread(pb, cbBmpBits, 1 /*count*/, pfile) != 1)
	{
		rc = -7;
		goto GetOut;
	}

	pbBmpBits = malloc(cbBmpBits);

	// data is actually stored with the width being rounded up to a multiple of 4
	biTrueWidth = (bmih.biWidth + 3) & ~3;

	// reverse the order of the data.
	pb += (bmih.biHeight - 1) * biTrueWidth;
	for (i = 0; i < bmih.biHeight; i++)
	{
		memmove(&pbBmpBits[biTrueWidth * i], pb, biTrueWidth);
		pb -= biTrueWidth;
	}

	pb += biTrueWidth;
	free(pb);

	*width = (uint16_t)bmih.biWidth;
	*height = (uint16_t)bmih.biHeight;
	// Set output parameters
	*ppbPalette = pbPal;
	*ppbBits = pbBmpBits;

GetOut:
	if (pfile)
		fclose(pfile);

	return rc;
}
