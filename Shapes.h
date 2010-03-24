/*
 *  Shapes.h
 *  StimulateOpenGL_II
 *
 *  Created by calin on 3/11/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#ifndef Shapes_H
#define Shapes_H
#include "Util.h"
#include <vector>
#include "GLHeaders.h"

struct Rect {
	Vec2 origin; // bottom-left corner
	Vec2 size;   // width, height
	Rect(const Vec2 & o=Vec2Zero, const Vec2 & s=Vec2Zero) : origin(o), size(s) {}
	
	bool intersects(const Rect & r) const;
	
	inline double left() const { return origin.x; }
	inline double right() const { return origin.x + size.w; }
	inline double bottom() const { return origin.y; }
	inline double top() const { return origin.y + size.h; }
};

/** this namespace is needed because stupid Windows headers pollute the global namespace with
	'Ellipse' and 'Rectangle' already! */
namespace Shapes { 

class Shape {
public:
	Vec2 position,  ///< basically, position, or the center of the AABB of the shape (shapes are anchored at center basically)
	     scale;        ///< basically, X/Y glScale
	Vec3 color;        ///< defaults to gray
	double angle; ///< the angle of rotation about the Z axis, in degrees
	bool noMatrixAttribPush; ///< defaults to false, if true, don't do the glPushAttrib()/glPushMatrix calls as a performance hack
	
public:
	Shape();
	virtual ~Shape();
	
	virtual void draw() = 0;
	
	virtual void applyChanges(); ///< default impl does nothing -- subclasses may regenerate vertices here, etc

	virtual Rect AABB() const = 0; ///< must reimplement in subclasses to return the axis-aligned-bounding-box (with scale, rotation, and position applied!)
	
	Vec2 bottomLeft() const { return AABB().origin; }
	
protected:
	virtual void drawBegin();
	virtual void drawEnd();
};

class Rectangle : public Shape {
public: 
	double width, height;
	
protected:
	static GLuint dl; ///< shared display list for all rectangles since it's just a unit square and we do out magic in the scaling
	
public:
	Rectangle(double width, double height);
	
	/// not as trivial as it seems since we have to account for rotation of the box!
	Rect AABB() const;
	
	void draw();
};

class Square : public Rectangle {
public:
	Square(double length) : Rectangle(length, length) {}
};

class Ellipse : public Shape {
public:
	double xradius,yradius;
	static const unsigned numVertices;
	
protected:
	static GLuint dl; ///< shared display list for _ALL_ ellipses since we use a unit circle at 0,0 with 128 vertices globally!
	
public:
	Ellipse(double radiusX, double radiusY);
	
	void draw();
	
	Rect AABB() const;
};

	
/// call this to pre-create the display lists for Rectangle and Ellipses so that it's ready for us and primed
void DoPerformanceHackInit();

} // end namespace Shapes

#endif

