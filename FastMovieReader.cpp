/*
 *  FastMovieReader.cpp
 *  StimulateOpenGL_II
 *
 *  Created by calin on 8/10/12.
 *  Copyright 2012 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */

#include "FastMovieReader.h"
#include "FastMovieFormat.h"
#include "Util.h"
#include <string.h>
#include <QVariant>

GenericMovieReader::~GenericMovieReader() {}

FastMovieReader::FastMovieReader(const QString &fileName)
: fileName(fileName), ctx(0) 
{}

FastMovieReader::~FastMovieReader()
{
	if (ctx) delete ctx;
	ctx = 0;
}

bool FastMovieReader::open() const
{
	if (ctx) return true;
	ctx = FM_Open(fileName.toUtf8().constData());
	if (!ctx) return false;
	return true;
}

bool FastMovieReader::canRead() const
{
	if (!ctx && !open()) return false;
	return !!ctx;
}

/* static */
bool FastMovieReader::canRead(const QString &fileName)
{
	return FM_IsFMV(fileName.toUtf8().constData());
}

int FastMovieReader::imageCount() const
{
	if (!ctx && !open()) return 0;
	if (ctx) return ctx->imgOffsets.size();
	return 0;
}

bool FastMovieReader::read(QImage *image)
{
	(void)image;
	return false;
	// unimplemented!
}

bool FastMovieReader::write(const QImage &image)
{
	// unimplemented!
	(void)image;
	return false;
}

bool FastMovieReader::supportsOption(ImageOption option) const
{
  return option == Size || option == Animation;
}

QVariant FastMovieReader::option(ImageOption option) const
{
    Q_UNUSED(option);
	return QVariant();
}
void FastMovieReader::setOption(ImageOption option, const QVariant &value)
{
    Q_UNUSED(option);
    Q_UNUSED(value);	
}


QSize FastMovieReader::size() const {
	if (!ctx && !open()) return QSize();
	if (ctx) {
		return QSize(ctx->width, ctx->height);
	}
	return QSize();
}

bool FastMovieReader::randomAccessRead(QImage *image, int imgnum, int *compFrameSize)
{
	--imgnum; // internally img numbers are 0-based
	if (!image && !ctx && !open() && (imgnum < 0 || imgnum >= (int)ctx->imgOffsets.size())) return false;
	if (ctx) {
		FM_Image *img = FM_ReadFrame(ctx, imgnum, 0, compFrameSize);
		if (!img) return false;
		if (img->desc.fmt != FM_LUMINOSITY && img->desc.bitdepth != 8) {
			Error() << "FastMovie fmt and bitdepth are not 0 and 8, respectively!";
			delete img;
			return false;
		}
		
#define FAST_SCAN_LINE(bits, bpl, y) (bits + (y) * bpl)
		
		
		QImage im(img->desc.width, img->desc.height, QImage::Format_Indexed8);
		if (im.byteCount() < img->data.size()) {
			Error() << "FastMovie returned image and expected image size mismatch!";
			delete img;
			return false;
		}
		int bpl1 = im.bytesPerLine(), bpl2 = img->desc.width;
		uchar *bits1 = im.bits(), *bits2 = (uchar *)img->data.constData();
		for (int y = 0; y < (int)img->desc.height; ++y) {
			memcpy(FAST_SCAN_LINE(bits1, bpl1, y), FAST_SCAN_LINE(bits2, bpl2, y), bpl2);
		}
		delete img;
		*image = im;
		return true;
	}
	return false;
}