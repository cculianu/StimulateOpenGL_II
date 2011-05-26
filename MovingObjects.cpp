#include "MovingObjects.h"
#include "Shapes.h"
#include "GLHeaders.h"
#include <math.h>

#define DEFAULT_TYPE BoxType
#define DEFAULT_LEN 8
#define DEFAULT_VEL 4
#define DEFAULT_POS_X 400
#define DEFAULT_POS_Y 300
#define DEFAULT_POS_Z 0
#define DEFAULT_BGCOLOR 1.0
#define DEFAULT_OBJCOLOR 0.0
#define DEFAULT_TFRAMES 120*60*60*10 /* 120fps*s */
#define DEFAULT_JITTERMAG 2
#define DEFAULT_RSEED -1
#define NUM_FRAME_VARS N_FVCols
#define DEFAULT_MAX_Z 1000
#define DEFAULT_SHININESS (50./128.)
#define DEFAULT_AMBIENT .2f
#define DEFAULT_DIFFUSE .8f
#define DEFAULT_EMISSION 0.f
#define DEFAULT_SPECULAR 1.f
#define DEFAULT_LIGHT_AMBIENT .5
#define DEFAULT_LIGHT_DIFFUSE 1.0
#define DEFAULT_LIGHT_SPECULAR 1.0
#define DEFAULT_LIGHT_CONSTANT_ATTENUATION 1.0
#define DEFAULT_LIGHT_LINEAR_ATTENUATION 0.0
#define DEFAULT_LIGHT_QUADRATIC_ATTENUATION 0.0


MovingObjects::MovingObjects()
    : StimPlugin("MovingObjects"), savedrng(false)
{
}

MovingObjects::~MovingObjects()
{
}

QString MovingObjects::objTypeStrs[] = { "box", "ellipsoid", "sphere", NULL };

MovingObjects::ObjData::ObjData() : shape(0) { initDefaults(); }

void MovingObjects::ObjData::initDefaults() {
	if (shape) delete shape, shape = 0;
	type = DEFAULT_TYPE, jitterx = 0., jittery = 0., jitterz = 0., 
	phi_o = 0.;
	spin = 0.;
	vel_vec_i = -1;
	len_vec_i = 0;
	len_vec.resize(1);
	vel_vec.resize(1);
	len_vec[0] = Vec2(DEFAULT_LEN, DEFAULT_LEN);
	vel_vec[0] = Vec3(DEFAULT_VEL,DEFAULT_VEL,0.0);
	
	pos_o = Vec3(DEFAULT_POS_X,DEFAULT_POS_Y,DEFAULT_POS_Z);
	v = vel_vec[0], vel = vel_vec[0];
	color = DEFAULT_OBJCOLOR;
	shininess = DEFAULT_SHININESS;
	ambient = DEFAULT_AMBIENT;
	diffuse = DEFAULT_DIFFUSE;
	emission = DEFAULT_EMISSION;
	specular = DEFAULT_SPECULAR;
	debugLvl = 0;
}

template <typename T, typename U, typename V>
void ChkAndClampParam(const char *paramName, T & paramVal, const U & minVal, const V & maxVal, int objNum = -1)
{
	bool bad = false;
	const T pvalin (paramVal);
	if (paramVal < minVal) paramVal = minVal, bad = true;
	if (paramVal > maxVal) paramVal = maxVal, bad = true;
	if (bad) Warning() << paramName << (objNum > -1 ? QString::number(objNum) : QString("")) << " bad parameter: Specified " << pvalin << ", needs to be in the range " << minVal << "-" << maxVal << ".  Parameter clamped to " << paramVal << "."; 
}

