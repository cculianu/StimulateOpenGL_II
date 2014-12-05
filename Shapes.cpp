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
#include <math.h>
#include "Util.h"
#include <QHash>

static const bool excessiveDebug = false;

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
	Vec2 cpos (canvasPosition());
	glTranslated(cpos.x, cpos.y, 0.0);
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

void Shape::copyProperties(const Shape *o)
{
	position = o->position;
	scale = o->scale;
	color = o->color;
	angle = o->angle;
	noMatrixAttribPush = o->noMatrixAttribPush;
}

	
/* static */ GradientShape::TexCache * GradientShape::tcache(0);
/* static */ int GradientShape::tcache_ct(0);
/* static */ GradientShape::DLCache *GradientShape::dcache; /// maps dl_grad display lists to counters.. implementing shared display lists
/* static */ int GradientShape::dcache_ct(0);

GradientShape::GradientShape() : gtex(0), grad_type(None), grad_freq(1.f), grad_angle(0.f), grad_offset(0.f), grad_min(0.f), grad_max(1.f), dl(0)
{
	if (!tcache) tcache = new TexCache, tcache_ct = 0;
	if (!dcache) dcache = new DLCache, dcache_ct = 0;
	++tcache_ct;
	++dcache_ct;
}	
GradientShape::~GradientShape()
{
	if (tcache) tcache->release(gtex); 
	gtex = 0;
	if (dcache) dcache->release(dl);
	dl = 0;
	if (tcache && --tcache_ct <= 0) {
		Debug() << "Gradient tex cache delete (cache current size: " << tcache->size() << " max size was: " << tcache->maxSize() << ")...";
		delete tcache; 
		tcache = 0;
		tcache_ct = 0;
	}
	if (dcache && --dcache_ct <= 0) {
		Debug() << "Display list cache delete (cache current size: " << dcache->size() << " max size was: " << dcache->maxSize() << ")...";
		delete dcache; 
		dcache = 0;
		dcache_ct = 0;
	}
}
	
	
void GradientShape::copyProperties(const Shape *from)
{
	Shape::copyProperties(from);
	const GradientShape *g;
	if ((g = dynamic_cast<const GradientShape *>(from))) {
		setGradient(g->grad_type, g->grad_freq, g->grad_angle, g->grad_offset, g->grad_min, g->grad_max);
	}
}

void GradientShape::setGradient(GradType t, float freq, float angle, float offset, float min, float max)
{
	grad_freq = freq;
	grad_angle = angle;
	grad_offset = offset;
	grad_type = t;
	if (min > max) { float tmp = min; min = max; max = tmp; }
	if (min < 0.f) min = 0.f; if (min > 1.f) min = 1.f;
	if (max < 0.f) max = 0.f; if (max > 1.f) max = 1.f;
	grad_min = min;
	grad_max = max;
	GLuint olddl = dl;
	setupDl();
	dcache->release(olddl);
}

void GradientShape::setupDl()
{
	const GLuint old_gtex = gtex;
	gtex = tcache->getAndRetain(grad_type, grad_min, grad_max);
	if (!typeId()) {
		Error() << "GradientShape::setupDl() -- cannot determine type_id for object! FIXME!";
	}
	dl = dcache->getAndRetain(Vec5bf(typeId(),gtex,grad_freq,grad_angle,grad_offset));
	const int ct = dcache->count(dl);
	if (excessiveDebug) Debug() << "setupDl(): gradient tex=" << gtex << ", dl=" << dl << " dlrefct=" << ct;
	tcache->release(old_gtex);
	// continue on in subclass..
	if (ct > 1) {
		if (excessiveDebug) Debug() << "GradientShape::setupDl(): dl " << dl << " already setup (has refct), aborting early..." ;
		return; // was already set up!
	} 
	else if (excessiveDebug) Debug() << "GradientShape:setupDl(): no performance improvement possible.. continuing on to call defineDl()...";	
	defineDl();
}
	
/* virtual */
void GradientShape::drawBegin()
{
	Shape::drawBegin();
	if (gtex) glEnable(GL_TEXTURE_1D);
	// continued in subclass
}
	
/* virtual */
void GradientShape::drawEnd()
{
	// continues from subclass..
	if (gtex) glDisable(GL_TEXTURE_1D);
	Shape::drawEnd();
}

