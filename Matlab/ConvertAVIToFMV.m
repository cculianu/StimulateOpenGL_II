function [] = ConvertAVIToFMV(infile,outfile)

    mm = mmreader(infile)
    nFrames = mm.NumberOfFrames;
    fmv = FastMovieWriter(outfile);
    idx = 1;
    while (idx <= nFrames)
        endidx = idx + 9;
        if (endidx > nFrames), endidx = nFrames; end;
        avi = aviread(infile,idx:endidx);
        for i=1:length(avi),
            %fr = avi(i);
            %cdata = fr.cdata';
            %cmap = fr.colormap*255;
            %outframe = zeros(size(cdata,1),size(cdata,2));
            AddFrame(fmv,avi(i).cdata');
        end;
        idx = endidx+1;
        disp(sprintf('processing frames: %d out of %d',idx-1,nFrames));
    end;
    Finalize(fmv);


end
