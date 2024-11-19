#importo le librerie necessarie
import numpy as np

#carico il file degli input da utilizzare
data = np.load('input-DA-UTILIZZARE.npy')

#conversione degli input in float32
data_32 = data.astype(np.float32)

#salvo il risultato in binario
data_32.tofile('input.bin')