unsigned GradientShape::DLCache::count(GLuint dl) const { 
	RefctMap::const_iterator it = refs.find(dl);
	if (it != refs.end()) return it.value();
	return 0;
}

GLuint GradientShape::DLCache::getAndRetain(const Vec5bf & props)
{
	GLuint ret = 0;
	Rev::const_iterator it = dlsRev.find(props);
	if (it != dlsRev.end())
		ret = it.value();
	if (!ret) {
		ret = glGenLists(1);
		Debug() << "DisplayList " << ret << " created.";
		if (ret) {
			dls[ret] = props;
			dlsRev[props] = ret;
		}
	}
	retain(ret);
	return ret;
}
void GradientShape::DLCache::release(GLuint dl)
{
	if (!dl) return;
	RefctMap::iterator it = refs.find(dl);
	if (it != refs.end()) {
		if (--(it.value()) <= 0) {
			if (dl) glDeleteLists(dl, 1);
			Debug() << "DisplayList " << dl << " deleted.";
			refs.erase(it);
			Map::iterator dlm_it = dls.find(dl);
			if (dlm_it != dls.end()) {
				dlsRev.remove(dlm_it.value());
				dls.erase(dlm_it);
			}
		}
	}
}
void GradientShape::DLCache::retain(GLuint dl)
{
	if (dl) {
		refs[dl] = refs[dl] + 1;
		if (size_max < size()) size_max = size();
	}
}
	
GradientShape::DLCache::~DLCache()
{
	for(RefctMap::const_iterator it = refs.begin(); it != refs.end(); ++it) {
		GLuint d = it.key();
		if (d) glDeleteLists(d, 1);
	}
	refs.clear(); dls.clear(); dlsRev.clear();
}


void GradientShape::TexCache::release(GLuint tex)
{
	if (!tex) return;
	RefctMap::iterator it = ref.find(tex);
	if (it != ref.end()) {
		if (--it.value() <= 0) {
			TexPropMap::iterator it2 = texProp.find(tex);
			if (it2 != texProp.end())  propTex.remove(it2.value());
			texProp.erase(it2);
			ref.erase(it);
			if (excessiveDebug) Debug() << "Deleting texture " << tex;
			glDeleteTextures(1, &tex);
			Debug() << "Texture " << tex << " deleted.";
		}
	} else {
		Debug() << "GradientShape::TexCache::release was given a tex_id `" << tex << "' but it doesn't exist in the cache!";
	}
}

void GradientShape::TexCache::release(GradType t, float min, float max)
{
	if (int(t) < 0 || int(t) > N_GradTypes) t = (GradType)0;
	if (t == None) return;
	if (min > max) { float t = min; min = max; max = t; }
	if (min < 0.f) min = 0.f; else if (min > 1.f) min = 1.f;
	if (max < 0.f) max = 0.f; else if (max > 1.f) max = 1.f;
	PropTexMap::const_iterator it = propTex.find(Vec3bf(float(int(t)), min, max));
	if (it != propTex.end()) release(it.value());
}
	
GLuint GradientShape::TexCache::getAndRetain(GradType t, float min, float max)
{
	if (int(t) < 0 || int(t) > N_GradTypes) t = (GradType)0;
	if (t == None) return 0;
	GLuint ret = 0;
	if (min > max) { float t = min; min = max; max = t; }
	if (min < 0.f) min = 0.f; else if (min > 1.f) min = 1.f;
	if (max < 0.f) max = 0.f; else if (max > 1.f) max = 1.f;
	const Vec3bf prop(float(int(t)),min,max);
	PropTexMap::const_iterator it = propTex.find(prop);
	if (it != propTex.end()) {
		RefctMap::iterator it2 = ref.find(it.value());
		if (it2 != ref.end()) {
			++it2.value();
			ret = it.value();
		} else {
			Error() << "INTERNAL ERROR IN GradientShape::TexCache::getAndRetain() -- prop is in propTex but not in ref! FIXME!";
		}
		if (excessiveDebug && ret) Debug() << "GradientShape::TexCache::getAndRetain(" << int(t) << "," << min << "," << max << ") found cached texture with id " << ret << " refct: " << (it2.value()-1);
	} else { // create new..
		if (excessiveDebug) Debug() << "propTex size: " << propTex.size();
		if ((ret = createTex(t, min, max))) {
			propTex[prop] = ret;
			texProp[ret] = prop;
			ref[ret] = 1;
			if (size_max < unsigned(ref.size())) size_max = ref.size();
		} else if (t != None) {
			Error() << "Got a 0 from GradientShape::TexCache::createTex()!";
		}
	}
	return ret;
}
	
