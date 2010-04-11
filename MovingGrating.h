#ifndef MovingGrating_H
#define MovingGrating_H
#include "GridPlugin.h"

class GLWindow;

/** \brief A plugin that draws a moving 'grating' or set of bars.

    This plugin basically draws a moving grating in the GLWindow.  The grating
    appears as a set of parallel bars with a periodic intensity.

    The grating is rotated at whatever angle the user specifies.

    It also moves across the screen from right to left at whatever speed
    the user specifies.

    For a full description of this plugin's parameters, it is recommended you see the \subpage plugin_params "Plugin Parameter Documentation"  for more details.
*/ 
class MovingGrating : public GridPlugin
{
	friend class GLWindow;

    float period;
    float speed;
    float angle;
    float totalTranslation;

	float dangle;
	int ccw;
	int tframes;

    float xscale,yscale;
	
	float min_color,max_color; ///< actual intensities used scaled to this range. This param should be clamped between [0,1]

	inline float scaleIntensity(float c) const { return c*(max_color-min_color) + min_color; }
	
	double (*waveFunc)(double);
	
protected:
    MovingGrating(); ///< can only be constructed by our friend class

    void drawFrame();
    bool init();

};


#endif
