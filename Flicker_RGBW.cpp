#include "Flicker_RGBW.h"
#include <string.h>
#include <limits.h>

Flicker_RGBW::Flicker_RGBW()
	: StimPlugin("Flicker_RGBW")
{
}

bool Flicker_RGBW::init()
{
	int max_active_frames_per_cycle;

	//if (getHWRefreshRate() != 120) {
	//	Error() << "Flicker plugin requires running on a monitor that is locked at 120Hz refresh rate!  Move the window to a monitor running at 120Hz and try again!";
	//	return false;
	//}
	if (!getParam("hz", hz)) hz = 120;
	// default duty_cycle for 240 Hz is 1, 120Hz 2, and everything else
	if (!getParam("duty_cycle", duty_cycle)) duty_cycle = (hz > 120 ? 1 : (hz > 60 ? 2 : 3) );  
	if (!getParam("intensity", intensity_f)) intensity_f = 1.0;
	// deal with 0->255 spec
	if (intensity_f > 1.0) intensity_f /= 255.0;
	if (intensity_f < 0. || intensity_f > 255.) Warning() << "intensity of " << intensity_f << " invalid, defaulting to 1.0", intensity_f = 1.0;
	GLubyte intensity = intensity_f * 255.0;
	if (!getParam("bgcolor", bgcolor)) bgcolor = 0.0; // re-default bgcolor to 0.0
	if (!getParam("max_active_frames_per_cycle", max_active_frames_per_cycle)) max_active_frames_per_cycle = -1;
	if (max_active_frames_per_cycle <= 0) max_active_frames_per_cycle = -1;
	

	// verify params
	activef = 1;
	switch (hz) {
		case 240: 
		case 120: cycf = 1; break;
		case 60: cycf = 2; break;
		case 40: cycf = 3; break;
		case 30: cycf = 4; break;
		case 24: cycf = 5; break;
		case 20: cycf = 6; break;
		case 17: cycf = 7; break;
		case 15: cycf = 8; break;
		case 13: cycf = 9; break;
		case 12: cycf = 10; break;
		default:
			Error() << "hz param to plugin needs to be one of: 240,120,60,40,30,24,20,17,15,13,12!";
			return false;
	}
	activef = cycf/2;
	if (max_active_frames_per_cycle > 0) {
		if (activef < max_active_frames_per_cycle) activef = cycf-1;
		if (activef > max_active_frames_per_cycle) activef = max_active_frames_per_cycle;
	}
	if (!activef) activef = 1;

	cycct = activect = 0;
	
	if (duty_cycle < 1 || duty_cycle > 3 || (hz > 120 && duty_cycle > 1)) {
		Error() << "duty_cycle param " << duty_cycle << " is invalid (or invalid for the specified hz param of " << hz << ").";
		return false;
	}

	GLubyte color[3] = {0,0,0};
	if (hz == 240) {
		// 240 Hz is special case of green + blue channels always
		color[1] = intensity;
		color[2] = intensity;
	} else { // otherwise pay attention to duty cycle to determine which channels to enable, NB: channels are in BRG order on projector
		switch(duty_cycle) {
			case 3: color[1] = intensity;
			case 2: color[0] = intensity;
			case 1: color[2] = intensity;
		}
	}

	for (int i = 0; i < 4; ++i) memcpy(colors[i], color, sizeof(color));
	GLint v[4][2] = {
		{ 0         ,  0          },
		{ 0         ,  height()   },
		{ width()   ,  height()   },
		{ width()   ,  0          },
	};
	memcpy(vertices, v, sizeof(v));

	return true;
}

void Flicker_RGBW::drawFrame()
{
	glClear( GL_COLOR_BUFFER_BIT ); // sanely clear

	if (activect-- > 0) { // if we aren't skipping a frame.. draw the flicker box
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		glVertexPointer(2, GL_INT, 0, vertices);
		glColorPointer(3, GL_UNSIGNED_BYTE, 0, colors);
		glDrawArrays(GL_QUADS, 0, 4); // draw the rectangle using the above color and vertex pointers

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}

	if (++cycct >= cycf) {
		cycct = 0;
		activect = activef;
	}
}
