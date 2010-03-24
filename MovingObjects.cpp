#include "MovingObjects.h"
#include "Shapes.h"
#include "GLHeaders.h"

#define DEFAULT_TYPE BoxType
#define DEFAULT_LEN 8
#define DEFAULT_VEL 4
#define DEFAULT_POS_X 400
#define DEFAULT_POS_Y 300
#define DEFAULT_BGCOLOR 1.0
#define DEFAULT_OBJCOLOR 0.0
#define DEFAULT_TFRAMES 120*60*60*10 /* 120fps*s */
#define DEFAULT_JITTERMAG 2
#define DEFAULT_RSEED -1
#define NUM_FRAME_VARS 10

MovingObjects::MovingObjects()
    : StimPlugin("MovingObjects")
{
}

MovingObjects::~MovingObjects()
{
}

QString MovingObjects::objTypeStrs[] = { "box", "ellipsoid" };

MovingObjects::ObjData::ObjData() : shape(0) { initDefaults(); }

void MovingObjects::ObjData::initDefaults() {
	if (shape) delete shape, shape = 0;
	type = DEFAULT_TYPE, jitterx = 0, jittery = 0, 
	len_maj_o = DEFAULT_LEN, 
	len_min_o = DEFAULT_LEN,
	phi_o = 0.;
	spin = 0.;
	vel_o = Vec2(DEFAULT_VEL,DEFAULT_VEL), pos_o = Vec2(DEFAULT_POS_X,DEFAULT_POS_Y),
	v = vel_o, vel = vel_o,
	tcyclecount = 0, targetcycle = 0, speedcycle = 0, delay = 0, color = DEFAULT_OBJCOLOR;
}

