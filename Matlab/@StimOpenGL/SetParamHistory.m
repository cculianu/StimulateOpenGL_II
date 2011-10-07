%    params = SetParamHistory(myobj, 'PluginName', string)
%
%                Set the realtime parameter history for the next run of the
%                plugin.  After this call, the next time the plugin is run
%                it will 'play back' the set of parameters that are
%                encapsulated in the passed-in history.  That is, this
%                function sets the next run of the plugin to be in 'param
%                history playback' mode, and it tells the plugin which
%                parameter history to play back.
% 
%                Typically you pass the string returned from a previous
%                call to GetParamHistory.m.
%
%                The typical workflow for the realtime param updates would
%                be as follows:
%
%                1. Set initial parameters for a plugin either by calling
%                SetParams.m through this Matlab API (or by loading a param
%                file--hitting 'L' in the StimGL console window).
%
%                2. After the initial parameters are set, start the plugin
%                normally.
%
%                3. After the plugin runs for a few (or many) frames, call
%                SetParams.m again, passing new parameters to the plugin in
%                realtime.  The next frame rendered by the plugin after the
%                call will use the new parameters (an exception to this is
%                the CheckerFlicker plugin which has a large frame queue so
%                with CheckerFlicker param updates appear after a delay of
%                a dozen or so frames).
%
%                4. Save the realtime param history by calling
%                GetParamHistory (this function), and saving the returned
%                string.
%
%                5. Later, to recreate the exact set of frames that the
%                plugin rendered above, one can use the saved param history
%                string by calling SetParamHistory.m, passing it the param
%                history string, before starting the plugin.
%
%                Optionally, the DumpFrame.m mechanism can also benefit
%                from the saved parameter history for re-creating the exact
%                frames that were rendered during a plugin run.
%
%                (See also GetParamHistory.m).
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
    ChkConn(s);
    CalinsNetMex('sendString', s.handle, sprintf('SETPARAMHISTORY %s\n', plugin));
    ReceiveREADY(s, sprintf('SETPARAMHISTORY %s', plugin));
    CalinsNetMex('sendString', s.handle, h);
    ReceiveOK(s);
    
    