function g = Finalize(g)
        if (g.handle >= 0),        
            GifWriterMex('finalize', g.handle);
            GifWriterMex('destroy', g.handle);
            g.handle = -1;
        end;
        
 end
