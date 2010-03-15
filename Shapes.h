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

class Shape {
public:
	Vec2 position,  ///< basically, position, or the center of the AABB of the shape (shapes are anchored at center basically)
	     scale;        ///< basically, X/Y glScale
	Vec3 color;        ///< defaults to gray
	double angle; ///< the angle of rotation about the Z axis, in degrees
	double savedColor[4];
	
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
	std::vector<Vec2> vertices;
	
public:
	Rectangle(double width, double height);
	
	void applyChanges(); ///< call this if you change length to apply and regenerate vertices

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
	const unsigned numVertices;
	
protected:
	std::vector<Vec2> sinCosTable;
	std::vector<Vec2> vertices;
	
public:
	Ellipse(double radiusX, double radiusY, unsigned numVertices);
	
	void applyChanges(); ///< call this for class properties/parameters changes to take effect, before calling draw
	
	void draw();
	
	Rect AABB() const;
};

#endif

