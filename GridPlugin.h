#ifndef GridPlugin_H
#define GridPlugin_H
#include "StimPlugin.h"

/// \brief Abstract class for plugins wishing to have access to a 'grid' for drawing
///
/// An abstract class that implements the 'grid' which is a 2d array of 
/// rectangles.  Subclasses have access to the setupGrid() and drawGrid()
/// methods, along with the setGrayLevel() and setColor() methods.
/// See the MovingGrating class for a class that uses the 'grid'.
class GridPlugin : public StimPlugin
{
protected:
    GridPlugin(const QString &name);
    virtual ~GridPlugin();
    
    /** \brief Define the grid and setup its data structures.  

    Call this before you can call setGrayLevel(), setColor(), or drawGrid().  
    This method sets up the vertex arrays and color array defining the grid 
    rectangles.

    @param low - the y coordinate of the bottom of the grid
    @param left - the x coordinate of the leftmost part of the grid
    @param xWidth - the width in pixels of the grid along the x dimension
    @param yWidth - the height in pixels of the grid along the y dimension
    @param xNumber - the number of grid rectangles in the x direction.  xWidth/xNumber determines the width of each grid rectangle.
    @param yNumber - the number of grid rectangles in the y direction.  yWidth/yNumber determines the height of each grid rectangle */
    void setupGrid(int low, int left, int xWidth, int yWidth, int xNumber, int yNumber);
    /// Renders the grid to the screen using a glDrawArrays command.
    void drawGrid();
    /// Sets the gray level of a single grid rectangle at row y col x to intensity
    inline void setGrayLevel(int x, int y, float intensity) { colors[x][y*6+9] = colors[x][y*6+10] = colors[x][y*6+11] = intensity; }
    /// Set the color of a single grid rectangle at row y col x to have color compontents r,g,b
    inline void setColor(int x, int y, float r, float g, float b) {colors[x][y*6+9]=r;colors[x][y*6+10]=g;colors[x][y*6+11]=b;}

public:
    void stop(bool doSave = false, bool use_gui = false, bool softStop = false); ///< reimplementation deletes the grid data

private:
    GLfloat **colors;
    GLint **vertices;
    int nCols, nStripes;
    void destroyGrid();
    
};
#endif
