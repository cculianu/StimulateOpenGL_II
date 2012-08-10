% SYNOPSIS
%     The FastMovieWriter class provides a simple mechanism for creating
%     StimulateOpenGL_II 'Movie' plugin movies.  This format, the .fmv
%     format is the preferred format to use with the StimGL II `Movie'
%     plugin, as it is the fastest and most compact format.
%
%     Currently the movies are a simple format where each frame is
%     grayscale (uses 256-grayscale 8-bit palette -- this is sufficient for
%     StimulateOpenGL_II, which uses grayscale frames anyway).
%  
%     Use AddFrame to add frames to the movie, and finally call Finalize to
%     finish saving the movie and close the file.
%
% FUNCTION REFERENCE
%    myobj = FastMovieWriter(outputfilename)
%
%                Constructor.  Constructs a new @FastMovieWriter instance 
%                and opens outputfilename for writing.  Call AddFrame
%                to add frames to the output file and Finalize to finsh 
%                saving the animated file. Preferably you should name the
%                output files using a .fmv filename extension, but this 
%                is not enforced.
%
%    myobj = AddFrame(myobj, frame)
%
%                Takes 2 arguments, the gifwriter class and a frame, an M x
%                N matrix of (0,1) intensity (color) values.  Adds the
%                frame to the movie.
%
%   myobj = Finalize(myobj)
%
%                This function must be called to end output to the file
%                and save unsaved data.  After this function is called, the
%                output movie is complete.
