/*
 *  FastMovieFormat.cpp
 *  StimulateOpenGL_II
 *
 *  Created by calin on 8/9/12.
 *  Copyright 2012 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#include "FastMovieFormat.h"
#include <stdlib.h>
#include <errno.h>

#ifdef NO_QT
#include <zlib.h>

static void zCompress(const void *inbuffer, unsigned nbytes, int compressionLevel,
					  void **outbuffer, unsigned *outbytes);
static void zUncompress(const void *inbuffer, unsigned nbytes,
					  void **outbuffer, unsigned *outbytes);
#else
#include <QByteArray>
#endif


#ifdef _MSC_VER

#if _MSC_VER >= 1400
// MS VC++ 2005 and above
#  define fseeko(stream, offset, origin) _fseeki64(stream, offset, origin)
#  define ftello(stream) _ftelli64(stream)

#else /* Older MSVC.. Lacks 64 bit file support in LIBC.. grrr... so we emulate using lower-level functions */

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#define fwrite fwriteo
#define fread freado

static int fwriteo(const void *buf, __int64 elemsize, int nelems, FILE *f)
{
	int n = _write(_fileno(f), buf, elemsize*nelems);
	return n / elemsize;
}

static int freado(void *buf, __int64 elemsize, int nelems, FILE *f)
{
	int n = _read(_fileno(f), buf, elemsize*nelems);
	return n / elemsize;	
}
static __int64 ftello(FILE *f) { return _telli64(_fileno(f)); }
static __int64 fseeko (FILE * f, __int64 offset, int origin) { return _lseeki64(_fileno(f), offset, origin); }
#endif
#endif

/*-----------------------------------------------------------------------------
 WRITE/OUTPUT FUNCTIONS
 -----------------------------------------------------------------------------*/
/// returns NULL on error, creates a new fmv file for output on success and returns ptr to context
FM_Context * FM_Create(const char *filename)
{
	FILE *f = fopen(filename, "w+b");
	if (!f) return 0;
	FM_Context * c = new FM_Context;
	c->file = f;
	c->isOutput = true;
	c->imgOffsets.clear();
	c->imgOffsets.reserve(16);
	
	FM_Header h;
	memset(&h, 0, sizeof(h));
	memcpy(h.magic, FM_MAGIC_STR, strlen(FM_MAGIC_STR));
	if ( fwrite(&h, sizeof(h), 1, c->file) != 1 ) {
		delete c;
		c = 0;
	}
	return c;
}
/// returns true on success
bool         FM_AddFrame(FM_Context *c, const void *pixels, 
						 unsigned width, unsigned height, 
						 unsigned compressionLevel,
						 unsigned depth, FM_Fmt fmt, 
						 bool comp, unsigned duration_ms)
{
	if (!c || !c->file || !c->isOutput || !width || !height || !depth) return false;
	
	//off_t savedOff = ftello(c->file);
	fseeko(c->file, 0, SEEK_END);
	int64_t imgoffset = ftello(c->file);
	if (c->imgOffsets.capacity() == c->imgOffsets.size()) {
		c->imgOffsets.reserve(c->imgOffsets.capacity()*2);
	}
	c->imgOffsets.push_back(static_cast<uint64_t>(imgoffset));
	
	FM_ImageDescriptor desc;
	memset(&desc, 0, sizeof(desc));
	desc.magic = FM_IMAGE_DESCRIPTOR_MAGIC;
	desc.width = width;
	desc.height = height;
	desc.comp = comp ? 1 : 0;
	desc.duration = duration_ms;
	desc.fmt = fmt;
	
	int pixsz = 1;
	switch (depth) {
		case 32: ++pixsz; 
		case 24: ++pixsz; 
		case 16: ++pixsz; 
		case 8:
			break;
		default:
			//fseeko(c->file, savedOff, SEEK_SET);
			return false;
	}
	
#ifndef NO_QT
	QByteArray d;
#endif
	void *data = (void *)pixels;
	unsigned datasz = width*height*pixsz;
	if (comp) {
		data = 0;
#ifdef NO_QT
		zCompress(pixels, datasz, compressionLevel, &data, &datasz);
#else
		d = qCompress((uchar *)pixels, datasz, compressionLevel);
		data = (void *)d.constData();
		if (!data) {
			return false;
		}
		datasz = d.size();
#endif
		
	}
	bool ret = false;
	desc.length = datasz;
	if ( data 
		 && fwrite(&desc, sizeof(desc), 1, c->file) == 1 
		 && (!datasz || fwrite(data, datasz, 1, c->file) == 1) ) {
		// update header .. tell it how many images we have
		fseeko(c->file, 0, SEEK_SET);
		if (c->width < width || c->height < height) {
			c->width = width;
			c->height = height;
		}		
		FM_Header h;
		if ( fread(&h, sizeof(h), 1, c->file) == 1 ) {
			h.nFrames = c->imgOffsets.size();
			h.width = c->width;
			h.height = c->height;
			fseeko(c->file, 0, SEEK_SET);
			fwrite(&h, sizeof(h), 1, c->file);
		}
		ret = true;
	}
	
#ifdef NO_QT
	if (comp) {
		free(data);
		data = 0;
	}
#endif
	
	//fseeko(c->file, savedOff, SEEK_SET);
	
	return ret;
}

