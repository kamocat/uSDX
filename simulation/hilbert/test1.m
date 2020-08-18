fs = 5e3; % Sample frequency
len = 7; % half-filter length
window = hamming(len*2+1)';

%%%% Hilbert Transform
a = (-len:len);
b = abs(rem(a,2)); %Remove even values
c = 2/pi./a.*b; %Scale and invert
c(len+1)=0; %Fix the divide-by-zero
c = c.* window;
figure(1);
subplot(1,2,1)
bar(c) % This is an FIR hilbert transform

%%%% Differentiation
d = (-1).**a;
d = d./a;
d(len+1) = 0; %Fix the divide by zero
d = d.* window;
%subplot(1,3,2)
%bar(d)

test_f = 0.01;
test_len = 200;
x = sin(2*pi*test_f.*(1:test_len));
%y = sosfilt([1 0 0 1 0 -1],x);
y = conv(x, c); % Hilbert transform
y = y(len+1:end-len); % Only take the middle section
x = conv(x, [-1 0 2 0 -1]); % Compensation Filter
x = x(3:end-2);
subplot(1,2,2)
plot(y);
hold on;
plot(x);
hold off;

phase = atan2(x,y);
phase = unwrap(phase);
%f = conv(phase,d); % differentiate
%f = f(len*2+1:end-len*2);
f = phase(2:end)-phase(1:end-1);
f *= 1/2/pi;
figure(2);
subplot(1,2,1)
plot(phase);
subplot(1,2,2)
plot(f);

##h = conv(c,d);
##h = round(h)
##subplot(1,3,3)
##bar(h)