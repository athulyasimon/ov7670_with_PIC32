function client(port)
%   provides a menu for accessing PIC32 motor control functions
%
%   client_menu(port)
%
%   Input Arguments:
%       port - the name of the com port.  This should be the same as what
%               you use in screen or putty in quotes ' '
%
%   Example:
%       client('/dev/ttyUSB0') (Linux/Mac)
%       client('COM3') (PC)
%
%   For convenience, you may want to embed this in a script that
%   contains your port number
   
% Opening COM connection
if ~isempty(instrfind)
    fclose(instrfind);
    delete(instrfind);
end

fprintf('Opening port %s....\n',port);

% settings for opening the serial port. baud rate 230400, hardware flow control
% wait up to 120 seconds for data before timing out
mySerial = serial(port, 'BaudRate', 230400, 'FlowControl', 'hardware','Timeout',120); 
% opens serial connection
fopen(mySerial);
% closes serial port when function exits
clean = onCleanup(@()fclose(mySerial));                                 

has_quit = false;
% menu loop
while ~has_quit
    % display the menu options
    fprintf('l: read one byte    k: Take a new image    p: Display image    o: Turns on test pattern   i: Turns off test pattern    q: Quit\n');
    % read the users choice
    selection = input('Enter Command: ', 's');
     
    % send the command to the PIC32
    fprintf(mySerial,'%c\n',selection);

    
    % take the appropriate action
    switch selection
        case 'l'                              	% read one byte
            n = fscanf(mySerial,'%d');
            fprintf('Read: %d\n',n);
        case 'k'
            n = fscanf(mySerial,'%d');
            fprintf('Read: %d\n',n);
            fprintf('Image stored\n');
        case 'i'
            n = fscanf(mySerial,'%d');
            fprintf('Read: %d\n',n);
            fprintf('Test Pattern off\n');
        case 'o'
            n = fscanf(mySerial,'%d');
            fprintf('Read: %d\n',n);
            fprintf('Test Pattern on\n');
        case 'p'	
            close all;
	    	pic_array = zeros(145,220);
			for j=1:145
				for i=1:220
 					pic_array(j,i) = fscanf(mySerial,'%d');
				end
			end
            fid = 1;                             % Insert true ‘fid’
			fprintf(fid, [repmat(' %d ', 1, 100) '\n'], pic_array'); %formats nicely
            
            image_array1 = [];
            image_array2 = [];
            for row=1:145
                pic_row1 = pic_array(row, 3:2:end);
                pic_row2 = pic_array(row, 2:2:end-1);
                if mean(pic_row1) > mean(pic_row2)
                    pic_rowa = pic_row2;
                    pic_rowb = pic_row1;
                else
                    pic_rowb = pic_row2;
                    pic_rowa = pic_row1;
                end
                  
                image_array1 = [image_array1; pic_rowa]; %y component
                image_array2 = [image_array2; pic_rowb]; %uv component
            end
            
            
            image_rgb = [];
            for row = 1:size(image_array1,1)
                pixel_num = 1;
                for col = 1:size(image_array1,2) - 4
                    y = image_array1(row, col);
                    if mod(col/2,2)==0                        
                        u = image_array2(row, col-1);
                        v = image_array2(row, col);
                    else
                        u = image_array2(row, col);
                        v = image_array2(row, col+1);
                    end
                    
                    %r = 1.164*(y - 16) + 1.596*(v - 128);
                    %g = 1.164*(y - 16) - 0.813*(v - 128) - 0.391*(u - 128);
                    %b = 1.164*(y - 16) + 2.018*(u - 128);
                    %temp_row = [temp_row r g b];

		    %color conversion from http://www.equasys.de/colorconversion.html
                    rgb2yuv_mat = [1.164 0 1.596; 1.164 -.392 -.813;1.164 2.017 0];
                    rgb = rgb2yuv_mat*[y-16;u-128;v-128];
                    
                    
                    image_rgb(row,pixel_num, 1) = rgb(1);
                    image_rgb(row,pixel_num, 2) = rgb(2);
                    image_rgb(row,pixel_num, 3) = rgb(3);
                    pixel_num = pixel_num + 1;
                end
               % image_rgb = [image_rgb; temp_row];
            end
                
            
            figure(1)
            surfc(image_array1)
            colormap(gray)
            view(2)
            axis equal;

            figure(2)
            surfc(image_array2)
            colormap(gray)
            view(2)
            axis equal;
            
            figure(3)
            image(image_rgb)
	    axis equal;

        case 'q'
          has_quit = true;              % exit matlab
        otherwise
            fprintf('Invalid Selection %c\n', selection);
    end
end

end
