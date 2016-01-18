#include <math.h>
#include <string>
#include <string.h>
#include <mex.h>
#include <matrix.h>
#include <stdio.h>

#include <map>
#ifndef NO_QT
#define NO_QT
#endif
#include "../../FastMovieFormat.h"

#ifdef __MACH__
extern int errno;
#endif

struct Context {
	FM_Context *ctx;
	int w, h;
	int frameCt;
	std::string fileName;
	
	Context() : ctx(0), w(0), h(0), frameCt(0) {}
	~Context() {
		endAnimCloseFile();
	}
	void endAnimCloseFile() {
		if (ctx) { 
			if (w && h) {
				mexPrintf("%d x %d FastMovie, %d frames, written to %s\n", w, h, frameCt, fileName.c_str());
			}
			FM_Close(ctx);
			ctx = 0;
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
		c->ctx = FM_Create(c->fileName.c_str());
		err = errno;
	} else {
		mexErrMsgTxt("Please pass in a filename (string) for the output file."); 
	}
	if (!c->ctx) {
		delete c; c = 0;
		std::string msg = std::string("Cannot open specified file for writing: ") + strerror(err); 
		mexErrMsgTxt(msg.c_str());
	}
	int h = ++ctxId;
	MapPut(h, c);
	RETURN(h);
}

void addFrame(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	if (nrhs != 3) mexErrMsgTxt("Three arguments required: handle, matrix, compression level.");
	Context *c = GetContext(nrhs, prhs);
	if (!c->ctx) {
		mexErrMsgTxt("Cannot add frame -- invalid object state.  Either the output file was closed or was never opened or some other error occurred.");
	}
	int sx = mxGetM(prhs[1]), sy = mxGetN(prhs[1]);
	int clevel = 1;
	
	if ( !mxIsDouble(prhs[2]) || mxGetM(prhs[2]) != 1 || mxGetN(prhs[2]) != 1)
		mexErrMsgTxt("Compression level must be a single double value.");
	
	clevel = static_cast<int>(*mxGetPr(prhs[2]));

	if (clevel < 0) clevel = 0;
	if (clevel > 9) clevel = 9;
	
	//mexPrintf("DBG: %d x %d matrix...\n", sx, sy);

	if (c->w && c->h) {
	  if (sx != c->w || sy != c->h) {
		  mexErrMsgTxt("Passed-in frame is not the same size as the first frame!");  
	  }
	}
	c->w = sx;
	c->h = sy;
	

	if (sx <= 0 || sy <= 0) {
		mexErrMsgTxt("Passed-in matrix cannot be empty!");
	}
	
	uint8_t *imgbuf = new uint8_t [sx*sy];
	
	void *m = mxGetData(prhs[1]);
	if (!m) {
		delete [] imgbuf;
		mexErrMsgTxt("Passed-in matrix is not valid!");
	}
	const int clsid = mxGetClassID(prhs[1]);
	
	if (clsid == mxUINT8_CLASS) {
		// optimized processing of pixels if the format is correct -- just do a memcpy!
		memcpy(imgbuf, m, sx*sy);
	} else {	
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
					case mxINT32_CLASS: color = ((int *)m)[y*sx + x]; break;
					case mxDOUBLE_CLASS: color = ((double *)m)[y*sx + x] * 255.; break;
					case mxSINGLE_CLASS: color = ((float *)m)[y*sx + x] * 255.; break;
					default:
						delete [] imgbuf;
						mexErrMsgTxt("Argument 2 must be a matrix of numeric type.");
				}
				if (color < 0) color = 0;
				if (color > 255) color = 255;
				(imgbuf + (y*sx))[x] = (uint8_t)color;
			}
		}
	}

	if (!FM_AddFrame(c->ctx, imgbuf, sx, sy, clevel)) {
		delete [] imgbuf;
		mexErrMsgTxt("Failed in call to FM_AddFrame().");
	}
	delete [] imgbuf;
	++c->frameCt;
	RETURN(1);
}


void finalize(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	if (nrhs != 1) mexErrMsgTxt("Argument required: handle.");

	Context *c = GetContext(nrhs, prhs);
	
	c->endAnimCloseFile(); 
}

