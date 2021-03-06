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

#define THRESHOLD_FOR_CACHE_CLEANUP 32 /* number of display lists to store in cache before auto-cleanup of non-used display lists.  Tune this if you're getting dropped frames in MovingObjects with lots of objects..? */

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
};

	
class GradientShape : public Shape {	
public:
	
	enum GradType { None=0, Cos, Sin, Saw, Tri, Squ, N_GradTypes };
		
	GradientShape();
	virtual ~GradientShape();
	
	/*virtual*/ void copyProperties(const Shape *from);

	void setGradient(GradType t, float freq, float angle, float offset, float min, float max);

	virtual int typeId() const = 0;
	
	static void doCacheGC() { if (dcache) dcache->doAutoCleanup(); }
	
protected:

	GLuint gtex;
	
	GradType grad_type;
	float grad_freq,  ///< default 1.0
	      grad_angle,  ///< default 0.0
	      grad_offset; ///< default 0.0
	
	float grad_min, grad_max;
			
	GLuint dl; ///< if non-zero, child class should use this display list instead of the static one associated with the class

	void setupDl();
	virtual void defineDl() = 0;
	// child classes should call these if they reimplement them!!
	virtual void drawBegin();
	virtual void drawEnd();
	
private:
	struct TexCache {
		TexCache() : size_max(0), hits(0), misses(0) {}
		~TexCache();

		GLuint getAndRetain(GradType type, float min, float max);
		void release(GLuint tex_id);
		void release(GradType type, float min, float max);
				
		unsigned size() const { return ref.size(); }
		unsigned maxSize() const { return size_max; }
		unsigned cacheHits() const { return hits; }
		unsigned cacheMisses() const { return misses; }
		
	private:
		typedef QMap<GLuint, int> RefctMap;
		typedef QMap<GLuint, Vec3bf> TexPropMap;
		typedef QMap<Vec3bf, GLuint> PropTexMap;
		
		GLuint createTex(GradType type, float min, float max);
		
		RefctMap ref;
		TexPropMap texProp;
		PropTexMap propTex;
		unsigned size_max, hits, misses; ///< debug stat...
	};
	
	static TexCache *tcache;
	static int tcache_ct;
	
	class DLCache {
		typedef QMap<GLuint, int> RefctMap; 
		typedef QMap<GLuint,Vec5bf> Map;
		typedef QMap<Vec5bf,GLuint> Rev;
		RefctMap refs; /// maps dl_grad display lists to counters.. implementing shared display lists
		Map dls;
		Rev dlsRev;
		unsigned size_max, hits, misses; ///< debug stat
				
	public:
		DLCache() : size_max(0), hits(0), misses(0) {}
		~DLCache();
		GLuint getAndRetain(const Vec5bf & props); ///< props are tex_id,freq,angle,offset!
		unsigned count(GLuint dl) const;
		void release(GLuint dl);
		void retain(GLuint dl);		
		unsigned size() const { return refs.size(); }
		unsigned maxSize() const { return size_max; }
		unsigned cacheHits() const { return hits; }
		unsigned cacheMisses() const { return misses; }
		void doAutoCleanup(); ///< do garbage collection on old display lists that aren't used.  Only done if the size of the cache exceeds THRESHOLD_FOR_CACHE_CLEANUP.  Called indirectly from MovingObjects::afterVSync()
		
	};
	
	static DLCache *dcache;
	static int dcache_ct;	
};
	
class Rectangle : public GradientShape {
public: 
	double width, height;
	
	/*virtual*/ int typeId() const { return 1; }
	
protected:
	/*virtual*/ void defineDl();
	
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
public:
	double xdiameter,ydiameter;
	static const unsigned numVertices;

	/*virtual*/ int typeId() const { return 2; }
	
protected:

	/*virtual*/void defineDl();

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

