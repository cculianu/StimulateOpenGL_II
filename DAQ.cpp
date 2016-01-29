#define _CRT_SECURE_NO_WARNINGS
#include "DAQ.h"
#include "Util.h"
#ifdef HAVE_NIDAQmx
#  include "NI/NIDAQmx.h"
#else
#  define FAKEDAQ
#ifndef Q_OS_WIN
#  warning Not a real NI platform.  All acquisition related functions are emulated!
#endif
#endif

#include <string.h>
#include <QString>
#include <QFile>
#include <QMessageBox>
#include <QApplication>
#include <QRegExp>
#include <QThread>
#include <QPair>
#include <QSet>
#include <QMap>
#include <QMutexLocker>
#include "TypeDefs.h"

#define DAQ_TIMEOUT 2.5

#define DAQmxErrChk(functionCall) do { if( DAQmxFailed(error=(functionCall)) ) { callStr = STR(functionCall); goto Error_Out; } } while (0)

namespace DAQ 
{
    static bool noDaqErrPrint = false;

    /// if empty map returned, no devices with AI!
    DeviceRangeMap ProbeAllAIRanges() 
    {
        DeviceRangeMap ret;
        Range r;
#ifdef HAVE_NIDAQmx
        double myDoubleArray[512];
        for (int devnum = 1; devnum <= 16; ++devnum) {
            memset(myDoubleArray, 0, sizeof(myDoubleArray));
            QString dev( QString("Dev%1").arg(devnum) );
            if (!DAQmxFailed(DAQmxGetDevAIVoltageRngs(dev.toUtf8().constData(), myDoubleArray, 512))) {
                for (int i=0; i<512; i=i+2) {
                    r.min = myDoubleArray[i];
                    r.max = myDoubleArray[i+1];
                    if (r.min == r.max) break;
                    ret.insert(dev, r);
                }
            }
        }
#else // !WINDOWS, emulate
        r.min = -2.5;
        r.max = 2.5;
        ret.insert("Dev1", r);
        r.min = -5.;
        r.max = 5.;
        ret.insert("Dev1", r);
#endif
        return ret;
    }
    /// if empty map returned, no devices with AO!
    DeviceRangeMap ProbeAllAORanges()
    {
        DeviceRangeMap ret;
        Range r;
#ifdef HAVE_NIDAQmx
        double myDoubleArray[512];
        for (int devnum = 1; devnum <= 16; ++devnum) {
            memset(myDoubleArray, 0, sizeof(myDoubleArray));
            QString dev( QString("Dev%1").arg(devnum) );
            if (!DAQmxFailed(DAQmxGetDevAOVoltageRngs(dev.toUtf8().constData(), myDoubleArray, 512))) {
                for (int i=0; i<512; i=i+2) {
                    r.min = myDoubleArray[i];
                    r.max = myDoubleArray[i+1];
                    if (r.min == r.max) break;
                    ret.insert(dev, r);
                }
            }
        }
#else // !WINDOWS, emulate
        r.min = -2.5;
        r.max = 2.5;
        ret.insert("Dev1", r);
        r.min = -5.;
        r.max = 5.;
        ret.insert("Dev1", r);
#endif
        return ret;
    }

    DeviceChanMap ProbeAllAIChannels() {
        bool savedPrt = noDaqErrPrint;
        noDaqErrPrint = true;
        DeviceChanMap ret;
        for (int devnum = 1; devnum <= 16; ++devnum) {
            QString dev( QString("Dev%1").arg(devnum) );
            QStringList l = GetAIChans(dev);
            if (!l.empty()) {
                ret[dev] = l;
            }
        }
        noDaqErrPrint = savedPrt;
        return ret;
    }

    DeviceChanMap ProbeAllAOChannels() {
        bool savedPrt = noDaqErrPrint;
        noDaqErrPrint = true;
        DeviceChanMap ret;
        for (int devnum = 1; devnum <= 16; ++devnum) {
            QString dev( QString("Dev%1").arg(devnum) );
            QStringList l = GetAOChans(dev);
            if (!l.empty()) {
                ret[dev] = l;
            }
        }
        noDaqErrPrint = savedPrt;
        return ret;
    }