template<typename T> T & GetDatum3(const int *dims, void *array, int m, int n, int p)
{
	int M = dims[0], N = dims[1], P = dims[2];
	T * arr = reinterpret_cast<T *>(array);
	return arr[m+n*M+p*M*N];
}

template<typename T> T & GetDatum2(int M, int N, void *array, int m, int n)
{
	T * arr = reinterpret_cast<T *>(array);
	return arr[m+n*M];	
}

void to_GS_8Bit(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	if(nlhs < 1) mexErrMsgTxt("One output argument required.");
	if(nrhs != 1) mexErrMsgTxt("Please pass a H x W x 3 array as the only argument to this function.");
	void *array = mxGetPr(prhs[0]);
	if (!array) {
		mexErrMsgTxt("Passed-in argument is not valid!");
	}
	const int clsid = mxGetClassID(prhs[0]);
	if (mxGetNumberOfDimensions(prhs[0]) != 3) {
		mexErrMsgTxt("Passed-in argument needs to be a H x W x 3 array!");
	}
	const int *dims = mxGetDimensions(prhs[0]);
	const int w = dims[1], h = dims[0];
	mxArray *out = mxCreateNumericMatrix(w, h, mxUINT8_CLASS, mxREAL); 
	uint8_t *o = (uint8_t *)mxGetData(out);
	int color = 0;
	for (int x = 0; x < w; ++x) {
		for (int y = 0; y < h; ++y) {
			color = 0;
			switch(clsid) {
				case mxCHAR_CLASS:
				case mxINT8_CLASS: color = (int(GetDatum3<int8_t>(dims, array, y, x, 0)) + int(GetDatum3<int8_t>(dims, array, y, x, 1)) + int(GetDatum3<int8_t>(dims, array, y, x, 2))) / 3 + 128; break;
				case mxUINT8_CLASS: color = (int(GetDatum3<uint8_t>(dims, array, y, x, 0)) + int(GetDatum3<uint8_t>(dims, array, y, x, 1)) + int(GetDatum3<uint8_t>(dims, array, y, x, 2))) / 3; break;
				case mxINT16_CLASS: color = (int(GetDatum3<int16_t>(dims, array, y, x, 0)) + int(GetDatum3<int16_t>(dims, array, y, x, 1)) + int(GetDatum3<int16_t>(dims, array, y, x, 2))) / 3; break;
				case mxUINT16_CLASS: color = (int(GetDatum3<uint16_t>(dims, array, y, x, 0)) + int(GetDatum3<uint16_t>(dims, array, y, x, 1)) + int(GetDatum3<uint16_t>(dims, array, y, x, 2))) / 3; break;
				case mxUINT32_CLASS: color = (int(GetDatum3<uint32_t>(dims, array, y, x, 0)) + int(GetDatum3<uint32_t>(dims, array, y, x, 1)) + int(GetDatum3<uint32_t>(dims, array, y, x, 2))) / 3; break;; break;
				case mxINT32_CLASS: color = (int(GetDatum3<int32_t>(dims, array, y, x, 0)) + int(GetDatum3<int32_t>(dims, array, y, x, 1)) + int(GetDatum3<int32_t>(dims, array, y, x, 2))) / 3; break;; break;
				case mxDOUBLE_CLASS: color = ((double(GetDatum3<double>(dims, array, y, x, 0)) + double(GetDatum3<double>(dims, array, y, x, 1)) + double(GetDatum3<double>(dims, array, y, x, 2))) / 3.) * 255.; break;
				case mxSINGLE_CLASS: color = ((double(GetDatum3<float>(dims, array, y, x, 0)) + double(GetDatum3<float>(dims, array, y, x, 1)) + double(GetDatum3<float>(dims, array, y, x, 2))) / 3.) * 255.; break;
				default:
					mexErrMsgTxt("Argument 1 must be a matrix of numeric type.");
			}
			GetDatum2<uint8_t>(w, h, o, x, y) = color;
		}
	}
	plhs[0] = out;
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
	{ "to_GS_8Bit", to_GS_8Bit },
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
      errString += "Unrecognized FastMovieWriterMex command.\nMust be one of: ";
      for (int i = 0; i < n_functions; ++i)
          errString += std::string("\n'") + functions[i].name + "'";
      mexErrMsgTxt(errString.c_str());
  }
}
