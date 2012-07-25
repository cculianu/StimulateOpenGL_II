% Takes 2 arguments, the gifwriter class and a frame, an M x N matrix of 
% (0,1) intensity (color) values.  Adds the frame to the animated GIF.
%
function g = AddFrame(g,frame)
   if (~isnumeric(frame)),
       error('Second argument to addFrame must be numeric array!');
   end;
   GifWriterMex('addFrame', g.handle, frame);
end
