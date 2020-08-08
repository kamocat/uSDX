fs = 5e3; % Sample frequency
len = 7; % half-filter length
%window = flattopwin(len*2+1)';

%%%% Hilbert Transform
if 1 % Type III FIR
  window = hamming(len*2+1);
  a = (-len:len);
  b = abs(rem(a,2)); %Remove even values
  c = 2/pi./a; %Scale and invert
  c(len+1)=0; % Remove NaN
  c = c.* window' .* b;
else % Type IV FIR
  len = 2*len+1;
  window = hamming(len+1);
  a = (-len:2:len);
  c = 2/pi./a; %Scale and invert
  c = c.* window';
end

B = c;
A = [1];
W = logspace(-4, -0.7, 512);
h = freqz(B, A, W);
figure(1);
subplot(2,1,1);
loglog(W,abs(h));
subplot(2,1,2);
g = unwrap(-angle(h));
g -= len * W; % Subtract constant group delay
semilogx(W,g/pi*180);