bool MovingObjects::init()
{
	
	initCameraDistance();

    moveFlag = true;
	is3D = false;

	
	// set up pixel color blending to enable 480Hz monitor mode

	// set default parameters
	// basic target attributes
	objs.clear();
	
	if (!getParam("numObj", numObj)
		&& !getParam("numObjs", numObj) ) numObj = 1;
	
	int numSizes = -1, numSpeeds = -1;

	fvHasPhiCol = fvHasZCol = fvHasZScaledCol = false;
	didScaledZWarning = 0;
	
	
	configSuppressesFrameVar.clear(); configSuppressesFrameVar.resize(numObj);
	
	for (int i = 0; i < numObj; ++i) {
		ConfigSuppressesFrameVar & csfv (configSuppressesFrameVar[i]);
		if (i > 0)	{
			paramSuffixPush(QString::number(i+1)); // make the additional obj params end in a number
		    csfv = configSuppressesFrameVar[0]; // copy csfv from first object as default for all objects, as per Leonardo 5.20.2011 email
		}
       	objs.push_back(!i ? ObjData() : objs.front()); // get defaults from first object
		ObjData & o = objs.back();
		o.objNum = i;
			
		// if any of the params below are missing, the defaults in initDefaults() above are taken
		
		QString otype; 
		if (getParam( "objType"     , otype)) {
			csfv[FV_objType] = true;
			
			otype = otype.trimmed().toLower();
			
			if (otype == "ellipse" || otype == "ellipsoid" || otype == "circle" || otype == "disk" || otype == "sphere"
				|| otype == "1") {
				if (otype == "sphere") //Warning() << "`sphere' objType not supported, defaulting to ellipsoid.";
					o.type = SphereType;
				else
					o.type = EllipseType;
			} else
				o.type = BoxType;
		}
		
		QVector<double> r1, r2;
		// be really tolerant with object maj/minor length variable names 
		csfv[FV_r1] = 
		  getParam( "objLenX" , r1)
			|| getParam( "rx"          , r1) 
			|| getParam( "r1"          , r1) 
			|| getParam( "objLen"   , r1)
			|| getParam( "objLenMaj", r1)
			|| getParam( "objLenMajor" , r1)
			|| getParam( "objMajorLen" , r1)
			|| getParam( "objMajLen" , r1)
			|| getParam( "xradius"   , r1);
		csfv[FV_r2] =
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
		for (int j = 0; j < r1.size(); ++j) {
			if (o.type == SphereType && !eqf(r1[j],r2[j])) {
				Warning() << "Obj " << i << ": sphere specified with objLenX != objLenY!  Ignoring objLenY (this means pure spheres only, spheroids not supported!)";
				r2[j] = r1[j];
			}
			o.len_vec[j] = Vec2(r1[j], r2[j]);
		}
		
		if (!o.len_vec.size()) {
			if (i) o.len_vec = objs.front().len_vec; // grab the lengths from predecessor if not specified
			else { o.len_vec.resize(1); o.len_vec[0] = Vec2Zero; }
		}
		if (numSizes < 0) numSizes = o.len_vec.size();
		else if (numSizes != o.len_vec.size()) {
			Error() << "Object " << i+1 << " has a lengths vector of size " << o.len_vec.size() << " but size " << numSizes << " was expected. All objects should have the same-sized lengths vector!";
			return false;
		}
		
		csfv[FV_phi] = getParam( "objPhi" , o.phi_o) || getParam( "phi", o.phi_o );  
		QVector<double> vx, vy, vz;
		getParam( "objVelx"     , vx); 
		getParam( "objVely"     , vy); 
		getParam( "objVelz"     , vz); 
		if (!vy.size()) vy = vx;
		if (vx.size() != vy.size()) {
			Error() << "Target velocity vectors mismatch for object " << i+1 << ": Specify two comma-separated lists (of the same length):  objVelX and objVelY, to create the targetVelocities vector!";
			return false;			
		}
		if (vz.size() != vx.size()) {
			vz.clear();
			vz.fill(0., vx.size());
		}
		o.vel_vec.resize(vx.size());
		for (int j = 0; j < vx.size(); ++j) {
			o.vel_vec[j] = Vec3(vx[j], vy[j], vz[j]);
			if (!is3D && !eqf(vz[j], 0.))
				is3D = true;
		}
		if (!o.vel_vec.size()) {
			if (i) o.vel_vec = objs.front().vel_vec;
			else { o.vel_vec.resize(1); o.vel_vec[0] = Vec3Zero; }
		}
		if (numSpeeds < 0) numSpeeds = o.vel_vec.size();
		else if (numSpeeds != o.vel_vec.size()) {
			Error() << "Object " << i+1 << " has a velocities vector of size " << o.vel_vec.size() << " but size " << numSpeeds << " was expected. All objects should have the same-sized velocities vector!";
			return false;
		}
		o.vel = o.vel_vec[0];
		o.v = Vec3Zero;
		getParam( "objXinit"    , o.pos_o.x); 
		getParam( "objYinit"    , o.pos_o.y); 
		bool hadZInit = getParam( "objZinit"    , o.pos_o.z);
		double zScaled;
		if (getParam( "objZScaledinit"    , zScaled)) {
			const double newZ = distanceToZ(zScaled);
			if (hadZInit) Warning() << "Object " << i << " had objZInit and objZScaledinit params specified, using the zScaled param and ignoring zinit.";
			if (hadZInit && !eqf(o.pos_o.z, newZ)) 
				Warning() << "objZInit and objzScaledInit disagree!";
			o.pos_o.z = newZ;
		}
		if (!is3D && !eqf(o.pos_o.z, 0.)) is3D = true;
		
		csfv[FV_color] = getParam( "objcolor"    , o.color);		ChkAndClampParam("objcolor", o.color, 0., 1., i);
		getParam( "objShininess", o.shininess); ChkAndClampParam("objShininess", o.shininess, 0., 1., i);
		getParam( "objAmbient", o.ambient);     ChkAndClampParam("objAmbient", o.ambient, 0., 1., i);
		getParam( "objDiffuse", o.diffuse);     ChkAndClampParam("objDiffuse", o.diffuse, 0., 1., i);
		getParam( "objEmission", o.emission);   ChkAndClampParam("objEmission", o.emission, 0., 1., i);
		getParam( "objSpecular", o.specular);   ChkAndClampParam("objSpecular", o.specular, -1., 1., i);
			
		if (!getParam( "objDebug", o.debugLvl)) o.debugLvl = 0;
		
		paramSuffixPop();
		
	}

	// note bgcolor already set in StimPlugin, re-default it to 1.0 if not set in data file
	if(!getParam( "bgcolor" , bgcolor))	     bgcolor = 1.;

	// trajectory stuff
	if(!getParam( "rndtrial" , rndtrial))	     rndtrial = 0;
		// rndtrial=0 -> repeat traj every tframes; 
		// rndtrial=1 new start point and speed every tframes; start=rnd(mon_x_pix,mon_y_pix); speed= random +-objVelx, objVely
		// rndtrial=2 random position and direction, static velocity
	    // rndtrial=3 keep old position of last tframes run, random velocity based on initialization range
		// rndtrial=4 keep old position of last tframes run, static speed, random direction
	if(!getParam( "rseed" , rseed))              rseed = -1;  //set start point of rnd seed;
	ran1Gen.reseed(rseed);
	
	bool dontInitFvars (false);
	
	// if we are looping the plugin, then we continue the random number generator state
	if (savedrng && rndtrial) {
		ran1Gen.reseed(saved_ran1state);
		Debug() << ".. continued RNG with seed " << saved_ran1state;
		dontInitFvars = true;
	}
	
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
	

	if (!(getParam("wrapEdge", wrapEdge) || getParam("wrap", wrapEdge))) 
			wrapEdge = false;

	if (!getParam("debugAABB", debugAABB))
			debugAABB = false;

	// Light position stuff...
	if (!(getParam("sharedLight", lightIsFixedInSpace) || getParam("fixedLight", lightIsFixedInSpace) || getParam("lightIsFixed", lightIsFixedInSpace) || getParam("lightIsFixedInSpace", lightIsFixedInSpace)))
		lightIsFixedInSpace = true;	
	QVector<double> tmpvec;
	if ((getParam("lightPosition", tmpvec) || getParam("lightPos", tmpvec)) && tmpvec.size() >= 3) {
		for (int i = 0; i < 3; ++i) lightPos[i] = tmpvec[i];
		lightPos[2] = -lightPos[2];  // invert Z: OpenGL coords have positive z towards camera and we are inverse of that
	} else {
		lightPos = Vec3(width(), height(), 100.0);
	}
	lightIsDirectional = !lightIsFixedInSpace;
	if (!getParam("lightAmbient", lightAmbient)) lightAmbient = DEFAULT_LIGHT_AMBIENT;
	if (!getParam("lightDiffuse", lightDiffuse)) lightDiffuse = DEFAULT_LIGHT_DIFFUSE;
	if (!getParam("lightSpecular", lightSpecular)) lightSpecular = DEFAULT_LIGHT_SPECULAR;
	if (   !getParam("lightConstantAttenuation", lightConstantAttenuation) 
		&& !getParam("lightAttenuationConstant", lightConstantAttenuation)) lightConstantAttenuation = DEFAULT_LIGHT_CONSTANT_ATTENUATION;
	if (   !getParam("lightLinearAttenuation", lightLinearAttenuation)
		&& !getParam("lightAttenuationLinear", lightLinearAttenuation)) lightLinearAttenuation = DEFAULT_LIGHT_LINEAR_ATTENUATION;
	if (   !getParam("lightQuadraticAttenuation", lightQuadraticAttenuation)
		&& !getParam("lightAttenuationQuadratic", lightQuadraticAttenuation)) lightQuadraticAttenuation = DEFAULT_LIGHT_QUADRATIC_ATTENUATION;
	ChkAndClampParam("lightAmbient", lightAmbient, 0.0, 1.0);
	ChkAndClampParam("lightDiffuse", lightDiffuse, 0.0, 1.0);
	ChkAndClampParam("lightSpecular", lightSpecular, -1.0, 1.0);
	ChkAndClampParam("lightConstantAttenuation", lightConstantAttenuation, 0., 2e9);
	ChkAndClampParam("lightLinearAttenuation", lightLinearAttenuation, 0., 2e9);
	ChkAndClampParam("lightQuadraticAttenuation", lightQuadraticAttenuation, 0., 2e9);
	
	initObjs();
	
	QString dummy;
	if (getParam("targetcycle", dummy) || getParam("speedcycle", dummy)) {
		Error() << "targetcycle and speedcycle params are no longer supported!  Instead, pass a comma-separated-list for the velocities and object sizes!";
		return false;
	}

	if ( getParam( "max_x_pix" , dummy) || getParam( "min_x_pix" , dummy) 
		|| getParam( "max_y_pix" , dummy) || getParam( "min_y_pix" , dummy) ) {
		Error() << "min_x_pix/max_x_pix/min_y_pix/max_y_pix no longer supported in MovingObjects.  Use the lmargin,rmargin,bmargin,tmargin params instead!";
		return false;
	}
	// these affect bounding box for motion
	max_x_pix = width() - rmargin;
	min_x_pix = lmargin;
	min_y_pix = bmargin;
	max_y_pix = height() - tmargin;
	
	if (lmargin < 0 || bmargin < 0 || tmargin < 0 || rmargin < 0) {
		Error() << "Specified a negative margin.  None of (tmargin,bmargin,lmargin,rmargin) can be negative.";
		return false;
	}
	if (lmargin+rmargin >= int(width()) || bmargin+tmargin >= int(height())) {
		Error() << "Specfied margins are larger than the window size!  Either specify mon_x_pix/mon_y_pix to encompass such" 
		<< " generous margins, or shrink them!";
		return false;
	}
	
    // the object area AABB -- a rect constrained by min_x_pix,min_y_pix and max_x_pix,max_y_pix
	canvasAABB = Rect(Vec2(min_x_pix, min_y_pix), Vec2(max_x_pix-min_x_pix, max_y_pix-min_y_pix)); 

	//if (!dontInitFvars) {
		ObjData o = objs.front();
		double x = o.pos_o.x, y = o.pos_o.y, r1 = o.len_vec.front().x, r2 = o.len_vec.front().y;
		frameVars->setVariableNames(   QString(          "frameNum objNum subFrameNum objType(0=box,1=ellipse,2=sphere) x y r1 r2 phi color z zScaled").split(QString(" ")));
		frameVars->setVariableDefaults(QVector<double>()  << 0     << 0   << 0        << o.type                << x
									                                                                             << y
									                                                                               << r1
									                                                                                  << r2
									                                                                                    << o.phi_o
									                                                                                         << o.color 
																																   << 0.0
																																	   << 1.0);
		frameVars->setPrecision(10, 9);
		frameVars->setPrecision(11, 9);
		
	//}
	
	
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
	
	dontCloseFVarFileAcrossLoops = bool(rndtrial && nFrames);

	// NB: the below is a performance optimization for Shapes such as Ellipse and Rectangle which create 1 display list per 
	// object -- the below ensures that the shared static display list is compiled after init is done so that we don't have to compile one later
	// while the plugin is running
	Shapes::InitStaticDisplayLists();
	
	if (!softCleanup) glGetIntegerv(GL_SHADE_MODEL, &savedShadeModel);
	
	if (!getParam("zBoundsFar", zBoundsFar)) zBoundsFar = DEFAULT_MAX_Z;
	if (zBoundsFar < 0.0) {
		Error() << "Specified zBoundsNear value is too small -- it needs to be positive nonzeo!";
		return false;
	}
	if (!getParam("zBoundsNear", zBoundsNear)) zBoundsNear = (-cameraDistance) + 100;
	if (zBoundsNear >= zBoundsFar) {
		Error() << "zBoundsNear needs to be less than zBoundsFar!  (zBoundsFar is " << zBoundsFar << ", zBoundsNear is " << zBoundsNear << ")";
		return false;
	}
	if (!have_fv_input_file) {
		if (is3D) 
			Debug() << "Param spec has some Z information for objects: will generate Z values for jitter and rndtrial!";
		else
			Debug() << "Param spec lacking Z information for objects: plugin will use 2D compatibility mode for jitter and rndtrial generation.";
	}
	
	initBoundingNormals();
	
	nSubFrames = ((int)fps_mode)+1; // hack :)	

	fvs_block.fill(QVector<QVector<double> >(nSubFrames),numObj);
	
	return true;
}

