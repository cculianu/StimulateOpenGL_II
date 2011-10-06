%    intval = GetNumParamsQueued(myobj, pluginname)
%
%                TODO: Description here.
function [ret] = GetNumParamsQueued(s, pluginname)

    if (~ischar(pluginname)), error('Parameter 2 to GetNumParamsQueued needs to be a plugin name!'); end;
    ret = sscanf(DoQueryCmd(s, sprintf('NUMPARAMSQUEUED %s',pluginname)), '%d');
