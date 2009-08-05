#include "MovingObjects.h"

MovingObjects::MovingObjects()
    : StimPlugin("MovingObjects")
{
}

MovingObjects::~MovingObjects()
{
}

bool MovingObjects::init()
{
        moveFlag = true;
        quad = 0;

	// set up pixel color blending to enable 480Hz monitor mode

	// set default parameters
	// basic target attributes
    if (!getParam("objType", objType))           objType = "box"; // possible types are box and circle
	if(!getParam( "objLen" , objLen))            objLen = 8; // diameter
	if(!getParam( "objVelx" , objVelx))          objVelx = 4;
	if(!getParam( "objVely" , objVely))	     objVely = 4;
	if(!getParam( "objXinit" , objXinit))	     objXinit = 400;
	if(!getParam( "objYinit" , objYinit))	     objYinit = 300;
	if(!getParam( "targetcycle" , targetcycle))  targetcycle = 0;
	if(!getParam( "speedcycle" , speedcycle))    speedcycle = 0;
	tcyclecount = 0;
	objLen_o = objLen;
	objVelx_o = objVelx;
	objVely_o = objVely;

	// units for target info; 0=pixels, 1=degrees (1=> convert to pixels via mon_*_cm info)
	if(!getParam( "objUnits" , objUnits))	     objUnits = 0;
	if(!getParam( "objcolor" , objcolor))	     objcolor = 0;
	if(!getParam( "bgcolor" , bgcolor))	     bgcolor = 1.;
	glClearColor(bgcolor,bgcolor,bgcolor,1.0);
	glColor4f(bgcolor,bgcolor,bgcolor,1.0);

	// use these for setting angular size and speed
	if(!getParam( "mon_x_cm" , mon_x_cm))	     mon_x_cm = 24.2f;
	if(!getParam( "mon_x_pix" , mon_x_pix))	     mon_x_pix = width();
	if(!getParam( "mon_y_cm" , mon_y_cm))	     mon_y_cm = 18.2f;
	if(!getParam( "mon_y_pix" , mon_y_pix))	     mon_y_pix = height();
	if(!getParam( "mon_z_cm" , mon_z_cm))	     mon_z_cm = 12;

	// trajectory stuff
	if(!getParam( "rndtrial" , rndtrial))	     rndtrial = 0;
		// rndtrial=0 -> repeat traj every tframes; 
		// rndtrial=1 new start point and speed every tframes; start=rnd(mon_x_pix,mon_y_pix); speed= +-objVelx, objVely
	if(!getParam( "rseed" , rseed))              rseed = -1;  //set start point of rnd seed;
        ran1Gen.reseed(rseed);
	if(!getParam( "tframes" , tframes))          tframes = 120*60*60*10; // 120fps*s
	if(!getParam( "delay" , delay))	             delay = 0;  // delay from start of tr to stim onset;
	if(!getParam( "traj_data_path" , traj_data_path)) traj_data_path = "";
	//loadALdata(traj_data_path);

	if(!getParam( "jitterlocal" , jitterlocal))  jitterlocal = false;
	if(!getParam( "jittermag" , jittermag))	     jittermag = 2;

	// this increases the frame rate by encoding each RGB as 3 separate frames
	// generallty used w/ DLP projector w/ removed color wheel, in which case
	// native FPS increases by 4x (since color wheel has 4-segments/frame, RGB-W)
	// (make sure W set to all black or all white on projector)
	if(!getParam( "quad_fps" , quad_fps) && !dual_fps) quad_fps = true; ///< defaults to quad_fps = true if nothing specified in config file

	// set up blending to allow individual RGB target motion frame control
	// blending factor depends on bgcolor...
	//NOTE: this may only be correct for bgcolor = [0 or 1].
	/*if (quad_fps) {
            glEnable(GL_BLEND);
            if (bgcolor >  0.5)
                glBlendFunc(GL_DST_COLOR,GL_ONE_MINUS_DST_COLOR);
            else glBlendFunc(GL_SRC_COLOR,GL_ONE_MINUS_SRC_COLOR);
        }*/
     initDisplayLists();
	 frameVars->setVariableNames(QString("frameNum x y").split(QString(" ")));
     return true;
}

void MovingObjects::initDisplayLists()
{
    objDL = glGenLists(1);
    glNewList(objDL, GL_COMPILE);
    QString ot = objType.toLower();
    
    if (ot == "circle" || ot == "disk" || ot == "sphere") {
        if (ot == "sphere") 
            Warning() << "Sphere not (yet) supported, defaulting to disk..";
        Log() << "Creating disk object..";
        quad = gluNewQuadric();
        gluDisk(quad, 0, objLen, 128, 1);
    }  else { // default to box object
        if (ot != "box") 
            Warning() << "Specified objType `" << ot << "' not valid, defaulting to 'box'.";
        Log() << "Creating box object..";
        glRecti( 0, 0, static_cast<int>(objLen), static_cast<int>(objLen) );
    }
    glEndList();
}

