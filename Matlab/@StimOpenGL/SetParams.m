%    myobj = SetParams(myobj, 'PluginName', params_struct)
%
%                Set the configuration parameters for a particular plugin.
%                Configuration parameters are a struct of name/value pairs
%                that plugins use to affect their runtime operation.  The
%                structure specified here will completely replace the
%                existing struct (if any) that the plugin was using for its
%                configuration parameters.  Note that each plugin maintains
%                its own set of configuration parameters, hence the need to
%                call SetParams specifying the plugin name.  As of version
%                2011.09.25, this call can now be made while the plugin in 
%                question is running; the parameters are applied immediately 
%                after the current (or next) frame is drawn, during the
%                'after VSync' period.  Note that if the parameters are 
%                rejected/invalid, this function will still return success,
%                however the plugin will print a message on the GUI console 
%                and revert to its previous (working) parameters if it was 
%                running, or if it was not running, it will just fail on 
%                start with the new parameters.
function [s] = SetParams(s, plugin, params)
    if (~ischar(plugin) | ~isstruct(params)),
        error('Arguments to stop are Stop(StimOpemGLOBJ, plugin_string, params_struct)');
    end;
    ChkConn(s);
%
%   Note as of version 2011.9.25 we support setting the params at runtime
%   so we removed this guard.
%
%    running = Running(s);
%    if (strcmpi(running, plugin)),
%        error('Cannot set params for a plugin while it''s running!  Stop() it first!');
%    end;
%
    CalinsNetMex('sendString', s.handle, sprintf('SETPARAMS %s\n', plugin));
    ReceiveREADY(s, sprintf('SETPARAMS %s', plugin));
    names = fieldnames(params);
    for i=1:length(names),
        f = params.(names{i});
        if (isnumeric(f)),
            line = sprintf('%g ', f); % possibly vectorized print
            line = sprintf('%s = %s\n', names{i}, line);
        elseif (ischar(f)),
            line = sprintf('%s = %s\n', names{i}, f);
        else 
            error('Field %s must be numeric scalar or a string', names{i});
        end;
        CalinsNetMex('sendString', s.handle, line);
    end;
    % end with blank line
    CalinsNetMex('sendString', s.handle, sprintf('\n'));
    ReceiveOK(s);
    
    