

import numpy as np

# Carica il file 7098 valori
data = np.load("input-DA-UTILIZZARE.npy")

# Impostazioni per le finestre
window_size = 99

# Crea le finestre di 99 valori (scorrendo con passo 1)
windows = []
for i in range(0, len(data) - window_size + 1, 1):
    windows.append((data[i:i + window_size]-522.0)/814.0)

# Converte la lista di finestre in un array NumPy
windows = np.array(windows)

# Salva il set di dati in un file .npz
np.savez("input-DA-UTILIZZARE.npz", windows=windows)


