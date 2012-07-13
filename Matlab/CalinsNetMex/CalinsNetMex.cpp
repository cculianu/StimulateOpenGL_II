#include <math.h>
#include <string>
#include <string.h>
#include <mex.h>
#include <matrix.h>

#include <map>

#include "NetClient.h"

typedef std::map<int, NetClient *> NetClientMap;

#ifndef _MSC_VER
#define strcmpi strcasecmp
#endif

static NetClientMap clientMap;
static int handleId = 0; // keeps getting incremented..

static NetClient * MapFind(int handle)
{
  NetClientMap::iterator it = clientMap.find(handle);
  if (it == clientMap.end()) return NULL;
  return it->second;
}

static void MapPut(int handle, NetClient *client)
{
  NetClient *old = MapFind(handle);
  if (old) delete old; // ergh.. this shouldn't happen but.. oh well.
  clientMap[handle] = client;
}

static void MapDestroy(int handle)
{
  NetClientMap::iterator it = clientMap.find(handle);
  if (it != clientMap.end()) {
    delete it->second;
    clientMap.erase(it);
  } else {
    mexWarnMsgTxt("Invalid or unknown handle passed to CalinsNetMex MapDestroy!");
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

static NetClient * GetNetClient(int nrhs, const mxArray *prhs[])
{
  int handle =  GetHandle(nrhs, prhs);
  NetClient *nc = MapFind(handle);
  if (!nc) mexErrMsgTxt("INTERNAL ERROR -- Cannot find the NetClient for the specified handle in CalinsNetMex!");
  return nc;
}

#define RETURN(x) do { (plhs[0] = mxCreateDoubleScalar(static_cast<double>(x))); return; } while (0)
#define RETURN_NULL() do { (plhs[0] = mxCreateDoubleMatrix(0, 0, mxREAL)); return; } while(0)

void createNewClient(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  if (nlhs != 1) mexErrMsgTxt("Cannot create a client since no output (lhs) arguments were specified!");
  if (nrhs != 2) mexErrMsgTxt("Need two input arguments: Host, port!");
  const mxArray *host = prhs[0], *port = prhs[1];
  if ( !mxIsChar(host) || mxGetM(host) != 1 ) mexErrMsgTxt("Hostname must be a string row vector!");
  if ( !mxIsDouble(port) || mxGetM(port) != 1 || mxGetN(port) != 1) mexErrMsgTxt("Port must be a single numeric value.");

  char *hostStr = mxArrayToString(host);
  unsigned short portNum = static_cast<unsigned short>(*mxGetPr(port));
  NetClient *nc = new NetClient(hostStr, portNum);
  mxFree(hostStr);
  nc->setSocketOption(Socket::TCPNoDelay, true);
  int h = handleId++;
  MapPut(h, nc);
  RETURN(h);
}

void tryConnection(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  NetClient *nc = GetNetClient(nrhs, prhs);
  //if(nlhs < 1) mexErrMsgTxt("One output argument required.");
  bool ok = false;
  try {
    ok = nc->connect();
    if (ok) nc->setSocketOption(Socket::TCPNoDelay, true);
  } catch (const SocketException & e) {
    const std::string why (e.why());
    if (why.length()) mexWarnMsgTxt(why.c_str());
    RETURN_NULL();
  }

  if (!ok) {
    mexWarnMsgTxt(nc->errorReason().c_str());
    RETURN_NULL();
  }

  RETURN(1);
}

void closeSocket(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  NetClient *nc = GetNetClient(nrhs, prhs);
  nc->disconnect();
  RETURN(1);
}


void sendString(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  NetClient *nc = GetNetClient(nrhs, prhs);

  if(nrhs != 2)		mexErrMsgTxt("Two arguments required: handle, string.");
  //if(nlhs < 1) mexErrMsgTxt("One output argument required.");
  if(mxGetClassID(prhs[1]) != mxCHAR_CLASS)
	  mexErrMsgTxt("Argument 2 must be a string.");

  char *tmp = mxArrayToString(prhs[1]);
  std::string theString (tmp);
  mxFree(tmp);

  try {
    nc->sendString(theString);
  } catch (const SocketException & e) {
    const std::string why (e.why());
    if (why.length()) mexWarnMsgTxt(why.c_str());
    RETURN_NULL();
  }
  RETURN(1);
}

void sendMatrix(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  NetClient *nc = GetNetClient(nrhs, prhs);

  if (nrhs != 2) mexErrMsgTxt("Two arguments required: handle, matrix.");
  unsigned long datalen = mxGetN(prhs[1]) * mxGetM(prhs[1]);
  switch(mxGetClassID(prhs[1])) {
  case mxINT8_CLASS:
  case mxUINT8_CLASS:
  case mxCHAR_CLASS: datalen *= sizeof(char); break;
  case mxINT16_CLASS:
  case mxUINT16_CLASS: datalen *= sizeof(short); break;
  case mxUINT32_CLASS:
  case mxINT32_CLASS: datalen *= sizeof(int); break;
  case mxDOUBLE_CLASS: datalen *= sizeof(double); break;
  case mxSINGLE_CLASS: datalen *= sizeof(float); break;
  default:
      mexErrMsgTxt("Argument 2 must be a matrix of numeric type.");
  }


  void *theMatrix = mxGetPr(prhs[1]);

  try {
      nc->sendData(theMatrix, datalen);
  } catch (const SocketException & e) {
      const std::string why (e.why());
      if (why.length()) mexWarnMsgTxt(why.c_str());
      RETURN_NULL();
  }

  RETURN(1);
}

void readString(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  if(nlhs < 1) mexErrMsgTxt("One output argument required.");
  NetClient *nc = GetNetClient(nrhs, prhs);

  try {
    std::string theString ( nc->receiveString() );
    plhs[0] = mxCreateString(theString.c_str());
  } catch (const SocketException & e) {
    const std::string why (e.why());
    if (why.length()) mexWarnMsgTxt(why.c_str());
    RETURN_NULL(); // note empty return..
  }
}

void readLine(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  if(nlhs < 1) mexErrMsgTxt("One output argument required.");
  NetClient *nc = GetNetClient(nrhs, prhs);

  try {
    std::string theString ( nc->receiveLine() );
    plhs[0] = mxCreateString(theString.c_str());
  } catch (const SocketException & e) {
    const std::string why (e.why());
    if (why.length()) mexWarnMsgTxt(why.c_str());
    RETURN_NULL(); // note empty return..
  }
}

void readLines(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  if(nlhs < 1) mexErrMsgTxt("One output argument required.");

  NetClient *nc = GetNetClient(nrhs, prhs);
  try {
    char **lines = nc->receiveLines();
    int m;
    for (m = 0; lines[m]; m++) {} // count number of lines
    plhs[0] = mxCreateCharMatrixFromStrings(m, const_cast<const char **>(lines));
    NetClient::deleteReceivedLines(lines);
  } catch (const SocketException &e) {
    const std::string why (e.why());
    if (why.length()) mexWarnMsgTxt(why.c_str());
    RETURN_NULL(); // empty return set
  }
}


void readMatrix(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  NetClient *nc = GetNetClient(nrhs, prhs);
  int ndims, datalen;
  if (!nlhs)
      mexErrMsgTxt("output (lhs) parameter is required.");
  if (nrhs < 3 || !mxIsChar(prhs[1]) || !mxIsDouble(prhs[2]) || (ndims=mxGetN(prhs[2])) < 2 || mxGetM(prhs[2]) != 1)
      mexErrMsgTxt("'readMatrix' needs arguments:\n Argument 1 handle\n Argument 2, a string 'double' or 'single' or 'uint8'\n Argument 3, a 1x4, 1x3 or 1x2 vector of dimensions for m,n[,o,p]");
  int buflen = (mxGetM(prhs[1]) * mxGetN(prhs[1]) * sizeof(mxChar)) + 1;
  std::string stdstr(buflen, '0');
  mxGetString(prhs[1], &stdstr[0], buflen);
  const char *str = stdstr.c_str();
  const double *dims_dbls = mxGetPr(prhs[2]);
  if (ndims > 4) ndims = 4;
  const int dims[] = { static_cast<int>(dims_dbls[0]),
                       static_cast<int>(dims_dbls[1]),
                       ndims >= 3 ? static_cast<int>(dims_dbls[2]) : 1,
                       ndims >= 4 ? static_cast<int>(dims_dbls[3]) : 1 };
  datalen = dims[0]*dims[1]*dims[2]*dims[3];
  mxClassID cls;
  if (!strcmpi(str, "double")) cls = mxDOUBLE_CLASS, datalen *= sizeof(double);
  else if (!strcmpi(str, "single")) cls = mxSINGLE_CLASS, datalen *= sizeof(float);
  else if (!strcmpi(str, "int8")) cls = mxINT8_CLASS, datalen *= sizeof(char);
  else if (!strcmpi(str, "uint8")) cls = mxUINT8_CLASS, datalen *= sizeof(char);
  else if (!strcmpi(str, "int16")) cls = mxINT16_CLASS, datalen *= sizeof(short);
  else if (!strcmpi(str, "uint16")) cls = mxUINT16_CLASS, datalen *= sizeof(short);
  else if (!strcmpi(str, "int32")) cls = mxINT32_CLASS, datalen *= sizeof(int);
  else if (!strcmpi(str, "uint32")) cls = mxUINT32_CLASS, datalen *= sizeof(int);
  else  mexErrMsgTxt("Output matrix type must be one of 'single', 'double', 'int8', 'uint8', 'int16', 'int16', 'int32', 'uint32'.");
  plhs[0] = mxCreateNumericArray(ndims, dims, cls, mxREAL);
  try {
      nc->receiveData(mxGetData(plhs[0]), datalen, true);
  } catch (const SocketException & e) {
      const std::string why (e.why());
      if (why.length()) mexWarnMsgTxt(why.c_str());
      mxDestroyArray(plhs[0]);
      plhs[0] = 0;  // nullify (empty) return..
      RETURN_NULL();
  }
}


void destroyClient(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
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
    { "create", createNewClient },
    { "destroy", destroyClient },
    { "connect", tryConnection },
    { "disconnect", closeSocket },
    { "sendString", sendString },
    { "sendMatrix", sendMatrix },
    { "readString", readString },
    { "readLines",  readLines},
    { "readLine",  readLine},
    { "readMatrix", readMatrix },
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
      errString += "Unrecognized CalinsNetMex command.\nMust be one of: ";
      for (int i = 0; i < n_functions; ++i)
          errString += std::string("\n'") + functions[i].name + "'";
      mexErrMsgTxt(errString.c_str());
  }
}