	DeviceChanMap ProbeAllDOChannels() {
        bool savedPrt = noDaqErrPrint;
        noDaqErrPrint = true;
        DeviceChanMap ret;
        for (int devnum = 1; devnum <= 16; ++devnum) {
            QString dev( QString("Dev%1").arg(devnum) );
            QStringList l = GetDOChans(dev);
            if (!l.empty()) {
                ret[dev] = l;
            }
        }
        noDaqErrPrint = savedPrt;
        return ret;
    }
	
	bool DOChannelExists(const QString & devChan) {
		DeviceChanMap chanMap (ProbeAllDOChannels());
		for (DAQ::DeviceChanMap::const_iterator it = chanMap.begin(); it != chanMap.end(); ++it) {
			for (QStringList::const_iterator it2 = (*it).begin(); it2 != (*it).end(); ++it2) {			
				const QString & item (*it2);
				if (0 == item.compare(devChan, Qt::CaseInsensitive)) {
					return true;
				}
			}
		}
		return false;
	}

#if HAVE_NIDAQmx
    typedef int32 (__CFUNC *QueryFunc_t)(const char [], char *, uInt32);
    
    static QStringList GetPhysChans(const QString &devname, QueryFunc_t queryFunc, const QString & fn = "") 
    {
        int error;
        const char *callStr = "";
        char errBuff[2048];        
        char buf[65536] = "";
        QString funcName = fn;

        if (!funcName.length()) {
            funcName = "??";
        }

        DAQmxErrChk(queryFunc(devname.toUtf8().constData(), buf, sizeof(buf)));
        return QString(buf).split(QRegExp("\\s*,\\s*"), QString::SkipEmptyParts);
        
    Error_Out:
        if( DAQmxFailed(error) )
            DAQmxGetExtendedErrorInfo(errBuff,2048);
        if( DAQmxFailed(error) ) {
            if (!noDaqErrPrint) {
                Error() << "DAQmx Error: " << errBuff;
                Error() << "DAQMxBase Call: " << funcName << "(" << devname << ",buf," << sizeof(buf) << ")";
            }
        }
        
        return QStringList();         
    }
#endif

    QStringList GetDOChans(const QString & devname) 
    {
#ifdef HAVE_NIDAQmx
        return GetPhysChans(devname, DAQmxGetDevDOLines, "DAQmxGetDevDOLines");
#else // !HAVE_NIDAQmx, emulated, 1 chan
        return QStringList(QString("%1/port0/line0").arg(devname));
#endif
    }

    QStringList GetAIChans(const QString & devname)
    {
#ifdef HAVE_NIDAQmx
        return GetPhysChans(devname, DAQmxGetDevAIPhysicalChans, "DAQmxGetDevAIPhysicalChans");
#else // !HAVE_NIDAQmx, emulated, 60 chans
        QStringList ret;
        if (devname == "Dev1") {
            for (int i = 0; i < 60; ++i) {
                ret.push_back(QString("%1/ai%2").arg(devname).arg(i));
            }
        }
        return ret;
#endif
    }

    QStringList GetAOChans(const QString & devname)
    {
#ifdef HAVE_NIDAQmx
        return GetPhysChans(devname, DAQmxGetDevAOPhysicalChans, "DAQmxGetDevAOPhysicalChans");
#else // !HAVE_NIDAQmx, emulated, 2 chans
        QStringList ret;
        if (devname == "Dev1") {
            for (int i = 0; i < 2; ++i) {
                ret.push_back(QString("%1/ao%2").arg(devname).arg(i));
            }
        }
        return ret;
#endif
    }

    /// returns the number of physical channels in the AI subdevice for this device, or 0 if AI not supported on this device
    unsigned GetNAIChans(const QString & devname)
    {
        return GetAIChans(devname).count();
    }
    /// returns the number of physical channels in the AO subdevice for this device, or 0 if AO not supported on this device
    unsigned GetNAOChans(const QString & devname)
    {
        return GetAOChans(devname).count();
    }


