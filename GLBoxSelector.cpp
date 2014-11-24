#include "GLBoxSelector.h"
#include <QEvent>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <math.h>
#include <QCursor>
#include <QSettings>

GLBoxSelector::GLBoxSelector(QGLWidget *parent)
: QObject(parent), glw(parent), hidden(true), enabled(false)
{
	glw->installEventFilter(this);
	init();
	box = Vec4f(0.,0.,1.,1.);
	grabPos = Vec2f(-1.f,-1.f);
	grabCorners = 0;
}
GLBoxSelector::~GLBoxSelector()
{

}

void GLBoxSelector::init()
{
	resized(glw->width(), glw->height());
}

bool GLBoxSelector::draw(GLenum which_colorbuffer)
{
	bool ret = false;
	
	if (!hidden) {
		glw->makeCurrent();
		GLint saved_dbuf;
		glGetIntegerv(GL_DRAW_BUFFER, &saved_dbuf);
		glDrawBuffer(which_colorbuffer);
		GLint matmode_saved;
		glGetIntegerv(GL_MATRIX_MODE, &matmode_saved);
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadMatrixf(projmatrix);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		
		GLint saved_polygonmode[2];
		GLint saved_sfactor, saved_dfactor;
		GLint savedPat(0), savedRepeat(0);
		GLfloat savedColor[4];
		GLfloat savedWidth;
		bool wasEnabled = glIsEnabled(GL_LINE_STIPPLE);
		bool hadBlend = glIsEnabled(GL_BLEND);
		bool hadSmooth = glIsEnabled(GL_LINE_SMOOTH);
		GLint hadca, hadva, hadta;
				
		glGetIntegerv(GL_POLYGON_MODE, saved_polygonmode);                 
		// save some values
		glGetFloatv(GL_CURRENT_COLOR, savedColor);
		glGetIntegerv(GL_LINE_STIPPLE_PATTERN, &savedPat);
		glGetIntegerv(GL_LINE_STIPPLE_REPEAT, &savedRepeat);
		glGetFloatv(GL_LINE_WIDTH, &savedWidth);
		glGetIntegerv(GL_BLEND_SRC, &saved_sfactor);
		glGetIntegerv(GL_BLEND_DST, &saved_dfactor);
		glGetIntegerv(GL_COLOR_ARRAY, &hadca);
		glGetIntegerv(GL_VERTEX_ARRAY, &hadva);
		glGetIntegerv(GL_TEXTURE_COORD_ARRAY, &hadta);
		if (!hadva) glEnableClientState(GL_VERTEX_ARRAY);
		if (hadta) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		if (hadca) glDisableClientState(GL_COLOR_ARRAY);
		if (!hadSmooth) glEnable(GL_LINE_SMOOTH);
		if (!hadBlend) glEnable(GL_BLEND);

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		
		// next, draw everything BUT the box with a flicker/gray area...
		glColor4f(.3f, .3f, .4f, .25f);
		if (wasEnabled) glDisable(GL_LINE_STIPPLE);
		glPolygonMode(GL_FRONT,GL_FILL);
		glLineWidth(1.f);
		glLineStipple(1,0xffff);
		
		const GLfloat verticesNonBox[] = {
			0.f,0.f, 1.f,0.f, 1.f,box.v2, 0.f,box.v2,  // bottom strip
			0.f,box.v2, box.v1,box.v2, box.v1,1.f, 0.f,1.f, // left strip
			box.v1,box.v4, 1.f,box.v4, 1.f,1.f, box.v1,1.f, // top strip
			box.v3,box.v2, 1.f,box.v2, 1.f,box.v4, box.v3,box.v4 // right strip
		};
		
		glVertexPointer(2, GL_FLOAT, 0, verticesNonBox);
		glDrawArrays(GL_QUADS,0,16);
		
		
		glEnable(GL_LINE_STIPPLE);
		//glPolygonMode(GL_FRONT, GL_LINE);
		// outline.. use normal alphas
		glLineWidth(4.0f);
		unsigned short pat = 0xcccc;
		int shift = static_cast<unsigned int>(Util::getTime() * 32.0) % 16;
		pat = ror(pat,shift);
			
		glLineStipple(1, pat);
		const GLfloat verticesBox[] = {
			box.v1, box.v2,
			box.v3, box.v2,
			box.v3, box.v4,
			box.v1, box.v4,
			box.v1, box.v2,
		};
		float dummy, colorscale = modff((Util::getTime() * 16.) / M_PI, &dummy);
		const float s = (sinf(colorscale)+1.f)/2.f, c = (sinf(colorscale*1.123)+1.f)/2.f,
		            d = (sinf(colorscale*.8123)+1.f)/2.f;

		// draw surrounding box
		glColor4f(c, s, d, .75);
		glVertexPointer(2, GL_FLOAT, 0, verticesBox);
		glDrawArrays(GL_LINE_LOOP, 0, 5); 
		
		// draw interior 'grid'
		glLineWidth(1.f);
		glLineStipple(1,0xaaaa);
		
		const GLfloat gw = (box.v3-box.v1)/3.f, gh = (box.v4-box.v2)/3.f;
		const GLfloat verticesGrid[] = {
			box.v1+gw, box.v2,
			box.v1+gw, box.v4,
			box.v1+(gw*2.f), box.v2,
			box.v1+(gw*2.f), box.v4,
			box.v1, box.v2+gh,
			box.v3, box.v2+gh,
			box.v1, box.v2+(2.f*gh),
			box.v3, box.v2+(2.f*gh),
		};
		glVertexPointer(2, GL_FLOAT, 0, verticesGrid);
		glDrawArrays(GL_LINES, 0, 8);
		
		// restore saved OpenGL state
		if (hadta) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		if (hadca) glEnableClientState(GL_COLOR_ARRAY);
		if (!hadva) glDisableClientState(GL_VERTEX_ARRAY);
		glBlendFunc(saved_sfactor, saved_dfactor);
		glPolygonMode(GL_FRONT, saved_polygonmode[0]);
		glColor4fv(savedColor);
		glLineStipple(savedRepeat, savedPat);
		glLineWidth(savedWidth);
		if (!wasEnabled) glDisable(GL_LINE_STIPPLE);
		if (!hadBlend) glDisable(GL_BLEND);
		if (!hadSmooth) glDisable(GL_LINE_SMOOTH);
		
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(matmode_saved);
		
		glDrawBuffer(saved_dbuf);
		ret = true;
	} 
	return ret;
}

