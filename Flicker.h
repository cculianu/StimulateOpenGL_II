#ifndef Flicker_H
#define Flicker_H
#include "StimPlugin.h"

class Flicker : public StimPlugin
{
	friend class GLWindow; ///< only our friend may construct us

	// Params: the below correspond one-to-one to params from the params file

	int hz; ///< a number that is either: 240, 120, 60, 40, 30, 24, 20, 17
	int duty_cycle; ///< 1, 2, 3, etc depending on Hz.  1 is the only valid value for 180Hz and 1-2 is valid for 120Hz and 3+ are valid for below that

	GLubyte intensity; ///< a number from 0->255.0

	// the vertices are determined from lmargin, rmargin, bmargin, tmargin params
	GLint vertices[4][2];
	GLubyte colors[4][3];

	int cyccur, ///< current position in total cycle
		cyctot; ///< the total cycle length which is duty_cycle + off_subframes = cyctot

protected:
	Flicker();

	bool init();
	void drawFrame();
	
	/* virtual */ bool applyNewParamsAtRuntime();

private:
	bool validateHz() const;
	bool validateDutyCycle() const;
	bool initFromParams();
};
#endif
