%% Generate Hilbert coeffecients
order = 8;
if 1
  order = floor(order/2)*2; %Make sure order is even
  %See instructions for generating poles/zeros at https://dsp.stackexchange.com/questions/8692/hilbert-transform-filter-for-audio-applications-using-iir-half-band-parallel-al
  Ik = e.^(pi./(2.^(1:1:order)));
  Ik = Ik.*((-1).**(1:order));
  Isos = zp2sos([Ik], 1./[Ik], 1);
  Qsos = zp2sos([-Ik], 1./[-Ik], 1);
  [b a] = sos2tf(Isos);
  h1 = freqz(b,a);
  Isos = zp2sos([Ik], 1./[Ik], 1/max(abs(h1)));
  Qsos = zp2sos([-Ik], 1./[-Ik], 1/max(abs(h1)));
else
   order = floor(order/2)*2+1; %Make sure order is odd
   if 1 % Type III FIR
    window = hamming(order*2+1);
    a = (-order:order);
    b = abs(rem(a,2)); %Remove even values
    c = 2/pi./a; %Scale and invert
    c(order+1)=0; % Remove NaN
    c = c.* window' .* b;
  else % Type IV FIR
    window = hamming(order+1);
    a = (-order:2:order);
    c = 2/pi./a; %Scale and invert
    c = c.* window';
  end
  Qsos = tf2sos(c,[1]);
  % delay compensation
  Isos = zeros(floor(order/2),6);
  Isos(:,3:4)=1;
end

%% Create test audio
fs = 20e3;
start_freq = 10; %Hz
stop_freq = 10e3; %Hz
duration = 3;    %Seconds
w = logspace(log10(start_freq),log10(stop_freq),fs*duration);
%t = linspace(start_freq, stop_freq, fs*duration);
t = sosfilt([1 0 0 1 -1 0], w); %Numerical integration
audio1 = cos(2*pi/fs*t);
scale = 2^11;
audio = round(audio1 * scale)/scale;
%audio = chirp([0:0.001:5]);
%% Hilbert transform
if 0
  h = hilbert(audio);
  I = real(h);
  Q = imag(h);
else
  I = sosfilt(Isos, audio);
  Q = sosfilt(Qsos, audio);
end
%% Reconstruction
if 0
  A = real(hilbert(I)); %To compensate for group delay
  B = imag(hilbert(Q)); 
  audio2 = (A-B)/2;
elseif 1
  A = sosfilt(Isos, I);
  B = sosfilt(Qsos, Q);
  audio2 = (A-B)/2;
else
  A = sosfilt(Qsos, I);
  B = sosfilt(Isos, Q);
  audio2 = (A+B)/2;
end
%%Playback
figure(1);
specgram(audio);
p1=audioplayer(audio1,fs,24);
play(p1);
pause(duration);
figure(2);
specgram(audio2);
p2=audioplayer(audio2,fs,8);
play(p2);
pause(duration);
figure(3);
semilogx(w,audio2);