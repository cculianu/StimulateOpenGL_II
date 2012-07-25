% SYNOPSIS
%     The GifWriter class provides a simple mechanism for creating
%     StimulateOpenGL_II 'Movie' plugin movies.  
%
%     Currently the movies are always animated GIFs where each frame is
%     grayscale (uses 256-grayscale 8-bit palette -- this is sufficient for
%     StimulateOpenGL_II, which uses grayscale frames anyway).
%  
%     Use AddFrame to add frames to the movie, and finally call Finalize to
%     finish saving the movie and close the file.
%
% FUNCTION REFERENCE
%    myobj = GifWriter(outputfilename)
%
%                Constructor.  Constructs a new instance of a @GifWriter 
%                and opens outputfilename for writing.  Call AddFrame
%                to add frames to the animated GIF and Finalize to finsh 
%                saving the animated GIF.
%
%    myobj = AddFrame(myobj, frame)
%
%                Takes 2 arguments, the gifwriter class and a frame, an M x
%                N matrix of (0,1) intensity (color) values.  Adds the
%                frame to the animated GIF.
%
%   myobj = Finalize(myobj)
%
%                This function must be called to end output to the GIF file
%                and save unsaved data.  After this function is called, the
%                output GIF is complete.
