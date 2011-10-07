%    string = GetParamHistory(myobj, 'PluginName')
%
%                Retrieve the  realtime parameter history for the plugin.
%                The retrieved value is a large free-form string that is
%                intended to be human and machine readable which
%                encapsulates the history of parameters that a plugin used
%                as it ran. This realtime parameter history is appended-to
%                each time SetParams.m is called on a plugin that is
%                currently running.  That is, this history is a precise log
%                of which parameters were submitted to the plugin at what
%                time (frame number).  The realtime parameter history
%                returned is a long free-form string which can later be
%                passed to SetParamHistory.m in order to play-back a
%                previous realtime parameter-updated session (useful in
%                conjunction with DumpFrame.m to recreate the conditions of
%                an experiment that had its parameters updated using
%                realtime param updates).
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
%                (See also SetParamHistory.m).
function [ret] = GetParamHistory(s, plugin)

    if (~ischar(plugin)), error ('Plugin argument (argument 2) must be a string'); end;
    
    ret = '';
    res = DoGetResultsCmd(s, sprintf('GETPARAMHISTORY %s', plugin));
    for i=1:length(res),
        ret = sprintf('%s%s\n',ret,res{i});
    end;
    
    