void MovingObjects::initObj(ObjData & o) {
	if (o.type == EllipseType) {
		o.shape = new Shapes::Ellipse(o.len_vec[0].x, o.len_vec[0].y);
	} else if (o.type == SphereType) {
		Shapes::Sphere *sph = 0;
		o.shape = sph = new Shapes::Sphere(o.len_vec[0].x);
		for (int i = 0; i < (int)o.len_vec.size(); ++i) o.len_vec[i].y = o.len_vec[i].x; ///< enforce lengths in x/y equal for spheres
		sph->lightIsFixedInSpace = lightIsFixedInSpace;
		for (int i = 0; i < 3; ++i)	sph->lightPosition[i] = lightPos[i]*2.0;
		sph->lightPosition[3] = lightIsDirectional ? 0.0f : 1.0f;
		for (int i = 0; i < 3; ++i) {
			sph->lightAmbient[i] = lightAmbient;
			sph->lightSpecular[i] = lightSpecular;
			sph->lightDiffuse[i] = lightDiffuse;
			sph->ambient[i] = o.ambient;
			sph->diffuse[i] = o.diffuse;
			sph->emission[i] = o.emission;
			sph->specular[i] = o.specular;
		}
		sph->lightAttenuations[0] = lightConstantAttenuation;
		sph->lightAttenuations[1] = lightLinearAttenuation;
		sph->lightAttenuations[2] = lightQuadraticAttenuation;
		sph->shininess = o.shininess*128.0;
		sph->lightAmbient[3] = 1.;
		sph->lightSpecular[3] = 1.;
		sph->lightDiffuse[3] = 1.;
		sph->ambient[3] = 1.0;
		sph->diffuse[3] = 1.0;
		sph->emission[3] = 1.0;
		sph->specular[3] = 1.0;
		o.color = 1.; // hard-code white color for spheres
		o.spin = 0.;
		o.phi_o = 0.; // no spin or phi for spheres...
	} else {  // box, etc
		o.shape = new Shapes::Rectangle(o.len_vec[0].x, o.len_vec[0].y);
	}
	o.shape->position = o.pos_o;
	o.shape->noMatrixAttribPush = true; ///< performance hack	
}

