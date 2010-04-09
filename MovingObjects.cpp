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
	phi_o = 0.;
	spin = 0.;
	vel_vec_i = -1;
	len_vec_i = 0;
	len_vec.resize(1);
	vel_vec.resize(1);
	len_vec[0] = Vec2(DEFAULT_LEN, DEFAULT_LEN);
	vel_vec[0] = Vec2(DEFAULT_VEL,DEFAULT_VEL);
	pos_o = Vec2(DEFAULT_POS_X,DEFAULT_POS_Y);
	v = vel_vec[0], vel = vel_vec[0];
	color = DEFAULT_OBJCOLOR;
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
	
	int numSizes = -1, numSpeeds = -1;
	
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
		
		QVector<double> r1, r2;
		// be really tolerant with object maj/minor length variable names 
		getParam( "objLenX" , r1)
		|| getParam( "rx"          , r1) 
		|| getParam( "r1"          , r1) 
	    || getParam( "objLen"   , r1)
		|| getParam( "objLenMaj", r1)
		|| getParam( "objLenMajor" , r1)
		|| getParam( "objMajorLen" , r1)
		|| getParam( "objMajLen" , r1)
		|| getParam( "xradius"   , r1);
		getParam( "objLenY" , r2)
		|| getParam( "ry"          , r2)
		|| getParam( "r2"          , r2)
		|| getParam( "objLenMinor" , r2) 
		|| getParam( "objLenMin"   , r2)
		|| getParam( "objMinorLen" , r2)
		|| getParam( "objMinLen"   , r2)
		|| getParam( "yradius"   , r2);
		
		if (!r2.size()) r2 = r1;
		if (r1.size() != r2.size()) {
			Error() << "Target size vectors mismatch for object " << i+1 << ": Specify two comma-separated lists (of the same length):  objLenX and objLenY, to create the targetSize vector!";
			return false;
		}
		o.len_vec.resize(r1.size());
		for (int j = 0; j < r1.size(); ++j)
			o.len_vec[j] = Vec2(r1[j], r2[j]);
		
		if (!o.len_vec.size()) {
			if (i) o.len_vec = objs.front().len_vec; // grab the lengths from predecessor if not specified
			else { o.len_vec.resize(1); o.len_vec[0] = Vec2Zero; }
		}
		if (numSizes < 0) numSizes = o.len_vec.size();
		else if (numSizes != o.len_vec.size()) {
			Error() << "Object " << i+1 << " has a lengths vector of size " << o.len_vec.size() << " but size " << numSizes << " was expected. All objects should have the same-sized lengths vector!";
			return false;
		}
		
		getParam( "objSpin"     , o.spin);
		getParam( "objPhi" , o.phi_o) || getParam( "phi", o.phi_o );  
		QVector<double> vx, vy;
		getParam( "objVelx"     , vx); 
		getParam( "objVely"     , vy); 
		if (!vy.size()) vy = vx;
		if (vx.size() != vy.size()) {
			Error() << "Target velocity vectors mismatch for object " << i+1 << ": Specify two comma-separated lists (of the same length):  objVelX and objVelY, to create the targetVelocities vector!";
			return false;			
		}
		o.vel_vec.resize(vx.size());
		for (int j = 0; j < vx.size(); ++j) {
			o.vel_vec[j] = Vec2(vx[j], vy[j]);
		}
		if (!o.vel_vec.size()) {
			if (i) o.vel_vec = objs.front().vel_vec;
			else { o.vel_vec.resize(1); o.vel_vec[0] = Vec2Zero; }
		}
		if (numSpeeds < 0) numSpeeds = o.vel_vec.size();
		else if (numSpeeds != o.vel_vec.size()) {
			Error() << "Object " << i+1 << " has a velocities vector of size " << o.vel_vec.size() << " but size " << numSpeeds << " was expected. All objects should have the same-sized velocities vector!";
			return false;
		}
		o.vel = o.vel_vec[0];
		o.v = Vec2Zero;
		getParam( "objXinit"    , o.pos_o.x); 
		getParam( "objYinit"    , o.pos_o.y); 
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
	if(!getParam( "tframes" , tframes) || tframes <= 0) tframes = DEFAULT_TFRAMES, ftChangeEvery = -1; 
	if (tframes <= 0 && (numSpeeds > 1 || numSizes > 1)) {
		Error() << "tframes needs to be specified because either the lengths vector or the speeds vector has length > 1!";
		return false;	
	}
	
	if(!getParam( "jitterlocal" , jitterlocal))  jitterlocal = false;
	if(!getParam( "jittermag" , jittermag))	     jittermag = DEFAULT_JITTERMAG;

	if (!getParam("ft_change_frame_cycle", ftChangeEvery) && !getParam("ftrack_change_frame_cycle",ftChangeEvery) 
		&& !getParam("ftrack_change_cycle",ftChangeEvery) && !getParam("ftrack_change",ftChangeEvery)) 
		ftChangeEvery = 0; // override default for movingobjects it is 0 which means autocompute
	

	if (!getParam("wrapEdge", wrapEdge) && !getParam("wrap", wrapEdge)) 
			wrapEdge = false;

	if (!getParam("debugAABB", debugAABB))
			debugAABB = false;

	initObjs();
	
	QString dummy;
	if (getParam("targetcycle", dummy) || getParam("speedcycle", dummy)) {
		Error() << "targetcycle and speedcycle params are no longer supported!  Instead, pass a comma-separated-list for the velocities and object sizes!";
		return false;
	}
	
    // the object area AABB -- a rect constrained by min_x_pix,min_y_pix and max_x_pix,max_y_pix
	canvasAABB = Rect(Vec2(min_x_pix, min_y_pix), Vec2(max_x_pix-min_x_pix, max_y_pix-min_y_pix)); 

	frameVars->setVariableNames(QString("frameNum objNum subFrameNum objType(0=box,1=ellipse) x y r1 r2 phi color").split(QString(" ")));
	
	
	if (ftChangeEvery == 0 && tframes > 0) {
		Log() << "Auto-set ftrack_change variable: FT_Change will be asserted every " << tframes << " frames.";		
		ftChangeEvery = tframes;
	} else if (ftChangeEvery > 0 && tframes > 0 && ftChangeEvery != tframes) {
		Warning() << "ftrack_change was defined in configuration and so was tframes (target cycle / speed cycle) but ftrack_change != tframes!";
	}
	if (!have_fv_input_file && int(nFrames) != tframes * numSizes * numSpeeds) {
		int oldnFrames = nFrames;
		nFrames = tframes * numSpeeds * numSizes;
		Warning() << "nFrames was " << oldnFrames << ", auto-set to match tframes*length(speeds)*length(sizes) = " << nFrames << "!";
	}
	// NB: the below is a performance optimization for Shapes such as Ellipse and Rectangle which create 1 display list per 
	// object -- the below ensures that the shared static display list is compiled after init is done so that we don't have to compile one later
	// while the plugin is running
	Shapes::InitStaticDisplayLists();
	
	return true;
}

