%    params = SetParamHistory(myobj, 'PluginName', string)
%
%       TODO: Fill in description
function [s] = SetParamHistory(s, plugin, h)

    if (~ischar(plugin)), error ('Plugin argument (argument 2) must be a string'); end;    
    if (~ischar(h)), error ('Plugin argument (argument 3) must be a string'); end;
    r = strfind(h, 'frameNum 0');      % must begin with frameNum 0...
    if (isempty(r) || r(1) ~= 1),
        error('Passed-in parameter history string appears invalid.');
    end;
    r2 = strfind(h, sprintf('\n\n'));  % must end with blank line, if not append one
    if (isempty(r2) || r2(1) ~= length(h)-1),
        h = sprintf('%s\n',h); % make sure it ends in a blank line!
    end;
    
    CalinsNetMex('sendString', s.handle, sprintf('SETPARAMHISTORY %s\n', plugin));
    ReceiveREADY(s, sprintf('SETPARAMHISTORY %s', plugin));
    CalinsNetMex('sendString', s.handle, h);
    ReceiveOK(s);
    
    