    /// returns true iff the device supports AI simultaneous sampling
    bool     SupportsAISimultaneousSampling(const QString & devname)
    {
#ifdef HAVE_NIDAQmx
        bool32 ret = false;
        if (DAQmxFailed(DAQmxGetDevAISimultaneousSamplingSupported(devname.toUtf8().constData(), &ret))) {
            Error() << "Failed to query whether dev " << devname << " AI supports simultaneous sampling.";
        }
        return ret;
#else // !HAVE_NIDAQmx, emulated
        (void)devname;
        return true;
#endif
    }

    double   MaximumSampleRate(const QString & dev, int nChans) 
    {
        double ret = 1e6;
        (void)dev; (void)nChans;
#ifdef HAVE_NIDAQmx
        float64 val;
        int32 e;
        if (nChans <= 0) nChans = 1;
        if (nChans == 1)
            e = DAQmxGetDevAIMaxSingleChanRate(dev.toUtf8().constData(), &val);
        else
            e = DAQmxGetDevAIMaxMultiChanRate(dev.toUtf8().constData(), &val);
        if (DAQmxFailed(e)) {
            Error() << "Failed to query maximum sample rate for dev " << dev << ".";
        } else {
            ret = val;
            if (nChans > 1 && !SupportsAISimultaneousSampling(dev)) {
                ret = ret / nChans;
            }
        }
#endif
        return ret;        
    }

    double   MinimumSampleRate(const QString & dev)
    {
        double ret = 10.;
        (void)dev;
#ifdef HAVE_NIDAQmx
        float64 val;
        if (DAQmxFailed(DAQmxGetDevAIMinRate(dev.toUtf8().constData(), &val))) {
            Error() << "Failed to query minimum sample rate for dev " << dev << ".";
            
        } else {
            ret = val;
        }
#endif
        return ret;
    }

    QString  GetProductName(const QString &dev)
    {
#ifdef HAVE_NIDAQmx
        char buf[65536] = "Unknown";
        if (DAQmxFailed(DAQmxGetDevProductType(dev.toUtf8().constData(), buf, sizeof(buf)))) {
            Error() << "Failed to query product name for dev " << dev << ".";
        } 
        // else..
        return buf;
#else
        (void)dev;
        return "FakeDAQ";
#endif
    }
	
#ifndef FAKEDAQ
	struct DAQTaskDesc {
		TaskHandle taskHandle;
		double minv, maxv;
		
        DAQTaskDesc() : taskHandle(0), minv(0.), maxv(0.) {}
	};
	
	typedef QMap<QString, DAQTaskDesc> ActiveDAQHandles;
	
	static ActiveDAQHandles activeAOHandles, activeDOHandles;
	
	static DAQTaskDesc FindAOHandle(const QString & devChan) 
	{
		ActiveDAQHandles::iterator it = activeAOHandles.find(devChan);
		if (it != activeAOHandles.end()) return it.value();
		return DAQTaskDesc();
	}

	static DAQTaskDesc FindDOHandle(const QString & devChan) 
	{
		ActiveDAQHandles::iterator it = activeDOHandles.find(devChan);
		if (it != activeDOHandles.end()) return it.value();
		return DAQTaskDesc();
	}
	
	static void ClearAOHandle(const QString & devChan) 
	{
		ActiveDAQHandles::iterator it = activeAOHandles.find(devChan);
		if (it != activeAOHandles.end()) {
			DAQTaskDesc & dtd = it.value();
			if (dtd.taskHandle) {
				DAQmxStopTask(dtd.taskHandle);
				DAQmxClearTask(dtd.taskHandle);
				dtd.taskHandle = 0;
			}
			activeAOHandles.erase(it);
		}
	}

	static void ClearDOHandle(const QString & devChan) 
	{
		ActiveDAQHandles::iterator it = activeDOHandles.find(devChan);
		if (it != activeDOHandles.end()) {
			DAQTaskDesc & dtd = it.value();
			if (dtd.taskHandle) {
				DAQmxStopTask(dtd.taskHandle);
				DAQmxClearTask(dtd.taskHandle);
				dtd.taskHandle = 0;
			}
			activeDOHandles.erase(it);
		}
	}
	
