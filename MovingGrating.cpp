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
	
	// frametrack box info
	if(!getParam( "ftrackbox_x" , ftrackbox_x))  ftrackbox_x = 0;
	if(!getParam( "ftrackbox_y" , ftrackbox_y))  ftrackbox_y = 10;
	if(!getParam( "ftrackbox_w" , ftrackbox_w))  ftrackbox_w = 40;


        xscale = width()/800.0;
        yscale = height()/600.0;

	totalTranslation = 0;

	if (returnvalue)
            setupGrid( -800, -800, 1, 1600, 1600, 1 );
	else 
            Error() <<  "Some parameter values could not be read";
        
	return returnvalue;    
}

void MovingGrating::drawFrame()
{
 	int framestate = (frameNum%2 == 0);
       
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
 
	// draw frame tracking flicker box at bottom of grid
	if (framestate) 
		glColor4f(1, 1, 1, 1);
	else glColor4f(0, 0, 0, 1);
 	glRecti(ftrackbox_x, ftrackbox_y, ftrackbox_x+ftrackbox_w, ftrackbox_y+ftrackbox_w);
       
        glPopMatrix();
}
