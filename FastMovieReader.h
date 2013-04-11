/*
 *  FastMovieReader.h
 *  StimulateOpenGL_II
 *
 *  Created by calin on 8/10/12.
 *  Copyright 2012 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#ifndef FastMovieReader_H
#define FastMovieReader_H


#include <QByteArray>
#include <QImage>
#include <QVector>
#include <QString>
#include <QIODevice>
#include <QImageReader>
#include <QPoint>


class GenericMovieReader : public QImageIOHandler
{
public:
	virtual ~GenericMovieReader();
	virtual bool randomAccessRead(QImage *image, int imgnum /* first image is 1, last is imageCount() */, int *compressedFrameSize = 0) = 0;
	virtual QByteArray name() const = 0;
	virtual QSize size() const = 0;
};

struct FM_Context;

class FastMovieReader : public GenericMovieReader
{
public:
    FastMovieReader(const QString &fileName);
    ~FastMovieReader();
	
	
	static bool canRead(const QString &fileName);
	
    bool canRead() const;
    bool read(QImage *image);
    bool write(const QImage &image);
	
	bool randomAccessRead(QImage *image, int imgnum /* first image is 1, last is imageCount() */, int *compressedFrameSize);
	
    QByteArray name() const { return fileName.toUtf8(); }
		
    QVariant option(ImageOption option) const; // unimpl
    void setOption(ImageOption option, const QVariant &value); // unimpl
    bool supportsOption(ImageOption option) const; // unimpl
	
    int imageCount() const;
    int loopCount() const { return -1; } // unimpl
    int nextImageDelay() const { return -1; } // unimpl
    int currentImageNumber() const { return -1; } // unimpl
		
	QSize size() const;
	
private:
	bool open() const;
	
    QString fileName;
	mutable FM_Context *ctx;
};



#endif