	void ResetDAQ() 
	{
        for (ActiveDAQHandles::iterator it = activeAOHandles.begin(); it != activeAOHandles.end(); ++it) {
			DAQTaskDesc & dtd = it.value();			
			if (dtd.taskHandle) {
				DAQmxStopTask(dtd.taskHandle);
				DAQmxClearTask(dtd.taskHandle);
				dtd.taskHandle = 0;				
			}
		}
        for (ActiveDAQHandles::iterator it = activeDOHandles.begin(); it != activeDOHandles.end(); ++it) {
			DAQTaskDesc & dtd = it.value();
			if (dtd.taskHandle) {
				DAQmxStopTask(dtd.taskHandle);
				DAQmxClearTask(dtd.taskHandle);
				dtd.taskHandle = 0;				
			}
		}
        activeAOHandles.clear();
        activeDOHandles.clear();
	}
#else
	void ResetDAQ() { Debug() << "DAQ::ResetDAQ() called, unimplemented in FakeDAQ."; }
#endif
	

#define DEFAULT_DEV "Dev1"
#define DEFAULT_DO 0


    bool WriteDO(const QString & devChan, bool onoff, bool closeDevice)
    {
        QString tmp;
#ifdef FAKEDAQ
		(void)devChan; (void)onoff;
		tmp.sprintf("Writing to fake DO: %s data: %s", devChan.toUtf8().constData(), onoff ? "line_hi" : "line_lo");
        Debug() << tmp;
		return true;
#else
        const char *callStr = "";
		const double t0 = getTime();
		
        // Task parameters
        int      error = 0;
        char        errBuff[2048];
		DAQTaskDesc dtd = FindDOHandle(devChan);
        TaskHandle   & taskHandle (dtd.taskHandle);
		bool dontClose = false;		
        
        // Write parameters
        uint32      w_data [1];

		if (!taskHandle) {
			ClearDOHandle(devChan);
			
			// Create Digital Output (DO) Task and Channel
			DAQmxErrChk (DAQmxCreateTask ("", &taskHandle));
			DAQmxErrChk (DAQmxCreateDOChan(taskHandle,devChan.toUtf8().constData(),"",DAQmx_Val_ChanPerLine));
		}

        // Start Task (configure port)
        //DAQmxErrChk (DAQmxStartTask (taskHandle));

        //  Only 1 sample per channel supported for static DIO
        //  Autostart ON

        if (!onoff) 
            w_data[0] = 0x0;
        else 
            w_data[0] = 0x1;


        DAQmxErrChk (DAQmxWriteDigitalScalarU32(taskHandle,1,DAQ_TIMEOUT,w_data[0],NULL));
		
		activeDOHandles[devChan] = dtd;    dontClose = true && !closeDevice;

		tmp.sprintf("Writing to DO: %s data: 0x%X took %f secs", devChan.toUtf8().constData(),(unsigned int)w_data[0],getTime()-t0);		
        Debug() << tmp;


    Error_Out:

        if (DAQmxFailed (error))
            DAQmxGetExtendedErrorInfo (errBuff, 2048);

        if (taskHandle != 0 && !dontClose)
            {
                DAQmxStopTask (taskHandle);
                DAQmxClearTask (taskHandle);
				taskHandle = 0;
				activeDOHandles.remove(devChan);
            }

        if (error) {
            QString e;
            e.sprintf("DAQmx Error %d: %s", error, errBuff);
            if (!noDaqErrPrint) 
                Error() << e;
			return false;
        }
		return true;
#endif
    }
		
