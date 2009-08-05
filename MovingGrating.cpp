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

	//glRotatef( 2.0, 0.0, 0.0, 1.0 );
	if( totalTranslation > period ){
		//glTranslatef( speed-period, 0.0, 0.0 );
		totalTranslation = totalTranslation - period + speed;
	}
	else{
		//glTranslatef( speed, 0.0, 0.0 );
		totalTranslation = totalTranslation + speed;
	}
	for( int i=0; i<1600; i++ )
		setGrayLevel( i,0,0.5+0.5*sin(2*3.14159*(i+totalTranslation)/period) );

	glRotatef( angle, 0.0, 0.0, 1.0 );

	drawGrid();

	glRotatef( -angle, 0.0, 0.0, 1.0 );
        
	glPopMatrix();

	frameVars->push(double(frameNum), double(totalTranslation)/double(period));
}
