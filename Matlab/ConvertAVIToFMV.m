function [] = ConvertAVIToFMV(infile,outfile)

 
    %mm = mmreader(infile)
    %nFrames = mm.NumberOfFrames;
    info = aviinfo(infile)
    gscale = IsGScale8Bit(info);
    if (~gscale),
        warning('Input movie is not 8-bit indexed greyscale, conversion may be slow!');
    end;
    nFrames = info.NumFrames;
    fmv = FastMovieWriter(outfile);
    idx = 1;
    disp(sprintf('processing frames: %d out of %d',idx-1,nFrames));
    while (idx <= nFrames)
        endidx = idx + 9;
        if (endidx > nFrames), endidx = nFrames; end;
        avi = aviread(infile,idx:endidx);
        for i=1:length(avi),
            if (gscale), 
                AddFrame(fmv,uint8(avi(i).cdata')-1);
            else
                % convert to 8 bit grayscale!
                AddFrame(fmv,ConvertToGScale8Bit(info,avi(i)));
            end;
        end;
        idx = endidx+1;
        disp(sprintf('processing frames: %d out of %d',idx-1,nFrames));
    end;
    Finalize(fmv);


end

function [ret] = IsGScale8Bit(info)
    ret = 0;
    if (strcmp(info.ImageType,'indexed') && info.NumColormapEntries == 256),
        frame = aviread(info.Filename,1);
        if (~strcmp(class(frame.cdata), 'uint8')),
            ret = 0;
            return;
        end;
        cmap = uint8(frame.colormap*255);
        for i=1:size(cmap,1),
            r = cmap(i,1:3);
            if ( r(1) ~= i-1 || r(1) ~= r(2) || r(1) ~= r(3) || r(2) ~= r(3)),
                ret = 0;
                return;
            end;
        end;
        ret = 1;
    end;
    return;
end

function [frame] = ConvertToGScale8Bit(info, aviframe)
    if (info.NumColormapEntries == 0),
        cdata = double(aviframe.cdata);
        if (~strcmp(class(aviframe.cdata), 'uint8') || size(cdata,3)~= 3),
            error('AVI Frame is not RGB data of type uint8!');
        end;
        frame = zeros(size(cdata,2), size(cdata,1), 'uint8');
        for x=1:size(frame,1),
            for y=1:size(frame,2),
                rgb = [ cdata(y,x,1) cdata(y,x,2) cdata(y,x,3) ];
                avg = uint8(sum(rgb) / size(rgb,2));
                frame(x,y) = avg;
            end;
        end;
    else
        % indexed
        if (~strcmp(info.ImageType,'indexed') || info.NumColormapEntries ~= 256 || ~strcmp(class(aviframe.cdata),'uint8')),
            error('For now, ConverAVIToFMV only supports AVI movies with ImageType=indexed and with a colormap of size 256');
        end;
        cdata = aviframe.cdata';
        cmap = aviframe.colormap*255;
        if (size(cmap,1) ~= 256),
            error('Only 256-color colormapped frames supported');
        end;
        if (~strcmp(class(cdata), 'uint8')),
            error('Only uint8 type frames supported as input');
        end;
        frame = uint8(zeros(size(cdata,1), size(cdata,2)));
        for x=1:size(cdata,1),
            for y = 1:size(cdata,2),
                index = cdata(x,y);
                avg = uint8(sum(double(cmap(index))) / size(cmap,2));
                frame(x,y) = avg;
            end;
        end;
        
    end;
end
