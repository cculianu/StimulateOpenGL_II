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
#include <QMap>
#include <QHash>

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
	
	virtual void setLengths(double l1, double l2) = 0;
	
	Vec2 bottomLeft() const { return AABB().origin; }
	
	static Vec2 canvasPosition(const Vec3 & real_position); ///< returns the canvas position given a real pos
	static Vec3 cposToRealPos(const Vec2 & canvas_position, double z);
	Vec2 canvasPosition() const; ///< returns the position of the object on the canvas after the object's Z position (distance) calculation is applied
	void setCanvasPosition(const Vec2 &);
	double distance() const; /**< the default distance, 1, means the object is on the 
							  default Z-plane.  2 makes the object 1/2 as small, 3 
							  1/4 as small, 4 1/8 as small, etc.  0 makes the object 
							  exist *ON* the camera (so it's no longer visible!), 
							  and values between 0 and 1 make the object be closer to 
							  the camera. */
	void setDistance(double d);
	
	virtual void copyProperties(const Shape *from);
	
protected:
	virtual void drawBegin();
	virtual void drawEnd();
	static GLuint tex_grad;
};

	
class GradientShape : public Shape {
public:
	
	enum GradType { GradCosine=0, GradSine, GradSaw, GradTri, GradSquare, N_GradTypes };
		
	GradientShape();
	virtual ~GradientShape();
	
	/*virtual*/ void copyProperties(const Shape *from);

	void setGradient(bool enabled, GradType t, float freq, float angle, float offset, float min, float max);

protected:
	
	struct TexCache {
		
		GLuint getAndRetain(GradType type, float min, float max);
		void release(GLuint tex_id);
		void release(GradType type, float min, float max);
		
		TexCache() : size_max(0) {}
		~TexCache();
		
		unsigned size() const { return ref.size(); }
		unsigned maxSize() const { return size_max; }
		
	private:
		typedef QMap<GLuint, int> RefctMap;
		typedef QMap<GLuint, Vec3f> TexPropMap;
		typedef QMap<Vec3f, GLuint> PropTexMap;
		
		GLuint createTex(GradType type, float min, float max);
		
		RefctMap ref;
		TexPropMap texProp;
		PropTexMap propTex;
		unsigned size_max; ///< debug stat...
	};
	
	static TexCache *tcache;
	static int tcache_ct;
	
	GLuint gtex;
	
	GradType grad_type;
	float grad_freq,  ///< default 1.0
	      grad_angle,  ///< default 0.0
	      grad_offset; ///< default 0.0
	
	float grad_min, grad_max;
		
	typedef QMap<GLuint, int> DLRefctMap; 
	typedef QMap<GLuint,Vec5f> DLMap;
	typedef QMap<Vec5f,GLuint> DLRev;
	static DLRefctMap dlRefcts; /// maps dl_grad display lists to counters.. implementing shared display lists
	static DLMap dls;
	static DLRev dlsRev;
	static GLuint dlGradGetAndRetain(const Vec5f & props); ///< props are tex_id,freq,angle,offset!
	static void dlGradRelease(GLuint dl);
	static void dlGradRetain(GLuint dl);
	
	GLuint dl_grad; ///< if non-zero, child class should use this display list instead of the static one associated with the class

	// child classes should call these if they reimplemen them!!
	virtual void setupDl(GLuint & displayList, bool use_grad_tex);
	virtual void drawBegin();
	virtual void drawEnd();
	
private:
	static int tex_grad_ct;
};
	
class Rectangle : public GradientShape {
	friend void CleanupStaticDisplayLists();
public: 
	double width, height;
		
protected:
	static GLuint dl; ///< shared display list for all rectangles since it's just a unit square and we do our magic in the scaling

	/*virtual*/ void setupDl(GLuint & displayList, bool use_grad_tex);
	
public:
	Rectangle(double width = 1., double height = 1.);
	
	/// not as trivial as it seems since we have to account for rotation of the box!
	Rect AABB() const;
	
	void setLengths(double l1, double l2) { width = l1; height = l2; }
	
	void draw();

	void copyProperties(const Shape *from);
};

class Square : public Rectangle {
public:
	Square(double length = 1.) : Rectangle(length, length) {}
};

class Ellipse : public GradientShape {
	friend void CleanupStaticDisplayLists();
public:
	double xdiameter,ydiameter;
	static const unsigned numVertices;

protected:
	static GLuint dl; ///< shared display list for _ALL_ ellipses since we use a unit circle at 0,0 with 128 vertices globally!

	/*virtual*/void setupDl(GLuint & displayList, bool use_grad_tex);

public:
	Ellipse(double diamX = 1., double diamY = 1.);
	
	void draw();
	void setLengths(double l1, double l2) { xdiameter = l1; ydiameter = l2; }
	
	Rect AABB() const;
	
	void copyProperties(const Shape *from);
private:
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
	
	double diameter;
	GLfloat lightAmbient[4], lightDiffuse[4], lightPosition[4], lightSpecular[4], 
	        specular[4], ambient[4], diffuse[4], emission[4], shininess;
	GLfloat lightAttenuations[3]; ///< constant, linear, and quadratic respectively
	bool lightIsFixedInSpace; ///< default false.  if true, then the light source is fixed in space and is not relative to the sphere
	
	Sphere(double diameter = 1.0);
	
	void draw();
	
	Rect AABB() const;
	
	void setLengths(double l1, double l2) { diameter = l1; (void)l2; }
	
	void copyProperties(const Shape *from);
	
protected:
	static GLUquadricObj *quadric;
	static GLuint dl;
	
private:
	GLfloat lightPosition_xf[4]; ///< tmp buffer used internally for computed lightPosition
};
	
/// call this to pre-create the display lists for Rectangle and Ellipses so that it's ready for us and primed
void InitStaticDisplayLists();
void CleanupStaticDisplayLists();

} // end namespace Shapes

extern uint qHash(const Vec4f &);
#endif

