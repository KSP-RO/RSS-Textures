#pragma once

#include <stdint.h>

struct TiffFile_t
{
	uint32_t width;
	uint32_t height;
	float* pixels;
};

bool ReadTiff(TiffFile_t& tiffFile, const char* filename);
