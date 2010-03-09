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

#ifdef LINUX
extern "C" {
#  include "scanproc.h"
#  include "scanproc.c"
}
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <unistd.h>
#  include <stdlib.h>
#elif defined(DARWIN)
extern int errno;
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <unistd.h>
#  include <stdlib.h>
#  include "BSDProcUtil.h"
#elif defined(WIN32)
#  include <windows.h>
#  include <tlhelp32.h>
#else
#  error Need to define one of LINUX, WIN32 or DARWIN 
#endif

#if defined(LINUX) || defined(DARWIN)
static bool Execute(const std::string & prog)
{
    pid_t pid = fork();
    if (pid > 0) {
        // parent
        int status;
        sleep(1);
        int ret = waitpid(pid, &status, WNOHANG);
        // failed
        if (ret == pid && WEXITSTATUS(status) == 123)
            return false;
        else if (ret < 0) {
            std::string err = std::string("wait: ") + strerror(errno) + "\n";
            mexErrMsgTxt(err.c_str());
            return false;
        }
        // if we get here let's assume the child started ok
        return true;
    } else if (pid == 0) {
        // child
        execlp(prog.c_str(), prog.c_str());
        exit(123); // if we get here, error
    } else { 
        // error 
        std::string err = std::string("fork: ") + strerror(errno) + "\n";
        mexErrMsgTxt(err.c_str());
    }
	return false;
}
#elif defined(WIN32)
static bool Execute(const std::string & prog)
{
    STARTUPINFO sinfo;
    PROCESS_INFORMATION pinfo;
    memset(&sinfo, 0, sizeof(sinfo));
    sinfo.cb = sizeof(sinfo);
    char *s = strdup(prog.c_str());
    
    BOOL b = CreateProcessA(NULL, s, NULL, NULL, FALSE, 0, NULL, NULL, &sinfo, &pinfo);
    free(s);
    if (b) {
        CloseHandle(pinfo.hProcess);
        CloseHandle(pinfo.hThread);
        return true;
    } else {
        char *msg = 0;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
                      0,
                      GetLastError(),
                      0,
                      (char *)&msg,
                      4096,
                      NULL);
        if (msg) {
            std::string err = std::string("Could not CreateProcess ") + prog + ": " + msg + "\n!!PLEASE CHECK YOUR PATH!!\n";
            LocalFree(msg);
            mexErrMsgTxt(err.c_str());
        }
    }
    return false;
}
#endif

static void ensureProgramIsRunning(int nlhs, mxArray ** plhs, 
                            int nrhs, const mxArray ** prhs)
{
    std::string prog;
    char *tmp = mxArrayToString(prhs[0]);
    prog = tmp;
    mxFree(tmp);
#if defined(LINUX) || defined(DARWIN)
#  ifdef LINUX
    pid_t *pids = pids_of_exe(prog.c_str());
    bool is_running = *pids;
    free(pids);
#  else // DARWIN
	bool is_running = IsBSDProcessRunning(prog.c_str());
#  endif
    if (is_running) {
        mexPrintf("Found a %s process already running\n", prog.c_str());
    } else {
        mexPrintf("Did not find a %s process already running, trying to start it up\n", prog.c_str());
        if (!Execute(prog) && !Execute(std::string("./") + prog)) {
            std::string s = "Could not start ";
            s += prog + " -- check your PATH environment variable?\n";
            mexErrMsgTxt(s.c_str());
            RETURN(0);
        }
        mexPrintf("Program '%s' started ok\n", prog.c_str());
    }
    RETURN(1);
#elif defined(WIN32)
    
    if (prog.find(".exe") != prog.length()-4) prog += ".exe";

    HANDLE hProcessSnap;
    HANDLE hProcess;
    PROCESSENTRY32 pe32;
    DWORD dwPriorityClass;

    // Take a snapshot of all processes in the system.
    hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    if( hProcessSnap == INVALID_HANDLE_VALUE )
    {
        mexErrMsgTxt("CreateToolhelp32Snapshot (of processes) failed");
        RETURN(0);
    }
    // Set the size of the structure before using it.
    pe32.dwSize = sizeof( PROCESSENTRY32 );

    // Retrieve information about the first process,
    // and exit if unsuccessful
    if( !Process32First( hProcessSnap, &pe32 ) )
    {
        mexErrMsgTxt("Process32First error" ); // show cause of failure
        CloseHandle( hProcessSnap );          // clean the snapshot object
        RETURN(0);
     }
    bool is_running = false;
    // Now walk the snapshot of processes, and find the one we want
    do
    {
        if (!strcmpi(pe32.szExeFile, prog.c_str())) {
            is_running = true;
            break;
        }
    } while( Process32Next( hProcessSnap, &pe32 ) );
    CloseHandle( hProcessSnap );
    if (is_running) {
        mexPrintf("Found a %s process already running\n", prog.c_str());
    } else {
        mexPrintf("Did not find a %s process already running, trying to start it up\n", prog.c_str());
        if (!Execute(prog) && !Execute(std::string("./") + prog)) {
            std::string s = "Could not start ";
            s += prog + " -- check your PATH environment variable?\n";
            mexErrMsgTxt(s.c_str());
            RETURN(0);
        }
        mexPrintf("Program '%s' started ok\n", prog.c_str());
        Sleep(2); // wait for it to really start up?
    }
    RETURN(1);
#endif    
}

static struct CommandFunction functions[] = 
{
    { "ensureProgramIsRunning", ensureProgramIsRunning, 1 },
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
      errString += "Unrecognized OSFuncs command.\nMust be one of: ";
      for (int i = 0; i < n_functions; ++i) {      
          errString += std::string("\n'") + functions[i].name + "'";
          mexErrMsgTxt(errString.c_str());
      }
  }
}
