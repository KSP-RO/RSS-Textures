// dds_converter.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <ddraw.h>
#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <io.h>
#include <math.h>
#include <algorithm>
#include <numeric>
#include <map>

#include "ReadTiff.h"

typedef struct {
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dwHeight;
    DWORD           dwWidth;
    DWORD           dwPitchOrLinearSize;
    DWORD           dwDepth;
    DWORD           dwMipMapCount;
    DWORD           dwReserved1[11];
    DDPIXELFORMAT   ddspf;
    DWORD           dwCaps;
    DWORD           dwCaps2;
    DWORD           dwCaps3;
    DWORD           dwCaps4;
    DWORD           dwReserved2;
} DDS_HEADER;



template<typename T>
inline T clamp(T value, T minval, T maxval)
{
    return max(minval, min(value, maxval));
}

template<typename T>
inline T wrap(T value, T maxval)
{
    return (value % maxval + maxval) % maxval;
}

template<typename T>
class CmdLineParam
{
public:
    CmdLineParam()
        : desc("")
        , defValue(T())
        , value(T())
    {}
    CmdLineParam(const char* d, T def = T())
        : desc(d)
        , defValue(def)
        , value(def)
    {}

    void setVal(T v) { value = v; }
    bool isDefault() const { return value == defValue; }
    operator T() const { return value; }

    const char* getDesc() const { return desc;  }

private:
    const char* desc;
    const T defValue;
    T value;
};

struct strless
{
    bool operator() (const char* a, const char* b) const
    {
        return _stricmp(a, b) < 0;
    }
};

typedef std::map<const char*, CmdLineParam<bool>, strless> BoolParams_t;
typedef std::map<const char*, CmdLineParam<int32_t>, strless> IntParams_t;
typedef std::map<const char*, CmdLineParam<double>, strless> FloatParams_t;
typedef std::map<const char*, CmdLineParam<const char*>, strless> StringParams_t;

bool ParseBoolParam(BoolParams_t& boolParams, const char* argName)
{
    BoolParams_t::iterator f = boolParams.find(argName);
    if (f != boolParams.end())
    {
        f->second.setVal(true);
        return true;
    }

    return false;
}

bool ParseIntParam(IntParams_t& intParams, const char* argName, const char* argVal)
{
    IntParams_t::iterator f = intParams.find(argName);
    if (f != intParams.end())
    {
        char* endP;
        long l = strtol(argVal, &endP, 10);
        if (endP != argVal)
            f->second.setVal(l);

        return true;
    }

    return false;
}

bool ParseFloatParam(FloatParams_t& floatParams, const char* argName, const char* argVal)
{
    FloatParams_t::iterator f = floatParams.find(argName);
    if (f != floatParams.end())
    {
        char* endP;
        double d = strtod(argVal, &endP);
        if (endP != argVal)
            f->second.setVal(d);

        return true;
    }

    return false;
}

bool ParseStringParam(StringParams_t& strParams, const char* argName, const char* argVal)
{
    StringParams_t::iterator f = strParams.find(argName);
    if (f != strParams.end())
    {
        f->second.setVal(argVal);
        return true;
    }

    return false;
}

void usage(const BoolParams_t& boolParams, const IntParams_t& intParams, const FloatParams_t& floatParams, const StringParams_t& strParams)
{
    printf("topoconv <infilename> [outfilename] [opts]\n");
    printf("  infilename should be raw 16-bit signed integer or 32-bit float topo data\n");
    printf("  outfilename is saved as DDS\n");

    for (const BoolParams_t::value_type& p : boolParams)
    {
        printf("  -%-22s %s\n", p.first, p.second.getDesc());
    }
    for (const IntParams_t::value_type& p : intParams)
    {
        char name[50];
        sprintf_s(name, "%s [%c]", p.first, p.first[0]);
        printf("  -%-22s %s\n", name, p.second.getDesc());
    }
    for (const FloatParams_t::value_type& p : floatParams)
    {
        char name[50];
        sprintf_s(name, "%s [%c]", p.first, p.first[0]);
        printf("  -%-22s %s\n", name, p.second.getDesc());
    }
    for (const StringParams_t::value_type& p : strParams)
    {
        char name[50];
        sprintf_s(name, "%s [%c]", p.first, p.first[0]);
        printf("  -%-22s %s\n", name, p.second.getDesc());
    }
}

