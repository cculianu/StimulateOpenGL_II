%    myobj = GifWriter(outputfilename)
%
%                Constructor.  Constructs a new instance of a @GifWriter 
%                and opens outputfilename for writing.

function [g] = GifWriter(outfile)

   g = struct;
   g.handle = GifWriterMex('create',outfile);
   g = class(g, 'GifWriter');
    
end
