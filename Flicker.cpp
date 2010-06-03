#include "Flicker.h"
#include <string.h>
#include <limits.h>

#ifdef _MSC_VER
static double round(double d) { return qRound(d); }
#endif

// number of ms per subframe
#define TBASE (1000./120./3.)

Flicker::Flicker()
	: StimPlugin("Flicker")
{
}

bool Flicker::init()
{
	//if (getHWRefreshRate() != 120) {
	//	Error() << "Flicker plugin requires running on a monitor that is locked at 120Hz refresh rate!  Move the window to a monitor running at 120Hz and try again!";
	//	return false;
	//}
	if (!getParam("hz", hz)) hz = 120;
	if (!validateHz()) {
		Error() << "hz parameter " << hz << " specified is invalid.  Must be one of 180, 120, 90, 60, 50, etc.";
		return false;
	}
	// default duty_cycle for 180 Hz is 1, 120Hz == 2, and below == 3
	if (!getParam("duty_cycle", duty_cycle)) duty_cycle = ( hz >= 180 ? 1 : (hz == 120 ? 2 : 3) );  
	if (!validateDutyCycle()) {
		Error() << "duty_cycle parameter " << duty_cycle << " specified is invalid for this hz setting.";
		return false;
	}
	float intensity_f;
	if (!getParam("intensity", intensity_f)) intensity_f = 1.0;
	// deal with 0->255 spec
	if (intensity_f < 0.) {
		Warning() << "intensity of " << intensity_f << " invalid, defaulting to 1.0";
		intensity_f = 1.0;
	}
	if (intensity_f > 1.0)
		intensity = intensity_f;
	else
		intensity = intensity_f * 255.0;
	if (!getParam("bgcolor", bgcolor)) bgcolor = 0.0; // re-default bgcolor to 0.0
	int dummy;
	if (getParam("max_active_frames_per_cycle", dummy)) 
		Warning() << "max_active_frames_per_cycle no longer supported for the 'Flicker' plugin (but it is still supported for legacy 'Flicker_RGBW' plugin)";
	

	// verify params
	GLint v[4][2] = {
		{ 0         ,  0          },
		{ 0         ,  height()   },
		{ width()   ,  height()   },
		{ width()   ,  0          },
	};
	memcpy(vertices, v, sizeof(v));

	cyccur = 0;
	cyctot = round(1000./hz/(1000./120./3.));

	return true;
}

void Flicker::drawFrame()
{
	glClear( GL_COLOR_BUFFER_BIT ); // sanely clear
		
	memset(colors[0], 0, sizeof(colors[0]));

	for (int i = 0; i < 3; ++i) {
		if (cyccur >= cyctot) cyccur = 0;

		if (cyccur < duty_cycle) {
			const char c = color_order[i];
			switch (c) {
				case 'b': colors[0][2] = intensity; break;
				case 'g': colors[0][1] = intensity; break;
				case 'r': colors[0][0] = intensity; break;
			}
		} 
		++cyccur;
	}
	for (int i = 1; i < 4; ++i)
		memcpy(colors[i], colors[0], sizeof(colors[0]));

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	glVertexPointer(2, GL_INT, 0, vertices);
	glColorPointer(3, GL_UNSIGNED_BYTE, 0, colors);
	glDrawArrays(GL_QUADS, 0, 4); // draw the rectangle using the above color and vertex pointers

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
}

bool Flicker::validateHz() const {
	const double pd = 1000./static_cast<double>(hz);
	//Debug() << "pd: " <<  pd << " tbase: " << TBASE << " pd/tbase*tbase - pd: " << (((pd/TBASE) * TBASE)-pd) << " pd/TBASE: " << pd/TBASE;
	// basically, true iff the period is an even multiple of the subframe time and it is >= 2 subframes' time
	return (pd / TBASE) >= 1.9999 
		   && (((pd/TBASE) * TBASE) - pd) <= 0.0001;
}

bool Flicker::validateDutyCycle() const {
	const double pd = 1000./static_cast<double>(hz);
	//Debug() << "dutycycle: " << duty_cycle << " pd: " << pd << " dutycycle*TBASE: " << duty_cycle * TBASE;
	return duty_cycle > 0 && duty_cycle * TBASE < pd;
}