int main(int argc, const char** argv)
{
    BoolParams_t boolParams({
        { "autoscale", BoolParams_t::mapped_type("Set heightscale automatically") },
        { "autooffset", BoolParams_t::mapped_type("Set heightoffs automatically") },
        { "bigendian", BoolParams_t::mapped_type("Source data is big endian (only for 16-bit int)") },
        { "fp32", BoolParams_t::mapped_type("Source data is 32-bit float") },
        { "bilinear", BoolParams_t::mapped_type("Use bilinear resampling") },
        { "median", BoolParams_t::mapped_type("Use median resampling") },
        { "nearest", BoolParams_t::mapped_type("Use nearest resampling") },
        { "xflip", BoolParams_t::mapped_type("Flip output horizontally") },
        { "yflip", BoolParams_t::mapped_type("Flip output vertically") },
        });

    IntParams_t intParams({
        { "width", IntParams_t::mapped_type("Output data width") },
        { "aspect", IntParams_t::mapped_type("Data aspect ratio", 2) },
        { "srcwidth", IntParams_t::mapped_type("Source file width") },
        { "cropleft", IntParams_t::mapped_type("Left edge for crop") },
        { "cropright", IntParams_t::mapped_type("Right edge for crop") },
        { "croptop", IntParams_t::mapped_type("Top edge for crop") },
        { "cropbottom", IntParams_t::mapped_type("Bottom edge for crop") },
        });

    FloatParams_t floatParams({
        { "heightoffs", FloatParams_t::mapped_type("Height offset in source units", 0.0) },
        { "heightscale", FloatParams_t::mapped_type("Height recale factor", 1.0) },
        { "inmeridian", FloatParams_t::mapped_type("Input data meridian", 0.0) },
        { "outmeridian", FloatParams_t::mapped_type("Output data meridian", 0.0) },
        { "coastdefine", FloatParams_t::mapped_type("Define coastline in source units", 0.0) },
        });

    StringParams_t strParams({
        { "f", StringParams_t::mapped_type("Output data format", "r16") },
        { "landtex", StringParams_t::mapped_type("Texture for land masking (tiff format)") },
        });

    if (argc < 2)
    {
        usage(boolParams, intParams, floatParams, strParams);
        return 1;
    }

    const char* infilename = nullptr;
    const char* outfilename = nullptr;

    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '-')
        {
            if (ParseBoolParam(boolParams, argv[i] + 1))
                continue;

            if (i + 1 < argc)
            {
                if (ParseIntParam(intParams, argv[i] + 1, argv[i + 1]) ||
                    ParseFloatParam(floatParams, argv[i] + 1, argv[i + 1]) ||
                    ParseStringParam(strParams, argv[i] + 1, argv[i + 1]))
                {
                    ++i;
                    continue;
                }
            }

            printf("Unknown parameter: %s\n", argv[i]);
            usage(boolParams, intParams, floatParams, strParams);
            return 1;
        }
        else if (!infilename)
        {
            infilename = argv[i];
        }
        else if (!outfilename)
        {
            outfilename = argv[i];
        }
        else
        {
            printf("Unexpected filename: %s\n", argv[i]);
            usage(boolParams, intParams, floatParams, strParams);
            return 1;
        }
    }

    TiffFile_t landTex = {};
    if (strParams["landtex"])
    {
        if (!ReadTiff(landTex, strParams["landtex"]))
        {
            printf("Cannot read land texture: %s\n", static_cast<const char*>(strParams["landtex"]));
            return 1;
        }
        if (landTex.pixels)
        {
            printf("Using land texture: %s\n", static_cast<const char*>(strParams["landtex"]));
        }
    }

    float* srcdata;
    int32_t src_width, src_height;
    int64_t pixels;
    int32_t aspect;

    TiffFile_t tiffFile;
    if (ReadTiff(tiffFile, infilename))
    {
        if (!tiffFile.pixels)
            return 1;
        src_width = tiffFile.width;
        src_height = tiffFile.height;
        aspect = src_width / src_height;
        pixels = int64_t(src_width) * int64_t(src_height);
        srcdata = tiffFile.pixels;
    }
    else
	{
		FILE* fp = nullptr;
		if (fopen_s(&fp, infilename, "rb") != 0)
		{
			printf("Unable to open %s\n", infilename);
			return 1;
		}

		aspect = intParams["aspect"];

		int64_t size = _filelengthi64(_fileno(fp));
		if (boolParams["fp32"])
			size /= 2;

		src_width = intParams["srcwidth"];
		if (src_width <= 0)
		{
			src_width = (int32_t)sqrt((double)size * aspect / 2);
			// sanitize rounding errors from double to int conversion
			src_width = ((src_width + 5) / 10) * 10;
		}
		src_height = src_width / aspect;

		printf("Loading file: %dx%d pixels\n", src_width, src_height);

		if (src_width <= 0 || src_height <= 0)
			return 1;

		pixels = int64_t(src_width) * int64_t(src_height);
		srcdata = new float[pixels];

		if (boolParams["fp32"])
		{
			fread(srcdata, 4, pixels, fp);
		}
		else
		{
			int16_t* loaddata = new int16_t[pixels];
			fread(loaddata, 2, pixels, fp);

			if (boolParams["bigendian"])
			{
				uint16_t* src = reinterpret_cast<uint16_t*>(loaddata);
#pragma omp parallel for
				for (int64_t i = 0; i < pixels; ++i)
				{
					srcdata[i] = int16_t(src[i] >> 8 | src[i] << 8);
				}
			}
			else
			{
#pragma omp parallel for
				for (int64_t i = 0; i < pixels; ++i)
				{
					srcdata[i] = loaddata[i];
				}
			}
		}

		fclose(fp);
    }

    if (intParams["cropleft"] < intParams["cropright"] && intParams["croptop"] < intParams["cropbottom"])
    {
        int32_t cropleft = intParams["cropleft"];
        int32_t cropright = intParams["cropright"];
        int32_t croptop = intParams["croptop"];
        int32_t cropbottom = intParams["cropbottom"];
        
        int32_t crop_width = cropright - cropleft;
        int32_t crop_height = cropbottom - croptop;
        int64_t crop_pixels = int64_t(crop_width) * int64_t(crop_height);
        float* cropdata = new float[crop_pixels];

#pragma omp parallel for
        for (int32_t y = 0; y < crop_height; ++y)
        {
            float* src = srcdata + (croptop + y) * src_width + cropleft;
            float* dst = cropdata + y * crop_width;

            memcpy(dst, src, crop_width * sizeof(float));
        }

        delete[] srcdata;
        srcdata = cropdata;

        src_width = crop_width;
        src_height = crop_height;
        pixels = crop_pixels;

        printf("Cropped section: %dx%d pixels\n", src_width, src_height);
    }
    
    int32_t dst_width = intParams["width"];
    if (dst_width < 2 || dst_width > src_width)
        dst_width = src_width;
    int32_t dst_height = dst_width / aspect;

    printf("Saving file: %dx%d pixels\n", dst_width, dst_height);

    uint16_t* dstdata = new uint16_t[dst_width * dst_height];

    double heightoffs = floatParams["heightoffs"];
    double heightscale = floatParams["heightscale"];

    if (boolParams["autoscale"] || boolParams["autooffset"])
    {
        double minheight = DBL_MAX;
        double maxheight = -DBL_MAX;

        for (int32_t i = 0; i < src_width * src_height; ++i)
        {
            minheight = min(minheight, srcdata[i]);
            maxheight = max(maxheight, srcdata[i]);
        }

        if (boolParams["autoscale"])
        {
            heightscale = 65535.0 / max(maxheight - minheight, 1e-6);
            printf("Using heightscale: %g (use deformity=%g)\n", heightscale, 65535.0 / heightscale);
        }
        if (boolParams["autooffset"])
        {
            heightoffs = -minheight;
            printf("Using heightoffs: %g\n", heightoffs);
        }
    }
    heightscale = clamp(heightscale, 1e-4, 1e4);
    heightoffs *= heightscale;

    double deltameridian = (floatParams["outmeridian"] - floatParams["inmeridian"]) * dst_width / 360.0;

    double latScale = double(src_height) / double(dst_height + 1);
    double lngScale = double(src_width) / double(dst_width);
    double landScale = double(landTex.width) / double(src_width);

    double coast = floatParams["coastdefine"] * heightscale;
    auto coastdefine = [coast](double h) -> double
    {
        if (fabs(h) < coast)
            return signbit(h) ? -coast : coast;
        return h;
    };

    auto sampleland = [landScale, landTex](int32_t lat, int32_t lng) -> float
    {
        if (landTex.pixels)
            return 1.0f - landTex.pixels[int32_t(lat * landScale + 0.5f) * landTex.width + int32_t(lng * landScale + 0.5f)] / 255.0f;
        return 0.0;
    };
    auto landdefine = [coast](double h, double landdef) -> double
    {
        return h + landdef * max(coast - h, 0.0);
    };

    if (boolParams["nearest"])
    {
#pragma omp parallel for
        for (int32_t y = 0; y < dst_height; ++y)
        {
            int32_t lat = int32_t(y * latScale + 0.5);

            for (int32_t x = 0; x < dst_width; ++x)
            {
                int32_t lng = int32_t((x + deltameridian) * lngScale + 0.5);
                lng = wrap(lng, src_width);

                double h = srcdata[lat * src_width + lng];
                double landdef = sampleland(lat, lng);

                h = coastdefine(landdefine(heightscale * h, landdef)) + heightoffs;

                dstdata[y * dst_width + x] = clamp(int32_t(h + 0.5), 0, (int32_t)UINT16_MAX);
            }
        }
    }
    else if (boolParams["bilinear"])
    {
#pragma omp parallel for
        for (int32_t y = 0; y < dst_height; ++y)
        {
            double lat = (y + 0.5) * latScale;
            int32_t minLat = int32_t(floor(lat));
            int32_t maxLat = min(minLat + 1, src_height - 1);
            double fLat = lat - minLat;

            for (int32_t x = 0; x < dst_width; ++x)
            {
                double lng = (x + deltameridian + 0.5) * lngScale;
                int32_t minLong = int32_t(floor(lng));
                int32_t maxLong = minLong + 1;
                double fLong = lng - minLong;

                minLong = wrap(minLong, src_width);
                maxLong = wrap(maxLong, src_width);

                double h;
                h  = srcdata[minLat * src_width + minLong] * (1.0 - fLat) * (1.0 - fLong);
                h += srcdata[maxLat * src_width + minLong] * fLat * (1.0 - fLong);
                h += srcdata[minLat * src_width + maxLong] * (1.0 - fLat) * fLong;
                h += srcdata[maxLat * src_width + maxLong] * fLat * fLong;

                double landdef;
                landdef  = sampleland(minLat, minLong) * (1.0 - fLat) * (1.0 - fLong);
                landdef += sampleland(maxLat, minLong) * fLat * (1.0 - fLong);
                landdef += sampleland(minLat, maxLong) * (1.0 - fLat) * fLong;
                landdef += sampleland(maxLat, maxLong) * fLat * fLong;

                h = coastdefine(landdefine(heightscale * h, landdef)) + heightoffs;

                dstdata[y * dst_width + x] = clamp(int32_t(h + 0.5), 0, (int32_t)UINT16_MAX);
            }
        }
    }
    else
    {
        bool median = boolParams["median"];

#pragma omp parallel for
        for (int32_t y = 0; y < dst_height; ++y)
        {
            size_t maxN = int32_t(ceil(latScale) * ceil(lngScale));
            float* hBuf = static_cast<float*>(alloca(maxN * sizeof(float)));
            float* landBuf = static_cast<float*>(alloca(maxN * sizeof(float)));

            int32_t minLat = int32_t(y * latScale + 0.5);
            int32_t maxLat = int32_t((y + 1) * latScale + 0.5);
            maxLat = min(maxLat, src_height);

            for (int32_t x = 0; x < dst_width; ++x)
            {
                int32_t minLong = int32_t((x + deltameridian) * lngScale + 0.5);
                int32_t maxLong = int32_t((x + deltameridian + 1) * lngScale + 0.5);

                int32_t n = 0;

                for (int32_t lat = minLat; lat < maxLat; ++lat)
                {
                    for (int32_t lng = minLong; lng < maxLong; ++lng)
                    {
                        hBuf[n] = srcdata[lat * src_width + wrap(lng, src_width)];
                        landBuf[n] = sampleland(lat, wrap(lng, src_width));
                        ++n;
                    }
                }

                double h;
                if (median)
                {
                    if (n & 1)
                    {
                        std::nth_element(hBuf, hBuf + n / 2, hBuf + n);
                        h = hBuf[n / 2];
                    }
                    else
                    {
                        std::partial_sort(hBuf, hBuf + n / 2, hBuf + n);
                        h = double(hBuf[n / 2] + hBuf[n / 2 - 1]) / 2.0;
                    }
                }
                else
                {
                    h = std::accumulate(hBuf, hBuf + n, 0.0) / n;
                }

                double landdef = std::accumulate(landBuf, landBuf + n, 0.0) / n;

                h = coastdefine(landdefine(heightscale * h, landdef)) + heightoffs;
                dstdata[y * dst_width + x] = clamp(int32_t(h + 0.5), 0, (int32_t)UINT16_MAX);
            }
        }
    }

    uint16_t minheight = UINT16_MAX;
    uint16_t maxheight = 0;

    for (int32_t i = 0; i < dst_height * dst_width; ++i)
    {
        minheight = min(dstdata[i], minheight);
        maxheight = max(dstdata[i], maxheight);
    }

    printf("Height Range: %g / %g\n", (int32_t(minheight) - heightoffs) / heightscale, (int32_t(maxheight) - heightoffs) / heightscale);

    if (boolParams["xflip"])
    {
#pragma omp parallel for
        for (int32_t y = 0; y < dst_height; ++y)
        {
            uint16_t* rowdata = dstdata + y * dst_width;
            for (int32_t x = 0; x < dst_width / 2; ++x)
            {
                std::swap(rowdata[x], rowdata[dst_width - x - 1]);
            }
        }
    }
    if (boolParams["yflip"])
    {
#pragma omp parallel for
        for (int32_t y = 0; y < dst_height / 2; ++y)
        {
            uint16_t* rowdata1 = dstdata + y * dst_width;
            uint16_t* rowdata2 = dstdata + (dst_height - y - 1) * dst_width;
            if (rowdata1 != rowdata2)
            {
                for (int32_t x = 0; x < dst_width; ++x)
                {
                    std::swap(rowdata1[x], rowdata2[x]);
                }
            }
        }
    }

    DDS_HEADER desc;
    memset(&desc, 0, sizeof(desc));

    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS | DDSD_PITCH | DDSD_PIXELFORMAT;
    desc.dwHeight = dst_height;
    desc.dwWidth = dst_width;
    desc.dwCaps = DDSCAPS_TEXTURE;
    
    desc.ddspf.dwSize = sizeof(desc.ddspf);

    if (_stricmp(strParams["f"], "r16") == 0)
    {
        desc.dwPitchOrLinearSize = dst_width * 2;
        desc.ddspf.dwFlags = DDPF_LUMINANCE;
        desc.ddspf.dwRGBBitCount = 16;
        desc.ddspf.dwRBitMask = 0xffff;
    }
    else if (_stricmp(strParams["f"], "ra8") == 0 || _stricmp(strParams["f"], "ga8") == 0)
    {
        desc.dwPitchOrLinearSize = dst_width * 4;
        desc.ddspf.dwFlags = DDPF_ALPHAPIXELS | DDPF_RGB;
        desc.ddspf.dwRGBBitCount = 32;
        desc.ddspf.dwRBitMask = 0xff;
        desc.ddspf.dwGBitMask = 0xff00;
        desc.ddspf.dwBBitMask = 0xff0000;
        desc.ddspf.dwRGBAlphaBitMask = 0xff000000;

        uint32_t* dstdata32 = new uint32_t[dst_width * dst_height];

        if (_stricmp(strParams["f"], "ra8") == 0)
        {
#pragma omp parallel for
            for (int32_t i = 0; i < dst_height * dst_width; ++i)
            {
                dstdata32[i] = (dstdata[i] & 0xff) | ((dstdata[i] & 0xff00) << 16);
            }
        }
        else
        {
#pragma omp parallel for
            for (int32_t i = 0; i < dst_height * dst_width; ++i)
            {
                dstdata32[i] = ((dstdata[i] & 0xff) << 8) | ((dstdata[i] & 0xff00) << 16);
            }
        }

        delete[] dstdata;
        dstdata = reinterpret_cast<uint16_t*>(dstdata32);
    }
    else if (_stricmp(strParams["f"], "r1") == 0)
    {
        desc.dwWidth = dst_width / 8;
        desc.dwPitchOrLinearSize = dst_width / 8;
        desc.ddspf.dwFlags = DDPF_LUMINANCE;
        desc.ddspf.dwRGBBitCount = 8;
        desc.ddspf.dwRBitMask = 0xff;

        uint8_t* dstdata1 = new uint8_t[dst_width * dst_height / 8];

#pragma omp parallel for
        for (int32_t i = 0; i < dst_height * dst_width; i += 8)
        {
            uint8_t data = 0;
            for (int j = 0; j < 8; ++j)
                data |= (dstdata[i + j] >= heightoffs) << j;
            dstdata1[i/8] = data;
        }

        delete[] dstdata;
        dstdata = reinterpret_cast<uint16_t*>(dstdata1);
    }
    else
    {
        printf("Unknown destination format, supported: r16, ra8, ga8, r1\n");
    }

    if (desc.dwPitchOrLinearSize)
    {
        if (!outfilename)
        {
            char* ofn = (char*)alloca(MAX_PATH);
            strcpy_s(ofn, MAX_PATH, infilename);
            strcat_s(ofn, 10, "_conv.dds");
            outfilename = ofn;
        }

        FILE* fp;
        if (fopen_s(&fp, outfilename, "wb") != 0)
        {
            printf("Unable to open %s", outfilename);
            return 1;
        }

        uint32_t magic = 0x20534444;
        fwrite(&magic, 4, 1, fp);
        fwrite(&desc, sizeof(desc), 1, fp);
        fwrite(dstdata, desc.dwPitchOrLinearSize, dst_height, fp);
        fclose(fp);
    }

