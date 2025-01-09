#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// __attribute__((packed)) on non-Intel arch may cause some unexpected error, plz be informed.
#pragma pack(push, 1)

struct BITMAPFILEHEADER
{
	uint16_t bfType;	  // 2  /* Magic identifier */
	uint32_t bfSize;	  // 4  /* File size in bytes */
	uint16_t bfReserved1; // 2
	uint16_t bfReserved2; // 2
	uint32_t bfOffBits;	  // 4 /* Offset to image data, bytes */
};

enum Compression
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
};

struct BITMAPINFOHEADER
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
};

struct RGBQUAD
{
	uint8_t rgbBlue;
	uint8_t rgbGreen;
	uint8_t rgbRed;
	uint8_t rgbReserved;
};
// for biBitCount is 16/24/32, it may be useless

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
		return -1000;
	}

	// File exists?
	pfile = fopen(szFile, "rb");
	if (pfile == NULL)
	{
		fprintf(stderr, "unable to open BMP file\n");
		return -1;
	}

	// Read file header
	if (fread(&bmfh, sizeof bmfh, 1, pfile) != 1)
	{
		fclose(pfile);
		return -2;
	}

	// Bogus file header check
	if (!(bmfh.bfReserved1 == 0 && bmfh.bfReserved2 == 0))
	{
		fclose(pfile);
		return -2000;
	}

	// Read info header
	if (fread(&bmih, sizeof bmih, 1, pfile) != 1)
	{
		fclose(pfile);
		return -3;
	}

	// Bogus info header check
	if (!(bmih.biSize == sizeof bmih && bmih.biPlanes == 1))
	{
		fprintf(stderr, "invalid BMP file header\n");
		fclose(pfile);
		return -3000;
	}

	// Bogus bit depth?  Only 8-bit supported.
	if (bmih.biBitCount != 8)
	{
		fprintf(stderr, "BMP file not 8 bit\n");
		fclose(pfile);
		return -4;
	}

	// Bogus compression?  Only non-compressed supported.
	if (bmih.biCompression != BI_RGB)
	{
		fprintf(stderr, "invalid BMP compression type\n");
		fclose(pfile);
		return -5;
	}

	// Figure out how many entries are actually in the table
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
	if (fread(rgrgbPalette, cbPalBytes, 1, pfile) != 1)
	{
		fclose(pfile);
		return -6;
	}

	// Convert to a packed 768-byte palette
	pbPal = (uint8_t *)malloc(768);
	if (pbPal == NULL)
	{
		fclose(pfile);
		return -7;
	}

	pb = pbPal;

	// Copy over used entries
	for (i = 0; i < static_cast<int>(bmih.biClrUsed); i++)
	{
		*pb++ = rgrgbPalette[i].rgbRed;
		*pb++ = rgrgbPalette[i].rgbGreen;
		*pb++ = rgrgbPalette[i].rgbBlue;
	}

	// Fill in unused entries with 0,0,0
	for (i = bmih.biClrUsed; i < 256; i++)
	{
		*pb++ = 0;
		*pb++ = 0;
		*pb++ = 0;
	}

	// Read bitmap bits (remainder of file)
	cbBmpBits = bmfh.bfSize - ftell(pfile);
	pb = (uint8_t *)malloc(cbBmpBits);
	if (fread(pb, cbBmpBits, 1, pfile) != 1)
	{
		free(pb);
		fclose(pfile);
		return -7;
	}

	pbBmpBits = (uint8_t *)malloc(cbBmpBits);

	// Data is actually stored with the width being rounded up to a multiple of 4
	biTrueWidth = (bmih.biWidth + 3) & ~3;

	// Reverse the order of the data
	pb += (bmih.biHeight - 1) * biTrueWidth;
	for (i = 0; i < bmih.biHeight; i++)
	{
		std::memcpy(&pbBmpBits[biTrueWidth * i], pb, biTrueWidth);
		pb -= biTrueWidth;
	}

	pb += biTrueWidth;
	free(pb);

	*width = (uint16_t)bmih.biWidth;
	*height = (uint16_t)bmih.biHeight;

	// Set output parameters
	*ppbPalette = pbPal;
	*ppbBits = pbBmpBits;

	fclose(pfile);
	return rc;
}