	bool WriteAO(const QString & devChan, double volts)
    {
        QString tmp;
#ifdef FAKEDAQ
		(void)devChan; (void)volts;
		tmp.sprintf("Writing to fake AO: %s data: %f", devChan.toUtf8().constData(), volts);
        Debug() << tmp;
		return true;
#else
        const char *callStr = "";
		const double t0 = getTime();
		// Task parameters
        int      error = 0;
        char        errBuff[2048];
		DAQTaskDesc dtd = FindAOHandle(devChan);
        TaskHandle   & taskHandle (dtd.taskHandle);
		bool dontClose = false;
		
		if (!taskHandle || volts > dtd.maxv || volts < dtd.minv) {
			ClearAOHandle(devChan);
			static bool didProbe = false;
			static DeviceRangeMap aoRanges;
			
			if (!didProbe) {
				aoRanges = ProbeAllAORanges();
				didProbe = true;
			}
			double & minv = (dtd.minv), & maxv(dtd.maxv);	minv = -5.; maxv = 5.;
			
			bool foundRange = false;
			
			for (DeviceRangeMap::const_iterator it = aoRanges.begin(); it != aoRanges.end(); ++it) {
				const Range & r = it.value();
				if (devChan.startsWith(it.key()) && volts <= r.max && volts >= r.min)
					if (!foundRange || r.max-r.min < maxv-minv)
						minv = r.min, maxv = r.max, foundRange = true;
			}
					
			// Create Digital Output (DO) Task and Channel
			DAQmxErrChk (DAQmxCreateTask ("", &taskHandle));
			DAQmxErrChk (DAQmxCreateAOVoltageChan(taskHandle,devChan.toUtf8().constData(),"",minv,maxv,DAQmx_Val_Volts,NULL));
			
			// Start Task (configure port)
			//DAQmxErrChk (DAQmxStartTask (taskHandle));			
		} 
			
        //  Autostart ON				
        DAQmxErrChk (DAQmxWriteAnalogScalarF64(taskHandle,1,DAQ_TIMEOUT,float64(volts),NULL));
		
		activeAOHandles[devChan] = dtd;		dontClose = true;
		
        tmp.sprintf("Writing to AO: %s data: %f took %f secs", devChan.toUtf8().constData(),volts,getTime()-t0);
        Debug() << tmp;

		
    Error_Out:
		
        if (DAQmxFailed (error))
            DAQmxGetExtendedErrorInfo (errBuff, 2048);
		
        if (taskHandle != 0 && !dontClose)
		{
			DAQmxStopTask (taskHandle);
			DAQmxClearTask (taskHandle);
			taskHandle = 0;
			activeAOHandles.remove(devChan);
		}
		
        if (error) {
            QString e;
            e.sprintf("DAQmx Error %d: %s", error, errBuff);
            if (!noDaqErrPrint) 
                Error() << e;
			return false;
        }
		return true;
#endif
    }
	


    TermConfig StringToTermConfig(const QString & txt) 
    {
        if (!txt.compare("RSE", Qt::CaseInsensitive))
            return RSE;
        else if (!txt.compare("NRSE", Qt::CaseInsensitive))
            return NRSE;
        else if (!txt.compare("Differential", Qt::CaseInsensitive) 
                 || !txt.compare("Diff", Qt::CaseInsensitive) )
            return Diff;
        else if (!txt.compare("PseudoDifferential", Qt::CaseInsensitive) 
                 || !txt.compare("PseudoDiff", Qt::CaseInsensitive) )
            return PseudoDiff;
        return Default;       
    }

    QString TermConfigToString(TermConfig t)
    {
        switch(t) {
        case RSE: return "RSE";
        case NRSE: return "NRSE";
        case Diff: return "Differential";
        case PseudoDiff: return "PseudoDifferential";
        default: break;
        }
        return "Default";
    }

} // end namespace DAQ


//-- #pragma mark Windows Hacks

///-- below is a bunch of ugly hacks for windows only to not have this .EXE depend on NI .DLLs!  

#if defined(Q_OS_WIN) && defined(HAVE_NIDAQmx)
#include <windows.h>

namespace DAQ {
	static HMODULE module = 0;
	
	bool Available(void) {
		static bool tried = false;
		//bool hadNoModule = !module;
		if (!module && !tried && !(module = LoadLibraryA("nicaiu.dll")) ) {
			//Warning() << "Could not find nicaiu.dll, NI functions disabled!";
			tried = true;
			return false;
		} else if (tried) return false;
		//if (hadNoModule)
		//	Log() << "Found and dynamically loaded NI Driver DLL: nicaiu.dll";
		return true;
	}
	