GradientShape::TexCache::~TexCache()
{
	for(RefctMap::const_iterator it = ref.begin(); it != ref.end(); ++it) {
		GLuint t = it.key();
		if (t) glDeleteTextures(1, &t);
	}
	ref.clear(); propTex.clear(); texProp.clear();
}
	
GLuint GradientShape::TexCache::createTex(GradType t, float min, float max)
{
	if (t == None) return 0;
	static const int TEXWIDTH = 256;
	GLuint tex = 0;
	glGenTextures(1, &tex);
	GLfloat pix[TEXWIDTH], f;
	for (int i = 0; tex && i < TEXWIDTH; ++i) {
		const GLfloat x(GLfloat(i)/GLfloat(TEXWIDTH-1)); // we do it this way so we can get 0.0 and 1.0 in our x domain..
		switch (t) {
			case Squ:
				f = x < 0.5f ? 0.0f : 1.0f;
				break;
			case Saw: {
				static const float swfact = (1.0f/0.95f);
				f = x*swfact;
				if (f >= 1.0f) {
					f = 1.0f-((f-1.0f)/(swfact-1.0f));
				}
			}
				break;
			case Tri:
				f = x*2.0f;
				if (f > 1.0) f = 1.0f-(f-1.0f);
				break;
			case Sin:
				f = (sinf(x*(2.0*M_PI))+1.0)/2.0;
				break;
			case None:
				glDeleteTextures(1, &tex);
				tex = 0;
				break;
			case Cos:
			default:
				f = (cosf(x*(2.0*M_PI))+1.0)/2.0;
				break;
		}
		if (f < 0.f) f = 0.f;
		if (f > 1.f) f = 1.f;			
		f = f*(max-min) + min;
		pix[i] = f;
	}
	glBindTexture(GL_TEXTURE_1D, tex);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_LUMINANCE, TEXWIDTH, 0, GL_LUMINANCE, GL_FLOAT, pix);
	glBindTexture(GL_TEXTURE_1D, 0); // unbind
	Debug() << "GradientShape::TexCache::createTex created tex " << tex << " of size " << TEXWIDTH << " type: " << int(t) << " min: " << min << " max: " << max;
	return tex;
}
	
void Rectangle::copyProperties(const Shape *o)
{
	GradientShape::copyProperties(o);	
	const Rectangle *r;
	if ((r=dynamic_cast<const Rectangle *>(o))) {
		width = r->width;
		height = r->height;
	}
}

void Ellipse::copyProperties(const Shape *o)
{
	GradientShape::copyProperties(o);
	const Ellipse *e;
	if ((e = dynamic_cast<const Ellipse *>(o))) {
		xdiameter = e->xdiameter;
		ydiameter = e->ydiameter;
	}
}

void Sphere::copyProperties(const Shape *o)
{
	Shape::copyProperties(o);
	const Sphere *s;
	if ((s=dynamic_cast<const Sphere *>(o))) {
		diameter = s->diameter;
		std::memcpy(lightAmbient, s->lightAmbient, sizeof(lightAmbient));
		std::memcpy(lightDiffuse, s->lightDiffuse, sizeof(lightDiffuse));
		std::memcpy(lightPosition, s->lightPosition, sizeof(lightDiffuse));
		std::memcpy(lightSpecular, s->lightSpecular, sizeof(lightSpecular));
		std::memcpy(ambient, s->ambient, sizeof(ambient));
		std::memcpy(diffuse, s->diffuse, sizeof(diffuse));
		std::memcpy(emission, s->emission, sizeof(emission));
		std::memcpy(specular, s->specular, sizeof(specular));
		std::memcpy(lightAttenuations, s->lightAttenuations, sizeof(lightAttenuations));
		shininess = s->shininess;
		lightIsFixedInSpace = s->lightIsFixedInSpace;
	}
}
	
static MovingObjects * movingObjects()
{
	MovingObjects *p = (MovingObjects *)stimApp()->glWin()->runningPlugin();
	if (!p || !dynamic_cast<MovingObjects *>((StimPlugin *)p)) {
		p = (MovingObjects *)stimApp()->glWin()->pluginFind("movingobjects");
		if (p) p = dynamic_cast<MovingObjects *>(p);
	}
	if (!p) Error() << "INTERNAL ERROR -- Cannot find MovingObjects plugin!";	
	return p;
}

