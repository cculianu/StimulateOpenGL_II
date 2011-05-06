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
#include <memory>
#include "MovingObjects.h"

namespace Shapes {

Shape::Shape() 
	: position(Vec3Zero), scale(Vec2Unit), color(Vec3Gray), angle(0.), noMatrixAttribPush(false)
{}

Shape::~Shape() 
{}

void Shape::drawBegin() {
	if (!noMatrixAttribPush) {
		glPushMatrix();
		glPushAttrib(GL_CURRENT_BIT);
	}
	const double d = distance() > 0.0 ? distance() : 0.000000000001;
	glTranslated(position.x/d, position.y/d, 0.0);
	// try and not do a glRotate call if angle is 0. (no rotation).  Hopefully this guard useful performance-wise..
	if (!eqf(angle, 0.)) glRotated(angle,0.,0.,1.);
	glScaled(scale.x/d, scale.y/d, 1.0);
	glColor3f(color.r,color.g,color.b);
}

void Shape::drawEnd() {
	if (!noMatrixAttribPush) {
		glPopAttrib();
		glPopMatrix();
	} else
		glLoadIdentity();
}
	

double Shape::distance() const {
	MovingObjects *p = (MovingObjects *)stimApp()->glWin()->runningPlugin();
	if (!p || !dynamic_cast<MovingObjects *>((StimPlugin *)p)) 
		p = (MovingObjects *)stimApp()->glWin()->pluginFind("movingobjects");
	if (p && dynamic_cast<MovingObjects *>((StimPlugin *)p)) 
		return p->zToDistance(position.z);
	return 0.;
}

void Shape::setDistance(double d) {
	double z = 0.;
	MovingObjects *p = (MovingObjects *)stimApp()->glWin()->runningPlugin();
	if (!p || !dynamic_cast<MovingObjects *>((StimPlugin *)p)) 
		p = (MovingObjects *)stimApp()->glWin()->pluginFind("movingobjects");
	if (p && dynamic_cast<MovingObjects *>((StimPlugin *)p)) 
		z = p->distanceToZ(d);
	position.z = z;
}

Vec2 Shape::canvasPosition() const {
	const double d = distance() > 0.0 ? distance() : 0.000000000001;
	return Vec2(position.x/d, position.y/d);
}

void Shape::setCanvasPosition(const Vec2 & v) {
	const double d = distance() > 0.0 ? distance() : 0.000000000001;
	position = Vec3(v.x*d, v.y*d, position.z);
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
	const double d = distance() > 0.0 ? distance() : 0.000000001;
	const double r_max (xradius/d);
	const double r_min (yradius/d);
	const double rot = DEG2RAD(angle);
	const double sin_rot = sin(rot);
	const double cos_rot = cos(rot);
	double t_nil = atan( -r_min * tan(rot) / r_max);
	double t_inf = atan(r_min * (cos_rot / sin_rot) / r_max);
	double rect_width = scale.x * fabs(2. * (r_max * cos(t_nil) * cos_rot - r_min * sin(t_nil) * sin_rot));
	double rect_height = scale.y * fabs(2. * (r_min * sin(t_inf) * cos_rot + r_max * cos(t_inf) * sin_rot));
	return Rect(Vec2(position.x/d-rect_width/2.,position.y/d-rect_height/2.),Vec2(rect_width,rect_height));
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
	const double d = distance() > 0.0 ? distance() : 0.000000001;
	
	if (!eqf(fmod(angle, 360.0),0.0)) {
		// we are rotated, do this complicated thing to account for rotation!

		const double theta = DEG2RAD(angle);
		const double cos_theta = cos(theta);
		const double sin_theta = sin(theta);
		const int n = 4;
		const Vec2 vertices[n] = {  Vec2( -.5*(width/d), -.5*(height/d) ),
									Vec2( .5*(width/d), -.5*(height/d) ),
									Vec2( .5*(width/d), .5*(height/d) ),
									Vec2( -.5*(width/d), .5*(height/d) ) };
			
		double minx = 1e6, miny = 1e6, maxx = -1e6, maxy = -1e6;
		const double x0 = position.x/d;
		const double y0 = position.y/d;
		for (int i = 0; i < n; ++i) {
			const double x = vertices[i].x + x0;
			const double y = vertices[i].y + y0;
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
	return Rect(
				Vec2( (-width/2. * scale.x + position.x)/d, (-height/2. * scale.y + position.y)/d ), 
				Vec2( (width*scale.x)/d, (height*scale.y)/d ) 
				);
}


void InitStaticDisplayLists() {
	Ellipse a(3,4);
	Rectangle b(4,5);

	(void)a; (void)b;
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
	gluDeleteQuadric(Sphere::quadric), Sphere::quadric = 0;
}

/* static */
const GLfloat
Sphere::DefaultLightAmbient[4] =  { 0.5f, 0.5f, 0.5f, 1.0f },
Sphere::DefaultLightDiffuse[4] =  { 1.0f, 1.0f, 1.0f, 1.0f },
Sphere::DefaultLightPosition[4] = { 400.0f, 250.0f, -100.0f, 0.0f },
Sphere::DefaultSpecular[4] =      { 1.0f, 1.0f, 1.0f, 1.0f },
Sphere::DefaultShininess =        50.0f;

/* static */ GLUquadricObj * Sphere::quadric = 0;
	
Sphere::Sphere(double radius, unsigned subdivisions)
: radius(radius), subdivisions(subdivisions), lightIsFixedInSpace(true)
{
	if (!quadric) {
		quadric = gluNewQuadric();
	}
	std::memcpy(lightAmbient, DefaultLightAmbient, sizeof(DefaultLightAmbient));
	std::memcpy(lightDiffuse, DefaultLightDiffuse, sizeof(DefaultLightDiffuse));
	std::memcpy(lightPosition, DefaultLightPosition, sizeof(DefaultLightPosition));
	std::memcpy(specular, DefaultSpecular, sizeof(DefaultSpecular));
	shininess = DefaultShininess;
}

void Sphere::draw()
{
	double d = distance();
	if (d < 0.0) d = 0.000000001;
	
	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
    glLoadIdentity();
	GLint vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);
    glOrtho( 0.0, vp[2], 0.0, vp[3], -10000.0, 10000.0 );	
	glMatrixMode( GL_MODELVIEW );
	
	glEnable(GL_LIGHTING);
	glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
	if (lightIsFixedInSpace) {
		glPushMatrix();
		glLoadIdentity();
		GLfloat l[4];
		std::memcpy(l, lightPosition, sizeof(l));
//		l[0] *= d;
//		l[1] *= d;
		//l[2] *= d;
		l[0] -= position.x;
		l[1] -= position.y;
		l[2] += position.z;
		glLightfv(GL_LIGHT0, GL_POSITION,l);
		glPopMatrix();
	} else {
		glLightfv(GL_LIGHT0, GL_POSITION,lightPosition);
	}
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);
	GLfloat specular_scaled[4];
	std::memcpy(specular_scaled, specular, sizeof(specular));
	for (int i = 0; i < 4; ++i) specular_scaled[i] /= d; // make the speculation shrink/grow with distance
	glMaterialfv(GL_FRONT, GL_SPECULAR, specular_scaled);
	glMaterialfv(GL_FRONT, GL_SHININESS, &shininess);
	
	const Vec2 scale_saved = scale;
	double radius_actual = scale.x * radius / d;
	scale.x = 1.0 * d; // multiply by d to counter-balance scaling in drawBegin()
	scale.y = 1.0 * d;
	double angle_saved = angle;
	angle = 0.; // angle is ignored for spheres 
	
	drawBegin();

	gluQuadricNormals(quadric, GLU_SMOOTH);
	gluQuadricDrawStyle(quadric , GLU_FILL);
	gluQuadricOrientation(quadric, GLU_OUTSIDE); 
	gluSphere(quadric, radius_actual, subdivisions, subdivisions);

	drawEnd();
	
	scale = scale_saved;
	angle = angle_saved;
	
	glDisable(GL_LIGHT0);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

Rect Sphere::AABB() const {
	const double d = distance() > 0.0 ? distance() : 0.000000001;
	return Rect(
				 Vec2( (position.x-radius*scale.x)/d,
					   (position.y-radius*scale.y)/d ),
				 Vec2( (2.*radius*scale.x)/d, (2.*radius*scale.y)/d )
				);	
}
	

} // end namespace Shapes

bool Rect::intersects(const Rect & r) const {
	return !(right() <= r.left()
			 || left() >= r.right()
			 || top() <= r.bottom()
			 || bottom() >= r.top());
}