void GLBoxSelector::resized(int w, int h)
{
	glw->makeCurrent();
	viewport.x = viewport.y = 0.f;
	viewport.v3 = w, viewport.v4 = h;
	GLint matmode_saved;
	glGetIntegerv(GL_MATRIX_MODE, &matmode_saved);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	gluOrtho2D( 0.0, 1.0, 0.0, 1.0 );
	glGetFloatv(GL_PROJECTION_MATRIX, projmatrix);
	glPopMatrix();
	glMatrixMode(matmode_saved);
}

bool GLBoxSelector::eventFilter(QObject *watched, QEvent *event)
{	
	if (!hidden && watched == glw) {
		QResizeEvent *re; QMouseEvent *me; QPaintEvent *pe;
		if ((re=dynamic_cast<QResizeEvent *>(event))) {
			QSize s = re->size();
			resized(s.width(), s.height());
		} else if ((me=dynamic_cast<QMouseEvent *>(event))) {
			if (enabled) processMouse(me);
		} else if ((pe=dynamic_cast<QPaintEvent *>(event))) {
			//draw();
		}
	}
	return false; // don't filter event
}

void GLBoxSelector::setEnabled(bool b) 
{ 
	if (!enabled && b) hadMouseTracking = glw->hasMouseTracking();
	enabled = b; 
	glw->setMouseTracking(enabled || hadMouseTracking);	
}

Vec2f GLBoxSelector::wc2gl(const Vec2i & windowCoords) const
{
	return Vec2f(windowCoords.x/viewport.v3, (viewport.v4-windowCoords.y)/viewport.v4);
}

Vec2i GLBoxSelector::gl2wc(const Vec2f & glcoords) const
{
	return Vec2i(glcoords.x*viewport.v3, (1.0 - glcoords.y)*viewport.v4);
}

