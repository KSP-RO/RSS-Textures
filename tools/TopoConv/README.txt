A tool for converting DEM data from NASA and similar sources to DDS heightmaps.

topoconv <infilename> [outfilename] [opts]
  infilename should be raw 16-bit signed integer or 32-bit float topo data
  outfilename is saved as DDS
  -bigendian              Source data is big endian (only for 16-bit int)
  -bilinear               Use bilinear resampling
  -fp32                   Source data is 32-bit float
  -median                 Use median resampling
  -nearest                Use nearest resampling
  -xflip                  Flip output horizontally
  -yflip                  Flip output vertically
  -width [w]              Output data width
  -coastdefine [c]        Define coastline in dst units
  -heightoffs [h]         Height offset in source units
  -heightscale [h]        Height recale factor
  -inmeridian [i]         Input data meridian
  -outmeridian [o]        Output data meridian
  -f [f]                  Output data format

Heightscale determines the scaling from source data to destination data.
	For sources in metres, a scale of around 2 will typically be adequate (giving a vertical resolution of 0.5m).
	For sources in km, a scale of around 2000 will typically be adequate (giving a vertical resolution of 0.5m).
	Care should be taken not to set the scale too high resulting in clipping of peaks.
	
Heightoffs should be set below the lowest point of the source data to avoid clipping of valleys.

When setting up Kopernicus cfg files, the offset should be set to Heightoffs converted to metres. The deformity should be set to the vertical resolution * 65535.
	For a typical metre scale source scaled by 2, this means set offset = Heightoffs and deformity = 32767.5.

Typical usage for metre scale 16-bit source data:
	TopoConv topo.raw topo_4k.dds -bigendian -width 4096 -heightscale 2 -heightoffs 10000 -median -outmeridian 90 -xflip -yflip -f r16

Typical usage for km scale 32-bit float source data:
	TopoConv topo.raw topo_4k.dds -fp32 -width 4096 -heightscale 2000 -heightoffs 10 -median -outmeridian 90 -xflip -yflip -f r16
	
Both of these resulting maps can be used in Kopernicus as follows:

	VertexHeightMapRSS
	{
		offset = -10000
		deformity = 32767.5
		map = topo_4k.dds
		order = 100
		enabled = true
	}
