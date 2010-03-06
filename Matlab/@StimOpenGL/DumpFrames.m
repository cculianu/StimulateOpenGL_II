%    imgdata = DumpFrames(myobj, frameNumber, count)
%
%                Retrieve count frames starting at 'frameNumber' from the currently
%                running plugin.  The returned matrix is a matrix of
%                unsigned chars with dimensions: 3 x width x height x count (width
%                and height are obtained from GetHeight and GetWidth method
%                calls).  Note that if frameNumber is in the past (that is,
%                lower than the current frameCount [see GetFrameCount]),
%                then the plugin may have to be restarted internally and
%                fast-forwarded to the specified frameNumber (a slow
%                operation).  Also note that if 'frameNumber' is some
%                number far in the future (much larger than frameCount) the
%                plugin will have to compute all the frames in between the
%                current frame and frameNumber (a slow operation).  By far
%                the slowest possible way to read frames is in reverse or
%                randomly, so avoid that usage pattern, if possible!
%                Optimal use of this function would be to call DumpFrame
%                specifying sequential frameNumbers, eg: DumpFrames(myObj,
%                100,5), DumpFrame(myObj, 105,4), DumpFrame(myObj, 109,7), etc.  
function [imgdat] = DumpFrames(s, frameNum, count)
    if (count > 240), 
        warning('More than 240 frames per DumpFrames call is not officially supported and may lead to low memory conditions!');
    end;
    plug=Running(s);
    if (isempty(plug)),
        imgdat = [];
        error('Cannot call DumpFrame when a plugin isn''t running!  Call Start() first!');
        return;
    end;
    if (~isnumeric(frameNum) || frameNum < 0),
        error('Frame number parameter needs to be a positive integer!');
    end;

    if (~IsPaused(s)),
        warning('Plugin was not paused -- pausing plugin in order to complete DumpFrame command...');
        Pause(s);
    end;
    pluginFrameNum = GetFrameCount(s);
    if (~isnumeric(count) | count <= 0),
        warning('Count specified is invalid, defaulting to 1');
        count = 1;
    end;
    if (frameNum <= pluginFrameNum),
        warning(sprintf('Frame count specified %d is <= the plugin''s current frame number of %d, restarting plugin (this is slow!!)',frameNum, pluginFrameNum));
        Stop(s);
        Start(s, plug);
    end;
    ChkConn(s);
    w=GetWidth(s);
    h=GetHeight(s);
    CalinsNetMex('sendString', s.handle, sprintf('getframe %d %d UNSIGNED BYTE\n', frameNum, count));
    line = CalinsNetMex('readLine', s.handle);
    if (strfind(line, 'BINARY')~=1),
        error('Expected BINARY DATA line, didn''t get it');
    end;
    imgdat=CalinsNetMex('readMatrix', s.handle, 'uint8', [3 w h count]);
    ReceiveOK(s);

    