#ifndef Flicker_H
#define Flicker_H
#include "StimPlugin.h"

class Flicker : public StimPlugin
{
	friend class GLWindow; ///< only our friend may construct us

	// Params: the below correspond one-to-one to params from the params file

	int hz; ///< a number that is either: 240, 120, 60, 40, 30, 24, 20, 17
	int duty_cycle; ///< 1, 2, or 3, depending on Hz.  1 is the only valid value for 240Hz and 1-3 are valid for 120 Hz and below
	int intensity; ///< a number from 0->255

	// the vertices are determined from lmargin, rmargin, bmargin, tmargin params
	GLint vertices[4][2];
	GLubyte colors[4][3];
	int cycf, activef; ///< number of total frames for 1 full cycle, number of frames active for 1 full cycle
	int cycct, activect;  ///< counters to above..

protected:
	Flicker();

	bool init();
	void drawFrame();


};
#endif
