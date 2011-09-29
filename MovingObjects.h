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
	/* virtual */ bool applyNewParamsAtRuntime(); ///< reimplemented from super

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
		int objNum; ///< which object number is this? used for debug printing mostly in the functions that operate on ObjData
		Shapes::Shape *shape; ///< pointer to object's geometry drawing implementation (see Shapes.h)
		double jitterx, jittery, jitterz;
		double phi_o; ///< phi and original phi, or rotation
		Vec3 v, vel,  // working velocity and real velocity?
		     pos_o; // original velocity, position, for targetcycle stuff
		Vec3 lastPos; // used for rndtrial > 1 stuff
		double spin; // default is 0.. otherwise spin is applied to object per-frame
		QVector<Vec2> len_vec;
		QVector<Vec3> vel_vec; ///< new targetcycle/speedcycle support for length and velocity vectors	
		int len_vec_i, vel_vec_i;
		float color, 
		      // params for sphere
		      shininess, ambient, diffuse, emission, specular; // intensity value
		int debugLvl;		
		
		ObjData(); // init all to 0
		void initDefaults();	
	};

	struct Obj2Render ///< used internally in doFrameDraw()
	{
		const ObjData & obj;
		const int k; /**< subframe number */
		Shapes::Shape *shapeCopy;
		Rect aabb;
		Obj2Render(const ObjData & obj, int k, const Rect & aabb) : obj(obj), k(k), shapeCopy(0), aabb(aabb) {}
	};
	friend struct Obj2Render;

	void drawObject(const int i, ///< rednered obj Num 
					Obj2Render & o2r);

	void initObj(ObjData & o);
	void reinitObj(ObjData & o, ObjType newType);
	static Shapes::Shape * newShape(ObjType t);
	static ObjType parseObjectType(const QString & stringFromParamFile);

	void wrapObject(ObjData & o, Rect & aabb) const;
	void doWallBounce(ObjData & o) const;
	void ensureObjectIsInBounds(ObjData & o) const;
	
	void applyRandomDirectionForRndTrial_2_4(ObjData & o);
	void applyRandomPositionForRndTrial_1_2(ObjData & o);

	void preReadFrameVarsForWholeFrame();
	void postWriteFrameVarsForWholeFrame();
		
	QList<ObjData> objs;
	QList<Shapes::Shape *> shapes2del;
	QVector<QVector<QVector<double> > >  fvs_block; ///< framevar data buffer -- indexed by [objnum][subframenum][fvarnum]

	int numObj;
	int nSubFrames;
	
	bool savedrng;  
	int saved_ran1state;
	
	int rndtrial;
	int tframes;
	int rseed;
	int jittermag;	
	bool jitterlocal;
    bool moveFlag, jitterFlag;
	bool wrapEdge;
	bool fvHasPhiCol, fvHasZCol, fvHasZScaledCol;
	bool lightIsFixedInSpace;
	bool lightIsDirectional;
	Vec3 lightPos;
	double lightAmbient, lightDiffuse, lightSpecular, lightConstantAttenuation, lightLinearAttenuation, lightQuadraticAttenuation;
	
	double min_x_pix,max_x_pix,min_y_pix,max_y_pix;
	
	bool debugAABB; ///< comes from param file -- if true draw a green box around each shape's AABB to debug AABB
	GLint savedShadeModel;
	
	double cameraDistance, majorPixelWidth;
	unsigned didScaledZWarning;
	double zBoundsNear, zBoundsFar; ///< when objects hit this Z, they bounce back
	bool is3D; ///< this flag controls if we jitter in Z, if rndtrial produces random Z values, and a bunch of other things
	QVector<Vec3> savedLastPositions;
	void initCameraDistance();
	void initBoundingNormals();
	
	//// used for bounds checking for object bouncing, vector of size 6
	QVector<Vec3> frustumNormals, boxNormals; ///< 0 = left edge, 1= right edge, 2 = bottom edge, 3 = top edge, 4 = near zbound, 5 = far zbounds
	
	enum FVCols { FV_frameNum=0, FV_objNum, FV_subFrameNum, FV_objType, FV_x, FV_y, FV_r1, FV_r2, FV_phi, FV_color, FV_z, FV_zScaled, 
		          N_FVCols };
	struct ConfigSuppressesFrameVar {
		bool col[N_FVCols]; ///< iff elements of array here are true, the config file suppresses framevar, and not vice-versa 
		ConfigSuppressesFrameVar() { for (int i = 0; i < (int)N_FVCols; ++i) col[i] = false; }
		ConfigSuppressesFrameVar & operator=(ConfigSuppressesFrameVar & other) { for (int i = 0; i < N_FVCols; ++i) col[i] = other.col[i];  return *this; }
		bool & operator[](int i) { return col[i]; }
		const bool & operator[](int i) const { return col[i]; }
	};
	QVector<ConfigSuppressesFrameVar> configSuppressesFrameVar;
	
public:
	double distanceToZ(double distance) const;
	double zToDistance(double z) const;
};

#endif
