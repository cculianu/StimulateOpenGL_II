#include "MovingGrating.h"
#include "DAQ.h"

#define TEXWIDTH 2048

static double squarewave(double x) {
	if (sin(x) > 0.) return 1.0;
	return -1.0;
}

static double trianglewave(double x) 
{
	static const double pio2 = M_PI/2.;
	
	x = fmod(x,2.*M_PI); // normalize to 1 full circle
	if (x < 0.) x = 2.*M_PI + x; // normalize to all positive
	
	if (x >= 0. && x <= pio2) return x/pio2;
	if (x > pio2 && x <= M_PI) return (M_PI-x)/pio2;
	if (x > M_PI && x <= 3.*pio2) return -((x-M_PI)/pio2);
	if (x > 3.*pio2 && x <= 2.*M_PI) return -((2.*M_PI-x)/pio2);
	Error() << "MovingGrating.cpp trianglewave() reached end of function without returning a valid quantity! FIXME!!";
	return 0.;
}

static double sawtoothwave(double x) {
	x = fmod(x,2.*M_PI); // normalize to 1 full circle
	if (x < 0.) x = 2.*M_PI + x; // normalize to all positive
	x = x/(2.*M_PI); // normalize to 0->1
	
	static const double swfact = (1.0/0.95);
	double f = x*swfact;
	if (f >= 1.0) {
		f = 1.0-((f-1.0)/(swfact-1.0));
	}
	return f*2.0 - 1.0;
}

MovingGrating::MovingGrating()
    : StimPlugin("MovingGrating"), spatial_freq(0.01f), temp_freq(1.0f), angle(0.f), dangle(0.f), tframes(-1), phase(0.0), tex(0)
{
    pluginDoesOwnClearing = false;

}

MovingGrating::~MovingGrating() {
    if (tex) glDeleteTextures(1, &tex);
    tex = 0;
}

bool MovingGrating::initFromParams()
{
	if( !getParam("spatial_freq", spatial_freq) ) spatial_freq = 0.01f;
	if( !getParam("temp_freq",  temp_freq) ) temp_freq = 1.0f;
	if( !getParam("angle", angle) ) angle = 0.f;
	if( !getParam("dangle", dangle) ) dangle = 0.f;
	
	if( !getParam("tframes", tframes) )	tframes = -1;

    tframesDO = "";
    if (getParam("tframesDO", tframesDO) && tframesDO.size()) {
        if (!DAQ::DOChannelExists(tframesDO)) {
            Error() << "tframesDO=" << tframesDO << " -- not a valid digital output channel!";
            return false;
        }
    }

	if ( !getParam("min_color", min_color)) min_color = 0.0;
	if ( !getParam("max_color", max_color)) max_color = 1.;
	
	if (min_color < 0. || max_color < 0.) {
		Error() << "min_color and max_color need to be > 0!";
		return false;
	}
	if (min_color > 1.9 || max_color > 1.9) {
		Warning() << "min_color/max_color should be in the range [0,1].  You specified numbers outside this range.  We are assuming you mean unsigned byte values, so we will scale your input based on the range [0,255]!";
		min_color /= 255.;
		max_color /= 255.;
	}
    
    phase = 0.0;
	
	QString wave;
	if (!getParam("wave", wave)) wave = "sin";
	if (wave.toLower().startsWith("sq")) waveFunc = &squarewave;
	else if (wave.toLower().startsWith("tr")) waveFunc = &trianglewave;
	else if (wave.toLower().startsWith("co")) waveFunc = &cos;
	else if (wave.toLower().startsWith("saw")) waveFunc = &sawtoothwave;
	else waveFunc = &sin;
	
	return true;	
}

void MovingGrating::redefineTexture(float min, float max, bool is_sub_img)
{
    GLfloat pix[TEXWIDTH];
    for (int i = 0; i < TEXWIDTH; ++i) {
        pix[i] = (waveFunc(GLfloat(i)/GLfloat(TEXWIDTH)*2.0*M_PI)+1.0)/2.0 * (max-min) + min;
        if (pix[i] < 0.f) pix[i] = 0.f;
        if (pix[i] > 1.f) pix[i] = 1.f;
    }
    glBindTexture(GL_TEXTURE_1D, tex);
    if (is_sub_img) {
        glTexSubImage1D(GL_TEXTURE_1D, 0, 0, TEXWIDTH, GL_LUMINANCE, GL_FLOAT, pix);
    } else {
        glTexImage1D(GL_TEXTURE_1D, 0, GL_LUMINANCE, TEXWIDTH, 0, GL_LUMINANCE, GL_FLOAT, pix);
    }
}

bool MovingGrating::init()
{
	if (!initFromParams()) return false;
	phase = 0.0;
	frameVars->setVariableNames(QString("frameNum phase spatial_freq angle min_color max_color ftrackBoxState(0=ON,1=off,2=change,3=start,4=end,-1=undefined)").split(" "));
	frameVars->setVariableDefaults(QVector<double>() << 0. << 0. << spatial_freq <<  angle << min_color << max_color << -1.);

    if (!tex) {
        glGenTextures(1, &tex);
    }
    redefineTexture(min_color, max_color, false);        
	return true;    
}

