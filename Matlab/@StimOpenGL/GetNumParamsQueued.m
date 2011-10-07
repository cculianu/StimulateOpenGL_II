%    intval = GetNumParamsQueued(myobj, pluginname)
%
%                Returns the size of the realtime parameter queue. The
%                realtime parameter queue is used when you have set a
%                reatime parameter history to play back using
%                SetParamHistory.m. The next time the plugins is run, the
%                parameter history entries in the history are enqueued and
%                played back in FIFO order.  This function returns the
%                number of parameter history entries still remaining in the
%                queue, or 0 if no parameter history is used (and/or all
%                param histories have already been dequeued/applied).  See
%                SerParamHistory.m and GetParamHistory.m, as well as
%                SetParams.m.
function [ret] = GetNumParamsQueued(s, pluginname)

    if (~ischar(pluginname)), error('Parameter 2 to GetNumParamsQueued needs to be a plugin name!'); end;
    ret = sscanf(DoQueryCmd(s, sprintf('NUMPARAMSQUEUED %s',pluginname)), '%d');
