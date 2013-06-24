%    myobj = AddFrame(myobj, frame[, compressionLevel])
%
%                Takes 2 arguments, the gifwriter class and a frame, an M x
%                N matrix of (0,1) intensity (color) values.  Adds the
%                frame to the movie file.  The optional third argument
%                defines the compression level to use for the internal zLib
%                compressor for the frame.  Valid values are 0 thru 9,
%                where 0 is no compression, 1 is fast compression, and 9 is
%                slower maximal compression. Default is 9, or the value 
%                specified in the FastMovieWriter constructer 
%                (see FastMovieWriter function reference).
function g = AddFrame(varargin)
   if (nargin < 2),
       error('AddFrame takes at least two arguments!');
   end;
   g = varargin{1};
   frame = varargin{2};
   clevel = g.clevel;
   if (nargin > 2),
       clevel = varargin{3};
   end;
   if (~isnumeric(frame)),
       error('Second argument to addFrame must be numeric array!');
   end;
   FastMovieWriterMex('addFrame', g.handle, frame, clevel);
end
