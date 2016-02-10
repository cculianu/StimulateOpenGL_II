#ifndef MovingGrating_H
#define MovingGrating_H
#include "StimPlugin.h"
class GLWindow;

/** \brief A plugin that draws a moving 'grating' or set of bars.

    This plugin basically draws a moving grating in the GLWindow.  The grating
    appears as a set of parallel bars with a periodic intensity.

    The grating is rotated at whatever angle the user specifies.

    It also moves across the screen from right to left at whatever speed
    the user specifies.

    For a full description of this plugin's parameters, it is recommended you see the \subpage plugin_params "Plugin Parameter Documentation"  for more details.
*/ 
class MovingGrating : public StimPlugin
{
	friend class GLWindow;

    float spatial_freq, ///< spatial frequency, in cycles/pixel.  So .01 is 100 pixel bar width
          temp_freq;    ///< temporary frequency.  How many full cycles/sec cross a given point
    float angle; ///< in degrees, how much the bars are rotated
	float dangle; ///< delta-angle.  every tframes modify the angle by this amount
	int tframes;

	float min_color,max_color; ///< actual intensities used scaled to this range. This param should be clamped between [0,1]
	
	double (*waveFunc)(double);
	
    double phase;
    
    GLuint tex;
        
protected:
    MovingGrating(); ///< can only be constructed by our friend class
    ~MovingGrating();
    
    void drawFrame();
    bool init();
	/* virtual */ bool applyNewParamsAtRuntime();
	/* virtual */ void afterFTBoxDraw(); 

private:
	bool initFromParams();
    
    void redefineTexture(float min, float max, bool is_sub_img);	
};


#endif