double Shape::distance() const {
	MovingObjects *p = movingObjects();
	if (p) return p->zToDistance(position.z);
	return 0.;
}

void Shape::setDistance(double d) {
	double z = 0.;
	MovingObjects *p = movingObjects();
	if (p) z = p->distanceToZ(d);
	position.z = z;
}

Vec2 Shape::canvasPosition() const { return canvasPosition(position); }

void Shape::setCanvasPosition(const Vec2 & v) {
	position = cposToRealPos(v, position.z);
}
	
/*static*/
Vec2 Shape::canvasPosition(const Vec3 & position)
{
	MovingObjects *m = movingObjects();
	Vec2 mid (m->width()*.5, m->height()*.5);
	double d = 0.0;
	if (m) d = m->zToDistance(position.z);
	if (eqf(d,0.0)) d=1e-9;
	Vec2 p(position.x-mid.x, position.y-mid.y);
	return p/d + mid;
}

/* static */
Vec3 Shape::cposToRealPos(const Vec2 & cpos, double z)
{
	// cpos = (pos-mid)/d + mid
	// cpos - mid = (pos-mid)/d
	// (cpos - mid)*d = pos-mid
	// (cpos - mid)*d + mid = pos
	MovingObjects *m = movingObjects();
	Vec2 mid (m->width()*.5, m->height()*.5);
	double d = 0.0;
	if (m) d = m->zToDistance(z);
	const Vec2 rpos2d ((cpos-mid)*d + mid);
	return Vec3 (rpos2d.x, rpos2d.y, z);
}
	
const unsigned Ellipse::numVertices = NUM_VERTICES_FOR_ELLIPSOIDS;
	
Ellipse::Ellipse(double lx, double ly)
	: xdiameter(lx), ydiameter(ly)
{
	if (!dl) setupDl();
}

void Ellipse::defineDl()
{
	static const double incr = DEG2RAD(360.0) / numVertices;
	double radian = 0.;
	glNewList(dl, GL_COMPILE);
	if (gtex) {
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glBindTexture(GL_TEXTURE_1D, gtex);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_REPEAT);		
	}
	glBegin(GL_POLYGON);
	for (unsigned i = 0; i < numVertices; ++i) {
		if (gtex) 
			glTexCoord1d(grad_offset + (grad_freq * ((1.0+cos(radian-grad_angle))/2.0)));
		glVertex2d(cos(radian)/2., sin(radian)/2.);
		radian += incr;
	}
	glEnd();
	if (gtex) glBindTexture(GL_TEXTURE_1D, 0);
	glEndList();	
}

void Ellipse::draw() {
	Vec2 scale_saved = scale;
	// emulate the xradius,yradius thing with just a glScale.. muahahaha!
	scale.x *= xdiameter;
	scale.y *= ydiameter;
	drawBegin();
	glCallList(dl);
	drawEnd();
	scale = scale_saved;
}

Rect Ellipse::AABB() const { 
	const double d = distance() > 0.0 ? distance() : 0.000000001;
	const double r_max (xdiameter/d);
	const double r_min (ydiameter/d);
	const double rot = DEG2RAD(angle);
	const double sin_rot = sin(rot);
	const double cos_rot = cos(rot);
	double t_nil = atan( -r_min * tan(rot) / r_max);
	double t_inf = atan(r_min * (cos_rot / sin_rot) / r_max);
	double rect_width = .5 * scale.x * fabs(2. * (r_max * cos(t_nil) * cos_rot - r_min * sin(t_nil) * sin_rot));
	double rect_height = .5 * scale.y * fabs(2. * (r_min * sin(t_inf) * cos_rot + r_max * cos(t_inf) * sin_rot));
	Vec2 cpos(canvasPosition());
	return Rect(Vec2(cpos.x-rect_width/2.,cpos.y-rect_height/2.),Vec2(rect_width,rect_height));
}

	
Rectangle::Rectangle(double w, double h)
: width(w), height(h)
{
	if (!dl) setupDl();
}

