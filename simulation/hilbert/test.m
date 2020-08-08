fs = 5e3; % Sample frequency
len = 7; % half-filter length
%window = flattopwin(len*2+1)';

%%%% Hilbert Transform
if 0 % Type III FIR
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

function mag = hilmag(test_f, fs, hilbert)
  test_f /= fs;
  test_len = max(100, 2/test_f + 2*length(hilbert));
  x = sin(2*pi*test_f.*(1:test_len));
  y = conv(x, hilbert);
  y = y(length(hilbert)+1:end-length(hilbert)); % Only take the middle section
  mag1 = rms(y);
  mag2 = rms(x);
  mag = [mag1 mag2];
end

function p = phase(test_f, fs, hilbert)
  test_f /= fs;
  test_len = max(100, 2/test_f + 2*length(hilbert));
  x = sin(2*pi*test_f.*(1:test_len));
  y = x;
  y = conv(y, hilbert);
  test_len = floor(test_len/2);
  b = floor(length(hilbert));
  amp = rms(x)/rms(y);
  p = atan2(x(test_len), y(test_len+b));
end

f = logspace(log10(10),log10(2400),200);
%f = linspace(10, 5000, 200);
m = zeros(length(f),2);
p = zeros(length(f),1);
for i=1:length(f)
  m(i,:)=hilmag(f(i), fs, c);
%  p(i,:)=phase(f(i), fs, c);
end
loglog(f,m)
%p = unwrap(p);
%plot(f,p)