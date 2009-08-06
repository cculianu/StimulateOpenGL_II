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
			if( totalTranslation > period ){
				totalTranslation = totalTranslation - period + speed;
			}
			else {
				totalTranslation = totalTranslation + speed;
			}
			totalTranslations[k] = totalTranslation;
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
		if( totalTranslation > period ) {
			totalTranslation = totalTranslation - period + speed;
		} else {
			totalTranslation = totalTranslation + speed;
		}
		frameVars->push(double(frameNum), double(totalTranslation)/double(period));

		for( int i=0; i<1600; i++ )
			setGrayLevel( i, 0, 0.5+0.5*sin(2*3.14159*(i+totalTranslation)/period) );
	}

	glRotatef( angle, 0.0, 0.0, 1.0 );

	drawGrid();

	glRotatef( -angle, 0.0, 0.0, 1.0 );
        
	glPopMatrix();

}