void Rectangle::defineDl()
{
	// put the unit square in a display list
	glNewList(dl, GL_COMPILE);
	if (gtex) {
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glBindTexture(GL_TEXTURE_1D, gtex);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_REPEAT);		
	}
	Vec2f texCoords[4];
	if (gtex) {
		texCoords[0] = Vec2f(0.f,0.f).rotated(grad_angle); 
		texCoords[1] = Vec2f(1.f,0.f).rotated(grad_angle);
		texCoords[2] = Vec2f(1.f,1.f).rotated(grad_angle);
		texCoords[3] = Vec2f(0.f,1.f).rotated(grad_angle); 
	}
	glBegin(GL_QUADS);	
	if (gtex) glTexCoord1d( (grad_freq * texCoords[0].x) + grad_offset);
	glVertex2d(-.5, -.5);
	if (gtex) glTexCoord1d( (grad_freq * texCoords[1].x) + grad_offset);
	glVertex2d(.5, -.5);
	if (gtex) glTexCoord1d( (grad_freq * texCoords[2].x) + grad_offset);
	glVertex2d(.5, .5);
	if (gtex) glTexCoord1d( (grad_freq * texCoords[3].x) + grad_offset);
	glVertex2d(-.5, .5);
	glEnd();
	if (gtex) glBindTexture(GL_TEXTURE_1D, 0);
	glEndList();	
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
	double d0 = distance();
	const double d = d0 > 0.0 ? d0 : EPSILON*2.;
	Vec2 cpos (canvasPosition());
	
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
		const double x0 = cpos.x;
		const double y0 = cpos.y;
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
				Vec2( (-width/2. * scale.x)/d + cpos.x, (-height/2. * scale.y)/d + cpos.y ), 
				Vec2( (width*scale.x)/d, (height*scale.y)/d ) 
				);
}


void InitStaticDisplayLists() {
	Sphere  sphere(1);
	(void)sphere;
}

static void DLCleanup(GLuint & dl) {
	if (dl) {
		glDeleteLists(dl,1);
		dl = 0;
	}
}
	
void CleanupStaticDisplayLists() {
	DLCleanup(Sphere::dl);
	gluDeleteQuadric(Sphere::quadric), Sphere::quadric = 0;
}

/* static */
const GLfloat
Sphere::DefaultLightAmbient[4] =  { 0.5f, 0.5f, 0.5f, 1.0f },
Sphere::DefaultLightDiffuse[4] =  { 1.0f, 1.0f, 1.0f, 1.0f },
Sphere::DefaultLightPosition[4] = { 400.0f, 250.0f, -100.0f, 0.0f },
Sphere::DefaultLightSpecular[4] =  { 1.0f, 1.0f, 1.0f, 1.0f },
Sphere::DefaultLightAttenuations[3] =  { 1.0f, 0.f, 0.f },

// material props	
Sphere::DefaultSpecular[4] =      { 1.0f, 1.0f, 1.0f, 1.0f },
Sphere::DefaultAmbient[4]  =      {.2f,.2f,.2f,1.f},
Sphere::DefaultDiffuse[4]  =      { .8f,.8f,.8f,1.f},
Sphere::DefaultEmission[4] =      { 0.f,0.f,0.f,1.f  },
	
Sphere::DefaultShininess =        50.0f;

/* static */ GLUquadricObj * Sphere::quadric = 0;
/* static */ GLuint Sphere::dl = 0;

Sphere::Sphere(double diam)
: diameter(diam), lightIsFixedInSpace(true)
{
	if (!quadric) {
		quadric = gluNewQuadric();
	}
/*	if (!dl) {
		dl = glGenLists(1);
		glNewList(dl, GL_COMPILE);
		gluQuadricNormals(quadric, GLU_SMOOTH);
		gluQuadricDrawStyle(quadric , GLU_FILL);
		gluQuadricOrientation(quadric, GLU_OUTSIDE); 
		gluSphere(quadric, 1.0, NUM_VERTICES_FOR_SPHEROIDS, NUM_VERTICES_FOR_SPHEROIDS);		
		glEndList();
	}
*/	std::memcpy(lightAmbient, DefaultLightAmbient, sizeof(DefaultLightAmbient));
	std::memcpy(lightDiffuse, DefaultLightDiffuse, sizeof(DefaultLightDiffuse));
	std::memcpy(lightPosition, DefaultLightPosition, sizeof(DefaultLightPosition));
	std::memcpy(lightSpecular, DefaultLightSpecular, sizeof(DefaultLightSpecular));
	std::memcpy(ambient, DefaultAmbient, sizeof(DefaultAmbient));
	std::memcpy(diffuse, DefaultDiffuse, sizeof(DefaultDiffuse));
	std::memcpy(emission, DefaultEmission, sizeof(DefaultEmission));
	std::memcpy(specular, DefaultSpecular, sizeof(DefaultSpecular));
	std::memcpy(lightAttenuations, DefaultLightAttenuations, sizeof(DefaultLightAttenuations));
	shininess = DefaultShininess;
	color = 1.0;
}