void MovingObjects::initObjs() {
	int i = 0;
	for (QList<ObjData>::iterator it = objs.begin(); it != objs.end(); ++it, ++i) {
		ObjData & o = *it;
		initObj(o);
		if (i < savedLastPositions.size())
			o.lastPos = savedLastPositions[i];
	}
}

void MovingObjects::cleanupObjs() {
	savedLastPositions.clear();
	if (softCleanup) savedLastPositions.reserve(objs.size());
	for (QList<ObjData>::iterator it = objs.begin(); it != objs.end(); ++it) {
		ObjData & o = *it;
		if (softCleanup) savedLastPositions.push_back(o.lastPos);
		delete o.shape, o.shape = 0;
	}	
	objs.clear();
	shapes2del.clear();
	numObj = 0;
}



void MovingObjects::cleanup()
{
	if ((savedrng = softCleanup)) {
		saved_ran1state = ran1Gen.currentSeed();		
	}
    cleanupObjs();
	if (!softCleanup) {
		Shapes::CleanupStaticDisplayLists();
		glShadeModel(savedShadeModel);
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
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ); 
        
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

void MovingObjects::reinitObj(ObjData & o, ObjType otype)
{
	Shapes::Shape *oldshape = o.shape;
	o.shape = 0;
	o.type = otype;
	initObj(o);
	if (oldshape) {
		o.shape->copyProperties(oldshape);
		// this is because some references to oldshape are still scoped in calling code!
		shapes2del.push_back(oldshape);
	}
}

void MovingObjects::applyRandomDirectionForRndTrial_2_4(ObjData & o)
{
	const double mag0 = o.debugLvl > 0 ? sqrt(o.vel.x*o.vel.x + o.vel.y*o.vel.y + o.vel.z*o.vel.z) : 0.;

	
	// now, try and get a fixed speed in a random direction, while respecting the 'plane' 
	// the object wanted to move in.  If it was in the xy plane, keep the new direction
	// in that plane, if xz plane, keep it in that plane, etc.
	double * quantities[3] = { 0,0,0 }, 
	* vs[3] = { 0,0,0 };
	int ix = 0;
	for (int j = 0; j < 3; ++j)
		// figure out if we have 3 nonzero components or less to the velocity vector...
		if (!eqf(o.vel[j],0.)) {
			quantities[ix] = &o.vel[j];
			vs[ix] = &o.v[j];
			++ix;
		}
	if (ix < 2) {
		// had less than 2 nonzero velocity components, so take whatever is left ...
		for (int j = 0; j < 3 && ix < 2; ++j) {
			if (!ix || quantities[0] != &o.vel[j]) {
				quantities[ix] = &o.vel[j];
				vs[ix] = &o.v[j];
				++ix;
			}
		}
	}
	if (ix < 3) { 
		// our original velocity was in a 2D plane, so pick a random direction in original plane
		const double r = sqrt(*quantities[0] * *quantities[0] + *quantities[1] * *quantities[1]);
		const double theta = ran1Gen()*2.*M_PI;
		o.v = Vec3Zero;
		*vs[0] = r*cos(theta);
		*vs[1] = r*sin(theta);											
	} else {
		// in full 3D mode -- we have 3 quantities for velocity so we rotate our original velocity vector using yaw, picth, roll!
		o.v = Vec3RotateEuler(o.vel, ran1Gen()*2.*M_PI, ran1Gen()*2.*M_PI, ran1Gen()*2.*M_PI);
	}

	if (o.debugLvl > 0) {
		const double mag = sqrt(o.v.x*o.v.x + o.v.y*o.v.y + o.v.z*o.v.z);
	    Debug() << "RndTrial" << rndtrial << " obj" << o.objNum << " vel=" << o.vel.x << "," << o.vel.y << "," << o.vel.z  <<  " |vel|=" << mag0
				<< " v=" << o.v.x << "," << o.v.y << "," << o.v.z << " |v|=" << mag;
	}

}

void MovingObjects::applyRandomPositionForRndTrial_1_2(ObjData & o) 
{
	const bool hasX = !eqf(o.vel.x,0.),
			   hasY = !eqf(o.vel.y,0.),
	           hasZ = !eqf(o.vel.z,0.);
	Vec2 cpos = o.shape->canvasPosition();
	if (hasX) cpos.x = ran1Gen()*canvasAABB.size.x + min_x_pix;
	if (hasY) cpos.y = ran1Gen()*canvasAABB.size.y + min_y_pix; 
	if (hasZ) ///< if we have some 3D Z motion, generate Z coord 
		o.shape->position.z = ran1Gen()*(zBoundsFar-zBoundsNear) + zBoundsNear;
	// otherwise keep old initial position...
	o.shape->setCanvasPosition(cpos);	
}


void MovingObjects::preReadFrameVarsForWholeFrame()
{
	for (int i = 0; i < numObj; ++i)
		for (int k = 0; k < nSubFrames; ++k) {
			QVector<double> & defs = frameVars->variableDefaults();
			// re-set the defaults because we *know* what the frameNum and subframeNum *should* be!
			defs[0] = frameNum;
			defs[2] = k;				
			QVector<double> & fv(fvs_block[i][k] = frameVars->readNext());
			if ( !frameNum && !i && !k ) {
				fvHasPhiCol = frameVars->hasInputColumn("phi");
				fvHasZCol = frameVars->hasInputColumn("z");
				fvHasZScaledCol = frameVars->hasInputColumn("zScaled");
			}
			if (fv.size() < NUM_FRAME_VARS && (frameNum || i || k)) {
				// at end of file?
				i = numObj;
				break;
			} 
			if (fv.size() < NUM_FRAME_VARS) {
				Error() << "Error reading frame " << frameNum << " from frameVar file! Datafile frame num differs from current frame number!  Do all the fps_mode and numObj parameters of the frameVar file match the current fps mode and numObjs?";	
				stop();
				return;
			}
		}	
}

void MovingObjects::postWriteFrameVarsForWholeFrame()
{
	for (int i = 0; i < numObj; ++i) {
		for (int k = 0; k < nSubFrames; ++k) {
			const QVector<double> & fv (fvs_block[i][k]);
			if (unsigned(fv.size()) != frameVars->nFields()) {
				Error() << "INTERNAL PLUGIN ERROR: FrameVar file configured for different number of fields than are being written!";
				stop();
				return;
			}
			frameVars->push(fv);
		}
	}
}

void MovingObjects::doFrameDraw()
{		
	if (have_fv_input_file) 
		preReadFrameVarsForWholeFrame();
		
	for (int k=0; k < nSubFrames; k++) {
		
		QMap <double, Obj2Render> objs2Render; ///< objects will be rendered in this order, this is ordered in reverse depth order

		
		for (QList<ObjData>::iterator it = objs.begin(); it != objs.end(); ++it) {		
			ObjData & o = *it;
			if (!o.shape) continue; // should never happen..

			Rect aabb = o.shape->AABB();
			double objLen, objLen_min;
			float  & objcolor (o.color);

			{ // Create a scope for the below references...				
				double & x  (o.shape->position.x),					 
					   & y  (o.shape->position.y),	
					   & z  (o.shape->position.z),
					   & vx (o.v.x),
					   & vy (o.v.y),
					   & vz (o.v.z),
					   & objVelx (o.vel.x),
					   & objVely (o.vel.y),
					   & objVelz (o.vel.z);
				
				double
					   & objXinit (o.pos_o.x),
					   & objYinit (o.pos_o.y),
					   & objZinit (o.pos_o.z);
				
				double  & objLen_o (o.len_vec[0].x), & objLen_min_o (o.len_vec[0].y);
				double  & objPhi (o.shape->angle); 
				
				double  & jitterx (o.jitterx), & jittery (o.jittery), & jitterz(o.jitterz);
								
				objLen = o.shape->scale.x * objLen_o;
				objLen_min = o.shape->scale.y * objLen_min_o; 

				// local target jitter
				if (jitterlocal) {
					jitterx = eqf(objVelx,0.) ? 0. : (ran1Gen()*jittermag - jittermag/2);
					jittery = eqf(objVely,0.) ? 0. : (ran1Gen()*jittermag - jittermag/2);
					jitterz = eqf(objVelz,0.) ? 0. : (ran1Gen()*jittermag - jittermag/2);
				}
				else 
					jitterx = jittery = jitterz = 0.;
							
				// MOVEMENT/ROTATION computation happens *always* but the fvar file variables below may override the results of this computation!
				if (moveFlag) {
					
					// wrap objects that floated past the edge of the screen
					if (wrapEdge && (!canvasAABB.intersects(aabb) 
									 || (is3D && (z > zBoundsFar 
												  || z < zBoundsNear))))
						wrapObject(o, aabb);
					
					else if (!wrapEdge) 
						doWallBounce(o);

					
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
						objVelz = o.vel_vec[o.vel_vec_i].z;
						
						// init position
						//if (0 == rndtrial) {
						// setup defaults for x,y,z and vx,vy,vz
						// these may be overridden by rndtrial code below
							x = objXinit;
							y = objYinit;
							z = objZinit;
							vx = objVelx; 
							vy = objVely;
							vz = objVelz;
						//} else 
						if (1 == rndtrial) {
							// random position and random V based on init sped 
							applyRandomPositionForRndTrial_1_2(o);
							vx = ran1Gen()*objVelx*2 - objVelx;
							vy = ran1Gen()*objVely*2 - objVely;
							vz = ran1Gen()*objVelz*2 - objVelz;
						} else if (2 == rndtrial) {
							// random position and direction
							// static speed
							applyRandomPositionForRndTrial_1_2(o);
							applyRandomDirectionForRndTrial_2_4(o);
							
						} else if (3 == rndtrial) {
							// position at t = position(t-1) + vx, vy
							// random v based on objVelx objVely range
							if (loopCt > 0) {
								x = o.lastPos.x;
								y = o.lastPos.y;
								z = o.lastPos.z;
							}
							vx = ran1Gen()*objVelx*2. - objVelx;
							vy = ran1Gen()*objVely*2. - objVely;
							vz = ran1Gen()*objVelz*2. - objVelz; 
						} else if (4 == rndtrial) {
							// position at t = position(t-1) + vx, vy
							// static speed, random direction
							if (loopCt > 0) {
								x = o.lastPos.x;
								y = o.lastPos.y;
								z = o.lastPos.z;
							}
							applyRandomDirectionForRndTrial_2_4(o);
						}
						objPhi = o.phi_o;
						if (is3D) ensureObjectIsInBounds(o);
						aabb = o.shape->AABB();
					}
					
					{
						Vec2 c = Shapes::Shape::canvasPosition(Vec3(x+vx+jitterx,y+vy+jittery,z+vz+jitterz));
						// update position after delay period
						// if jitter pushes us outside of motion box then do not jitter this frame
						if ((int(frameNum)%tframes /*- delay*/) >= 0) { 
							if (!wrapEdge && ((c.x + aabb.size.w/2 > max_x_pix) 
											  ||  (c.x - aabb.size.w/2 < min_x_pix)))
								x += vx;
							else 
								x += vx + jitterx;
							if (!wrapEdge && ((c.y + aabb.size.h/2 > max_y_pix) 
											  || (c.y - aabb.size.h/2 < min_y_pix))) 
								y += vy;
							else 
								y += vy + jittery;
							
							if (!wrapEdge && (z+vz+jitterz > zBoundsFar
											  || z+vz+jitterz < zBoundsNear)) 
								z += vz;
							else 
								z += vz + jitterz;
							
							aabb = o.shape->AABB();
							o.lastPos = o.shape->position; // save last pos

							// also apply spin
							objPhi += o.spin;
						}
					}
					
				} // END if moveFlag

				if (have_fv_input_file) {
					ConfigSuppressesFrameVar & csfv (configSuppressesFrameVar[o.objNum]);
						
					const QVector<double> & fv (fvs_block[o.objNum][k]); 
										
					if (fv.size() < NUM_FRAME_VARS && frameNum) {
						// at end of file?
						Warning() << name() << "'s frame_var file ended input, stopping plugin.";
						have_fv_input_file = false;
						stop();
						return;
					} 
					if (fv.size() < NUM_FRAME_VARS || fv[FV_frameNum] != frameNum) {
						Error() << "Error reading frame " << frameNum << " from frameVar file! Datafile frame num differs from current frame number!  Do all the fps_mode and numObj parameters of the frameVar file match the current fps mode and numObjs?";	
						stop();
						return;
					}
					if (fv[FV_objNum] != o.objNum) {
						Error() << "Error reading object " << o.objNum << " from frameVar file! Datafile object num differs from current object number!  Do all the fps_mode and numObj parameters of the frameVar file match the current fps mode and numObjs?";	
						stop();
						return;						
					}
					if (fv[FV_subFrameNum] != k) {
						Error() << "Error reading subframe " << k << " from frameVar file! Datafile subframe num differs from current subframe numer!  Do all the fps_mode and numObj parameters of the frameVar file match the current fps mode and numObjs?";	
						stop();
						return;												
					}
					

					
					bool didInitLen = false, redoAABB = false;;
					const ObjType otype = ObjType(csfv[FV_objType] ? o.type : int(fv[FV_objType]));
					const double r1 = ( csfv[FV_r1] ? objLen : fv[FV_r1] ), ///< keep the old objlen if config file is set to suppress framevar...
								 r2 = ( csfv[FV_r2] ? objLen_min : fv[FV_r2] );

					x = fv[FV_x];
					y = fv[FV_y];

					bool zChanged = false;
					if (fvHasPhiCol && !csfv[FV_phi]) 
						objPhi = fv[FV_phi];
					if (fvHasZCol && !csfv[FV_z])
						z = fv[FV_z], zChanged = true;
					else 
						z = 0.0, zChanged = false;
					
					double newZ = z;
					if (fvHasZScaledCol && fvHasZCol && !csfv[FV_zScaled]) {
						newZ = distanceToZ(fv[FV_zScaled]);
						if (fabs(newZ - z) >= 0.0001 && didScaledZWarning <= 3) {
							Warning() << "Encountered z & zScaled column in framevar file and they disagree.  Ignoring zScaled and using z. (Could the render window size have changed?)";						
							++didScaledZWarning;
						}
					}
					
					if (fvHasZScaledCol && !fvHasZCol) {
						z = newZ, zChanged = true;
					}
					
					if (!is3D && !eqf(z, 0.)) is3D = true;
					
						
					if (!k && !frameNum) {
						// do some required initialization if on frame 0 for this object to make sure r1, r2 and  obj type jive
						o.phi_o = objPhi;
						o.pos_o = Vec3(x,y,z);
						objLen = objLen_o = r1;
						objLen_min = objLen_min_o = r2;
						o.shape->scale.x = 1.0;
						o.shape->scale.y = 1.0;
						reinitObj(o, otype);
						didInitLen = true;
						redoAABB = true;
					}
					
					// handle type change mid-plugin-run
					if (o.type != otype) {
						reinitObj(o, otype);
						redoAABB = true;
					}					
					// handle length changes mid-plugin-run
					if (!didInitLen && (!eqf(r1, objLen) || !eqf(r2, objLen_min))) {
						o.shape->setRadii(r1, r2);
						objLen = objLen_o = r1;
						objLen_min = objLen_min_o = r2;
						redoAABB = true;
					}
					
					if (zChanged || redoAABB) aabb = o.shape->AABB();
					objcolor = csfv[FV_color] ? objcolor : fv[FV_color];
					if (o.type == SphereType) objcolor = 1.0;
				} //  end have_fv_input_file

			} // end alias reference scope
			
			// enqueue draw stim if after delay period, or if have fv file
			if (have_fv_input_file || (int(frameNum)%tframes) >= 0) {
				/// enqueue object to be rendered in depth order
				objs2Render.insertMulti(-o.shape->position.z, Obj2Render(o, aabb));
				
				if (!have_fv_input_file /**< doing output.. */) {
					double fnum = frameNum;

					// in rndtrial mode, save 1 big file with fnum being a derived value.  HACK!
					if (rndtrial && loopCt && nFrames) fnum = frameNum + loopCt*nFrames;

					// nb: push() needs to take all doubles as args!
					QVector<double> & fv (fvs_block[o.objNum][k]);
					fv.resize(N_FVCols);
					fv[FV_frameNum] = fnum;
					fv[FV_objNum] = o.objNum;
					fv[FV_subFrameNum] = k;
					fv[FV_objType] = o.type;
					fv[FV_x] = o.shape->position.x;
					fv[FV_y] = o.shape->position.y;
					fv[FV_r1] = objLen;
					fv[FV_r2] = objLen_min;
					fv[FV_phi] = o.shape->angle;
					fv[FV_color] = objcolor;
					fv[FV_z] = o.shape->position.z;
					fv[FV_zScaled] = o.shape->distance();
				}
			}
		} // end obj loop
		
		// now, render all objects for this subframe that were enqueued above, and render them in
		// depth order, ascending.
		for (QMap<double,Obj2Render>::iterator it = objs2Render.begin(); it != objs2Render.end(); ++it) {
			drawObject(k, it->obj, it->aabb);
		} 
		
	} // end K loop
	
	// shapes2del guards against possible dangling alias/references...
	for (QList<Shapes::Shape *>::iterator it = shapes2del.begin(); it != shapes2del.end(); ++it)
		delete *it;
	shapes2del.clear();
	
	// lastly, write out fvars for this frame -- we need to do it this way because framevar order in file no longer matches
	// rendering order in this plugin, so we save them above as we render, and then write them out 
	//  in subframe order, grouped by object number
	// .. we do them in blocks for all the objects and subframes for this frame
	// the inverse of this (read a frame's worth of vars and reorder it) is at the beginning of this function
	if (!have_fv_input_file)
		postWriteFrameVarsForWholeFrame();	
}

