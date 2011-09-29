#include "MovingObjects_Old.h"

MovingObjects_Old::MovingObjects_Old()
    : StimPlugin("MovingObjects_Old")
{
}

MovingObjects_Old::~MovingObjects_Old()
{
}

bool MovingObjects_Old::init()
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

	// use these for setting angular size and speed
	if(!getParam( "mon_x_cm" , mon_x_cm))	     mon_x_cm = 24.2f;
	if(!getParam( "mon_x_pix" , mon_x_pix))	     mon_x_pix = width();
	if(!getParam( "mon_y_cm" , mon_y_cm))	     mon_y_cm = 18.2f;
	if(!getParam( "mon_y_pix" , mon_y_pix))	     mon_y_pix = height();
	if(!getParam( "mon_z_cm" , mon_z_cm))	     mon_z_cm = 12;

	// bounding box for motion
	if(!getParam( "max_x_pix" , max_x_pix))	     max_x_pix = mon_x_pix;
	if(!getParam( "min_x_pix" , min_x_pix))	     min_x_pix = 0;
	if(!getParam( "max_y_pix" , max_y_pix))	     max_y_pix = mon_y_pix;
	if(!getParam( "min_y_pix" , min_y_pix))	     min_y_pix = 0;

	// note bgcolor already set in StimPlugin, re-default it to 1.0 if not set in data file
	if(!getParam( "bgcolor" , bgcolor))	     bgcolor = 1.;

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
	frameVars->setVariableDefaults(QVector<double>() << 0. << double(objXinit) << double(objYinit));
     return true;
}

void MovingObjects_Old::initDisplayLists()
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

void MovingObjects_Old::cleanupDisplayLists()
{
    glDeleteLists(objDL, 1);
    objDL = 0xffffffff;
    if (quad) gluDeleteQuadric(quad), quad = 0;
}

void MovingObjects_Old::cleanup()
{
    //glDisable(GL_BLEND);
    cleanupDisplayLists();
}

bool MovingObjects_Old::processKey(int key)
{
    switch ( key ) {
    case 'm':
    case 'M':
        moveFlag = !moveFlag;
        return true;
    }
    return StimPlugin::processKey(key);
}

void MovingObjects_Old::drawFrame()
{
	// Done in calling code.. glClear( GL_COLOR_BUFFER_BIT ); 

	// local target jitter
	if (jitterlocal) {
		jitterx = (ran1Gen()*jittermag - jittermag/2);
		jittery = (ran1Gen()*jittermag - jittermag/2);
	}
	else 
		jitterx = jittery = 0;
        
    if (fps_mode != FPS_Single) {
            glEnable(GL_BLEND);
            if (bgcolor >  0.5)
                glBlendFunc(GL_DST_COLOR,GL_ONE_MINUS_DST_COLOR);
            else glBlendFunc(GL_SRC_COLOR,GL_ONE_MINUS_SRC_COLOR);
    }

	doFrameDraw();
   
    if (fps_mode != FPS_Single) {
		if (bgcolor >  0.5)
			glBlendFunc(GL_DST_COLOR,GL_ONE);
        else glBlendFunc(GL_SRC_COLOR,GL_ONE);
			glDisable(GL_BLEND);
    }

}

void MovingObjects_Old::doFrameDraw()
{
	const int niters = ((int)fps_mode)+1; // hack :)
	for (int k=0; k < niters; k++) {
		    QVector<double> fv;
			if (have_fv_input_file) {
				fv = frameVars->readNext();
				if (fv.size() < 3 && frameNum) {
					// at end of file?
					Warning() << name() << "'s frame_var file ended input, stopping plugin.";
					have_fv_input_file = false;
					stop();
					return;
				} 
				if (fv.size() < 3 || fv[0] != frameNum) {
					Error() << "Error reading frame " << frameNum << " from frameVar file! Datafile frame num differs from current frame number!  Does the fps_mode of the frameVar file match the current fps mode?";	
					stop();
					return;
				}
			}
		
            if( moveFlag ){
			
                // adjust for wall bounce
                if ((x + vx + objLen/2 > max_x_pix) ||  (x + vx + objLen/2 < min_x_pix))
                    vx = -vx;
                if ((y + vy + objLen/2 > max_y_pix) || (y + vy  + objLen/2 < min_y_pix))
                    vy = -vy; 

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
                            objVelx = objVelx_o; // if targetcycle done, reset velocity
                            objVely = objVely_o; // if targetcycle done, reset velocity
                        }
                        else {
                            objVelx = objVelx*2; // double target velocity every tframes
                            objVely = objVely*2; // double target velocity every tframes
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
				// if jitter pushes us outside of motion box then do not jitter this frame
                if ((int(frameNum)%tframes - delay) > 0) { 
					if ((x + vx + objLen/2 + jitterx > max_x_pix) ||  (x + vx + objLen/2 + jitterx < min_x_pix))
						x += vx;
					else x+= vx + jitterx;
					if ((y + vy + objLen/2 + jittery > max_y_pix) || (y + vy + objLen/2 + jittery < min_y_pix)) 
						y += vy;
					else y += vy + jittery;
                }
            }

            //x = trajdata[frameNum] + jitterx;
            //y = trajdata[frameNum+tframes] + jittery;
            // draw stim if out of delay period
            if (fv.size() || (int(frameNum)%tframes - delay) >= 0) {
				float r,g,b;

				if (fps_mode == FPS_Triple || fps_mode == FPS_Dual) {
					b = g = r = bgcolor;					
					// order of frames comes from config parameter 'color_order' but defaults to RGB
					if (k == b_index) b = objcolor;
					else if (k == r_index) r = objcolor;
					else if (k == g_index) g = objcolor;

				} else 
					b = g = r = objcolor;

				glColor3f(r,g,b);

                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
				if (fv.size()) { x = fv[1], y = fv[2]; }
                glTranslatef(x, y, 1);
                glCallList(objDL);
                glPopMatrix();
				if (!fv.size()) 
					// nb: push() needs to take all doubles as args!
					frameVars->push(double(frameNum), double(x), double(y));
            }

	}
}
