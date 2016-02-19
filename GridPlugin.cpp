#include "GridPlugin.h"

GridPlugin::GridPlugin(const QString &name)
    : StimPlugin(name), colors(0), vertices(0), nCols(0), nStripes(0)
{}

GridPlugin::~GridPlugin() { destroyGrid(); }

void GridPlugin::stop(bool doSave, bool use_gui, bool softStop) { StimPlugin::stop(doSave,use_gui,softStop); destroyGrid(); }

void GridPlugin::destroyGrid() 
{
    for ( int i = 0; i < nCols; ++i ) {
        delete [] vertices[i];
        vertices[i] = 0;
    }
    for( int i = 0; i < nCols; ++i ){
        delete [] colors[i];
        colors[i] = 0;
    }

    delete [] vertices;
    vertices = 0;
    delete [] colors;
    colors = 0;
    nCols = 0;
    nStripes = 0;
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void GridPlugin::setupGrid(int left, int low, int xWidth, int yWidth, int Nx, int Ny)
{
        destroyGrid(); // make sure previous grid is gone if any

	// (left, low) is the lower left corner of the grid
	// xWidth and yWidth are the side lengths of the rectangles
	// Nx and Ny are the number of rectangles in each direction

	// set the class variables
	nCols = Nx;
	nStripes = Ny;

	// allocate memory in first dimension (x direction) as pointers to hold the memory in the other dimension
	// for each x location, a strip of of rectangles will be used
	vertices = new GLint*[Nx];
	colors = new GLfloat*[Nx];

	// allocate memory in the second dimension (y direction)
	for( int i=0; i<Nx; i++ ){
		vertices[i] = new GLint[Ny*4+4+3];
		// vertices[i] is an array of integers for the (i+1)-th strip along the x direction
		// it contains alternating values for x and y coordinates of vertices
		// the first rectangle needs 4 specified vertices
		// every subsequent vertex needs 2 additional vertices
		// thus Ny*4+4 integers are required

		// set x-coordinates of vertices:
		for( int j=0; j<Ny+1; j++ ){
			vertices[i][j*4] = left + i*xWidth;
			vertices[i][j*4+2] = left + (i+1)*xWidth;
		}
		// set y-coordinates of vertices:
		for( int j=0; j<Ny+1; j++ )
			vertices[i][j*4+1] = vertices[i][j*4+3] = low + j*yWidth;

		// allocate memory for colors of the vertices
		colors[i] = new GLfloat[Ny*6+6];
		for( int j=0; j<Ny*6+6; j++ )
			colors[i][j] = 0.5; // set all colors to gray
	}

        glEnableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_VERTEX_ARRAY);
}

void GridPlugin::drawGrid()  
{
    for(int i = 0; i < nCols; ++i) { 
        glColorPointer(3, GL_FLOAT, 0, colors[i]);
        glVertexPointer(2, GL_INT, 0, vertices[i]);
        glDrawArrays(GL_QUAD_STRIP, 0, nStripes*2 + 2);
    }
}