void MovingObjects::drawObject(const int k, ObjData & o, const Rect & aabb)
{
	const bool depthWorkaround = fps_mode == FPS_Single;
	
	if (depthWorkaround) {
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glTranslatef(0.,0.,-o.shape->position.z);
	}
	
	const float & objcolor (o.color);
	
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
	
	if (o.type == SphereType) 
		glShadeModel(GL_SMOOTH);
	else
		glShadeModel(savedShadeModel);
	
	if (o.debugLvl >= 2) {
		const double t0 = getTime();
		o.shape->draw();
		const double tf = getTime();
		Debug() << "frame:" << frameNum << " obj:" << o.objNum << " took " << (tf-t0)*1e6 << " usec to draw";
	} else
		o.shape->draw();
	
	///DEBUG HACK FOR AABB VERIFICATION					
	if (debugAABB && k==0) {
		const Rect & r (aabb);
		glColor3f(0.,1.,0); 
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glBegin(GL_QUADS);
		glVertex2f(r.origin.x,r.origin.y);
		glVertex2f(r.origin.x+r.size.w,r.origin.y);
		glVertex2f(r.origin.x+r.size.w,r.origin.y+r.size.h);
		glVertex2f(r.origin.x,r.origin.y+r.size.h);	
		glEnd();
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	if (depthWorkaround) {
		glPopMatrix();
	}
	
}

void MovingObjects::wrapObject(ObjData & o, Rect & aabb) const {
	Shapes::Shape & s = *o.shape;
	
	if (is3D) {
		 // first, wrap object in Z, if 3D mode
		if (s.position.z > zBoundsFar)
			s.position.z = zBoundsNear + (s.position.z-zBoundsFar);
		if (s.position.z < zBoundsNear)
			s.position.z = zBoundsFar - (zBoundsNear-s.position.z);
	}
	
	Vec2 cpos = s.canvasPosition();
	aabb = s.AABB();
	
	// wrap right edge
	if (aabb.left() >= canvasAABB.right()) cpos.x = (canvasAABB.left()-aabb.size.w/2) + (aabb.left() - canvasAABB.right());
	// wrap left edge
	if (aabb.right() <= canvasAABB.left()) cpos.x = (canvasAABB.right()+aabb.size.w/2) - (canvasAABB.left()-aabb.right());
	// wrap top edge
	if (aabb.bottom() >= canvasAABB.top()) cpos.y = (canvasAABB.bottom()-aabb.size.h/2) + (aabb.bottom() - canvasAABB.top());
	// wrap bottom edge
	if (aabb.top() <= canvasAABB.bottom()) cpos.y = (canvasAABB.top()+aabb.size.h/2) - (canvasAABB.bottom()-aabb.top());
	
	s.setCanvasPosition(cpos);
	aabb = s.AABB();
}

void MovingObjects::doWallBounce(ObjData & o) const {
	const Vec3 & p (o.shape->position);
	Vec3 porig(p);
	Vec3 & v (o.v);
	Vec3 fpos = p + v; ///< future position
	int dim = 0;
	if (!eqf(v.x,0.)) ++dim;
	if (!eqf(v.y,0.)) ++dim;
	if (!eqf(v.z,0.)) ++dim;
	
	const QVector<Vec3> & normals (dim >= 3 ? frustumNormals : boxNormals);
	//if (o.debugLvl > 0)
	//	Debug() << "Obj" << o.objNum << " dimensionality: " << dim;
		
	// first, wrap object in Z, if 3D mode
	if (fpos.z > zBoundsFar) {
		// reflect off far wall
		v=v.reflect(normals[5]);
		fpos = p + v;
	}
		
	if (fpos.z < zBoundsNear) {
		v=v.reflect(normals[4]);
		fpos = p + v;
	}
	Rect aabb;
#define REDO_AABB() ( \
	o.shape->position = fpos, \
	aabb = (o.shape->AABB()), /* get aabb of future position */ \
	o.shape->position = porig )
	
	REDO_AABB();
				
	if (aabb.left() < canvasAABB.left())
		v = v.reflect(normals[0]), fpos = p + v, REDO_AABB();
	if (aabb.right() > canvasAABB.right())
		v = v.reflect(normals[1]), fpos = p + v, REDO_AABB();
	if (aabb.bottom() < canvasAABB.bottom())
		v = v.reflect(normals[2]), fpos = p + v, REDO_AABB();
	if (aabb.top() > canvasAABB.top())
		v = v.reflect(normals[3]), fpos = p + v, REDO_AABB();
}

