#ifndef MovingObjects_H
#define MovingObjects_H
#include "StimPlugin.h"

/** \brief A plugin that draws an moving target.

    This plugin basically draws a moving target in the GLWindow, which has its
    own trajectory and which bounces when it reaches the edge of the 
    GLWindow.

    The target may be a circle or a box.

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
        void initDisplayLists();
        void cleanupDisplayLists();

	unsigned short *trajdata;		// image data for each ou-movie frame		
	float x;
	float y;
	float vx;
	float vy;
	float jitterx;
	float jittery;
	int	objLen;
	int objLen_o;
	int objVelx;
	int objVely;
	int objVelx_o;
	int objVely_o;
	int objXinit;
	int objYinit;
	int objUnits;
	int targetcycle;
	int speedcycle;
	int tcyclecount;
	int delay;
	float objcolor;
	float bgcolor;
	float mon_x_cm;
	float mon_x_pix;
	float mon_y_cm;
	float mon_y_pix;
	float mon_z_cm;
	bool rndtrial;
	int tframes;
	int rseed;
	QString traj_data_path;
	int jittermag;
	bool jitterlocal;
	int	ftrackbox_x;
	int ftrackbox_y;
	int ftrackbox_w;
	float ftrackbox_c;
	bool quad_fps;
        bool moveFlag, jitterFlag;
        QString objType; ///< new config param -- can be 'box' or 'disk'/'circle'
        GLuint objDL; ///< display list for the moving object
        GLUquadric *quad; ///< valid iff objType == circle or objType == sphere
};

#endif
