#include "MovingGrating.h"


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
		if (fps_mode == FPS_Dual) {
			for( int i=0; i<1600; i++ ) {
				float r = 0.5+0.5*sin(2*3.14159*(i+totalTranslations[1])/period),
					  g = 0.f,
					  b = 0.5+0.5*sin(2*3.14159*(i+totalTranslations[0])/period);
				setColor(i, 0, r, g, b);
			}
		} else { // quad_fps
			for( int i=0; i<1600; i++ ) {
				float r = 0.5+0.5*sin(2*3.14159*(i+totalTranslations[2])/period),
					  g = 0.5+0.5*sin(2*3.14159*(i+totalTranslations[1])/period),
					  b	= 0.5+0.5*sin(2*3.14159*(i+totalTranslations[0])/period);
				setColor(i, 0, r, g, b);
			}
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
			setGrayLevel( i, 0, 0.5+0.5*sin(2*3.14159*(i+totalTranslation)/period) );
	}

	glRotatef( angle, 0.0, 0.0, 1.0 );

	drawGrid();

	glRotatef( -angle, 0.0, 0.0, 1.0 );
        
	glPopMatrix();

}