void MovingObjects::ensureObjectIsInBounds(ObjData & o) const
{
	int i = 0;
	const Vec3 origOrigPos (o.shape->position);
	
	do { 
		
		bool modified = false;
		Rect aabb;
		double diff;
		Vec3 fpos (o.shape->position+o.v), porig(o.shape->position);

		if (!i) {
			if ( (diff=fpos.z-zBoundsNear) < 0) fpos.z -= diff, o.shape->position.z -= diff, porig = o.shape->position, modified = true;
			if ( (diff=fpos.z-zBoundsFar) > 0)  fpos.z -= diff, o.shape->position.z -= diff, porig = o.shape->position, modified = true;
		}
		
		REDO_AABB();
		
		// FUDGE: objects that are out-of-bounds get fudged in-bounds here...
		Vec2 cpos = Shapes::Shape::canvasPosition(fpos);
	/*	const double objLen (o.shape->scale.x * o.len_vec[0].x), objLen_min (o.shape->scale.y * o.len_vec[0].y); 
		const double hw = objLen/2., hh = objLen_min/2.;
		if (cpos.x-hw < min_x_pix) cpos.x = min_x_pix+hw, modified = true;
		else if (cpos.x+hw > max_x_pix) cpos.x = max_x_pix-hw, modified = true;
		if (cpos.y-hh < min_y_pix) cpos.y = min_y_pix+hh, modified = true;
		else if (cpos.y+hh > max_y_pix) cpos.y = max_y_pix-hh, modified = true;*/
		
		if ((diff = aabb.left() - canvasAABB.left()) < 0)
			cpos.x -= diff, modified = true;
		if ((diff = aabb.right() - canvasAABB.right()) > 0)
			cpos.x -= diff, modified = true;
		if ((diff = aabb.bottom() - canvasAABB.bottom()) < 0)
			cpos.y -= diff, modified = true;
		if ((diff = aabb.top() - canvasAABB.top()) > 0)
			cpos.y -= diff, modified = true;

		if (rndtrial) {
			if (!i && modified) {
				Debug() << "RndTrial: object " << o.objNum << " randomly positioned out of bounds. Nudging it back in bounds...";				
			}
		} else {
			if (modified) {
				if (!i)
					Warning() << "Object " << o.objNum << " initial position out of bounds, will move it in bounds (if it fits)...";
				else {
					Error() << "Object " << o.objNum << " cannot be fully placed in bounds (is objZinit too close to camera?).  Object may be lost offscreen and/or wall bouncing may fail spectacularly. Please specify saner initial position params for this object!";
					o.shape->position = origOrigPos;
					return;
				}
			}
		}
		
		if (!i && modified) 
			o.shape->setCanvasPosition(cpos);
	} while (++i < 2);
	
}

