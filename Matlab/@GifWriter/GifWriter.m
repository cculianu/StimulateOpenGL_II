%    myobj = GifWriter(outputfilename)
%
%                Constructor.  Constructs a new instance of a @GifWriter 
%                and opens outputfilename for writing.  Call AddFrame
%                to add frames to the animated GIF and Finalize to finsh 
%                saving the animated GIF.
function [g] = GifWriter(outfile)

   g = struct;
   g.handle = GifWriterMex('create',outfile);
   g = class(g, 'GifWriter');
    
end
