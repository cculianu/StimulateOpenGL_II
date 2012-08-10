% Takes 2 arguments, the gifwriter class and a frame, an M x N matrix of 
% (0,1) intensity (color) values.  Adds the frame to the movie file.
%
function g = AddFrame(g,frame)
   if (~isnumeric(frame)),
       error('Second argument to addFrame must be numeric array!');
   end;
   FastMovieWriterMex('addFrame', g.handle, frame);
end
