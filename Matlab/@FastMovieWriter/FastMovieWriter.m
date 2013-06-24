%    myobj = FastMovieWriter(outputfilename)
%    myobj = FastMovieWriter(outputfilename,compression_level)
%
%                Constructor.  Constructs a new @FastMovieWriter instance 
%                and opens outputfilename for writing.  Call AddFrame
%                to add frames to the output file and Finalize to finsh 
%                saving the animated file. Preferably you should name the
%                output files using a .fmv filename extension, but this 
%                is not enforced.  The optional compression_level argument
%                specifies how hard to try and compress the movie frame. 
%                The default is 9 (maximal compression).  A value of 0
%                means no compression and is fast, especially for 
%                high-entropy frame data where compression is useless.  
%                Note that the AddFrame call itself can also take a 
%                compression_level argument as well, which overrides the 
%                default compression level set for the class.
function [g] = FastMovieWriter(varargin)

   if (nargin < 1),
       error 'Please pass at least 1 argument to FastMovieWriter';
   end;
   if (nargin > 1),
       clevel = varargin{2};
   else
       clevel = 9;
   end;
   outfile = varargin{1};
   g = struct;
   g.handle = FastMovieWriterMex('create',outfile);
   g.clevel = clevel;
   g = class(g, 'FastMovieWriter');
    
end
