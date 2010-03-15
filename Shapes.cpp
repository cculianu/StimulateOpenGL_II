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

namespace Shapes {
	
Shape::Shape() 
	: position(Vec2Zero), scale(Vec2Unit), color(Vec3Gray), angle(0.)
{}

Shape::~Shape() 
{}

void Shape::drawBegin() {
	glPushMatrix();
	glTranslated(position.x, position.y, 0.0);	
	glScaled(scale.x, scale.y, 1.0);
	glRotated(angle,0.,0.,1.);
	glGetDoublev(GL_CURRENT_COLOR, savedColor);
	glColor3d(color.r,color.g,color.b);
}

void Shape::applyChanges() { /* nothing.. */ }

void Shape::drawEnd() {
	glColor4d(savedColor[0], savedColor[1], savedColor[2], savedColor[3]);
	glPopMatrix();
}

Ellipse::Ellipse(double rx, double ry, unsigned numv)
	: xradius(rx), yradius(ry), numVertices(numv)
{
	sinCosTable.reserve(numVertices);
	const double incr = DEG2RAD(360.0) / numVertices;
	double radian = 0.;
	for (unsigned i = 0; i < numVertices; ++i) {
		sinCosTable.push_back(Vec2(cos(radian), sin(radian)));
		radian += incr;
	}
	vertices.resize(numVertices);
	applyChanges();
}

void Ellipse::applyChanges()
{
	//const Vec2 v0 (xradius, yradius); // anchor ellipse at bottom-left by offsetting all points by 'radius'
	
	for (unsigned i = 0; i < numVertices; ++i) {
		const Vec2 & tableval = sinCosTable[i];
		Vec2 & v = vertices[i];
		v.x = /*v0.x +*/ tableval.x*xradius;
		v.y = /*v0.y +*/ tableval.y*yradius;
	}
}

void Ellipse::draw() {
	drawBegin();
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_DOUBLE, 0, &vertices[0]);
	glDrawArrays(/*GL_LINE_LOOP*/GL_POLYGON, 0, numVertices);
	glDisableClientState(GL_VERTEX_ARRAY);		
	drawEnd();
}

Rect Ellipse::AABB() const { 
	const double r_max = xradius;
	const double r_min = yradius;
	const double rot = DEG2RAD(angle);
	double t_nil = atan( -r_min * tan(rot) / r_max);
	double t_inf = atan(r_min * (cos(rot) / sin(rot)) / r_max);
	double rect_width = scale.x * fabs(2. * (r_max * cos(t_nil) * cos(rot) - r_min * sin(t_nil) * sin(rot)));
	double rect_height = scale.y * fabs(2. * (r_min * sin(t_inf) * cos(rot) + r_max * cos(t_inf) * sin(rot)));
	return Rect(Vec2(position.x-rect_width/2.,position.y-rect_height/2.),Vec2(rect_width,rect_height));
}


Rectangle::Rectangle(double w, double h)
: width(w), height(h)
{
	applyChanges();
}

void Rectangle::applyChanges()
{
	vertices.resize(4);
/*  BOTTOM LEFT ANCHORING:
    vertices[0] = Vec2(0., 0.);
	vertices[1] = Vec2(width,  0.);
	vertices[2] = Vec2(width,  height);
	vertices[3] = Vec2(0, height);
 */
	// CENTER anchoring!
    vertices[0] = Vec2(-width/2., -height/2.);
	vertices[1] = Vec2(width/2.,  -height/2.);
	vertices[2] = Vec2(width/2.,  height/2.);
	vertices[3] = Vec2(-width/2., height/2.);
}

void Rectangle::draw() {
	drawBegin();
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_DOUBLE, 0, &vertices[0]);
	glDrawArrays(GL_QUADS, 0, vertices.size());
	glDisableClientState(GL_VERTEX_ARRAY);		
	drawEnd();
}



Rect Rectangle::AABB() const {
	const double theta = DEG2RAD(angle);
	const int n = vertices.size();
	double minx = 1e6, miny = 1e6, maxx = -1e6, maxy = -1e6;
	const double & x0 = position.x;
	const double & y0 = position.y;
	for (int i = 0; i < n; ++i) {
		const double & x = vertices[i].x+position.x;
		const double & y = vertices[i].y+position.y;
		const double dx = scale.x * (x-x0), dy = scale.y * (y-y0);
		double x2 = x0+dx*cos(theta)+dy*sin(theta);
		double y2 = y0-dx*sin(theta)+dy*cos(theta);
		if (x2 > maxx) maxx = x2;
		if (y2 > maxy) maxy = y2;
		if (x2 < minx) minx = x2;
		if (y2 < miny) miny = y2;
	}
	return Rect(Vec2(minx, miny), Vec2(maxx-minx, maxy-miny));
}

} // end namespace Shapes

bool Rect::intersects(const Rect & r) const {
	return !(right() <= r.left()
			 || left() >= r.right()
			 || top() <= r.bottom()
			 || bottom() >= r.top());
}