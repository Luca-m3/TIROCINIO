#importo le librerie necessarie
import numpy as np

print("carico il file degli input da utilizzare")
data = np.load('input-DA-UTILIZZARE.npy')

#conversione degli input in float32
data_32 = data.astype(np.float32)

print("salvo il risultato in binario nel file input.bin")
data_32.tofile('input.bin')

input("premere invio per terminare")