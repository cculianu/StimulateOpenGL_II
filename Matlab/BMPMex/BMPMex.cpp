#include <math.h>
#include <string>
#include <string.h>
#include <mex.h>
#include <matrix.h>
#include <stdio.h>
#ifndef _MSC_VER
#define strcmpi strcasecmp
#endif

#define RETURN(x) do { (plhs[0] = mxCreateDoubleScalar(static_cast<double>(x))); return; } while (0)
#define RETURN_NULL() do { (plhs[0] = mxCreateDoubleMatrix(0, 0, mxREAL)); return; } while(0)

struct CommandFunction
{
	const char *name;
	void (*func)(int, mxArray **, int, const mxArray **);
        int nRhsMin;
};

void convertRGBToBGR(int nlhs, mxArray **plhs, int nrhs, const mxArray **prhs)
{
    if (nrhs < 1 || !mxIsUint8(prhs[0])) {
        mexErrMsgTxt("input array of uint8 type needed");
    }
    unsigned char *elems = (unsigned char *)mxGetData(prhs[0]);
    int nelems = mxGetNumberOfElements(prhs[0]);
    plhs[0] = mxCreateNumericArray(mxGetNumberOfDimensions(prhs[0]), mxGetDimensions(prhs[0]), mxUINT8_CLASS, mxREAL);
    unsigned char *elems_out = (unsigned char *)mxGetData(plhs[0]);
    for (int i = 0; i < nelems; i+=3) {
        elems_out[i] = elems[i+2];
        elems_out[i+1] = elems[i+1];
        elems_out[i+2] = elems[i];
    }
}

#ifdef _MSC_VER
#define PACKED
#pragma pack(push, 1)
#else
#define PACKED __attribute__((packed))
#endif


struct BMPInfo {
   // info header follows
    unsigned int size;               /* Header size in bytes      */
    int width,height;                /* Width and height of image */
    unsigned short int planes;       /* Number of colour planes   */
    unsigned short int bits;         /* Bits per pixel            */
    unsigned int compression;        /* Compression type          */
    unsigned int imagesize;          /* Image size in bytes       */
    int xresolution,yresolution;     /* Pixels per meter          */
    unsigned int ncolours;           /* Number of colours         */
    unsigned int importantcolours;   /* Important colours         */

    BMPInfo()  { 
        memset(this, 0, sizeof(*this));
        size = sizeof(*this);
        planes = 1;
    }
} PACKED;

struct BMPHeader {
   unsigned short int type;                 /* Magic identifier            */
   unsigned int fsize;                      /* File size in bytes          */
   unsigned short int reserved1, reserved2;
   unsigned int offset;                     /* Offset to image data, bytes */
   BMPHeader() {
        memset(this, 0, sizeof(*this));
        memcpy(&type, "BM", 2); 
        offset = sizeof(*this) + sizeof(BMPInfo);
   }
} PACKED;

#ifdef _MSC_VER
#undef PACKED
#pragma pack(pop)
#else
#undef PACKED
#endif

void saveBMP(int nlhs, mxArray **plhs, int nrhs, const mxArray **prhs)
{
    std::string fname;
    if (nrhs < 1 || !mxIsChar(prhs[0])) {
        mexErrMsgTxt("first argument must be filename string");
    } else {
        char tmp[256];
        mxGetString(prhs[0], tmp, sizeof(tmp)-1);
        fname = tmp;
    }

    if (nrhs < 2 || !mxIsUint8(prhs[1])) {
        mexErrMsgTxt("second argument must be input array of uint8 type needed");
    }
    int w,h;
    if (nrhs < 3 || !mxIsNumeric(prhs[2]) || (w=int(mxGetPr(prhs[2])[0])) <= 0) {
        mexErrMsgTxt("third argument must be width in pixels of img");
    }
    if (nrhs < 4 || !mxIsNumeric(prhs[3]) || (h=int(mxGetPr(prhs[3])[0])) <= 0) {
        mexErrMsgTxt("fourth argument must be height in pixels of img");
    }
    if (w*h*3 > mxGetNumberOfElements(prhs[1])) {
        mexErrMsgTxt("specified width and height exceed size of image data array.");
    }
    FILE *f = fopen(fname.c_str(), "wb");
    if (!f) {
        mexErrMsgTxt("could not open output file");
    }

    BMPHeader hdr;
    hdr.fsize = 54+w*h*3;
    BMPInfo nfo;
    nfo.bits = 24;
    nfo.width = w;
    nfo.height = h;
    nfo.imagesize = w*h*3;
    nfo.xresolution = 2048;
    nfo.yresolution = 2048;
    fwrite(&hdr, 14, 1, f);
    fwrite(&nfo, 40, 1, f);
    fwrite(mxGetData(prhs[1]), w*h*3, 1, f);
    fclose(f);
    mexPrintf("saveBMP: Saved %s\n", fname.c_str());
    RETURN(1);    
}

static struct CommandFunction functions[] = 
{
    { "convertRGBToBGR", convertRGBToBGR, 1 },
    { "saveBMP", saveBMP, 4 },
};

static const int n_functions = sizeof(functions)/sizeof(struct CommandFunction);

void mexFunction( int nlhs, mxArray *plhs[],
                  int nrhs, const mxArray *prhs[])
{
  const mxArray *cmd;
  int i;
  std::string cmdname, errString = "";
  char *tmp = 0, buf[20];
  
  /* Check for proper number of arguments. */
  if(nrhs < 1) {
      errString += "At least one input arguments are required, the commandname.\n";
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
        if (nrhs-1 < functions[i].nRhsMin) {
            sprintf(buf, "%d", functions[i].nRhsMin);
            errString += std::string("command '") + cmdname + "' requires " + buf + " arguments.\n";
            goto err_out; 
        }
        // a match.., call function for the command, popping off first prhs
        functions[i].func(nlhs, plhs, nrhs-1, prhs+1); // call function by function pointer...
        return;
    }
  }
  if (i == n_functions) { // cmdname didn't match anything we know about
  err_out:
      errString += "Unrecognized BMPMex command.\nMust be one of: ";
      for (int i = 0; i < n_functions; ++i) {      
          errString += std::string("\n'") + functions[i].name + "'";
          mexErrMsgTxt(errString.c_str());
      }
  }
}