void MovingObjects::initCameraDistance()
{
	majorPixelWidth = width() > height() ? width() : height();
	const double dummyObjLen = 25.0;
	double bestDiff = 1e9, bestDist = 1e9;
	// d=15; dd=101; (rad2deg(2 * atan(.5 * (d/dd))) / 90) * 640
	for (double D = 0.; D < majorPixelWidth; D += 1.0) {
		double diff = fabs(dummyObjLen - (RAD2DEG(2. * atan(.5 * (dummyObjLen/D))) / 90.) * majorPixelWidth);
		if (diff < bestDiff) bestDiff = diff, bestDist = D;
	}
	cameraDistance = bestDist;
	Log() << "Camera distance in Z is " << cameraDistance;
}

void MovingObjects::initBoundingNormals()
{
	frustumNormals.resize(6);
	
	Vec3 a,b,t;
	Vec3 p1, p2, p3;
	// left edge
	p1 = Shapes::Shape::cposToRealPos(Vec2(min_x_pix, min_y_pix), 0.0); 
	p2 = Shapes::Shape::cposToRealPos(Vec2(min_x_pix, min_y_pix+1), 0.0);
	p3 = Shapes::Shape::cposToRealPos(Vec2(min_x_pix, min_y_pix+1), 1.0);
	a = (p2-p1).normalized();
	b = (p3-p1).normalized();
	frustumNormals[0] = t = a.cross(b).normalized();
	// right edge
	p1 = Shapes::Shape::cposToRealPos(Vec2(max_x_pix, min_y_pix), 0.0); 
	p2 = Shapes::Shape::cposToRealPos(Vec2(max_x_pix, min_y_pix), 1.0);
	p3 = Shapes::Shape::cposToRealPos(Vec2(max_x_pix, min_y_pix+1), 1.0);
	a = (p2-p1).normalized();
	b = (p3-p1).normalized();
	frustumNormals[1] = t = a.cross(b).normalized();
	// bottom edge
	p1 = Shapes::Shape::cposToRealPos(Vec2(min_x_pix, min_y_pix), 0.0); 
	p2 = Shapes::Shape::cposToRealPos(Vec2(min_x_pix, min_y_pix), 1.0);
	p3 = Shapes::Shape::cposToRealPos(Vec2(min_x_pix+1, min_y_pix), 1.0);
	a = (p2-p1).normalized();
	b = (p3-p1).normalized();
	frustumNormals[2] = t = a.cross(b).normalized();
	// top edge
	p1 = Shapes::Shape::cposToRealPos(Vec2(min_x_pix, max_y_pix), 0.0); 
	p2 = Shapes::Shape::cposToRealPos(Vec2(min_x_pix+1, max_y_pix), 0.0);
	p3 = Shapes::Shape::cposToRealPos(Vec2(min_x_pix+1, max_y_pix), 1.0);
	a = (p2-p1).normalized();
	b = (p3-p1).normalized();
	frustumNormals[3] = t = a.cross(b).normalized();
	// near edge
	const double z=distanceToZ(.5); 
	p1 = Shapes::Shape::cposToRealPos(Vec2(min_x_pix, min_y_pix), z); 
	p2 = Shapes::Shape::cposToRealPos(Vec2(min_x_pix+1, min_y_pix), z);
	p3 = Shapes::Shape::cposToRealPos(Vec2(min_x_pix+1, min_y_pix+1), z);
	a = (p2-p1).normalized();
	b = (p3-p1).normalized();
	frustumNormals[4] = t = a.cross(b).normalized();
	// far edge
	p1 = Shapes::Shape::cposToRealPos(Vec2(min_x_pix, min_y_pix), zBoundsFar); 
	p2 = Shapes::Shape::cposToRealPos(Vec2(min_x_pix, min_y_pix+1), zBoundsFar);
	p3 = Shapes::Shape::cposToRealPos(Vec2(min_x_pix+1, min_y_pix+1), zBoundsFar);
	a = (p2-p1).normalized();
	b = (p3-p1).normalized();
	frustumNormals[5] = t = a.cross(b).normalized();

	// boxNormals apply to objects that have a dimensionality of 2 or less to their motion vector.. don't really use a frustum 
	// at all, since we want to bounce orthogonally to the screen edge
	boxNormals.resize(6);
	boxNormals[0] = Vec3(1,0,0);
	boxNormals[1] = Vec3(-1,0,0);
	boxNormals[2] = Vec3(0,1,0);
	boxNormals[3] = Vec3(0,-1,0);
	boxNormals[4] = Vec3(0,0,1);
	boxNormals[5] = Vec3(0,0,-1);		
	
}

double MovingObjects::distanceToZ(double d) const
{
	return (cameraDistance * d) - cameraDistance;
}

double MovingObjects::zToDistance(double z) const
{
	if (eqf(cameraDistance,0.)) return 0.;
	return z/cameraDistance + 1.0;
}
