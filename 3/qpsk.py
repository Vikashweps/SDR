import numpy as np
from matplotlib import pyplot as plt

fc = 5 # frquency
num_bits = 10
bits = np.array([0, 1, 1, 0, 0, 1, 0, 1, 1, 1])
print("Bits:", bits)

# Группируем биты по парам
num_symbols = num_bits // 2
symbols_I = np.zeros(num_symbols)
symbols_Q = np.zeros(num_symbols)

# mapping
for i in range(num_symbols):
    b0 = bits[2 * i]  # первый бит пары
    b1 = bits[2 * i + 1]  # второй бит пары
    if b0 == 0 and b1 == 0:
        symbols_I[i] = 1
        symbols_Q[i] = 1
    elif b0 == 0 and b1 == 1:
        symbols_I[i] = -1
        symbols_Q[i] = 1
    elif b0 == 1 and b1 == 0:
        symbols_I[i] = 1
        symbols_Q[i] = -1
    else:  # b0 == 1 and b1 == 1
        symbols_I[i] = -1
        symbols_Q[i] = -1

print("Symbols i:", symbols_I)
print("Symbols q:", symbols_Q)

#ipsampling
i = 0
upI = np.zeros(num_bits * 10)
upQ = np.zeros(num_bits * 10)
for i in range(num_bits//2):
    upI[i*10] = symbols_I[i]
    upQ[i*10] = symbols_Q[i]

print("Upsampled I:", upI[:20])  # first 20 samples
print("Upsampled Q:", upQ[:20])

h = np.ones(10) #impulse
I = np.convolve(h,upI, mode='full') #forming filter elfilter
Q = np.convolve(h,upQ, mode='full')
print(" I:", I[:20])  # first 20 samples
print(" Q:", Q[:20])

#выходной сигнал
t = np.linspace(-1, 1, 109)
s_t = I * np.cos(2 * np.pi * fc * t) - Q * np.sin(2 * np.pi * fc * t)
plt.plot(t, s_t)
plt.title('QPSK Signal')
plt.xlabel('Sample Index')
plt.ylabel('Amplitude')
plt.grid(True)
plt.show()