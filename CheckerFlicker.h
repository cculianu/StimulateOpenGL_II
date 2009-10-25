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

    std::vector<FrameCreator *> fcs;
    void cleanupFCs();

    int stixelWidth;	///< width of stixel in x direction
    int stixelHeight;	///< height of stixel in y direction
	Rand_Gen rand_gen;
    float meanintensity; ///< mean light intensity between 0 and 1
    float contrast;	 ///< defined as Michelsen contrast for black/white mode
    // and as std/mean for gaussian mode
    const int w, h;           ///< window width/height cached here
    int lmargin;	///< number of pixels left blank at left
    int rmargin;	///< number of pixels left blank at right
    int bmargin;	///< number of pixels left blank at bottom
    int tmargin;	///< number of pixels left blank at top
    int originalSeed;	///< seed for random number generator at initialization
    int Nblinks;	///< number of blinks (i.e. how often each frame is repeated)
    int blinkCt;
    int Nx;		///< number of stixels in x direction
    int Ny;		///< number of stixels in y direction
    int xpixels, ypixels;
    unsigned nCoresMax;
    unsigned fbo;       ///< iff nonzero, the number of framebuffer objects to use for prerendering -- this is faster than `prerender' but not always supported?
    int ifmt, fmt, type; ///< for glTexImage2D() call..
    int nConsecSkips;
	unsigned rand_displacement_x, rand_displacement_y; ///< if either are nonzero, each frame is displaced by this amont in pixels (not stixels!) randomly in x or y direction
    // Note only one of display_lists, prerender, or fbo may be active above

    GLuint *fbos, *texs; ///< array of fbo object id's and the texture ids iff fbo is nonzero
	Vec2i *disps; ///< frame displacements per fbo object

    friend class GLWindow;
    unsigned gaussColorMask; ///< this will always be a power of 2 minus 1
    std::vector<GLubyte> gaussColors;
    void genGaussColors();
    inline GLubyte getColor(unsigned entropy) {  return gaussColors[entropy&gaussColorMask]; }

    std::deque<unsigned> newnums, oldnums; ///< queue of texture indices into the texs[] array above.  oldest onest are in back, newest onest in front
    unsigned num;
    double lastAvgTexSubImgProcTime;
    volatile int lastFramegen;
	volatile unsigned frameGenAvg_usec;
	unsigned origThreadAffinityMask;

    inline void putNum() { oldnums.push_back(num);  }
    inline unsigned takeNum() { 
        if (newnums.size()) {
            num = newnums.front(); 
            newnums.pop_front(); 
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
            newnums.push_back(num);
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
    void cleanupFBO(); ///< cleanip for FBO mode

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
};


#endif
