%    myobj = SetParamQueue(myobj, 'PluginName', cell_array_of_param_queue_struct)
%
%                Enqueue a set of parameters for a particular plugin, to be
%                applied at particular frame numbers.  See `SetParams` 
%                for a description of what a param_struct should contain.
%
%                The third argument to this function describes when
%                and which parameters are to be applied during the course
%                of a plugin's run.  Parameters are applied *BEFORE* the
%                frame number in question is drawn to the screen, as if 
%                SetParams had been called for that frame.  As such, all
%                parameters (not just those that you intent to change) need
%                to be present in the params_struct for the frame in
%                question specified to this function.
%
%                Use this mechanism for fine-grained/frame-level control of
%                a plugin.  For example, in the MovingObjects plugin, use
%                of this mechanism can be combined with options such as
%                objStepwiseVel to dynamically change the velocity of
%                on-screen objects, as the plugin runs.
%
%                The cell_array_of_param_queue_struct specified should
%                contain a struct with two fields: `frameNum' and `params'.
%                The frameNum specifies before which frame to apply
%                `params'.  `params' is identical to the struct required
%                for the SetParams.m call, and conceptually you can think
%                of it as: "for _frameNum_, SetParams will be called applying
%                _params_".
%             
%                Note that this function requires that params be specified
%                for frameNum=0 as the first element of the passed-in cell
%                aray, which will be the params applied to the plugin when
%                it is started.  Not specifying params for frameNum=0 is
%                not supported and will either return an error or not
%                function correctly.  Also, combining the use of SetParams
%                or SetParamHistory with this function is not supported and
%                can generate unexpected results.
function [s] = SetParamQueue(s, plugin, param_array)
    if (~ischar(plugin) | ~iscell(param_array) | length(param_array) < 1 | ~isstruct(param_array{1})),
        error('Arguments to SetParamQueue(StimOpemGLOBJ, plugin_string, cell_array_of_param_queue_structs)');
    end;
    f0s=param_array{1};
    if (~isstruct(f0s) | ~isfield(f0s,'frameNum') | ~isfield(f0s,'params') | ~isnumeric(f0s.frameNum) | ~isstruct(f0s.params) | f0s.frameNum ~= 0),
        error('First element of 3rd argument''s array must contain the fields "frameNum" and "params" and frameNum must be set to 0!');
    end;
    
    ChkConn(s);

    % force call to SetParams for frame 0
    SetParams(s, plugin, f0s.params);

    CalinsNetMex('sendString', s.handle, sprintf('SETPARAMQUEUE %s\n', plugin));
    ReceiveREADY(s, sprintf('SETPARAMQUEUE %s', plugin));
    for h=1:length(param_array),
        pq = param_array{h};
        if (~isstruct(pq) | ~isfield(pq,'frameNum') | ~isfield(pq,'params')),
            error('For element %d of the cell array passed as arg 3, the contents of the cell are invalid.  Please pass a cell array of structs containing: "frameNum" and "params" as fields.', h);
        end;
        frameNum=pq.frameNum;
        params=pq.params;
        if (~isnumeric(frameNum) | ~isstruct(params)),
            error('Element %d of passed-in 3rd arg needs to contain a struct with "frameNum" (a number) and "params" (a struct) as its two fields.',h);
        end;
        line = sprintf('frameNum %d {\nPARAMS {\n',frameNum);
        CalinsNetMex('sendString', s.handle, line);
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
        line = sprintf('}\nCHANGED { } }\n');
        CalinsNetMex('sendString', s.handle, line);
    end;
    % end with blank line
    CalinsNetMex('sendString', s.handle, sprintf('\n'));
    ReceiveOK(s);
    
    