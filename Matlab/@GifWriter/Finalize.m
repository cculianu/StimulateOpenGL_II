% This function must be called to end output to the GIF file and save
% unsaved data.  After this function is called, the output GIF is complete.
function g = Finalize(g)
        if (g.handle >= 0),        
            GifWriterMex('finalize', g.handle);
            GifWriterMex('destroy', g.handle);
            g.handle = -1;
        end;
        
 end
