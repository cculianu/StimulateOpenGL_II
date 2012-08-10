/*
 *  FastMovieFormat.h
 *  StimulateOpenGL_II
 *
 *  Created by calin on 8/9/12.
 *  Copyright 2012 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#ifndef FastMovieFormat_H
#define FastMovieFormat_H
#include <stdio.h>
#include <stdlib.h>


#ifdef _MSC_VER

typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;

#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


enum FM_Fmt {  FM_LUMINOSITY = 0, FM_RGB, FM_BGR, FM_RGBA, FM_ARGB };
enum FM_Comp { FM_No_Comp = 0, FM_ZLib_Comp };
	
struct FM_Header {
#define FM_MAGIC_STR "StimGLFMv1"
	    char magic[16]; ///< for now, the FAST_MOVIE_MAGIC_STR
	uint32_t nFrames;   ///< the number of animation frames contained in the file
	    char reserved[12]; ///< padding to make this struct 32 bytes
};

struct FM_ImageDescriptor {
#define FM_IMAGE_DESCRIPTOR_MAGIC 0x1337    
	uint16_t magic;     ///< guard against image corruption?
	uint32_t width;     ///< width of image/animation, in pix
	uint32_t height;    ///< height of image/animation, in pix
	uint32_t bitdepth;  ///< usually 8,16,24,32, for now we only support 8
	uint32_t fmt;       ///< for now, since we only support 8 bit, always FM_LUMINOSITY (or 0)
	uint32_t comp;      ///< should always be FM_Zlib_Comp, for now
	uint32_t duration;  ///< the duration of the animation frame, in ms. set to 0 for stimgl
	uint32_t length;    ///< the length of the image data (compressed) that follows 
};

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string.h> /* for memset/memcpy */
#include <vector>
#include <utility>
#include <sys/types.h>
#ifndef NO_QT
#include <QByteArray>
#endif

typedef std::pair<uint32_t, uint32_t> ImgSize;

struct FM_Context
{
	FILE *file;
	std::vector<off_t> imgOffsets;
	std::vector<ImgSize> imgSizes;
	bool isOutput; 
	
	FM_Context() : file(0), isOutput(true) {}
	~FM_Context() {	if (file) fclose(file); file = 0; }
};

struct FM_Image
{
	FM_ImageDescriptor desc;
#ifdef NO_QT
	uint8_t *data;

	FM_Image() { memset(&desc, 0, sizeof(desc)); data = 0; }	
	~FM_Image() { if (data) free(data); data = 0; }
#else
	QByteArray data;

	FM_Image() { memset(&desc, 0, sizeof(desc));  }	

#endif
	
};


/*-----------------------------------------------------------------------------
  WRITE/OUTPUT FUNCTIONS
 -----------------------------------------------------------------------------*/
/// returns NULL on error, creates a new fmv file for output on success and returns ptr to context
FM_Context * FM_Create(const char *filename); 
/// returns true on success
bool         FM_AddFrame(FM_Context *ctx, const void *pixels, 
						 unsigned width, unsigned height, 
						 unsigned depth = 8, FM_Fmt = FM_LUMINOSITY, 
						 bool comp = true, unsigned duration_ms = 0);

/*-----------------------------------------------------------------------------
  READ/INPUT FUNCTIONS
 -----------------------------------------------------------------------------*/
/// open .fmv file for input. returns pointer to context on success
FM_Context * FM_Open(const char *filenmae);
/// read a frame from the .fmv file, caller should delete returned pointer
FM_Image *   FM_ReadFrame(FM_Context *ctx, unsigned frame_id /* first frame is frame 0 */);
/// returns true if the filename is openable and readable as an .fmv file!
bool         FM_IsFMV(const char *filename);
						  
/*-----------------------------------------------------------------------------
 READ/WRITE FUNCTIONS (applicable to both)
 -----------------------------------------------------------------------------*/
/** call this on input or output.  MANDATORY for output: to finalize the image
    in both cases frees the context pointer, closes file, etc. */
void         FM_Close(FM_Context *); 


#endif

#endif
