clear all; close all; clc;

%visualizzo l'output ottenuto in python
py = fopen("OUTPUT\output_originale_py.bin");
orig_py = fread(py,100000,"float32");
fclose(py);
py = fopen("OUTPUT\output_quant_py.bin");
quant_py = fread(py,100000,"float32");
in = fopen("input.bin");
input = fread(in,100000,"float32");
fclose all;

input = input(99:end);
tiledlayout(2,1);
% Top plot
nexttile;
plot(input,"black");
title('Input');
xlabel('samples');
ylabel('power [W]');
% Bottom plot
nexttile;
plot(orig_py,"blue");
title('Output rete originale in Python');
xlabel('samples');
ylabel('power [W]');

err = abs(quant_py - orig_py);
t = 1 : 1 : length(err);
figure;
tiledlayout(2,1);
% Top plot
nexttile;
plot(quant_py,"blue");
title('Output rete ottimizzata in Python');
xlabel('samples');
ylabel('power [W]');
% Bottom plot
nexttile;
scatter(t,err);
title('Errore assoluto');

err_med = mean(err);
mse = mean((err).^2);
dev_stan = sqrt(sum((err - mean(err)).^2)) / length(err);
disp('Confronto tra rete originale ed ottimizzata in Python')
disp(['Errore assoluto massimo: ', num2str(max(err))])
disp(['Errore medio: ', num2str(err_med)]);
disp(['Errore quadratico medio (MSE): ', num2str(mse)]);
disp(['Deviazione standard: ', num2str(dev_stan)]);

%output ottenuto su piattaforma embedded
f = fopen("OUTPUT\originale.BIN");
keras = fread(f, 100000, "float32");
fclose(f);
f = fopen("OUTPUT\convertita.BIN");
tflite = fread(f,100000,"float32");
fclose(f);
f = fopen("OUTPUT\ottimizzata.BIN");
quant = fread(f,100000,"float32");
fclose(f);
f = fopen("OUTPUT\com_low.BIN");
low = fread(f,100000,"float32");
fclose(f);
f = fopen("OUTPUT\com_med.BIN");
med = fread(f,100000,"float32");
fclose(f);
f = fopen("OUTPUT\com_high.BIN");
high = fread(f,100000,"float32");
fclose all;
%scarto i primi valori, ottenuti in stm inserendo degli zeri fittizi
%all'inizio della prima finestra
keras = keras(99:end);
tflite = tflite(99:end);
quant = quant(99:end);
low = low(99:end);
med = med(99:end);
high = high (99:end);

figure;
tiledlayout(3,2);
nexttile;
plot(keras,"cyan");
xlabel('samples');
ylabel('power [W]');
title('rete originale');
nexttile;
plot(tflite,"cyan");
xlabel('samples');
ylabel('power [W]');
title('rete convertita');
nexttile;
plot(quant,"cyan");
xlabel('samples');
ylabel('power [W]');
title('rete ottimizzata');
nexttile;
plot(low,"cyan");
xlabel('samples');
ylabel('power [W]');
title('compressione low');
nexttile;
plot(med,"cyan");
xlabel('samples');
ylabel('power [W]');
title('compressione medium');
nexttile;
plot(high,"cyan");
xlabel('samples');
ylabel('power [W]');
title('compressione high');

ass = zeros(6,length(orig_py));
for i = 1:length(orig_py)
ass(1,i) = abs(keras(i) - orig_py(i));
ass(2,i) = abs(tflite(i) - orig_py(i));
ass(3,i) = abs(quant(i) - orig_py(i));
ass(4,i) = abs(low(i) - orig_py(i));
ass(5,i) = abs(med(i) - orig_py(i));
ass(6,i) = abs(high(i) - orig_py(i));
end;
maxs = zeros (6,1);
err_meds = zeros(6,1);
mses = zeros(6,1);
dev_stans = zeros(6,1);
for i = 1:6
    maxs(i) = max(ass(i, :));
    err_meds(i) = mean(ass(i));
    mses(i) = mean((ass(i)).^2);
    dev_stans(i) = sqrt(sum((ass(i, :) - mean(ass(i))).^2)) / length(orig_py);
end
y = [maxs, err_meds , mses , dev_stans];
x = ["originale" "convertito" "ottimizzato" "fattore low" "fattore medium" "fattore high"];
figure;
b = bar(x, y);  % Dati da graficare
set(gca, 'YScale', 'log');
grid on;
colororder("reef");
legend('errore massimo','MAE','MSE','deviazione standard','Location','northwest');



f = fopen("OUTPUT\quantizzata.bin");
quantizzata = fread(f,100000,"float32");
quantizzata = quantizzata(99:end);
fclose all;
err = abs(quantizzata - orig_py);
err_med = mean(err);
mse = mean((err).^2);
dev_stan = sqrt(sum((err - mean(err)).^2)) / length(err);
figure;
tiledlayout(2,1);
% Top plot
nexttile;
plot(quantizzata,"blue");
title('Output rete quantizzata');
xlabel('samples');
ylabel('power [W]');
% Bottom plot
nexttile;
plot(orig_py,"blue");
title('Output rete originale in python');
xlabel('samples');
ylabel('power [W]');
figure;
scatter(t,err);
title('Errore assoluto');
disp('Confronto tra rete originale e quantizzata')
disp(['Errore assoluto massimo: ', num2str(max(err))])
disp(['Errore medio: ', num2str(err_med)]);
disp(['Errore quadratico medio (MSE): ', num2str(mse)]);
disp(['Deviazione standard: ', num2str(dev_stan)]);