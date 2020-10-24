%% Generate Hilbert coeffecients
order = 8;
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


%% Create test waveform
start_freq = 5
stop_freq = 50;
n = 1e3;
w = logspace(log10(start_freq),log10(stop_freq),n);