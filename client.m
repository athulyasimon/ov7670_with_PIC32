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
    fprintf('l: read one byte    k: Take a new image    p: Display image    q: Quit\n');
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
        case 'p'	
            close all;
	    	pic_array = zeros(145,220);
			for j=1:145
				for i=1:220
 					pic_array(j,i) = fscanf(mySerial,'%d');
%                     if pic_array(j,i) == 255
%                         pic_array(j,i) = 0;
%                     end
				end
			end
            fid = 1;                             % Insert true ‘fid’
			fprintf(fid, [repmat(' %d ', 1, 100) '\n'], pic_array');
            
        case 'q'
          has_quit = true;              % exit matlab
        otherwise
            fprintf('Invalid Selection %c\n', selection);
    end
end

end
