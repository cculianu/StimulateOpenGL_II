%    myobj = FastMovieWriter(outputfilename)
%
%                Constructor.  Constructs a new @FastMovieWriter instance 
%                and opens outputfilename for writing.  Call AddFrame
%                to add frames to the output file and Finalize to finsh 
%                saving the animated file. Preferably you should name the
%                output files using a .fmv filename extension, but this 
%                is not enforced.
function [g] = FastMovieWriter(outfile)

   g = struct;
   g.handle = FastMovieWriterMex('create',outfile);
   g = class(g, 'FastMovieWriter');
    
end