	template <typename T> void tryLoadFunc(T * & func, const char *funcname) {
		if (!func && Available()) {
			func = reinterpret_cast<T *> (GetProcAddress( module, funcname ));
			if (!func) {
				//Warning() << "Could not find the function " << funcname << " in nicaiu.dll, NI functionality may fail!";
				return;				
			}
		}
	}
}

extern "C" {	
	//*** Set/Get functions for DAQmx_Dev_DO_Lines ***
	int32 __CFUNC DAQmxGetDevDOLines(const char device[], char *data, uInt32 bufferSize) {
		static int32 (__CFUNC *func)(const char *, char *, uInt32) = 0;
		DAQ::tryLoadFunc(func, "DAQmxGetDevDOLines");
		//Debug() << "DAQmxGetDevDOLines called";
		if (func) return func(device, data, bufferSize);
		return DAQmxErrorRequiredDependencyNotFound;
	}
	
	int32 __CFUNC DAQmxWriteDigitalScalarU32   (TaskHandle taskHandle, bool32 autoStart, float64 timeout, uInt32 value, bool32 *reserved) {
		static int32 (__CFUNC *func) (TaskHandle, bool32, float64, uInt32, bool32 *) = 0;
		DAQ::tryLoadFunc(func, "DAQmxWriteDigitalScalarU32");
		//Debug() << "DAQmxWriteDigitalScalarU32 called";
		if (func) return func(taskHandle, autoStart, timeout, value, reserved);
		return DAQmxErrorRequiredDependencyNotFound;
	}

	int32 __CFUNC  DAQmxStopTask (TaskHandle taskHandle) {
		static int32 (__CFUNC  *func) (TaskHandle) = 0;
		DAQ::tryLoadFunc(func, "DAQmxStopTask");
		Debug() << "DAQmxStopTask called";
		if (func) return func(taskHandle);
		return DAQmxErrorRequiredDependencyNotFound;
	}

	int32 __CFUNC  DAQmxClearTask (TaskHandle taskHandle) {
		static int32 (__CFUNC  *func) (TaskHandle) = 0;
		DAQ::tryLoadFunc(func, "DAQmxClearTask");
		//Debug() << "DAQmxClearTask called";
		if (func) return func(taskHandle);
		return DAQmxErrorRequiredDependencyNotFound;
	}
	
	int32 __CFUNC DAQmxCreateDOChan (TaskHandle taskHandle, const char lines[], const char nameToAssignToLines[], int32 lineGrouping) {
		static int32 (__CFUNC *func)(TaskHandle, const char *, const char *, int32 lineGrouping) = 0;
		DAQ::tryLoadFunc(func, "DAQmxCreateDOChan");
		//Debug() << "DAQmxCreateDOChan called";
		if (func) return func(taskHandle,lines,nameToAssignToLines,lineGrouping);
		return DAQmxErrorRequiredDependencyNotFound;
	}

	int32 __CFUNC     DAQmxGetExtendedErrorInfo (char errorString[], uInt32 bufferSize) {
		static int32 (__CFUNC *func) (char *, uInt32) = 0;
		DAQ::tryLoadFunc(func, "DAQmxGetExtendedErrorInfo");
		//Debug() << "DAQmxGetExtendedErrorInfo called";
		if (func) return func(errorString,bufferSize);
		strncpy(errorString, "DLL Missing", bufferSize);
		return DAQmxSuccess;		
	}

	int32 __CFUNC     DAQmxCreateTask          (const char taskName[], TaskHandle *taskHandle) {
		static int32 (__CFUNC *func) (const char *, TaskHandle *) = 0;
		static const char * const fname = "DAQmxCreateTask";
		DAQ::tryLoadFunc(func, fname);
		//Debug() << fname << " called";
		if (func) return func(taskName,taskHandle);
		return DAQmxErrorRequiredDependencyNotFound;				
	}

	int32 __CFUNC DAQmxGetDevAIMaxMultiChanRate(const char device[], float64 *data) {
		static int32 (__CFUNC *func)(const char *, float64 *) = 0;
		static const char * const fname = "DAQmxGetDevAIMaxMultiChanRate";
		DAQ::tryLoadFunc(func, fname);
		//Debug() << fname << " called";
		if (func) return func(device,data);
		return DAQmxErrorRequiredDependencyNotFound;				
	}
	
	int32 __CFUNC DAQmxGetDevAISimultaneousSamplingSupported(const char device[], bool32 *data) {
		static int32 (__CFUNC *func)(const char *, bool32 *) 	 = 0;
		const char *fname = "DAQmxGetDevAISimultaneousSamplingSupported";
		DAQ::tryLoadFunc(func, fname);
		//Debug() << fname << " called";
		if (func) return func(device,data);
		return DAQmxErrorRequiredDependencyNotFound;						
	}
	
	int32 __CFUNC DAQmxGetDevAIMaxSingleChanRate(const char device[], float64 *data) {
		static int32 (__CFUNC *func)(const char *, float64 *) 	 = 0;
		const char *fname = "DAQmxGetDevAIMaxSingleChanRate";
		DAQ::tryLoadFunc(func, fname);
		//Debug() << fname << " called";
		if (func) return func(device,data);
		return DAQmxErrorRequiredDependencyNotFound;						
	}
	
	int32 __CFUNC DAQmxGetDevAIPhysicalChans(const char device[], char *data, uInt32 bufferSize) {
		static int32 (__CFUNC *func)(const char *, char *, uInt32) 	 = 0;
		const char *fname = "DAQmxGetDevAIPhysicalChans";
		DAQ::tryLoadFunc(func, fname);
		//Debug() << fname << " called";
		if (func) return func(device,data,bufferSize);
		return DAQmxErrorRequiredDependencyNotFound;								
	}
	
	int32 __CFUNC DAQmxGetDevAOVoltageRngs(const char device[], float64 *data, uInt32 arraySizeInSamples) {
		static int32 (__CFUNC *func)(const char *, float64 *, uInt32) 	 = 0;
		const char *fname = "DAQmxGetDevAOVoltageRngs";
		DAQ::tryLoadFunc(func, fname);
		//Debug() << fname << " called";
		if (func) return func(device,data,arraySizeInSamples);
		return DAQmxErrorRequiredDependencyNotFound;										
	}

	int32 __CFUNC DAQmxGetDevAIVoltageRngs(const char device[], float64 *data, uInt32 arraySizeInSamples) {
		static int32 (__CFUNC *func)(const char *, float64 *, uInt32) 	 = 0;
		const char *fname = "DAQmxGetDevAIVoltageRngs";
		DAQ::tryLoadFunc(func, fname);
		//Debug() << fname << " called";
		if (func) return func(device,data,arraySizeInSamples);
		return DAQmxErrorRequiredDependencyNotFound;												
	}
	
	int32 __CFUNC DAQmxGetDevAIMinRate(const char device[], float64 *data) {
		static int32 (__CFUNC *func)(const char *, float64 *) 	 = 0;
		const char *fname = "DAQmxGetDevAIMinRate";
		DAQ::tryLoadFunc(func, fname);
		//Debug() << fname << " called";
		if (func) return func(device,data);
		return DAQmxErrorRequiredDependencyNotFound;														
	}

	int32 __CFUNC DAQmxGetDevAOPhysicalChans(const char device[], char *data, uInt32 bufferSize) {
		static int32 (__CFUNC *func)(const char *, char  *, uInt32) 	 = 0;
		const char *fname = "DAQmxGetDevAOPhysicalChans";
		DAQ::tryLoadFunc(func, fname);
		//Debug() << fname << " called";
		if (func) return func(device,data,bufferSize);
		return DAQmxErrorRequiredDependencyNotFound;																
	}

	int32 __CFUNC DAQmxGetDevProductType(const char device[], char *data, uInt32 bufferSize) {
		static int32 (__CFUNC *func)(const char *, char  *, uInt32) 	 = 0;
		const char *fname = "DAQmxGetDevProductType";
		DAQ::tryLoadFunc(func, fname);
		//Debug() << fname << " called";
		if (func) return func(device,data,bufferSize);
		return DAQmxErrorRequiredDependencyNotFound;																		
	}


}
#else
namespace DAQ {
	bool Available() { return true; /* emulated, but available! */ }
}
#endif