static bool Reindex(FM_Context *c, unsigned nframes, void *arg = 0, FM_ProgressFn prog = 0, std::string *errmsg = 0)
{
	int lastpct = -1;
	if (c->imgOffsets.size() != 0) c->imgOffsets.resize(0);
	if (c->imgOffsets.capacity() != nframes) c->imgOffsets.reserve(nframes);
	if (errmsg) *errmsg = "";
	
	FILE * & f = c->file;
	if ( fseeko(f, sizeof(FM_Header), SEEK_SET) ) {
		if (errmsg) *errmsg = "Rebuild index failed: cannot seek past header";
		return false;
	}
	
	// scan file..
	for (unsigned i = 0; i < nframes; ++i) {
		c->imgOffsets.push_back(static_cast<uint64_t>(ftello(f))); // save image offset..
		FM_ImageDescriptor desc;
		memset(&desc, 0, sizeof(desc));
		if ( fread(&desc, sizeof(desc), 1, f) != 1 ) {
			if (errmsg) {
				char buf[64];
				sprintf(buf, "%d", i);
				*errmsg = std::string("Failed to read image #") + buf + "'s descriptor from file.";
			}
			return false;
		}
		
		// now advance to next image
		if ( fseeko(f, desc.length, SEEK_CUR) ) {
			// eek! seek error!  image file corrupt/truncated??
			if (errmsg) {
				char buf[64];
				sprintf(buf, "%d", i);
				*errmsg = std::string("Seek error for image ") + buf + ".  File truncated?";
			}				
			return false;
		}
		if ( prog && lastpct < int((i*100) / nframes) ) {
			lastpct = int( (i*100) / nframes);
			prog(arg, lastpct);
		}
	}
	
	return true;
}

/*-----------------------------------------------------------------------------
 READ/INPUT FUNCTIONS
 -----------------------------------------------------------------------------*/
/// open .fmv file for input. returns pointer to context on success
FM_Context * FM_Open(const char *filename, std::string *errmsg, bool rebuildIndex)
{
	if (errmsg) *errmsg = "";
	FILE *f = fopen(filename, "rb");
	if (!f) {
		if (errmsg) {
			const int e = errno;
			*errmsg = std::string("Cannot open ") + filename + " for reading: " + strerror(e);
		}
		return 0;
	}
	FM_Header h;
	memset(&h, 0, sizeof(h));
	if (fread(&h, sizeof(h), 1, f) != 1) { 
		if (errmsg) {
			*errmsg = "Failed to read header from file.";
		}
		fclose(f); 
		return 0; 
	}
	if (strncmp(h.magic, FM_MAGIC_STR, sizeof(h.magic))) { 
		if (errmsg) *errmsg = "Header appears invalid or is corrupt.";
		fclose(f); 
		return 0; 
	}
	
	FM_Context *c = new FM_Context;
	c->isOutput = false;
	c->file = f;
	c->imgOffsets.reserve(h.nFrames);
	c->width = h.width;
	c->height = h.height;
	if (!h.indexRecordOffset) {
		if (rebuildIndex) {
			if (!Reindex(c, h.nFrames, 0, 0, errmsg)) {
				delete c;
				return 0;
			}
		} else {
			if (errmsg) *errmsg = std::string("File ") + filename + " is missing the index record or is truncated.  Did you forget to call Finalize()?";
			delete c;
			return 0;
		}
	} else {
		// file has index record, so read it
		if ( fseeko(f, h.indexRecordOffset, SEEK_SET) ) {
			// eek! seek error! image file corrupt/truncated??
			if (errmsg) *errmsg = "Seek error for index record. File truncated?";
			delete c;
			return 0;
		}
		FM_IndexRecord ir;
		if ( fread(&ir, sizeof(ir), 1, f) != 1 ) {
			// read error in index record...
			if (errmsg) *errmsg = "Error reading index record.";
			delete c;
			return 0;
		}
		if (ir.magic != FM_INDEX_RECORD_MAGIC) {
			// eek, corrupt file??
			if (errmsg) *errmsg = "Index record appears invalid or corrupt.";
			delete c;
			return 0;
		}
		if (ir.length != h.nFrames*sizeof(uint64_t)) {
			// error.. trunacted file?
			if (errmsg) *errmsg = "Index record is too short. File truncated?";
			delete c;
			return 0;
		}
		c->imgOffsets.resize(h.nFrames);
		if ( fread(&c->imgOffsets[0], sizeof(uint64_t), h.nFrames, f) != h.nFrames ) {
			// error.. short object count/file truncated?
			if (errmsg) *errmsg = "Index record is too short. File truncated?";
			delete c;
			return 0;
		}
	}
	return c;
}