#if 0
    {
        desc.dwPitchOrLinearSize = dst_width;
        desc.ddspf.dwFlags = DDPF_LUMINANCE;
        desc.ddspf.dwRGBBitCount = 8;
        desc.ddspf.dwRBitMask = 0xff;

        uint8_t* dstdata1 = new uint8_t[dst_width * dst_height];

        const float Gx[] = {
             1,  0, -1,
             2,  0, -2,
             1,  0, -1
        };
        const float Gy[] = {
             1,  2,  1,
             0,  0,  0,
            -1, -2, -1
        };

        coast = 10.0;
#pragma omp parallel for
        for (int32_t y = 1; y < dst_height - 1; ++y)
        {
            for (int32_t x = 1; x < dst_width - 1; ++x)
            {
                float s1 = 0.0f;
                float s2 = 0.0f;

                for (int j = -1; j <= 1; ++j)
                {
                    for (int i = -1; i <= 1; ++i)
                    {
                        float px = float(clamp((dstdata[(y + j) * dst_width + x + i] - heightoffs) / heightscale, -coast, coast)) + coast;
                        s1 += px * Gx[(j+1)*3 + i+1];
                        s2 += px * Gy[(j+1)*3 + i+1];
                    }
                }

                dstdata1[y * dst_width + x] = uint8_t(clamp(sqrtf(s1 * s1 + s2 * s2) * 127.0f / float(coast), 0.f, 255.0f));
            }
        }

        FILE* fp;
        if (fopen_s(&fp, "sobel_test.dds", "wb") != 0)
        {
            printf("Unable to open %s", outfilename);
            return 1;
        }

        uint32_t magic = 0x20534444;
        fwrite(&magic, 4, 1, fp);
        fwrite(&desc, sizeof(desc), 1, fp);
        fwrite(dstdata1, desc.dwPitchOrLinearSize, dst_height, fp);
        fclose(fp);
    }
#endif

    return 0;
}
