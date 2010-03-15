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
		BoxType=0, EllipseType
	};
	
	static QString objTypeStrs[];
	
	struct ObjData {
		ObjType type; /// box or disk or ellipse
		Shapes::Shape *shape; ///< pointer to object's geometry drawing implementation (see Shapes.h)
		float jitterx, jittery;
		float len_maj_o, len_min_o; ///< original lengths: for box, len_maj == len_min.  for ellipse, len_maj is the xradius, len_min the yradius..
		float phi_o; ///< phi and original phi, or rotation
		Vec2 v, vel,  // working velocity and real velocity?
		     vel_o, pos_o; // original velocity, position, for targetcycle stuff
		float spin; // default is 0.. otherwise spin is applied to object per-frame
		int tcyclecount, targetcycle, speedcycle, delay;
		float color; // intensity value
		
		ObjData(); // init all to 0
		void initDefaults();
	};

	void initObj(ObjData & o);
	
	void wrapObject(ObjData & o, const Rect & aabb) const;

	QList<ObjData> objs;
	int numObj;
	
	float mon_x_pix;
	float mon_y_pix;
	float max_x_pix;
	float min_x_pix;
	float max_y_pix;
	float min_y_pix;
	bool rndtrial;
	int tframes;
	int rseed;
	int jittermag;
	bool jitterlocal;
    bool moveFlag, jitterFlag;
	bool wrapEdge;
};

#endif