/// returns true if the filename is openable and readable as an .fmv file!
bool         FM_IsFMV(const char *filename)
{
	FILE *f = fopen(filename, "rb");
	if (!f) return false;
	FM_Header h;
	memset(&h, 0, sizeof(h));
	bool ret = true;
	fread(&h, sizeof(h), 1, f);
	if (strncmp(h.magic, FM_MAGIC_STR, sizeof(h.magic))) { ret = false; }
	fclose(f);
	return ret;
}

/// read a frame from the .fmv file
FM_Image * FM_ReadFrame(FM_Context *c, unsigned frame_id /* first frame is frame 0 */, std::string *errmsg)
{
	char frame_idstr[32];
	sprintf(frame_idstr, "%u", frame_id);
	
	if (!c || c->isOutput || frame_id >= c->imgOffsets.size()) {
		if (errmsg) *errmsg = std::string("Frame ") + frame_idstr + ": context invalid or requested frame exceeds number of frames in file.";
		return 0;
	}
	
//	off_t savedOff = ftello(c->file);
	if ( fseeko(c->file, c->imgOffsets[frame_id], SEEK_SET) ) {
		if (errmsg) *errmsg = std::string("Frame ") + frame_idstr + ": seek error.";
		return 0;
	}

	FM_Image *ret = new FM_Image;
	if ( fread(&ret->desc, sizeof(ret->desc), 1, c->file) != 1
		 || ret->desc.magic != FM_IMAGE_DESCRIPTOR_MAGIC) {
		if (errmsg) *errmsg = std::string("Frame ") + frame_idstr + ": cannot read frame descriptor or frame descriptor corrupt.";
		delete ret; 
		ret = 0;
	} else {		
#ifdef NO_QT
		ret->data = (uint8_t *)malloc(ret->desc.length);
		if (!ret->data) {
			if (errmsg) *errmsg = std::string("Frame ") + frame_idstr + ": out of memory for image data.";
			delete ret;
			ret = 0;
		} else {
			if ( fread((void *)ret->data, ret->desc.length, 1, c->file) != 1 ) {
				if (errmsg) *errmsg = std::string("Frame ") + frame_idstr + ": failed to read image data.";
				delete ret;
				ret = 0;
			} else if (ret->desc.comp) {
				void *out = 0;
				unsigned outbytes = 0;
				zUncompress(ret->data, ret->desc.length, &out, &outbytes);
				if (!out) {
					if (errmsg) *errmsg = std::string("Frame ") + frame_idstr + ": error decompressing image data. File corrupt?";
					delete ret;
					ret = 0;
				} else {
					ret->desc.comp = 0;
					ret->desc.length = outbytes;
					free(ret->data);
					ret->data = (uint8_t *)out;
#else
		ret->data.resize(ret->desc.length);
		if (ret->data.size() != (int)ret->desc.length) {
			if (errmsg) *errmsg = std::string("Frame ") + frame_idstr + ": out of memory for image data.";
			delete ret;
			ret = 0;
		} else {
			if ( fread((void *)ret->data.data(), ret->data.size(), 1, c->file) != 1 ) {
				if (errmsg) *errmsg = std::string("Frame ") + frame_idstr + ": failed to read image data.";
				delete ret;
				ret = 0;
			} else if (ret->desc.comp) {
				ret->data = qUncompress(ret->data);
				if (ret->data.isNull()) {
					if (errmsg) *errmsg = std::string("Frame ") + frame_idstr + ": error decompressing image data.";
					delete ret;
					ret = 0;
				} else {
					ret->desc.comp = 0;
					ret->desc.length = ret->data.size();
#endif
		
				}
			}
		}
	}
	
//	fseek(c->file, savedOff, SEEK_SET);
	return ret;
}

bool FM_CheckForErrors(const char *filename, void *arg, FM_ProgressFn pfun, FM_ErrorFn efun)
{
	std::string errmsg("");
	FM_Context *c = FM_Open(filename, &errmsg);
	if (!c) {
		if (efun) efun(arg, errmsg);
		return false;
	}
	const unsigned n = c->imgOffsets.size();
	int lastpct = -1;
	for (unsigned i = 0; i < n; ++i) {
		FM_Image *img = FM_ReadFrame(c, i, &errmsg);
		if (!img) {
			if (efun) efun(arg, errmsg);
			FM_Close(c);
			return false;
		}
		delete img;
		if (static_cast<int>(((i*100)+1) / n) > lastpct) {
			lastpct = ((i*100)+1) / n;
			if (pfun && !pfun(arg, lastpct)) {
				FM_Close(c);
				return false;
			}
		}
	}
	FM_Close(c);
	return true;
}
			
/*-----------------------------------------------------------------------------
 READ/WRITE FUNCTIONS (applicable to both)
 -----------------------------------------------------------------------------*/
/** call this on input or output.  MANDATORY for output: to finalize the image
 in both cases frees the context pointer, closes file, etc. */
void         FM_Close(FM_Context *c)
{
	if (!c) return;
	if (c->isOutput) {
		FM_Header h;
		fseeko(c->file, 0, SEEK_SET);
		if ( fread(&h, sizeof(h), 1, c->file) == 1 ) {
			// update header
			fseeko(c->file, 0, SEEK_END);
			h.indexRecordOffset = ftello(c->file);
			h.nFrames = c->imgOffsets.size();
			fseeko(c->file, 0, SEEK_SET);
			fwrite(&h, sizeof(h), 1, c->file);
		}
		// now write out the index record
		fseeko(c->file, 0, SEEK_END);
		FM_IndexRecord ir;
		ir.magic = FM_INDEX_RECORD_MAGIC;
		ir.length = c->imgOffsets.size() * sizeof(uint64_t);
		fwrite(&ir, sizeof(ir), 1, c->file);
		if ( c->imgOffsets.size() ) 
			fwrite(&c->imgOffsets[0], sizeof(uint64_t), c->imgOffsets.size(), c->file);
	}	
	delete c; 
}

bool         FM_RebuildIndex(const char *filename, void * arg, FM_ProgressFn progfn, FM_ErrorFn errfn)
{
	std::string errmsg;
	
	FILE *f = fopen(filename, "rb");
	if (!f) {
		if (errfn) errfn(arg, std::string("Cannot open ") + filename + " for reading: " + strerror(errno));
		return false;
	}
	FM_Header h;
	memset(&h, 0, sizeof(h));
	if (fread(&h, sizeof(h), 1, f) != 1) { 
		if (errfn) errfn(arg, "Failed to read header from file.");
		fclose(f); 
		return false; 
	}
	if (strncmp(h.magic, FM_MAGIC_STR, sizeof(h.magic))) { 
		if (errfn) errfn(arg, "Header appears invalid or is corrupt.");
		fclose(f); 
		return false; 
	}
	
	FM_Context *c = new FM_Context;
	if (!c) {
		if (errfn) errfn(arg, "Out of memory.");
		return false;
	}
	c->isOutput = false;
	c->file = f;
	c->imgOffsets.reserve(h.nFrames);
	c->width = h.width;
	c->height = h.height;
	
	if (!Reindex(c, h.nFrames, arg, progfn, &errmsg)) {
		if (errfn) errfn(arg, errmsg);
		FM_Close(c);
		return false;
	}
	fclose(c->file);
	f =  c->file = fopen(filename, "r+b");
	if (!f) {
		if (errfn) errfn(arg, "Cannot open file for writing!");
		FM_Close(c);
		return false;
	}
	int64_t offset = h.indexRecordOffset;
	int seektype = SEEK_SET;
	if ( !h.indexRecordOffset ) {
		fseeko(f, 0, SEEK_END);
		h.indexRecordOffset = ftello(f);
		fseeko(f, 0, SEEK_SET);
		if ( fwrite(&h, sizeof(h), 1, f) != 1 ) {
			if (errfn) errfn(arg, "Cannot re-write header!");
			FM_Close(c);
			return false;
		}
		offset = 0;
		seektype = SEEK_END;
	}
	if ( fseeko(f, offset, seektype) ) {
		if (errfn) errfn(arg, "Cannot seek to write index record!");
		FM_Close(c);
		return false;
	}
	FM_IndexRecord ir;
	ir.magic = FM_INDEX_RECORD_MAGIC;
	ir.length = c->imgOffsets.size() * sizeof(uint64_t);
	if ( fwrite(&ir, sizeof(ir), 1, f) != 1 ) {
		if (errfn) errfn(arg, "Cannot write new index record header!");
		FM_Close(c);
		return false;
	}
	if ( fwrite(&c->imgOffsets[0], sizeof(uint64_t), c->imgOffsets.size(), f) != c->imgOffsets.size() ) {
		if (errfn) errfn(arg, "Cannot write new index record; write returned short record count!");
		FM_Close(c);
		return false;
	}
	FM_Close(c);
	return true;
}

#if defined(NO_QT)
void zCompress(const void* data, unsigned nbytes, int compressionLevel,
			   void **out, unsigned *outbytes)
{
    if (nbytes == 0) {
		*out = malloc(4);
		memset(*out, 0, 4);
		*outbytes = 4;
        return ;
    }
    if (!data) {
        return;
	}
    if (compressionLevel < -1 || compressionLevel > 9)
        compressionLevel = -1;
	
    unsigned long len = nbytes + nbytes / 100 + 13;
    int res;
	*out = 0;
	*outbytes = 0;
	unsigned char *bazip;
    do {
		*out = realloc(*out, len + 4);
        res = ::compress2(((unsigned char*)(*out))+4, &len, (unsigned char*)data, nbytes, compressionLevel);
		
        switch (res) {
			case Z_OK:
				*out = realloc(*out, len + 4);
				bazip = (unsigned char *)*out;
				bazip[0] = (nbytes & 0xff000000) >> 24;
				bazip[1] = (nbytes & 0x00ff0000) >> 16;
				bazip[2] = (nbytes & 0x0000ff00) >> 8;
				bazip[3] = (nbytes & 0x000000ff);
				*outbytes = len + 4;
				break;
			case Z_MEM_ERROR:
				//qWarning("qCompress: Z_MEM_ERROR: Not enough memory");
				free(*out);
				*out = 0;
				break;
			case Z_BUF_ERROR:
				len *= 2;
				break;
        }
    } while (res == Z_BUF_ERROR);
}

void zUncompress(const void *d, unsigned nbytes,
				 void **out, unsigned *outbytes)
{
	const char *data = (const char *)d;
	if (!data || !out || !outbytes) {
//		qWarning("qUncompress: Data is null");
		return;
	}
	*out = 0;
	*outbytes = 0;
	
	if (nbytes <= 4) {
		//if (nbytes < 4 || (data[0]!=0 || data[1]!=0 || data[2]!=0 || data[3]!=0))
		//	qWarning("qUncompress: Input data is corrupted");
		return;
	}
	unsigned long expectedSize = (data[0] << 24) | (data[1] << 16) | (data[2] <<  8) | (data[3]      );
	unsigned long len = expectedSize;
	if (!len) len = 1;
	
	unsigned char *p = 0;
	
	while(1) {
		unsigned long alloc = len;
		if (len  >= static_cast<unsigned long>(1 << 31)) {
			//qWarning("qUncompress: Input data is corrupted");
			return;
		}
		p = (unsigned char *)realloc(p, alloc);
		if (!p) {
			return;
		}
		
		int res = ::uncompress((unsigned char*)p, &len, ((unsigned char*)data)+4, nbytes-4);
		
		switch (res) {
			case Z_OK:
				if (len != alloc) {
					if (len  >= (unsigned long)(1 << 31)) {
						//QByteArray does not support that huge size anyway.
						//qWarning("qUncompress: Input data is corrupted");
						free(p);
						return;
					}
				}
				*out = p;
				*outbytes = len;
				return;
				
				free(p);
				return;
				
			case Z_BUF_ERROR:
				len *= 2;
				continue;
				
			case Z_MEM_ERROR:
			case Z_DATA_ERROR:
			default:
				free(p);
				return;
		}
	}		
}
#endif
