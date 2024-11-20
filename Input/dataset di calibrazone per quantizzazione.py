

import numpy as np

print("Carico il file di input")
data = np.load("input-DA-UTILIZZARE.npy")

# Impostazioni per le finestre
window_size = 99

print("Creo il dataset di calibrazione")
windows = []
for i in range(0, len(data) - window_size + 1, 1):
    windows.append((data[i:i + window_size]-522.0)/814.0)

# Converte la lista di finestre in un array NumPy
windows = np.array(windows)

print("Salvo il set di dati in un file .npz")
np.savez("input-DA-UTILIZZARE.npz", windows=windows)

input("premere invio per terminare")


