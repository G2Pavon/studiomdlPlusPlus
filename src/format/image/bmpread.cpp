#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "format/image/bmp.hpp"

int load_bmp(const char *szFile, uint8_t **ppbBits, uint8_t **ppbPalette, int *width, int *height)
{
	int i, rc = 0;
	FILE *pfile = nullptr;
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER bmih;
	RGBQUAD rgrgbPalette[256];
	uint32_t cbBmpBits;
	uint8_t *pbBmpBits;
	uint8_t *pb, *pbPal = nullptr;
	uint32_t cbPalBytes;
	uint32_t biTrueWidth;

	// Bogus parameter check
	if (!(ppbPalette != nullptr && ppbBits != nullptr))
	{
		fprintf(stderr, "invalid BMP file\n");
		return -1000;
	}

	// File exists?
	pfile = fopen(szFile, "rb");
	if (pfile == nullptr)
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
	if (pbPal == nullptr)
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
