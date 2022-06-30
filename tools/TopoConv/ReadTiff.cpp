
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <io.h>

#include "ReadTiff.h"

class AutoCloseFile
{
	FILE* fp;
public:
	AutoCloseFile(FILE* f) : fp(f) {}
	~AutoCloseFile() { fclose(fp); }
};

bool ReadTiff(TiffFile_t& tiffFile, const char* filename)
{
	FILE* fp = nullptr;
	if (fopen_s(&fp, filename, "rb") != 0)
		return false;
	AutoCloseFile acf(fp);

	uint8_t header[8];
	if (fread(header, 1, 8, fp) != 8)
		return false;

	bool bigendian;
	if (header[0] == 'I' && header[1] == 'I' && header[2] == 42 && header[3] == 0)
		bigendian = false;
	else if (header[0] == 'M' && header[1] == 'M' && header[2] == 0 && header[3] == 42)
		bigendian = true;
	else
	{
		return false;
	}

	tiffFile.pixels = nullptr;

	const auto& convertU32 = [bigendian](uint32_t x) -> uint32_t { if (bigendian) return _byteswap_ulong(x); else return x; };
	const auto& convertU16 = [bigendian](uint16_t x) -> uint16_t { if (bigendian) return _byteswap_ushort(x); else return x; };

	uint32_t nextIFD = convertU32(*reinterpret_cast<uint32_t*>(header + 4));

	uint32_t bpp = 8;
	uint32_t* stripoffsets = nullptr;
	uint32_t* stripbytes = nullptr;
	uint32_t stripcount = 0;
	uint32_t sampleformat = 1;

	while (nextIFD != 0)
	{
		_fseeki64(fp, nextIFD, SEEK_SET);

		uint16_t numEntries;
		if (fread(&numEntries, 2, 1, fp) != 1)
		{
			printf("Could not read %s: Read error\n", filename);
			return true;
		}
		numEntries = convertU16(numEntries);

		for (uint32_t i = 0; i < numEntries; ++i)
		{
			uint8_t entry[12];
			if (fread(entry, 1, 12, fp) != 12)
			{
				printf("Could not read %s: Read error\n", filename);
				return true;
			}

			uint16_t tag = convertU16(*reinterpret_cast<uint16_t*>(entry));
			uint16_t type = convertU16(*reinterpret_cast<uint16_t*>(entry + 2));
			uint32_t count = convertU32(*reinterpret_cast<uint32_t*>(entry + 4));
			uint32_t offset;
			if (count == 1 && type == 3)
				offset = convertU16(*reinterpret_cast<uint16_t*>(entry + 8));
			else
				offset = convertU32(*reinterpret_cast<uint32_t*>(entry + 8));

			const auto& readstrips = [fp, filename, bigendian, convertU16, convertU32](uint32_t*& strips, uint16_t type, uint32_t count, uint32_t offset) -> bool {
				strips = new uint32_t[count];
				if (count == 1)
				{
					strips[0] = offset;
				}
				else
				{
					int64_t offs = _ftelli64(fp);
					_fseeki64(fp, offset, SEEK_SET);
					if (type == 3)
					{
						uint16_t* strip16 = reinterpret_cast<uint16_t*>(strips + count / 2);
						if (fread(strip16, 2, count, fp) != count)
						{
							printf("Could not read %s: Read error\n", filename);
							return false;
						}
						for (uint32_t i = 0; i < count; ++i)
							strips[i] = convertU16(strip16[i]);

					}
					else if (type == 4)
					{
						if (fread(strips, 4, count, fp) != count)
						{
							printf("Could not read %s: Read error\n", filename);
							return false;
						}
						if (bigendian)
						{
							for (uint32_t i = 0; i < count; ++i)
								strips[i] = convertU32(strips[i]);
						}
					}
					else
					{
						printf("Could not read %s: Invalid file\n", filename);
						return false;
					}
					_fseeki64(fp, offs, SEEK_SET);
				}
				return true;
			};

			switch (tag)
			{
			case 256:
				tiffFile.width = offset;
				break;
			case 257:
				tiffFile.height = offset;
				break;
			case 258:
				bpp = offset;
				break;
			case 259:
				if (offset != 1)
				{
					printf("Could not read %s: Cannot handle compressed tiffs\n", filename);
					return true;
				}
				break;
			case 262:
				if (offset > 1)
				{
					printf("Could not read %s: Can only handle greyscale tiffs\n", filename);
					return true;
				}
				break;
			case 273:
				stripcount = count;
				if (!readstrips(stripoffsets, type, count, offset))
					return true;
				break;
			case 277:
				if (offset > 1)
				{
					printf("Could not read %s: Can only handle greyscale tiffs\n", filename);
					return true;
				}
				break;
			case 279:
				stripcount = count;
				if (!readstrips(stripbytes, type, count, offset))
					return true;
				break;
			case 339:
				sampleformat = offset;
				break;
			//default:
				//printf("Ignored tag: %d\n", tag);
			}
		}

		if (fread(&nextIFD, 4, 1, fp) != 1)
		{
			printf("Could not read %s: Read error\n", filename);
			return true;
		}
		nextIFD = convertU32(nextIFD);
	}

	if (stripcount == 0)
	{
		printf("Could not read %s: Invalid file\n", filename);
		return true;
	}

	printf("Loading tiff: %dx%d pixels\n", tiffFile.width, tiffFile.height);

	uint32_t datasize = 0;
	for (uint32_t i = 0; i < stripcount; ++i)
		datasize += stripbytes[i];

	uint8_t* pixeldata = new uint8_t[datasize];

	uint8_t* src = pixeldata;
	for (uint32_t i = 0; i < stripcount; ++i)
	{
		_fseeki64(fp, stripoffsets[i], SEEK_SET);

		if (fread(src, 1, stripbytes[i], fp) != stripbytes[i])
		{
			printf("Could not read %s: Read error\n", filename);
			return true;
		}

		src += stripbytes[i];
	}

	tiffFile.pixels = new float[tiffFile.width * tiffFile.height];
	int64_t pixels = int64_t(tiffFile.width) * int64_t(tiffFile.height);

	float* dst = tiffFile.pixels;

	if (bpp == 8)
	{
		if (sampleformat == 2)
		{
			int8_t* src8 = reinterpret_cast<int8_t*>(pixeldata);
#pragma omp parallel for
			for (int64_t i = 0; i < pixels; ++i)
			{
				dst[i] = src8[i];
			}

		}
		else
		{
			src = pixeldata;
#pragma omp parallel for
			for (int64_t i = 0; i < pixels; ++i)
			{
				dst[i] = src[i];
			}
		}
	}
	else if (bpp == 16)
	{
		if (sampleformat == 2)
		{
			uint16_t* src16 = reinterpret_cast<uint16_t*>(pixeldata);
#pragma omp parallel for
			for (int64_t i = 0; i < pixels; ++i)
			{
				dst[i] = (int16_t)convertU16(src16[i]);
			}
		}
		else
		{
			uint16_t* src16 = reinterpret_cast<uint16_t*>(pixeldata);
#pragma omp parallel for
			for (int64_t i = 0; i < pixels; ++i)
			{
				dst[i] = convertU16(src16[i]);
			}
		}
	}

	delete[] pixeldata;
	return true;
}
