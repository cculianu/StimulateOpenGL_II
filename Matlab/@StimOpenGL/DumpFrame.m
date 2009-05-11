%    imgdata = DumpFrame(myobj, frameNumber)
%
%                Retrieve frame number 'frameNumber' from the currently
%                running plugin.  The returned matrix is a matrix of
%                unsigned chars with dimensions: 3 x width x height (width
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
%                specifying sequential frameNumbers, eg: DumpFrame(myObj,
%                100), DumpFrame(myObj, 101), DumpFrame(myObj, 102), etc.  
function [imgdat] = DumpFrame(s, frameNum)
    if (isempty(Running(s))),
        imgdat = [];
        error('Cannot call DumpFrame when a plugin isn''t running!  Call Start() first!');
        return;
    end;
    if (~isnumeric(frameNum)),
        error('Frame number parameter needs to be numeric');
    end;
   
    if (~IsPaused(s)),
        warning('Plugin was not paused -- pausing plugin in order to complete DumpFrame command...');
        Pause(s);
    end;
    
    ChkConn(s);
    w=GetWidth(s);
    h=GetHeight(s);
    CalinsNetMex('sendString', s.handle, sprintf('getframe %d UNSIGNED BYTE\n', frameNum));
    line = CalinsNetMex('readLine', s.handle);
    if (strfind(line, 'BINARY')~=1),
        error('Expected BINARY DATA line, didn''t get it');
    end;   
    imgdat=CalinsNetMex('readMatrix', s.handle, 'uint8', [3 w h]);


    