bool MovingObjects::init()
{
    moveFlag = true;
	
	// set up pixel color blending to enable 480Hz monitor mode

	// set default parameters
	// basic target attributes
	objs.clear();
	
	if (!getParam("numObj", numObj)
		&& !getParam("numObjs", numObj) ) numObj = 1;
	
	for (int i = 0; i < numObj; ++i) {
		if (i > 0)	paramSuffixPush(QString::number(i+1)); // make the additional obj params end in a number
       	objs.push_back(!i ? ObjData() : objs.front()); // get defaults from first object
		ObjData & o = objs.back();
			
		// if any of the params below are missing, the defaults in initDefaults() above are taken
		
		QString otype; 
		if (getParam( "objType"     , otype)) {
			otype = otype.trimmed().toLower();
			
			if (otype == "ellipse" || otype == "ellipsoid" || otype == "circle" || otype == "disk" || otype == "sphere") {
				if (otype == "sphere") Warning() << "`sphere' objType not supported, defaulting to ellipsoid.";
				o.type = EllipseType;
			} else
				o.type = BoxType;
		}
		
		// be really tolerant with object maj/minor length variable names 
		getParam( "rx"          , o.len_maj_o) 
		|| getParam( "r1"          , o.len_maj_o) 
	    || getParam( "objLen"   , o.len_maj_o)
		|| getParam( "objLenMaj", o.len_maj_o)
		|| getParam( "objLenMajor" , o.len_maj_o)
		|| getParam( "objMajorLen" , o.len_maj_o)
		|| getParam( "objMajLen" , o.len_maj_o);
		getParam( "ry"          , o.len_min_o)
		|| getParam( "r2"          , o.len_min_o)
		|| getParam( "objLenMinor" , o.len_min_o) 
		|| getParam( "objLenMin"   , o.len_min_o)
		|| getParam( "objMinorLen" , o.len_min_o)
		|| getParam( "objMinLen"   , o.len_min_o);
		
		getParam( "objSpin"     , o.spin);
		getParam( "objPhi" , o.phi_o) || getParam( "phi", o.phi_o );  
		getParam( "objVelx"     , o.vel_o.x); 
		getParam( "objVely"     , o.vel_o.y); o.vel = o.vel_o;
		getParam( "objXinit"    , o.pos_o.x); 
		getParam( "objYinit"    , o.pos_o.y); 
		getParam( "targetcycle" , o.targetcycle);
		getParam( "speedcycle"  , o.speedcycle);
		getParam( "delay"       , o.delay);  // delay from start of tr to stim onset;
		getParam( "objcolor"    , o.color);
									
		paramSuffixPop();
		
	}

	// use these for setting angular size and speed
	if(!getParam( "mon_x_pix" , mon_x_pix))	     mon_x_pix = width();
	if(!getParam( "mon_y_pix" , mon_y_pix))	     mon_y_pix = height();

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
	if(!getParam( "tframes" , tframes))          tframes = DEFAULT_TFRAMES; 

	if(!getParam( "jitterlocal" , jitterlocal))  jitterlocal = false;
	if(!getParam( "jittermag" , jittermag))	     jittermag = DEFAULT_JITTERMAG;


	if (!getParam("wrapEdge", wrapEdge) && !getParam("wrap", wrapEdge)) 
			wrapEdge = false;

	if (!getParam("debugAABB", debugAABB))
			debugAABB = false;

	initObjs();
	
    // the object area AABB -- a rect constrained by min_x_pix,min_y_pix and max_x_pix,max_y_pix
	canvasAABB = Rect(Vec2(min_x_pix, min_y_pix), Vec2(max_x_pix-min_x_pix, max_y_pix-min_y_pix)); 

	frameVars->setVariableNames(QString("frameNum objNum subFrameNum objType(0=box,1=ellipse) x y r1 r2 phi color").split(QString(" ")));
	
	
	if (ftChangeEvery <= -1 && tframes > 0) {
		Log() << "Auto-set ftrack_change variable: FT_Change will be asserted every " << tframes << " frames.";		
		ftChangeEvery = tframes;
	} else if (ftChangeEvery > -1 && tframes > 0 && ftChangeEvery != tframes) {
		Warning() << "ftrack_change was defined in configuration and so was tframes (target cycle / speed cycle) but ftrack_change != tframes!";
	}
	// NB: the below is a performance optimization for Shapes such as Ellipse and Rectangle which create 1 display list per 
	// object -- the below ensures that the shared static display list is compiled after init is done so that we don't have to compile one later
	// while the plugin is running
	Shapes::DoPerformanceHackInit();
	
	return true;
}

void MovingObjects::initObj(ObjData & o) {
	if (o.type == EllipseType) {
		o.shape = new Shapes::Ellipse(o.len_maj_o, o.len_min_o);
	} else {  // box, etc
		o.shape = new Shapes::Rectangle(o.len_maj_o, o.len_min_o);
	}
	o.shape->position = o.pos_o;	
	o.shape->noMatrixAttribPush = true; ///< performance hack
}

void MovingObjects::initObjs() {
	for (QList<ObjData>::iterator it = objs.begin(); it != objs.end(); ++it) {
		initObj(*it);
	}
}

void MovingObjects::cleanupObjs() {
	for (QList<ObjData>::iterator it = objs.begin(); it != objs.end(); ++it) {
		ObjData & o = *it;
		delete o.shape, o.shape = 0;
	}	
	objs.clear();
	numObj = 0;
}



void MovingObjects::cleanup()
{
    cleanupObjs();
}

bool MovingObjects::processKey(int key)
{
    switch ( key ) {
    case 'm':
    case 'M':
        moveFlag = !moveFlag;
        return true;
	case 'j':
	case 'J':
		jitterlocal = !jitterlocal;
		return true;
    }
    return StimPlugin::processKey(key);
}

