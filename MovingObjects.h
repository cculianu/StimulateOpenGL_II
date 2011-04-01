#ifndef MovingObjects_H
#define MovingObjects_H
#include "StimPlugin.h"
#include <QList>
#include "Shapes.h"

/** \brief A plugin that draws K moving objects targets.  Targets may be square boxes or 
           ellipses (or circles).

    This plugin basically draws K (possibly moving) shapes in the GLWindow, 
    each having major axis r1 and minor axis r2 and phi rotation.

    A variety of parameters control this plugin's operation, and it is
    recommended you see the \subpage plugin_params "Plugin Parameter Documentation"  for more details.
*/ 
class MovingObjects : public StimPlugin
{
    friend class GLWindow;
    MovingObjects();
public:
    virtual ~MovingObjects();

protected:
    bool init(); ///< reimplemented from super
    void cleanup(); ///< reimplemented from super
    void drawFrame(); ///< reimplemented
    bool processKey(int key); ///< remiplemented

private:
    void initObjs();
    void cleanupObjs();
	void doFrameDraw();

	Rect canvasAABB;

	enum ObjType { 
		BoxType=0, EllipseType, SphereType
	};
	
	static QString objTypeStrs[];
	
	struct ObjData {
		ObjType type; /// box or disk or ellipse
		Shapes::Shape *shape; ///< pointer to object's geometry drawing implementation (see Shapes.h)
		float jitterx, jittery, jitterz;
		float phi_o; ///< phi and original phi, or rotation
		Vec3 v, vel,  // working velocity and real velocity?
		     pos_o; // original velocity, position, for targetcycle stuff
		float spin; // default is 0.. otherwise spin is applied to object per-frame
		QVector<Vec2> len_vec;
		QVector<Vec3> vel_vec; ///< new targetcycle/speedcycle support for length and velocity vectors	
		int len_vec_i, vel_vec_i;
		float color; // intensity value
		
		ObjData(); // init all to 0
		void initDefaults();		
	};

	void initObj(ObjData & o);
	
	void wrapObject(ObjData & o, Rect & aabb) const;

	QList<ObjData> objs;
	int numObj;
	
	bool savedrng;  
	int saved_ran1state;
	
	bool rndtrial;
	int tframes;
	int rseed;
	int jittermag;	
	bool jitterlocal;
    bool moveFlag, jitterFlag;
	bool wrapEdge;
	bool fvHasPhiCol, fvHasZCol, fvHasZScaledCol;
	
	float min_x_pix,max_x_pix,min_y_pix,max_y_pix;
	
	bool debugAABB; ///< comes from param file -- if true draw a green box around each shape's AABB to debug AABB
	GLint savedShadeModel;
	
	double cameraDistance, majorPixelWidth;
	unsigned didScaledZWarning;
	double maxZ; ///< when objects hit this Z, they bounce back
	bool jitterInZ;
	void initCameraDistance();
	
public:
	double distanceToZ(double distance) const;
	double zToDistance(double z) const;
};

#endif
