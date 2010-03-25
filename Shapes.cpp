/*
 *  Shapes.cpp
 *  StimulateOpenGL_II
 *
 *  Created by calin on 3/11/10.
 *  Copyright 2010 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#include "Shapes.h"
#include "GLHeaders.h"

#define NUM_VERTICES_FOR_ELLIPSOIDS 128 /* number of vertices used to approximate ellipsoids (circles, ellipses)
higher number is better resolution, but sacrifices performance. */

namespace Shapes {
	
Shape::Shape() 
	: position(Vec2Zero), scale(Vec2Unit), color(Vec3Gray), angle(0.), noMatrixAttribPush(false)
{}

Shape::~Shape() 
{}

void Shape::drawBegin() {
	if (!noMatrixAttribPush) {
		glPushMatrix();
		glPushAttrib(GL_CURRENT_BIT);
	}
	glTranslated(position.x, position.y, 0.0);
	// try and not do a glRotate call if angle is 0. (no rotation).  Hopefully this guard useful performance-wise..
	if (!eqf(angle, 0.)) glRotated(angle,0.,0.,1.);
	glScaled(scale.x, scale.y, 1.0);
	glColor3f(color.r,color.g,color.b);
}

void Shape::applyChanges() { /* nothing.. */ }

void Shape::drawEnd() {
	if (!noMatrixAttribPush) {
		glPopAttrib();
		glPopMatrix();
	} else
		glLoadIdentity();
}

/* static */ GLuint Ellipse::dl = 0;
const unsigned Ellipse::numVertices = NUM_VERTICES_FOR_ELLIPSOIDS;
	
Ellipse::Ellipse(double rx, double ry)
	: xradius(rx), yradius(ry)
{
	if (!dl) {
		const double incr = DEG2RAD(360.0) / numVertices;
		double radian = 0.;
		dl = glGenLists(1);
		glNewList(dl, GL_COMPILE);
			glBegin(GL_POLYGON);
				for (unsigned i = 0; i < numVertices; ++i) {
					glVertex2d(cos(radian), sin(radian));
					radian += incr;
				}
			glEnd();
		glEndList();
	}	
}
	
void Ellipse::draw() {
	Vec2 scale_saved = scale;
	// emulate the xradius,yradius thing with just a glScale.. muahahaha!
	scale.x *= xradius;
	scale.y *= yradius;
	drawBegin();
	glCallList(dl);
	drawEnd();
	scale = scale_saved;
}

Rect Ellipse::AABB() const { 
	const double & r_max (xradius);
	const double & r_min (yradius);
	const double rot = DEG2RAD(angle);
	const double sin_rot = sin(rot);
	const double cos_rot = cos(rot);
	double t_nil = atan( -r_min * tan(rot) / r_max);
	double t_inf = atan(r_min * (cos_rot / sin_rot) / r_max);
	double rect_width = scale.x * fabs(2. * (r_max * cos(t_nil) * cos_rot - r_min * sin(t_nil) * sin_rot));
	double rect_height = scale.y * fabs(2. * (r_min * sin(t_inf) * cos_rot + r_max * cos(t_inf) * sin_rot));
	return Rect(Vec2(position.x-rect_width/2.,position.y-rect_height/2.),Vec2(rect_width,rect_height));
}


/* static */ GLuint Rectangle::dl = 0; ///< shared display list for all rectangles!
	
Rectangle::Rectangle(double w, double h)
: width(w), height(h)
{
	if (!dl) {
		dl = glGenLists(1);
		// put the unit square in a display list
		glNewList(dl, GL_COMPILE);
			glBegin(GL_QUADS);	
				glVertex2d(-.5, -.5);
				glVertex2d(.5, -.5);
				glVertex2d(.5, .5);
				glVertex2d(-.5, .5);
			glEnd();
		glEndList();
	}
}


void Rectangle::draw() {
	Vec2 scale_saved = scale;
	// emulate the width and height thing with just a glScale.. muahahaha!
	scale.x *= width;
	scale.y *= height;
	drawBegin();
	glCallList(dl);
	drawEnd();
	scale = scale_saved;
}



Rect Rectangle::AABB() const {
	if (!eqf(fmod(angle, 360.0),0.0)) {
		// we are rotated, do this complicated thing to account for rotation!

		const double theta = DEG2RAD(angle);
		const double cos_theta = cos(theta);
		const double sin_theta = sin(theta);
		const int n = 4;
		const Vec2 vertices[n] = {  Vec2( -.5*width, -.5*height ),
									Vec2( .5*width, -.5*height ),
									Vec2( .5*width, .5*height ),
									Vec2( -.5*width, .5*height ) };
			
		double minx = 1e6, miny = 1e6, maxx = -1e6, maxy = -1e6;
		const double & x0 = position.x;
		const double & y0 = position.y;
		for (int i = 0; i < n; ++i) {
			const double & x = vertices[i].x+position.x;
			const double & y = vertices[i].y+position.y;
			const double dx = scale.x * (x-x0), dy = scale.y * (y-y0);
			double x2 = x0+dx*cos_theta+dy*sin_theta;
			double y2 = y0-dx*sin_theta+dy*cos_theta;
			if (x2 > maxx) maxx = x2;
			if (y2 > maxy) maxy = y2;
			if (x2 < minx) minx = x2;
			if (y2 < miny) miny = y2;
		}
		return Rect(Vec2(minx, miny), Vec2(maxx-minx, maxy-miny));
	}
	
	// else.. we are not rotatated
	// since we are aligned to origin, return *this rectangle* (scaled)
	return Rect(Vec2(-width/2. * scale.x, -height/2. * scale.y), Vec2(width*scale.x, height*scale.y));
}

void InitStaticDisplayLists() {
	Ellipse a(3,4);
	Rectangle b(4,5);
	
	// noops, but call it to ensure above isn't compile out or warned against
	a.applyChanges(); 
	b.applyChanges();	
}

static void DLCleanup(GLuint & dl) {
	if (dl) {
		glDeleteLists(dl,1);
		dl = 0;
	}
}
	
void CleanupStaticDisplayLists() {
	DLCleanup(Ellipse::dl);
	DLCleanup(Rectangle::dl);
}

	
} // end namespace Shapes

bool Rect::intersects(const Rect & r) const {
	return !(right() <= r.left()
			 || left() >= r.right()
			 || top() <= r.bottom()
			 || bottom() >= r.top());
}