void GLBoxSelector::processMouse(QMouseEvent *me)
{
#define GRAB_POS_INVALID() (grabPos.x < 0.0f)
#define GRAB_POS_INVALIDATE() (grabPos.x = grabPos.y = -1.f)
	const bool leftButIsDown = me->buttons()&Qt::LeftButton;
	const float thresh = wc2gl(Vec2i(10,viewport.v4)).magnitude();
	Vec2f c1(box.x,box.y), c2(box.v3,box.y), c3(box.v3,box.v4), c4(box.x,box.v4);
	Vec2f pos = wc2gl(Vec2i(me->x(),me->y()));
	const bool movingBox = leftButIsDown && !GRAB_POS_INVALID() && glw->cursor().shape() == Qt::ClosedHandCursor;
	if (!leftButIsDown) GRAB_POS_INVALIDATE(), grabCorners = 0;
	glw->unsetCursor();
	if (!movingBox && !grabCorners) {
		if (pos.distance(c2) <= thresh) // bottom right
			glw->setCursor(Qt::SizeFDiagCursor), grabCorners = 1<<2;
		else if (pos.distance(c4) <= thresh) // top left 
			glw->setCursor(Qt::SizeFDiagCursor), grabCorners = 1<<4; 
		else if (pos.distance(c1) <= thresh) // bottom left
			glw->setCursor(Qt::SizeBDiagCursor), grabCorners = 1<<1;
		else if (pos.distance(c3) <= thresh) // top right corner
			glw->setCursor(Qt::SizeBDiagCursor), grabCorners = 1<<3;
		else if (pos.x >= c1.x && pos.x <= c2.x) {
			if (fabsf(pos.y-c1.y) <= thresh) // bottom side of box
				glw->setCursor(Qt::SizeVerCursor), grabCorners = (1<<1)|(1<<2);
			else if (fabsf(pos.y-c3.y) <= thresh)  // top side of box
				glw->setCursor(Qt::SizeVerCursor), grabCorners = (1<<3)|(1<<4);
		}
		if (!grabCorners && pos.y >= c1.y && pos.y <= c3.y) {
			if (fabsf(pos.x-c1.x) <= thresh)  // left side of box
				glw->setCursor(Qt::SizeHorCursor), grabCorners = (1<<1)|(1<<4);
			else if (fabsf(pos.x-c3.x) <= thresh) // right side of box
				glw->setCursor(Qt::SizeHorCursor), grabCorners = (1<<2)|(1<<3);
		} 
		if (!grabCorners && !(pos.x <= c1.x) && !(pos.x >= c3.x) && !(pos.y <= c1.y) && !(pos.y >= c3.y)) {
			glw->setCursor(leftButIsDown ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
			if (leftButIsDown) grabPos = pos;
		}
		if (!leftButIsDown) grabCorners = 0;
	} else if (movingBox && !grabCorners) { // movingBox!
		glw->setCursor(Qt::ClosedHandCursor);
		if (leftButIsDown && !GRAB_POS_INVALID()) {
			Vec2f movvec = pos-grabPos;
			grabPos = grabPos + movvec;
			box.v1 += movvec.x;
			box.v2 += movvec.y;
			box.v3 += movvec.x;
			box.v4 += movvec.y;
		}
		if(box.v1 < 0.f) box.v3 += -box.v1, box.v1=0.f;
		else if (box.v3 > 1.f) box.v1 -= box.v3-1.f, box.v3 = 1.f;
		else if (box.v2 < 0.f) box.v4 += -box.v2, box.v2 = 0.f;
		else if (box.v4 > 1.f) box.v2 -= box.v4-1.f, box.v4 = 1.f;
	} else if (!movingBox && grabCorners) {
		const bool movc1 = (grabCorners&(1<<1)), movc2 = (grabCorners&(1<<2)),	 movc3 = (grabCorners&(1<<3)), movc4 = (grabCorners&(1<<4));
		if (movc1 && movc2) {
			// dragging bottom side of box
			glw->setCursor(Qt::SizeVerCursor);
			c1.y = c2.y = pos.y;
			if (c1.y > 1.f) c1.y = c2.y = 1.f;
			if (c1.y < 0.f) c1.y = c2.y = 0.f;
			if (c1.y >= c3.y) grabCorners = (1<<3)|(1<<4);
		} else if (movc4 && movc3) { // top line of box is dragging
			glw->setCursor(Qt::SizeVerCursor);
			c4.y = c3.y = pos.y;
			if (c4.y > 1.f) c4.y = c3.y = 1.f;
			if (c4.y < 0.f) c4.y = c3.y = 0.f;					
			if (c4.y <= c1.y) grabCorners = (1<<1)|(1<<2);
		} else if (movc2 && movc3) { // right side of box is dragging
			glw->setCursor(Qt::SizeHorCursor);
			c2.x = c3.x = pos.x;
			if (c3.x > 1.f) c3.x = c2.x = 1.f;
			if (c3.x < 0.f) c3.x = c2.x = 0.f;
			if (c3.x <= c1.x) grabCorners = (1<<1)|(1<<4);
		} else if (movc1 && movc4) { // left side of box is dragging
			glw->setCursor(Qt::SizeHorCursor);
			c1.x = c4.x = pos.x;
			if (c1.x > 1.f) c1.x = c4.x = 1.f;
			if (c1.x < 0.f) c1.x = c4.x = 0.f;
			if (c1.x >= c2.x) grabCorners = (1<<2)|(1<<3);
		} else if (movc1) {
			glw->setCursor(Qt::SizeBDiagCursor);
			c1 = pos;
			if (c1.x < 0) c1.x = 0;
			if (c1.y < 0) c1.y = 0;
			if (c1.x > 1.f) c1.x = 1.f;
			if (c1.y > 1.f) c1.y = 1.f;
			c4.x = c1.x;
			c2.y = c1.y;
			if (c1.x >= c3.x && c1.y >= c3.y) grabCorners = (1<<3);
			else if (c1.x >= c2.x) grabCorners = (1<<2);
			else if (c1.y >= c4.y) grabCorners = (1<<4);
		} else if (movc3) {
			glw->setCursor(Qt::SizeBDiagCursor);
			c3 = pos;
			if (c3.x < 0) c3.x = 0;
			if (c3.y < 0) c3.y = 0;
			if (c3.x > 1.f) c3.x = 1.f;
			if (c3.y > 1.f) c3.y = 1.f;
			c4.y = c3.y;
			c2.x = c3.x;
			if (c1.x >= c3.x && c1.y >= c3.y) grabCorners = (1<<1);
			else if (c3.x <= c4.x) grabCorners = (1<<4);
			else if (c3.y <= c2.y) grabCorners = (1<<2);
		} else if (movc2) {
			glw->setCursor(Qt::SizeFDiagCursor);
			c2 = pos;
			if (c2.x < 0) c2.x = 0;
			if (c2.y < 0) c2.y = 0;
			if (c2.x > 1.f) c2.x = 1.f;
			if (c2.y > 1.f) c2.y = 1.f;
			c1.y = c2.y;
			c3.x = c2.x;
			if (c2.x >= c4.x && c2.y >= c4.y) grabCorners = (1<<4);
			else if (c1.x >= c2.x) grabCorners = (1<<1);
			else if (c3.y <= c2.y) grabCorners = (1<<3);
		} else if (movc4) {
			glw->setCursor(Qt::SizeFDiagCursor);
			c4 = pos;
			if (c4.x < 0) c4.x = 0;
			if (c4.y < 0) c4.y = 0;
			if (c4.x > 1.f) c4.x = 1.f;
			if (c4.y > 1.f) c4.y = 1.f;
			c1.x = c4.x;
			c3.y = c4.y;
			if (c2.x >= c4.x && c2.y >= c4.y) grabCorners = (1<<2);
			else if (c3.x <= c4.x) grabCorners = (1<<3);
			else if (c1.y >= c4.y) grabCorners = (1<<1);
		}
		box = Vec4f ( MIN(MIN(c1.x,c2.x),MIN(c3.x,c4.x)),
					 MIN(MIN(c1.y,c2.y),MIN(c3.y,c4.y)),
					 MAX(MAX(c1.x,c2.x),MAX(c3.x,c4.x)),
					 MAX(MAX(c1.y,c2.y),MAX(c3.y,c4.y)) );
	}
	chkBoxSanity();
}

void GLBoxSelector::chkBoxSanity()
{
	if (box.x < 0.f) box.x = 0.f;
	if (box.x > 1.f) box.x = 1.f;
	if (box.y < 0.f) box.y = 0.f;
	if (box.y > 1.f) box.y = 1.f;
	if (box.v3 < 0.f) box.v3 = 0.f;
	if (box.v3 > 1.f) box.v3 = 1.f;
	if (box.v4 < 0.f) box.v4 = 0.f;
	if (box.v4 > 1.f) box.v4 = 1.f;	
}

void GLBoxSelector::setBox(const Vec2i & o, const Vec2i & s)
{
	box.v1 = o.x/viewport.v3;
	box.v2 = o.y/viewport.v4;
	box.v3 = box.v1 + s.x/viewport.v3;
	box.v4 = box.v2 + s.y/viewport.v4;
	chkBoxSanity();
}

Vec4i GLBoxSelector::getBox() const
{
	return Vec4i (
				  box.v1*viewport.v3,
				  box.v2*viewport.v4,
				  (box.v3-box.v1) * viewport.v3,
				  (box.v4-box.v2) * viewport.v4
				  );
}

Vec4f GLBoxSelector::getBoxf() const
{
	return Vec4f(box.v1,box.v2,box.v3-box.v1,box.v4-box.v2);
}
void GLBoxSelector::setBoxf(const Vec4f & b)
{
	box.v1 = b.v1;
	box.v2 = b.v2;
	box.v3 = b.v1+b.v3;
	box.v4 = b.v2+b.v4;
	chkBoxSanity();
}

void GLBoxSelector::saveSettings() const
{
    QSettings settings("janelia.hhmi.org", "StimulateOpenGL_II");
	settings.beginGroup("GLBoxSelector");
	Vec4f b = getBoxf();
	settings.setValue("box", b.toString());
	settings.endGroup();
}

void GLBoxSelector::loadSettings() 
{
    QSettings settings("janelia.hhmi.org", "StimulateOpenGL_II");
	settings.beginGroup("GLBoxSelector");

	QString s = settings.value("box", "0,0,1,1").toString();
	Vec4f v;
	if (v.fromString(s)) setBoxf(v);
	settings.endGroup();	
}

