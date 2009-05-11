%    plugname = Running(myobj)
%
%                Determing which plugin, if any, is currently active and
%                running.  An active plugin is one for which 'Start' was
%                called but 'Stop' has not yet been called, or which has
%                not terminated on its own (plugins may terminate on their
%                own at any time, but at the time of this writing no
%                existing plugin does so).  Returns the plugin name that is
%                running as a string, or the empty string if no plugins are
%                running.
function [ret] = Running(s)

    ChkConn(s);
    CalinsNetMex('sendString', s.handle, sprintf('RUNNING\n'));
    line = CalinsNetMex('readLine', s.handle);
    if (strfind(line, 'OK')==1), 
        ret = '';
        return;
    end;
    ret = line;
    ReceiveOK(s);


    
