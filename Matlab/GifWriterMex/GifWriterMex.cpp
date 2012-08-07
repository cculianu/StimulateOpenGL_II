#include <math.h>
#include <string>
#include <string.h>
#include <mex.h>
#include <matrix.h>
#include <stdio.h>

#include <map>
#include "gd/gd.h"

struct Context {
	FILE *outf;
	gdImagePtr imgLast;
	bool needAnimEnd;
	int frameCt;
	std::string fileName;
	
	Context() : outf(0), imgLast(0), needAnimEnd(false), frameCt(0) {}
	~Context() {
		endAnimCloseFile();
		setImgLast(0); 
	}
	void setImgLast(gdImagePtr img) {
		if (imgLast && imgLast != img) gdImageDestroy(imgLast);
		imgLast = img;
	}
	void endAnimCloseFile() {
		if (outf) { 
			if (imgLast) {
				mexPrintf("%d x %d animated GIF, %d frames, written to %s\n", imgLast->sx, imgLast->sy, frameCt, fileName.c_str());
			}			
			if (needAnimEnd) {
				if (frameCt == 1 && imgLast)
					gdImageGif(imgLast, outf);
				else
					gdImageGifAnimEnd(outf);
			}
			needAnimEnd = false;
			fclose(outf); 
			outf = 0; 
			setImgLast(0);
		}
	}
};


typedef std::map<int, Context *> ContextMap;

#ifndef _MSC_VER
#define strcmpi strcasecmp
#endif


static ContextMap contextMap;
static int ctxId = 0; // keeps getting incremented..

static Context * MapFind(int handle)
{
  ContextMap::iterator it = contextMap.find(handle);
  if (it == contextMap.end()) return NULL;
  return it->second;
}

static void MapPut(int handle, Context *c)
{
  Context *old = MapFind(handle);
  if (old) delete old; // ergh.. this shouldn't happen but.. oh well.
  contextMap[handle] = c;
}

static void MapDestroy(int handle)
{
  ContextMap::iterator it = contextMap.find(handle);
  if (it != contextMap.end()) {
    delete it->second;
    contextMap.erase(it);
  } else {
    mexWarnMsgTxt("Invalid or unknown handle passed to GifWriterMex MapDestroy!");
  }
}

static int GetHandle(int nrhs, const mxArray *prhs[])
{
  if (nrhs < 1)
    mexErrMsgTxt("Need numeric handle argument!");

  const mxArray *handle = prhs[0];

  if ( !mxIsDouble(handle) || mxGetM(handle) != 1 || mxGetN(handle) != 1)
    mexErrMsgTxt("Handle must be a single double value.");

  return static_cast<int>(*mxGetPr(handle));
}

static Context * GetContext(int nrhs, const mxArray *prhs[])
{
  int handle =  GetHandle(nrhs, prhs);
  Context *c = MapFind(handle);
  if (!c) mexErrMsgTxt("INTERNAL ERROR -- Cannot find the Context for the specified handle in GifWriterMex!");
  return c;
}

#define RETURN(x) do { (plhs[0] = mxCreateDoubleScalar(static_cast<double>(x))); return; } while (0)
#define RETURN_NULL() do { (plhs[0] = mxCreateDoubleMatrix(0, 0, mxREAL)); return; } while(0)

void createNewContext(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	if (nlhs != 1) mexErrMsgTxt("Cannot create a context since no output (lhs) arguments were specified!");
	const mxArray *fn = prhs[0];
	if ( !mxIsChar(fn) || mxGetM(fn) != 1 ) mexErrMsgTxt("Filename must be a string row vector!");

	char *fnStr = mxArrayToString(fn);
	int err = 0;
	Context *c = new Context;
	if (fnStr) {
		c->fileName = fnStr;
		mxFree(fnStr);
		c->outf = fopen(c->fileName.c_str(), "wb");
		err = errno;
	} else {
		mexErrMsgTxt("Please pass in a filename (string) for the output file."); 
	}
	if (!c->outf) {
		delete c;
		std::string msg = std::string("Cannot open specified file for writing: ") + strerror(err); 
		mexErrMsgTxt(msg.c_str());
	}
	int h = ++ctxId;
	MapPut(h, c);
	RETURN(h);
}

void setPalette(gdImagePtr img) 
{
	for (int i = 0; i < 256; ++i) {
		img->red[i] = img->green[i] = img->blue[i] = i;
		img->open[i] = 1;
		img->alpha[i] = 255;
	}
	img->colorsTotal = 256;
}

