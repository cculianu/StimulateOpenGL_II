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

#define NUM_VERTICES_FOR_ELLIPSOIDS 128 /* number of vertices used to approximate ellipsoids (circles, ellipses)
higher number is better resolution, but sacrifices performance. */
#define NUM_VERTICES_FOR_SPHEROIDS 32 /* number of vertices used to approximate spheres */

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
	Vec3 position;  ///< basically, position, or the center of the AABB of the shape (shapes are anchored at center basically)
	Vec2 scale;     ///< basically, X/Y glScale
	Vec3 color;     ///< defaults to gray
	double angle; ///< the angle of rotation about the Z axis, in degrees
	bool noMatrixAttribPush; ///< defaults to false, if true, don't do the glPushAttrib()/glPushMatrix calls as a performance hack
	
public:
	Shape();
	virtual ~Shape();
	
	virtual void draw() = 0;
	
	virtual Rect AABB() const = 0; ///< must reimplement in subclasses to return the axis-aligned-bounding-box (with scale, rotation, and position applied!)
	
	virtual void setRadii(double r1, double r2) = 0;
	
	Vec2 bottomLeft() const { return AABB().origin; }
	
	static Vec2 canvasPosition(const Vec3 & real_position); ///< returns the canvas position given a real pos
	Vec2 canvasPosition() const; ///< returns the position of the object on the canvas after the object's Z position (distance) calculation is applied
	void setCanvasPosition(const Vec2 &);
	double distance() const; /**< the default distance, 1, means the object is on the 
							  default Z-plane.  2 makes the object 1/2 as small, 3 
							  1/4 as small, 4 1/8 as small, etc.  0 makes the object 
							  exist *ON* the camera (so it's no longer visible!), 
							  and values between 0 and 1 make the object be closer to 
							  the camera. */
	void setDistance(double d);
	
protected:
	virtual void drawBegin();
	virtual void drawEnd();
};

class Rectangle : public Shape {
	friend void CleanupStaticDisplayLists();
public: 
	double width, height;
		
protected:
	static GLuint dl; ///< shared display list for all rectangles since it's just a unit square and we do our magic in the scaling
	
public:
	Rectangle(double width, double height);
	
	/// not as trivial as it seems since we have to account for rotation of the box!
	Rect AABB() const;
	
	void setRadii(double r1, double r2) { width = r1; height = r2; }

	void draw();
};

class Square : public Rectangle {
public:
	Square(double length) : Rectangle(length, length) {}
};

class Ellipse : public Shape {
	friend void CleanupStaticDisplayLists();
public:
	double xradius,yradius;
	static const unsigned numVertices;
	
protected:
	static GLuint dl; ///< shared display list for _ALL_ ellipses since we use a unit circle at 0,0 with 128 vertices globally!
	
public:
	Ellipse(double radiusX, double radiusY);
	
	void draw();
	void setRadii(double r1, double r2) { xradius = r1; yradius = r2; }
	
	Rect AABB() const;
};

class Sphere : public Shape {
	friend void CleanupStaticDisplayLists();
public:
	
	static const GLfloat 
	                     // LIGHT PROPS
	                     DefaultLightAmbient[4], // .5,.5,.5,1.0
	                     DefaultLightDiffuse[4], // 1,1,1,1
	                     DefaultLightPosition[4], // 1,1,1,0
					     DefaultLightSpecular[4], // 1,1,1,1
	                     DefaultLightAttenuations[3], // 1,0,0
						 // MATERIAL PROPS
	                     DefaultSpecular[4], // 1,1,1,1
						 DefaultAmbient[4], // .2,.2,.2,1
						 DefaultDiffuse[4], // .8,.8,.8,1
						 DefaultEmission[4], // 0,0,0,1
						 DefaultShininess; // 50.0
	
	double radius;
	GLfloat lightAmbient[4], lightDiffuse[4], lightPosition[4], lightSpecular[4], 
	        specular[4], ambient[4], diffuse[4], emission[4], shininess;
	GLfloat lightAttenuations[3]; ///< constant, linear, and quadratic respectively
	
	Sphere(double radius);
	
	void draw();
	
	Rect AABB() const;
	
	void setRadii(double r1, double r2) { radius = r1; (void)r2; }
	
	bool lightIsFixedInSpace; ///< default false.  if true, then the light source is fixed in space and is not relative to the sphere
	
protected:
	static GLUquadricObj *quadric;
	static GLuint dl;
};
	
/// call this to pre-create the display lists for Rectangle and Ellipses so that it's ready for us and primed
void InitStaticDisplayLists();
void CleanupStaticDisplayLists();

} // end namespace Shapes

#endif

