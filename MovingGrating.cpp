#include "MovingGrating.h"

#define TEXWIDTH 2048

static double squarewave(double x) {
	if (sin(x) > 0.) return 1.0;
	return -1.0;
}

MovingGrating::MovingGrating()
    : StimPlugin("MovingGrating"), spatial_freq(0.01f), temp_freq(1.0f), angle(0.f), dangle(0.f), tframes(-1), phase(0.0), tex(0)
{
    pluginDoesOwnClearing = true;

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
	else waveFunc = &sin;

	return true;	
}

bool MovingGrating::init()
{
	if (!initFromParams()) return false;
	phase = 0.0;
	
	frameVars->setVariableNames(QString("frameNum phase spatial_freq angle").split(" "));
	frameVars->setVariableDefaults(QVector<double>() << 0. << 0. << spatial_freq <<  angle);

    if (!tex) {
        glGenTextures(1, &tex);
    }
    GLfloat pix[TEXWIDTH];
    for (int i = 0; i < TEXWIDTH; ++i) {
        pix[i] = (waveFunc(GLfloat(i)/GLfloat(TEXWIDTH)*2.0*M_PI)+1.0)/2.0 * (max_color-min_color) + min_color;
        if (pix[i] < 0.f) pix[i] = 0.f;
        if (pix[i] > 1.f) pix[i] = 1.f;
    }
    glBindTexture(GL_TEXTURE_1D, tex);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_LUMINANCE, TEXWIDTH, 0, GL_LUMINANCE, GL_FLOAT, pix);
        
	return true;    
}


void MovingGrating::drawFrame()
{
    const int nIters = int(fps_mode)+1;
    const double dT = (1.0/double(stimApp()->refreshRate()))/double(nIters);
    GLboolean savedMask[4];
    GLfloat savedClear[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, savedMask);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, savedClear);
    glClearColor(min_color, min_color, min_color, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    for (int k = 0; k < nIters; ++k) {
        glPushMatrix();
        
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
        if (fv.size() >= 2) {
            phase = fv[1];
            if (fv.size() > 2) spatial_freq = fv[2];
            if (fv.size() > 3) angle = fv[3];
        } else {
            // advance phase here..
            phase += dT * temp_freq;
            phase = fmod(phase, 1.0);
        }
        
        if (spatial_freq < 0.) spatial_freq = -spatial_freq;
                    
        if ((frameNum > 0) && tframes > 0 && !(frameNum % tframes)) {		
            if (ftChangeEvery < 1) ftAssertions[FT_Change] = true;		
            angle = angle + dangle;
        }

        if (angle > 360.0) {
            angle = fmodf(angle, 360.0f);
        } else if (angle < -360.0) {
            angle = fmodf(-angle, 360.0f);
            angle = -angle;
        }

        if (!fv.size()) frameVars->push(double(frameNum), phase, spatial_freq, angle);        

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
        GLfloat t[] = { 0.f+phase, w/bw+phase, w/bw+phase, 0.f+phase };
        
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


