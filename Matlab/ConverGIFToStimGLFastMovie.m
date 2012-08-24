function [] = ConvertGIFToStimGLFastMovie(filename,out)
    tic;
    [f,m]=imread(filename, 'frames', 'all');
    for i=1:size(f,4), 
        a=f(:,:,1,i); 
        for x=1:size(a,1), 
            for y=1:size(a,2),
                idx=a(x,y);
                v=m(idx+1,:); 
                ii=uint8(sum(v)/3.0 * 255.0); 
                a(x,y)=ii; 
            end; 
        end;
        f(:,:,1,i) = a;
    end;
    fw=FastMovieWriter(out);
    for i=1:size(f,4), AddFrame(fw, f(:,:,1,i)'); end;
    Finalize(fw);
    toc
    