void Sphere::draw()
{
	double d = distance();
	if (d < 0.0) d = 1e-9;
	GLfloat lAmb[4], lDif[4], lSpec[4], spec[4], amb[4], dif[4], emis[4];
	for (int i = 0; i < 4; ++i) {
		GLfloat c = 1.f;
		switch(i) {
			case 0: c=color.r; break;
			case 1: c=color.g; break;
			case 2: c=color.b; break;
		}
		lAmb[i] = lightAmbient[i]*c;
		lDif[i] = lightDiffuse[i]*c;
		lSpec[i] = lightSpecular[i]*c;
		spec[i] = specular[i]*c;
		amb[i] = ambient[i]*c;
		dif[i] = diffuse[i]*c;
		emis[i] = emission[i]*c;
	}
	GLint depthEnabled = 0, depthFunc = 0;
	glGetIntegerv(GL_DEPTH_TEST, &depthEnabled);
	glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
	if (!depthEnabled) 
		glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_LIGHTING);
	glLightfv(GL_LIGHT0, GL_AMBIENT, lAmb);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lDif);
	glLightfv(GL_LIGHT0, GL_SPECULAR, lSpec);
	if (lightIsFixedInSpace) {
		glPushMatrix();
		glLoadIdentity();
		GLfloat *l = lightPosition_xf;
		std::memcpy(l, lightPosition, sizeof(lightPosition_xf));
		l[0] -= position.x;
		l[1] -= position.y;
		l[2] += position.z;
		glLightfv(GL_LIGHT0, GL_POSITION, l);
		glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, lightAttenuations[0]);
		glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, lightAttenuations[1]);
		glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, lightAttenuations[2]);
		glPopMatrix();
	} else {
		glLightfv(GL_LIGHT0, GL_POSITION,lightPosition);
	}
	glEnable(GL_LIGHT0);
	glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
	glMaterialf(GL_FRONT, GL_SHININESS, shininess);
	glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
	glMaterialfv(GL_FRONT, GL_EMISSION, emis);
	
	const Vec2 scale_saved = scale;
	double diam_actual = scale.x * diameter / d;
	scale.x = 1.0 * d; // multiply by d to counter-balance scaling in drawBegin()
	scale.y = 1.0 * d;
	double angle_saved = angle;
	angle = 0.; // angle is ignored for spheres 
	
	drawBegin();
	
	gluQuadricNormals(quadric, GLU_SMOOTH);
	gluQuadricDrawStyle(quadric , GLU_FILL);
	gluQuadricOrientation(quadric, GLU_OUTSIDE); 
	gluSphere(quadric, diam_actual/2.0, NUM_VERTICES_FOR_SPHEROIDS, NUM_VERTICES_FOR_SPHEROIDS);

	drawEnd();
	
	scale = scale_saved;
	angle = angle_saved;
	
	glDisable(GL_LIGHT0);
	glDisable(GL_LIGHTING);
	glDepthFunc(depthFunc);
	if (!depthEnabled) 
		glDisable(GL_DEPTH_TEST);		
}

Rect Sphere::AABB() const {
	const double d = distance() > 0.0 ? distance() : 0.000000001;
	Vec2 cpos(canvasPosition());
	return Rect(
				 Vec2( cpos.x-diameter*.5*scale.x/d,
					   cpos.y-diameter*.5*scale.y/d ),
				 Vec2( (diameter*scale.x)/d, (diameter*scale.y)/d )
				);	
}
	

} // end namespace Shapes

bool Rect::intersects(const Rect & r) const {
	return !(right() <= r.left()
			 || left() >= r.right()
			 || top() <= r.bottom()
			 || bottom() >= r.top());
}


uint qHash(const Util::Vec4T<float>& k)
{
	QString s;
	s.sprintf("%5.5f,%5.5f,%5.5f,%5.5f",k.x,k.y,k.z,k.w);
	//Debug() << "qHash(Vec4f) produced string `" << s << "' for (" << k.toString() << ")";
	return qHash(s);
}