void addFrame(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	if (nrhs != 2) mexErrMsgTxt("Two arguments required: handle, matrix.");
	Context *c = GetContext(nrhs, prhs);
	if (!c->outf) {
		mexErrMsgTxt("Cannot add frame -- invalid object state.  Either the output file was closed or was never opened or some other error occurred.");
	}
	int sx = mxGetM(prhs[1]), sy = mxGetN(prhs[1]);

	//mexPrintf("DBG: %d x %d matrix...\n", sx, sy);

	if (c->imgLast) {
	  if (sx != c->imgLast->sx || sy != c->imgLast->sy) {
		  mexErrMsgTxt("Passed-in frame is not the same size as the first frame in the animated gif!");  
	  }
	}
	gdImagePtr img = gdImageCreatePalette(sx, sy);

	//mexPrintf("DBG: created img...\n");
	//RETURN_NULL();

	setPalette(img);
	
	void *m = mxGetPr(prhs[1]);
	if (!m) {
		gdImageDestroy(img);
		mexErrMsgTxt("Passed-in matrix is not valid!");
	}
	const int clsid = mxGetClassID(prhs[1]);
	
	for (int x = 0; x < sx; ++x) {
		for (int y = 0; y < sy; ++y) {
			int color = 0;
			
			switch(clsid) {
				case mxCHAR_CLASS:
				case mxINT8_CLASS: color = static_cast<int>(((signed char *)m)[y*sx + x]) + 128; break;
				case mxUINT8_CLASS: color = ((unsigned char *)m)[y*sx + x]; break;
				case mxINT16_CLASS: color = ((short *)m)[y*sx + x]; break;
				case mxUINT16_CLASS: color = ((unsigned short *)m)[y*sx + x]; break;
				case mxUINT32_CLASS: color = ((unsigned int *)m)[y*sx + x]; break;
				case mxINT32_CLASS: color = ((int *)m)[y*sx + y]; break;
				case mxDOUBLE_CLASS: color = ((double *)m)[y*sx + x] * 255.; break;
				case mxSINGLE_CLASS: color = ((float *)m)[y*sx + x] * 255.; break;
				default:
					gdImageDestroy(img);
					mexErrMsgTxt("Argument 2 must be a matrix of numeric type.");
			}
			if (color < 0) color = 0;
			if (color > 255) color = 255;
			gdImageSetPixel(img, x, y, color);
		}
	}
	
	if (!c->imgLast) {
		c->setImgLast(img);
		// defer call to gdImageGifAnimBegin because we may only get 1 frame total, in which case we
		// will just write out the GIF using gfImageGif..
	} else {
		if (c->frameCt == 1) {
			// we got more than 1 frame already, so we do an 'anim'
			gdImageGifAnimBegin(c->imgLast, c->outf, 1, 0xffff);
			gdImageGifAnimAdd(c->imgLast, c->outf, 0, 0, 0, 0, 1, NULL /* NO optimized animations */);
		}
		gdImageGifAnimAdd(img, c->outf, 0, 0, 0, 0, 1, NULL/* *NO* optimized animations supported! */);
	}
	c->setImgLast(img);
	c->needAnimEnd = true;
	++c->frameCt;
	RETURN(1);
}


void finalize(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	if (nrhs != 1) mexErrMsgTxt("Argument required: handle.");

	Context *c = GetContext(nrhs, prhs);
	
	c->endAnimCloseFile(); ///< implicitly calls gdImageGifAnimEnd
	c->setImgLast(0);
}


void destroyContext(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  int h = GetHandle(nrhs, prhs);
  MapDestroy(h);
  RETURN(1);
}


struct CommandFunction
{
	const char *name;
	void (*func)(int, mxArray **, int, const mxArray **);
};

static struct CommandFunction functions[] =
{
    { "create", createNewContext },
    { "destroy", destroyContext },
	{ "addFrame", addFrame },
	{ "finalize", finalize },
};

static const int n_functions = sizeof(functions)/sizeof(struct CommandFunction);

void mexFunction( int nlhs, mxArray *plhs[],
                  int nrhs, const mxArray *prhs[])
{
  const mxArray *cmd;
  int i;
  std::string cmdname, errString = "";
  char *tmp = 0;

  /* Check for proper number of arguments. */
  if(nrhs < 2) {
      errString += "At least two input arguments are required.\n";
      goto err_out;
  } else
      cmd = prhs[0];

  if (!mxIsChar(cmd)) {
      errString +=  "First argument must be a string.\n";
      goto err_out;
  }
  if (mxGetM(cmd) != 1) {
      errString += "First argument must be a row vector.\n";
      goto err_out;
  }
  tmp = mxArrayToString(cmd);
  cmdname = tmp;
  mxFree(tmp);
  for (i = 0; i < n_functions; ++i) {
      // try and match cmdname to a command we know about
    if (::strcmpi(functions[i].name, cmdname.c_str()) == 0 ) {
        // a match.., call function for the command, popping off first prhs
        functions[i].func(nlhs, plhs, nrhs-1, prhs+1); // call function by function pointer...
        return;
    }
  }
  if (i == n_functions) { // cmdname didn't match anything we know about
  err_out:
      errString += "Unrecognized GifWriterMex command.\nMust be one of: ";
      for (int i = 0; i < n_functions; ++i)
          errString += std::string("\n'") + functions[i].name + "'";
      mexErrMsgTxt(errString.c_str());
  }
}