static inline bool my_feqf(const float f1, const float f2) {
    static const float epsilon = 0.001f;
    return fabsf(f1-f2) < epsilon;
}

void MovingGrating::drawFrame()
{
    if (tframesDO.length() && tframes > 0) {
        if (!(frameNum%tframes))
            DAQ::WriteDO(tframesDO, true);  // set tframesDO line high every tframes, if using tframes
        else if (frameNum && !((frameNum-1)%(tframes)))
            DAQ::WriteDO(tframesDO, false); // set tframesDO line low the frame after tframes, if using tframes
    }

    const int nIters = int(fps_mode)+1;
    QVector<QVector<double> > fvs_cached;
    fvs_cached.reserve(nIters);
    float min_min_color = have_fv_input_file ? 1e6 : min_color;
    
    // pre-cache frame vars here if we use frame vars so we can figure out
    // what clear color to use..
    for (int k = 0; have_fv_input_file && k < nIters; ++k) {
        fvs_cached.push_back(frameVars->readNext());
        QVector<double> & fv (fvs_cached.back());
        if (fv.size() < 2 && frameNum) {
            break;
        }
        if (fv.size() > 4 && fv[4] < min_min_color) min_min_color = fv[4];
    }
    if (have_fv_input_file && min_min_color > 1.f)
        min_min_color = min_color;
    
    const double dT = (1.0/double(stimApp()->refreshRate()))/double(nIters);
    float new_min_color = min_color, new_max_color = max_color;
    GLboolean savedMask[4];
    GLfloat savedClear[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, savedMask);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, savedClear);
		
    // TODO: make the glClear() here take into account possibly changing min_color values??
    glClearColor(min_min_color, min_min_color, min_min_color, 1.0);
	
    clearScreen();
    
    for (int k = 0; k < nIters; ++k) {
        glPushMatrix();
        
        QVector<double> fv;
        if (have_fv_input_file) {
            //fv = frameVars->readNext();
            if (k < fvs_cached.size()) fv = fvs_cached[k];
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
        if (fv.size() >= 2) {
            phase = fv[1];
            if (fv.size() > 2) spatial_freq = fv[2];
            if (fv.size() > 3) angle = fv[3];
            if (fv.size() > 4) new_min_color = fv[4];
            if (fv.size() > 5) new_max_color = fv[5];
        } else {
            // advance phase here..
            phase += dT * temp_freq;
            phase = fmod(phase, 1.0);
        }
        
        if (spatial_freq < 0.) spatial_freq = -spatial_freq;
                    
        if ((frameNum > 0) && tframes > 0 && !(frameNum % tframes) && !k) {
            if (ftChangeEvery < 1) ftAssertions[FT_Change] = true;		
            angle = angle + dangle;
        }

        if (angle > 360.0) {
            angle = fmodf(angle, 360.0f);
        } else if (angle < -360.0) {
            angle = fmodf(-angle, 360.0f);
            angle = -angle;
        }

        if (!my_feqf(min_color, new_min_color) || !my_feqf(max_color,new_max_color)) {
            min_color = new_min_color;
            max_color = new_max_color;
            //const double t0 = Util::getTime();
            redefineTexture(min_color, max_color, true);
            //Debug() << "redefine texture time (subimage) = " << ((Util::getTime()-t0)*1e3) << " msec";
        }

        if (!fv.size()) frameVars->enqueue(double(frameNum), phase, spatial_freq, angle, min_color, max_color, -1);

        const float w = width()*2, h = height()*2, hw=w/2., hh=h/2.;
        
        glTranslatef( width()/2, height()/2, 0);
        glRotatef( angle, 0.0, 0.0, 1.0 );

        
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glEnable(GL_TEXTURE_1D);
        glBindTexture(GL_TEXTURE_1D, tex);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        GLfloat v[] = { -hw,-hh, hw,-hh, hw,hh, -hw,hh };
        const double bw = (spatial_freq <= 0. ? 1.0 : 1.0/spatial_freq);
        GLfloat t[] = { 0.f+float(phase), w/float(bw+phase), w/float(bw+phase), 0.f+float(phase) };
        
        glVertexPointer(2, GL_FLOAT, 0, v);
        glTexCoordPointer(1, GL_FLOAT, 0, t);
        
        if (fps_mode != FPS_Single) {
            if (k == r_index) glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
            if (k == g_index) glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
            if (k == b_index) glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
        }
        glDrawArrays(GL_QUADS, 0, 4);

        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisable(GL_TEXTURE_1D);
            
        glPopMatrix();
    }
    glColorMask(savedMask[0], savedMask[1], savedMask[2], savedMask[3]);
    glClearColor(savedClear[0], savedClear[1], savedClear[2], savedClear[3]);
}

/* virtual */
bool MovingGrating::applyNewParamsAtRuntime() 
{
	return initFromParams();
}

/* reimplemented from super */
void MovingGrating::afterFTBoxDraw()
{
	if (!frameVars || !frameVars->queueCount()) return;
	for (QList<QVector<double> >::iterator it = frameVars->getQueue().begin();  it != frameVars->getQueue().end(); ++it)
	{
		QVector<double> & b(*it);
		if (b.size() >= 7) b[b.size()-1] = double(currentFTState);
	}
	frameVars->commitQueue();
}

