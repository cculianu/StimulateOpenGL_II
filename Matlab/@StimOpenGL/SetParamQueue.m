%    myobj = SetParamQueue(myobj, 'PluginName', cell_array_of_param_queue_struct)
%
%                Enqueue a set of parameters for a particular plugin, to be
%                applied at particular frame numbers.  
%
%                The third argument to this function describes when
%                and which parameters are to be applied during the course
%                of a plugin's run.  Parameters are applied *BEFORE* the
%                frame number in question is drawn to the screen, as if 
%                SetParams had been called for that frame.  As such, all
%                parameters (not just those that you intend to change) need
%                to be present in the params_struct for the frame in
%                question specified to this function.
%
%                The format for the third argument is a cell array of 
%                structs, each struct should have the following fields: 
%                  .frameNum   -- When to apply these parameters
%                  .params     -- A param_struct.  See `SetParams` 
%                                 for a description of what a param_struct 
%                                 should contain. parms will be applied 
%                                 right before frame number frameNum
%                                 is drawn.
%
%                In other words, the frameNum specifies before which frame
%                to apply `params'.  `params' is identical to the struct
%                required for the SetParams.m call, and conceptually you
%                can think of it as: "for frame `frameNum', SetParams will
%                be called applying argument `params'".
%
%                Use this mechanism for fine-grained/frame-level control of
%                a plugin.  For example, one can use this mechanism in
%                addition to the setAOlines / setDOLines plugin parameters
%                to set specific DAQ lines as specific frames are
%                drawn.
%             
%                Note that this function requires that params be specified
%                for frameNum=0 as the first element of the passed-in cell
%                aray, which will be the params applied to the plugin when
%                it is started.  Not specifying params for frameNum=0 is
%                not supported and will either return an error or not
%                function correctly.  Also, combining the use of SetParams
%                or SetParamHistory with this function is not supported and
%                can generate unexpected results.
%
%                EXAMPLE USAGE:
%
%                % ... [ CODE THAT SETS UP myparams HERE ] ...
%
%                myparams.setDOstates = '1 1 1 1 1 0 1 1'; 
%                mystruct.frameNum = 0;
%                mystruct.params = myparams;
%
%                myparams.setDOstates = '0 0 0 0 0 1 0 0';
%                mystruct2.frameNum = 45;
%                mystruct2.params = myparams;
%
%                 
%                SetParamQueue(s, 'MovingObjects', ...
%                                                { mystruct, mystruct2 } );
%
%                The above sets up a param queue for frame 0 (required) and
%                frame 45.  On frame 45, parameter 'setDOstates' is to be
%                altered (which would have the effect of changing the DO
%                lines after frme 45 is drawn).
%
%                Thus, it's possible to enqueue changes to parameters as a
%                plugin runs.  This facility is particularly useful for
%                DO/AO, but can be useful for other plugin parameters as
%                well.
function [s] = SetParamQueue(s, plugin, param_array)
    if (~ischar(plugin) | ~iscell(param_array) | length(param_array) < 1 | ~isstruct(param_array{1})),
        error('Arguments to SetParamQueue(StimOpemGLOBJ, plugin_string, cell_array_of_param_queue_structs)');
    end;
    f0s=param_array{1};
    if (~isstruct(f0s) | ~isfield(f0s,'frameNum') | ~isfield(f0s,'params') | ~isnumeric(f0s.frameNum) | ~isstruct(f0s.params)),
        error('The 3rd argument''s array must contain a cell array of structs whose fields are "frameNum" (a number) and "params" (a struct)');
    end;
    if ( f0s.frameNum ~= 0 ),
        error('The array of structs specified MUST have its first element specify frameNum=0!');
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
    
    