void MovingObjects::cleanupDisplayLists()
{
    glDeleteLists(objDL, 1);
    objDL = 0xffffffff;
    if (quad) gluDeleteQuadric(quad), quad = 0;
}

void MovingObjects::cleanup()
{
    //glDisable(GL_BLEND);
    cleanupDisplayLists();
}

bool MovingObjects::processKey(int key)
{
    switch ( key ) {
    case 'm':
    case 'M':
        moveFlag = !moveFlag;
        return true;
    }
    return StimPlugin::processKey(key);
}

void MovingObjects::drawFrame()
{
	// set bg
	//glColor3f(bgcolor,bgcolor,bgcolor);
	//glRecti(0,0,mon_x_pix,mon_y_pix);
        // for some reason glClearColor() goes way, FIXME -- for now workaround is to re-set it each frame, which is OK since this plugin is lightweight! 
        glClearColor(bgcolor, bgcolor, bgcolor, 1.0);
	glClear( GL_COLOR_BUFFER_BIT ); 

	// local target jitter
	if (jitterlocal) {
		jitterx = (ran1Gen()*jittermag - jittermag/2);
		jittery = (ran1Gen()*jittermag - jittermag/2);
	}
	else 
		jitterx = jittery = 0;
        
    if (quad_fps || dual_fps) {
            glEnable(GL_BLEND);
            if (bgcolor >  0.5)
                glBlendFunc(GL_DST_COLOR,GL_ONE_MINUS_DST_COLOR);
            else glBlendFunc(GL_SRC_COLOR,GL_ONE_MINUS_SRC_COLOR);
    }

	const int niters = quad_fps ? 3 : (dual_fps ? 2 : 1);
	for (int k=0; k < niters; k++) {
            if( moveFlag ){
			
                // adjust for wall bounce
                if ((x + vx + jitterx > mon_x_pix) ||  (x + vx + jitterx < 0)) {
                    vx = -vx;
                }
                if ((y + vy + jittery > mon_y_pix) || (y + vy + jittery < 0)) {
                    vy = -vy; 
                }
                // initialize position iff k==0 and frameNum is a multiple of tframes
                if ( !k && !(frameNum%tframes)) {
				
                    // update target size if size-series
                    if ((targetcycle > 0) && (frameNum > 0)) {
                        if (tcyclecount++ == targetcycle) {
                            tcyclecount = 0;
                            objLen = objLen_o; // if targetcycle done, reset objLen
                        }
                        else objLen = objLen*2; // double target size every tframes
                    }
				
                    if ((speedcycle > 0) && (frameNum > 0)) {
                        if (tcyclecount++ == speedcycle) {
                            tcyclecount = 0;
                            objVelx = objVelx_o; // if targetcycle done, reset objLen
                            objVely = objVely_o; // if targetcycle done, reset objLen
                        }
                        else {
                            objVelx = objVelx*2; // double target size every tframes
                            objVely = objVely*2; // double target size every tframes
                        }
                    }
                    // init position
                    if (!rndtrial) {
                        x = objXinit;
                        y = objYinit;
                        vx = objVelx; //ran1( seed ) * 10;
                        vy = objVely; //ran1( seed ) * 10;
                    }
                    else {
                        x = ran1Gen()*mon_x_pix;
                        y = ran1Gen()*mon_y_pix;
                        vx = ran1Gen()*objVelx*2 - objVelx;
                        vy = ran1Gen()*objVely*2 - objVely; 
                    }
                }

                // update position after delay period
                if ((int(frameNum)%tframes - delay) > 0) { 
                    x += vx + jitterx;
                    y += vy + jittery;
                }
            }

            //x = trajdata[frameNum] + jitterx;
            //y = trajdata[frameNum+tframes] + jittery;
            // draw stim if out of delay period
            if ((int(frameNum)%tframes - delay) >= 0) {
				float r,g,b;

				if (quad_fps) {
					b = g = r = bgcolor;
					switch(k) {
						// bgr order of frames
						case 0: b = objcolor; break;
						case 1: g = objcolor; break;
						case 2: r = objcolor; break;
					}
				} else if (dual_fps) {
					b = g = r = bgcolor;
					switch(k) {
						// bgr order of frames
						case 0: b = objcolor; break;
						case 1: r = objcolor; break;
					}
				} else 
					b = g = r = objcolor;

				glColor3f(r,g,b);

                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glTranslatef(x, y, 1);
                glCallList(objDL);
                glPopMatrix();
				// nb: push() needs to take all doubles as args!
				frameVars->push(double(frameNum), double(x), double(y));
            }

	}
   
    if (quad_fps || dual_fps) {
		if (bgcolor >  0.5)
			glBlendFunc(GL_DST_COLOR,GL_ONE);
        else glBlendFunc(GL_SRC_COLOR,GL_ONE);
			glDisable(GL_BLEND);
    }

}
