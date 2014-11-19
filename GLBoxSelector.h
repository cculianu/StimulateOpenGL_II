#ifndef GLBoxSelector_H
#define GLBoxSelector_H
#include "Util.h"
#include "GLHeaders.h"
#include <QGLWidget>
#include <QColor>

class GLBoxSelector : public QObject {
public:
	GLBoxSelector(QGLWidget *parent);
	~GLBoxSelector();
	
	bool isHidden() const { return hidden; }
	void setHidden(bool b) { hidden = b; }
	bool isEnabled() const { return enabled; }
	void setEnabled(bool);
	
	void init(); ///< call this from GLWidget's initializeGL function
	bool draw(GLenum colorBuffer = GL_BACK); ///< call this from GLWidget's paintGL() function to paint htis widget
	
	bool eventFilter(QObject *watched, QEvent *event);

	void setBox(const Vec2i & origin, const Vec2i & size);
	void setBox(const Vec4i &r) { setBox(Vec2i(r.x,r.y),Vec2i(r.v3,r.v4)); }
	void setBox(int x, int y, int w, int h) { setBox(Vec4i(x,y,w,h)); }
	Vec4i getBox() const; ///< returned: .v1 = x, .v2 = y, .v3 = w, .v4 = h
	Vec4f getBoxf() const;
	void setBoxf(const Vec4f & box);
	
	void saveSettings() const;
	void loadSettings();
	
protected:		
	void resized(int w, int h);
	
private:
	Vec2f wc2gl(const Vec2i & windowCoords) const;
	Vec2i gl2wc(const Vec2f & glcoords) const;
	void processMouse(QMouseEvent *me);
	void chkBoxSanity();
	
	QGLWidget *glw;
	bool hidden,enabled,hadMouseTracking;
	Vec4f viewport; ///< .v1 = minx, .v2 = miny, .v3 = w, .v4 = h
	GLfloat projmatrix[16];
	
	Vec4f box; ///< bottomleft and top right 2D vertices in 1 vector..
	
	Vec2f grabPos;
	int grabCorners;
};

#endif