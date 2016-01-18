#include "Sawtooth.h"
#include <string.h>
#include <limits.h>

// number of ms per subframe
#define TBASE (1000./120./3.)

Sawtooth::Sawtooth()
	: StimPlugin("Sawtooth")
{
}

bool Sawtooth::initFromParams()
{
	
	if (!getParam("Nloops", Nloops)) Nloops = -1;
	if (!Nloops) Nloops = -1;
	float intensity_lof, intensity_hif;
	if (!getParam("intensity_low", intensity_lof)) intensity_lof = 0.0;
	// deal with 0->255 spec
	if (intensity_lof < 0. || intensity_lof > 255.0) {
		Error() << "intensity_low of " << intensity_lof << " invalid. Specify a number from 0 -> 255";
		return false;
	}
	intensity_low = qRound(intensity_lof);
	if (!getParam("intensity_high", intensity_hif)) intensity_hif = 255.0;
	// deal with 0->255 spec
	if (intensity_hif < 0. || intensity_hif > 255.0) {
		Error() << "intensity_high of " << intensity_hif << " invalid. Specify a number from 0 -> 255";
		return false;
	}
	intensity_high = qRound(intensity_hif);
	if (intensity_lof < 1.0 && intensity_hif <= 1.0) {
		// they specified numbers from 0->1.0
		intensity_low = intensity_lof * 255.0;
		intensity_high = intensity_hif * 255.0;
	}
	if (intensity_low > intensity_high) {
		Error() << "intensity_low needs to be less than intensity_high!";
		return false;
	}
	if (!getParam("bgcolor", bgcolor)) bgcolor = 0.0; // re-default bgcolor to 0.0
	
	// verify params
	GLint v[4][2] = {
		{ 0         ,  0          },
		{ 0         ,  height()   },
		{ width()   ,  height()   },
		{ width()   ,  0          },
	};
	memcpy(vertices, v, sizeof(v));

	cyclen = intensity_high - intensity_low + 1;

	return true;
}

bool Sawtooth::init()
{
	if (!initFromParams()) return false;
	loopct = 0;
	cyccur = 0;

	return true;
}

void Sawtooth::drawFrame()
{
	// Done in calling code.. glClear( GL_COLOR_BUFFER_BIT ); // sanely clear
		
	if (Nloops > -1 && loopct >= Nloops) {
		// end plugin
		Log() << "Sawtooth looped " << Nloops << " times, stopping.";
		stop();
	}

	memset(colors[0], 0, sizeof(colors[0]));
	
	for (int i = 0; i < ((int)fps_mode)+1; ++i) {
		if (cyccur >= cyclen) cyccur = 0, ++loopct;
		int intensity = cyccur + intensity_low;
		if (fps_mode == FPS_Single) {
			colors[0][0] = colors[0][1] = colors[0][2] = intensity;	
		} else {
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

/* virtual */ bool Sawtooth::applyNewParamsAtRuntime() 
{
	return initFromParams();
}
