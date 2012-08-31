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
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>


#if defined(_MSC_VER) || defined(WIN32)

typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
#define PACKED /* nothing */
#pragma pack(push, 1)

#else /* GCC, non-Windows */

#include <stdint.h>
#define PACKED __attribute__ ((__packed__))

#endif

#ifdef __cplusplus
extern "C" {
#endif


enum FM_Fmt {  FM_LUMINOSITY = 0, FM_RGB, FM_BGR, FM_RGBA, FM_ARGB };
enum FM_Comp { FM_No_Comp = 0, FM_ZLib_Comp };
	
struct PACKED FM_Header {
#define FM_MAGIC_STR "StimGLFMv2"
	char magic[16]; ///< for now, the FAST_MOVIE_MAGIC_STR
	uint32_t nFrames;   ///< the number of animation frames contained in the file
	uint32_t width; ///< the width of the image frame, in pixels 
	uint32_t height; ///< the height of the total image frame, in pixels
	uint32_t reserved; ///< padding...
	uint64_t indexRecordOffset; ///< where in the file to find the index record.  if 0, then build index ourselves 
};

struct PACKED FM_ImageDescriptor {
#define FM_IMAGE_DESCRIPTOR_MAGIC 0x1337f00d    
	uint32_t magic;     ///< guard against image corruption?
	uint32_t width;     ///< width of image/animation, in pix
	uint32_t height;    ///< height of image/animation, in pix
	uint32_t bitdepth;  ///< usually 8,16,24,32, for now we only support 8
	uint32_t fmt;       ///< for now, since we only support 8 bit, always FM_LUMINOSITY (or 0)
	uint32_t comp;      ///< should always be FM_Zlib_Comp, for now
	uint32_t duration;  ///< the duration of the animation frame, in ms. set to 0 for stimgl
	uint32_t length;    ///< the length of the image data (compressed) that follows 
};

struct PACKED FM_IndexRecord {
#define FM_INDEX_RECORD_MAGIC 0x12341234
	uint32_t magic;
	uint32_t reserved;
	uint64_t length;
	char padding[16];
	// indices for each frame follow..., each index is a 64-bit unsigned offset into the file for that frame
};
	
	
#if defined(_MSC_VER) || defined(WIN32)
#pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string.h> /* for memset/memcpy */
#include <vector>
#include <utility>
#include <string>
#include <sys/types.h>
#ifndef NO_QT
#include <QByteArray>
#endif

#undef PACKED

struct FM_Context
{
	FILE *file;
	std::vector<uint64_t> imgOffsets;
	bool isOutput; 
	unsigned width, height;
	
	FM_Context() : file(0), isOutput(true), width(0), height(0) {}
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
						 unsigned width, unsigned height, unsigned compressionLevel = 1,
						 unsigned depth = 8, FM_Fmt = FM_LUMINOSITY, 
						 bool comp = true, unsigned duration_ms = 0);

/*-----------------------------------------------------------------------------
  READ/INPUT FUNCTIONS
 -----------------------------------------------------------------------------*/
/// open .fmv file for input. returns pointer to context on success
FM_Context * FM_Open(const char *filename, std::string *errmsg = 0, bool rebuildIndexIfMissing = false);
/// read a frame from the .fmv file, caller should delete returned pointer
FM_Image *   FM_ReadFrame(FM_Context *ctx, unsigned frame_id /* first frame is frame 0 */, std::string *errmsg = 0);
/// returns true if the filename is openable and readable as an .fmv file!
bool         FM_IsFMV(const char *filename);

typedef void (*FM_ErrorFn)(void *, const std::string &);
typedef bool (*FM_ProgressFn)(void *, int);

/** Error checking function.  Scans the specified FMV file and checks it for 
	errors. Returns true if the file validates to a valid FMV, otherwise false.
    Optionally, you can specify a progress function, and an error function as 
    callbacks. The progress function is called as the file is scanned (about 
    once every 1% of progress) and it takes 2 params, the void * arg, as well as 
    a percentage indicating scan progress from 0 to 100).  The error function
    is called with a specific message describing the error.  After the error 
    function is called, FM_CheckForErrors will return immediately with a false
    result. */  
bool         FM_CheckForErrors(const char *filename, void *arg = 0, 
							   FM_ProgressFn progfunc = 0, FM_ErrorFn errorfunc = 0);

/*-----------------------------------------------------------------------------
 READ/WRITE FUNCTIONS (applicable to both)
 -----------------------------------------------------------------------------*/
/** call this on input or output.  MANDATORY for output: to finalize the image
    in both cases frees the context pointer, closes file, etc. */
void         FM_Close(FM_Context *); 

/** call this to (re)build the index on a pre-existing slightly corrupted .fmv */
bool         FM_RebuildIndex(const char *filename, void * = 0, FM_ProgressFn = 0, FM_ErrorFn = 0);
#endif

#endif
