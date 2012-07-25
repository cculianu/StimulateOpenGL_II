function g = AddFrame(g,frame)
   if (~isnumeric(frame)),
       error('Second argument to addFrame must be numeric array!');
   end;
   GifWriterMex('addFrame', g.handle, frame);
end