void MovingObjects::initObj(ObjData & o) {
	if (o.type == EllipseType) {
		o.shape = new Shapes::Ellipse(o.len_vec[0].x, o.len_vec[0].y);
	} else {  // box, etc
		o.shape = new Shapes::Rectangle(o.len_vec[0].x, o.len_vec[0].y);
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
	if (!softCleanup) {
		Shapes::CleanupStaticDisplayLists();
	}
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
		       & objXinit (o.pos_o.x),
			   & objYinit (o.pos_o.y);
		
		const double  & objLen_o (o.len_vec[0].x), & objLen_min_o (o.len_vec[0].y);
		double  & objPhi (o.shape->angle); 
		
		float  & jitterx (o.jitterx), & jittery (o.jittery);
		
		float  & objcolor (o.color);
		
		double objLen (o.shape->scale.x * objLen_o), objLen_min (o.shape->scale.y * objLen_min_o); 

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
							o.len_vec[0] = Vec2(r1,r2);
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
							if (++o.vel_vec_i >= o.vel_vec.size()) {
								o.vel_vec_i = 0;
								++o.len_vec_i;
							}
							if (o.len_vec_i >= o.len_vec.size()) o.len_vec_i = 0;
							
							objLen = o.len_vec[o.len_vec_i].x;
							objLen_min = o.len_vec[o.len_vec_i].y;
							
								
							// apply new length by adjusting object scale
							o.shape->scale.x = objLen / objLen_o;
							o.shape->scale.y = objLen_min / objLen_min_o;
							
							objVelx = o.vel_vec[o.vel_vec_i].x;
							objVely = o.vel_vec[o.vel_vec_i].y;
						
							// init position
							if (!rndtrial) {
								x = objXinit;
								y = objYinit;
								vx = objVelx; 
								vy = objVely; 
							}
							else {
								x = ran1Gen()*mon_x_pix;
								y = ran1Gen()*mon_y_pix;
								vx = ran1Gen()*objVelx*2 - objVelx;
								vy = ran1Gen()*objVely*2 - objVely; 
							}
							objPhi = o.phi_o;
							
						}

						// update position after delay period
						// if jitter pushes us outside of motion box then do not jitter this frame
						if ((int(frameNum)%tframes /*- delay*/) > 0) { 
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
				if (fv.size() || (int(frameNum)%tframes /*- delay*/) >= 0) {
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
