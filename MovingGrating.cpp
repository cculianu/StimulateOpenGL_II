#include "MovingGrating.h"

static double squarewave(double x) {
	if (sin(x) > 0.) return 1.0;
	return -1.0;
}

MovingGrating::MovingGrating()
    : GridPlugin("MovingGrating"), xscale(1.f), yscale(1.f)
{
}

bool MovingGrating::init()
{
	bool returnvalue = true;

	if( !getParam("period", period) ) returnvalue = false;
	if( !getParam("speed",  speed) ) returnvalue = false;
	if( !getParam("angle", angle) ) returnvalue = false;
	if( !getParam("dangle", dangle) ) dangle = angle;	

	if( !getParam("tframes", tframes) )	tframes = -1;
	
	if ( !getParam("min_color", min_color)) min_color = 0.;
	if ( !getParam("max_color", max_color)) max_color = 1.;
	if ( !getParam("max_color2", max_color2)) max_color2 = max_color;
	if ( !getParam("reversal", reversal) ) reversal = 0;
	if (reversal < 0) reversal = 0;

	if (min_color < 0. || max_color < 0.) {
		Error() << "min_color and max_color need to be > 0!";
		return false;
	}
	if (min_color > 1.9 || max_color > 1.9) {
		Warning() << "min_color/max_color should be in the range [0,1].  You specified numbers outside this range.  We are assuming you mean unsigned byte values, so we will scale your input based on the range [0,255]!";
		min_color /= 255.;
		max_color /= 255.;
	}

	QString wave;
	if (!getParam("wave", wave)) wave = "sin";
	if (wave.toLower().startsWith("sq")) waveFunc = &squarewave;
	else waveFunc = &sin;
	
    xscale = width()/800.0;
    yscale = height()/600.0;

	totalTranslation = 0;

	if (returnvalue)
            setupGrid( -800, -800, 1, 1600, 1600, 1 );
	else 
            Error() <<  "Some parameter values could not be read";
        
	frameVars->setVariableNames(QString("frameNum phase").split(" "));

	return returnvalue;    
}

inline float MovingGrating::scaleIntensity(float c) const 
{ 
	float ret = c*(max_color-min_color) + min_color; 
	if (reversal > 0 && int(frameNum % (reversal*2)) >= reversal) {
		float hcolor = (max_color+min_color)/2;
		if (ret > hcolor) {
			ret = (max_color2-hcolor)*(ret - hcolor)/(max_color-hcolor) + hcolor;
		}
	}
	return ret;
}


void MovingGrating::drawFrame()
{   
	glClear( GL_COLOR_BUFFER_BIT );

	glPushMatrix();
        
    glScalef(xscale, yscale, 1.f);

	if (fps_mode != FPS_Single) {
		float totalTranslations[3];
		const int n_iters = ((int)fps_mode)+1;
		for (int k = 0; k < n_iters; ++k) {
			QVector<double> fv;
			if (have_fv_input_file) {
				fv = frameVars->readNext();
				if (fv.size() < 2 && frameNum) {
					// at end of file?
					Warning() << name() << "'s frame_var file ended input, stopping plugin.";
					parent->pauseUnpause();
					have_fv_input_file = false;
					glPopMatrix();
					stop();
					return;
				} 
				if (fv.size() < 2 || fv[0] != frameNum) {
					Error() << "Error reading frame " << frameNum << " from frameVar file! Datafile frame num differs from current frame number!  Does the fps_mode of the frameVar file match the current fps mode?";			
					stop();
					return;
				}
			}
			if (fv.size()) {
				totalTranslation = fv[1];
			} else if( totalTranslation > period ){
				totalTranslation = totalTranslation - period + speed;
			} else {
				totalTranslation = totalTranslation + speed;
			}
			totalTranslations[k] = totalTranslation;
			if (!fv.size())
				frameVars->push(double(frameNum), double(totalTranslation)/double(period));
		}
		for (int i = 0; i < 1600; ++i) {
			float r = 0., g = 0., b = 0.;
			for (int ff = 0; ff < (fps_mode == FPS_Dual ? 2 : 3); ++ff) {
				const float val = scaleIntensity(0.5+0.5*waveFunc(2.*M_PI*(i+totalTranslations[ff])/period));
				if (ff == r_index) r = val;
				if (ff == g_index) g = val;
				if (ff == b_index) b = val;
			}
			setColor(i, 0, r, g, b);
		}
	} else { // !quad_fps && !dual_fps
		QVector<double> fv;
		if (have_fv_input_file) {
			fv = frameVars->readNext();
			if (fv.size() < 2 && frameNum) {
				// at end of file?
				Warning() << name() << "'s frame_var file ended input, pausing plugin.";
				parent->pauseUnpause();
				have_fv_input_file = false;
				glPopMatrix();
				stop();
				return;
			} 
			if (fv.size() < 2 || fv[0] != frameNum) {
				fv.clear();
				Error() << "Error reading frame " << frameNum << " from frameVar file! Datafile frame num differs from current frame number!  Does the fps_mode of the frameVar file match the current fps mode?";			
				stop();
				return;
			}
		}
		if (fv.size()) {
			totalTranslation = fv[1];
		} else if( totalTranslation > period ) {
			totalTranslation = totalTranslation - period + speed;
		} else {
			totalTranslation = totalTranslation + speed;
		}
		if (!fv.size())
			frameVars->push(double(frameNum), double(totalTranslation)/double(period));
		
		for( int i=0; i<1600; i++ )
			setGrayLevel( i, 0, scaleIntensity(0.5+0.5*waveFunc(2.*M_PI*(i+totalTranslation)/period)) );
	}
	
	if (ftChangeEvery < 1 && reversal && frameNum > 0 && !(frameNum%reversal)) ftAssertions[FT_Change] = true;
	
	if ((frameNum > 0) && tframes > 0 && !(frameNum % tframes)) {
		
		if (ftChangeEvery < 1) ftAssertions[FT_Change] = true;
		
		angle = angle + dangle;
	}

	glRotatef( angle, 0.0, 0.0, 1.0 );

	drawGrid();

	glRotatef( -angle, 0.0, 0.0, 1.0 );
        
	glPopMatrix();
}

