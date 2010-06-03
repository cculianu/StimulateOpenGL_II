#ifndef Sawtooth_H
#define Sawtooth_H
#include "StimPlugin.h"

class Sawtooth : public StimPlugin
{
	friend class GLWindow; ///< only our friend may construct us

	// Params: the below correspond one-to-one to params from the params file
	int Nloops;
	int num_loops;
	GLubyte intensity_low, intensity_high; ///< a number from 0->255

	// the vertices are determined from lmargin, rmargin, bmargin, tmargin params
	GLint vertices[4][2];
	GLubyte colors[4][3];

	int cyclen, cyccur,	loopct; 

protected:
	Sawtooth();

	bool init();
	void drawFrame();

private:
};
#endif