void MovingObjects::drawFrame()
{
	glClear( GL_COLOR_BUFFER_BIT ); 
        
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

void MovingObjects::doFrameDraw()
{
	int objNum = 0;
	for (QList<ObjData>::iterator it = objs.begin(); it != objs.end(); ++it, ++objNum) {		
		ObjData & o = *it;
		if (!o.shape) continue; // should never happen..		
		double & x  (o.shape->position.x),
		       & y  (o.shape->position.y),
		       & vx (o.v.x),
		       & vy (o.v.y),
		       & objVelx (o.vel.x),
		       & objVely (o.vel.y);
		const double
		       & objVelx_o (o.vel_o.x),
		       & objVely_o (o.vel_o.y),
		       & objXinit (o.pos_o.x),
			   & objYinit (o.pos_o.y);
		
		const float  & objLen_o (o.len_maj_o), & objLen_min_o (o.len_min_o);
		double  & objPhi (o.shape->angle); 
		
		float  & jitterx (o.jitterx), & jittery (o.jittery);
		
		float  & objcolor (o.color);
		
		double objLen (o.shape->scale.x * objLen_o), objLen_min (o.shape->scale.y * objLen_min_o); 
		int & tcyclecount(o.tcyclecount);
		const int & targetcycle(o.targetcycle),
		          & speedcycle(o.speedcycle),
		          & delay(o.delay);

		// local target jitter
		if (jitterlocal) {
			jitterx = (ran1Gen()*jittermag - jittermag/2);
			jittery = (ran1Gen()*jittermag - jittermag/2);
		}
		else 
			jitterx = jittery = 0;
		
		const int niters = ((int)fps_mode)+1; // hack :)
		
		for (int k=0; k < niters; k++) {
				QVector<double> fv;

				const Rect aabb = o.shape->AABB();

				if (have_fv_input_file) {
					fv = frameVars->readNext();
					if (fv.size() < NUM_FRAME_VARS && frameNum) {
						// at end of file?
						Warning() << name() << "'s frame_var file ended input, stopping plugin.";
						have_fv_input_file = false;
						stop();
						return;
					} 
					if (fv.size() < NUM_FRAME_VARS || fv[0] != frameNum) {
						Error() << "Error reading frame " << frameNum << " from frameVar file! Datafile frame num differs from current frame number!  Do all the fps_mode and numObj parameters of the frameVar file match the current fps mode and numObjs?";	
						stop();
						return;
					}
					if (fv[1] != objNum) {
						Error() << "Error reading object " << objNum << " from frameVar file! Datafile object num differs from current object number!  Do all the fps_mode and numObj parameters of the frameVar file match the current fps mode and numObjs?";	
						stop();
						return;						
					}
					if (fv[2] != k) {
						Error() << "Error reading subframe " << k << " from frameVar file! Datafile subframe num differs from current subframe numer!  Do all the fps_mode and numObj parameters of the frameVar file match the current fps mode and numObjs?";	
						stop();
						return;												
					}
					

					
					bool didInitLen = false;
					const ObjType otype = ObjType(int(fv[3]));
					double r1 = fv[6], r2 = fv[7];
					if (!k && !frameNum) {
						// do some required initialization if on frame 0 for this object to make sure r1, r2 and  obj type jive
						if (otype != o.type || !eqf(r1, objLen) || !eqf(r2, objLen_min)) {
							delete o.shape;							
							o.type = otype;
							o.len_maj_o = r1;
							o.len_min_o = r2;
							didInitLen = true;
							initObj(o);
						}		
					}
					
					x = fv[4];
					y = fv[5];
					// handle length changes mid-plugin-run
					if (!didInitLen && (!eqf(r1, objLen) || !eqf(r2, objLen))) {
						if (o.type == EllipseType) {
							Shapes::Ellipse *e = (Shapes::Ellipse *)o.shape;
							e->xradius = r1;
							e->yradius = r2;
						} else {
							Shapes::Rectangle *r = (Shapes::Rectangle *)o.shape;
							r->width = r1;
							r->height = r2;
						} 
						o.shape->applyChanges(); ///< essentialy a NOOP
					}
					// handle type change mid-plugin-run
					if (o.type != otype) {
						Shapes::Shape *oldshape = o.shape;
						o.shape = 0;
						o.type = otype;
						initObj(o);
						o.shape->position = oldshape->position;
						o.shape->scale = oldshape->scale;
						o.shape->color = oldshape->color;
						o.shape->angle = oldshape->angle;
						delete oldshape;
					}
					
					objPhi = fv[8];
					objcolor = fv[9];
					
				} else {
				

					if (moveFlag) {

						// wrap objects that floated past the edge of the screen
						if (wrapEdge && !canvasAABB.intersects(aabb))
							wrapObject(o, aabb);

						
						if (!wrapEdge) {
							
							// adjust for wall bounce
							if ((x + vx + aabb.size.w/2 > max_x_pix) ||  (x + vx - aabb.size.w/2 < min_x_pix))
								vx = -vx;
							if ((y + vy + aabb.size.h/2 > max_y_pix) || (y + vy  - aabb.size.h/2 < min_y_pix))
								vy = -vy; 
							
						}

						// initialize position iff k==0 and frameNum is a multiple of tframes
						if ( !k && !(frameNum%tframes)) {
						
							// update target size if size-series
							if ((targetcycle > 0) && (frameNum > 0)) {
								if (tcyclecount++ == targetcycle) {
									tcyclecount = 0;
									objLen = objLen_o; // if targetcycle done, reset objLen
									objLen_min = objLen_min_o;
								} else {
									objLen *= 2.; // double target size every tframes
									objLen_min *= 2.;
								}
								
								// apply new length by adjusting object scale
								o.shape->scale.x = objLen / objLen_o;
								o.shape->scale.y = objLen_min / objLen_min_o;
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
							if (!wrapEdge && ((x + vx + aabb.size.w/2 + jitterx > max_x_pix) 
											  ||  (x + vx - aabb.size.w/2 + jitterx < min_x_pix)))
								x += vx;
							else 
								x+= vx + jitterx;
							if (!wrapEdge && ((y + vy + aabb.size.h/2 + jittery > max_y_pix) 
											  || (y + vy - aabb.size.h/2 + jittery < min_y_pix))) 
								y += vy;
							else 
								y += vy + jittery;
							
							// also apply spin
							objPhi += o.spin;
						}

					}
					
				} //  end !have_fv_input_file

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

					o.shape->color = Vec3(r, g, b);
					o.shape->draw();
					
					///DEBUG HACK FOR AABB VERIFICATION					
					if (debugAABB && k==0) {
						Rect r = aabb;
						glColor3f(0.,1.,0); 
						glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
						glBegin(GL_QUADS);
						glVertex2f(r.origin.x,r.origin.y);
						glVertex2f(r.origin.x+r.size.w,r.origin.y);
						glVertex2f(r.origin.x+r.size.w,r.origin.y+r.size.h);
						glVertex2f(r.origin.x,r.origin.y+r.size.h);	
						glEnd();
						glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
						//objPhi += 1.;
					}					
					
					if (!fv.size()) 
						// nb: push() needs to take all doubles as args!
						frameVars->push(double(frameNum), double(objNum), double(k), double(o.type), double(x), double(y), double(objLen), double(objLen_min), double(objPhi), double(objcolor));
				}

		}
	}
}

void MovingObjects::wrapObject(ObjData & o, const Rect & aabb) const {
	Shapes::Shape & s = *o.shape;

	// wrap right edge
	if (aabb.left() >= canvasAABB.right()) s.position.x = (-aabb.size.w/2) + (aabb.left() - canvasAABB.right());
	// wrap left edge
	if (aabb.right() <= canvasAABB.left()) s.position.x = (canvasAABB.right()+aabb.size.w/2) - (canvasAABB.left()-aabb.right());
	// wrap top edge
	if (aabb.bottom() >= canvasAABB.top()) s.position.y = (-aabb.size.h/2) + (aabb.bottom() - canvasAABB.top());
	// wrap bottom edge
	if (aabb.top() <= canvasAABB.bottom()) s.position.y = (canvasAABB.top()+aabb.size.h/2) - (canvasAABB.bottom()-aabb.top());
}
