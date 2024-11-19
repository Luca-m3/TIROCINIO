#libreria per gestire gli array
import numpy as np
#libreria per il machine learning
import tensorflow as tf
#modulo per calcolare il tempo di inferenza
import time  
# carico modello originale e dati
interpreter = tf.lite.Interpreter(model_path='model_convertita.tflite')
interpreter.allocate_tensors()
input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()
data = np.fromfile('input.bin', dtype=np.float32)
data = data.astype(np.float32)
output = []
lunghezza_finestra = 99
#crea il file di report dove si scrivono i tempi di inferenza
with open('report_originale_py.txt', 'w') as report:
#ciclo nel quale si creano le finestre
    for i in range(len(data) - lunghezza_finestra + 1):
#standardizzazione dell'input e creazione della j-esima finestra
        finestre = (data[i:i + lunghezza_finestra] - 522.0) / 814.0
        input_data = finestre.reshape(1,99)
        start_time = time.perf_counter()  # start time
        interpreter.set_tensor(input_details[0]['index'], input_data)
        interpreter.invoke()
#output finestra j-esima e standardizzazione
        output_data = interpreter.get_tensor(output_details[0]['index']) * 3999.0
        end_time = time.perf_counter()  # stop time
        elapsed_time = end_time - start_time 
        report.write(f"ciclo {i}: {elapsed_time:.8f} secondi\n")
        output.append(output_data)
# Salva l'output
float_vector = np.array([arr.item() for arr in output], dtype=np.float32)
float_vector.tofile('output_originale_py.bin')


# carico modello quantizzato e dati
interpreter = tf.lite.Interpreter(model_path='ottimizzata.tflite')
interpreter.allocate_tensors()
input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()
data = np.fromfile('input.bin', dtype=np.float32)
data = data.astype(np.float32)
output = []
lunghezza_finestra = 99
#crea il file di report dove si scrivono i tempi di inferenza
with open('report_quant_py.txt', 'w') as report:
#ciclo nel quale si creano le finestre
    for i in range(len(data) - lunghezza_finestra + 1):
#standardizzazione dell'input e creazione della j-esima finestra
        finestre = (data[i:i + lunghezza_finestra] - 522.0) / 814.0
        input_data = finestre.reshape(1,99)
        start_time = time.perf_counter()  # start time
        interpreter.set_tensor(input_details[0]['index'], input_data)
        interpreter.invoke()
#output finestra j-esima e standardizzazione
        output_data = interpreter.get_tensor(output_details[0]['index']) * 3999.0
        end_time = time.perf_counter()  # stop time
        elapsed_time = end_time - start_time 
        report.write(f"ciclo {i}: {elapsed_time:.8f} secondi\n")
        output.append(output_data)
# Salva l'output
float_vector = np.array([arr.item() for arr in output], dtype=np.float32)
float_vector.tofile('output_quant_py.bin')