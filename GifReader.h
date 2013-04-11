#ifndef GifReader_H
#define GifReader_H

#include <QByteArray>
#include <QImage>
#include <QVector>
#include <QString>
#include <QIODevice>
#include <QImageReader>
#include <QPoint>
#include "FastMovieReader.h"

class GIFFormat;
class GifReader : public GenericMovieReader
{
public:
    GifReader();
    ~GifReader();

    bool canRead() const;
    bool read(QImage *image);
    bool write(const QImage &image);

	bool randomAccessRead(QImage *image, int imgnum /* first image is 1, last is imageCount() */, int *compressedFrameSize);

    QByteArray name() const;

    static bool canRead(QIODevice *device);

    QVariant option(ImageOption option) const;
    void setOption(ImageOption option, const QVariant &value);
    bool supportsOption(ImageOption option) const;

    int imageCount() const;
    int loopCount() const;
    int nextImageDelay() const;
    int currentImageNumber() const;

	bool isAnimatedGifNonOptimized() const; ///< return true iff animated gif is non-optimized
	
	void copyImageLengthsAndOffsets(const GifReader & other);
	
	QSize size() const { if (!imageSizes.size()) return QSize(0,0); return imageSizes.front(); }
	
private:
    bool imageIsComing() const;
    GIFFormat *gifFormat;
    QString fileName;
    mutable QByteArray buffer;
    mutable QImage lastImage;

    mutable int nextDelay;
    mutable int loopCnt;
    int frameNumber;
    mutable QVector<QSize> imageSizes;
	mutable QVector<QPoint> topCorners;
	mutable QVector<int> imageOffsets, imageLengths;
    mutable bool scanIsCached, isOptimizedIsCached, isOptimized;
};

#endif // GifReader_H
