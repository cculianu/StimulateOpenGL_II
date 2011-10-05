%    params = GetParamHistory(myobj, 'PluginName')
%
%       TODO: Fill in description
function [ret] = GetParamHistory(s, plugin)

    if (~ischar(plugin)), error ('Plugin argument (argument 2) must be a string'); end;
    
    ret = '';
    res = DoGetResultsCmd(s, sprintf('GETPARAMHISTORY %s', plugin));
    for i=1:length(res),
        ret = sprintf('%s%s\n',ret,res{i});
    end;
    
    