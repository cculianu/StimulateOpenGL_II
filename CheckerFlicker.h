#ifndef CheckerFlicker_H
#define CheckerFlicker_H
#include "StimPlugin.h"
#include <deque>
#include <vector>

class GLWindow;
struct Frame;
class FrameCreator;

enum Rand_Gen {
	Uniform = 0, Gauss, Binary, N_Rand_Gen
};

// SFMT based random number generator	
#include "sfmt.hpp"
typedef sfmt_19937_generator SFMT_Generator;

/**
   \brief A class for drawing randomly-generated 'checkers' of arbitrary width 
          and height to the GLWindow.  


   For a full description of this plugin's parameters, it is recommended you 
   see the \subpage plugin_params "Plugin Parameter Documentation"  for more 
   details.
*/
class CheckerFlicker : public StimPlugin
{
	Q_OBJECT

    friend class FrameCreator;

	SFMT_Generator sfmt;
	
    std::vector<FrameCreator *> fcs;
    void cleanupFCs();

    int stixelWidth;	///< width of stixel in x direction
    int stixelHeight;	///< height of stixel in y direction
	Rand_Gen rand_gen;
    float meanintensity; ///< mean light intensity between 0 and 1
    float contrast;	 ///< defined as Michelsen contrast for black/white mode
    // and as std/mean for gaussian mode
    int w, h;           ///< window width/height cached here
    int originalSeed;	///< seed for random number generator at initialization
    int Nx;		///< number of stixels in x direction
    int Ny;		///< number of stixels in y direction
    int xpixels, ypixels;
    unsigned nCoresMax;
    unsigned fbo;       ///< iff nonzero, the number of framebuffer objects to use for prerendering -- this is faster than `prerender' but not always supported?
    int ifmt, fmt, type; ///< for glTexImage2D() call..
    int nConsecSkips;
	bool verboseDebug; ///< iff true, spam lots of debug output to app console 
	unsigned rand_displacement_x, rand_displacement_y; ///< if either are nonzero, each frame is displaced by this amont in pixels (not stixels!) randomly in x or y direction
    // Note only one of display_lists, prerender, or fbo may be active above

    GLuint *fbos, *texs; ///< array of fbo object id's and the texture ids iff fbo is nonzero
	Vec2i *disps; ///< frame displacements per fbo object

	/// for rendering -- the vertex and texture coord buffers which are just the corners of a quad covering the checker area
	GLint texCoords[8], vertices[8];
	
    friend class GLWindow;
    unsigned gaussColorMask; ///< this will always be a power of 2 minus 1
    std::vector<GLubyte> gaussColors;
    void genGaussColors();
    inline GLubyte getColor(unsigned entropy) { return gaussColors[entropy&gaussColorMask]; }

    std::deque<unsigned> nums, oldnums; ///< queue of texture indices into the texs[] array above.  oldest onest are in back, newest onest in front
    unsigned num;
    double lastAvgTexSubImgProcTime, minTexSubImageProcTime, maxTexSubImageProcTime;
    volatile int lastFramegen;
	volatile unsigned frameGenAvg_usec;
	unsigned origThreadAffinityMask;

    inline void putNum() { oldnums.push_back(num);  }
    inline unsigned takeNum() { 
        if (nums.size()) {
            num = nums.front(); 
            nums.pop_front(); 
            return num;
        } else if (oldnums.size()) {
            num = oldnums.front(); 
            oldnums.pop_front();
            return num; 
        } // else...
        Error() << "INTERNAL ERROR: takeNum() ran out of numbers!";
        return 0;
    }
    inline unsigned newFrameNum()  { 
        unsigned num;
        if (oldnums.size()) {
            num = oldnums.front();
            oldnums.pop_front();
            nums.push_back(num);
            return num;
        } // else...
        Error() << "INTERNAL ERROR: newFrameNum() ran out of numbers in oldnums!";
        return 0;
    }
    void setNums();

    Frame *genFrame(std::vector<unsigned> & entropy_buf);

    bool initPrerender();  ///< init for 'prerender to sysram'
    bool initFBO(); ///< init for 'prerender to FBO'
    void cleanupPrerender(); ///< cleanup for prerender frames mode
    void cleanupFBO(); ///< cleanup for FBO mode
	Rand_Gen parseRandGen(const QString &) const;
	bool initFromParams(); ///< reusable init code for both real init and realtime apply params init
	void initFromParamsNonCritical(); ///< reusable init code for both real init and realtime apply params init -- the init done here is 'non-critical' and can REALLY be done in realtime (no destruction of expensive stuff required)
	bool checkForCriticalParamChanges(); ///< called by applyNewParamsAtRuntime() to see if we need to do a full-reinit
	void doPostInit(bool resetFrameNume = true); ///< called by init() and applyNewParamsAtRuntime() after full re-init
protected:
    CheckerFlicker(); ///< can only be constructed by our friend class
    ~CheckerFlicker();

    /// Draws the next frame that is already in the texture buffer to the screen.
    void drawFrame();
    /// Informs FrameCreator threads to generate more frames and pops off 1 Frame from a FrameCreator queue and loads it onto the video board using FBO
    void afterVSync(bool isSimulated = false);
    bool init();
	unsigned initDelay(void); ///< reimplemented from superclass -- returns an init delay of 500ms
    void cleanup(); 
    void save();
	/* virtual */ bool applyNewParamsAtRuntime(); ///< reimplemented from superclass -- reapplies new params at runtime and reinits state/frame creators if need be
};


#endif
