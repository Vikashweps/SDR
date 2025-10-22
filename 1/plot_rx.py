import numpy as np
import matplotlib.pyplot as plt

# Чтение файла .pcm
name = "rx.pcm"

real = []
imag = []
count = []
counter = 0

with open(name, "rb") as f:
    index = 0
    while (byte := f.read(2)):
        if index % 2 == 0:
            real.append(int.from_bytes(byte, byteorder='little', signed=True))
            count.append(counter)
            counter += 1
        else:
            imag.append(int.from_bytes(byte, byteorder='little', signed=True))
        index += 1

# Построение графика
plt.plot(count, real, color='blue', label='I')
plt.plot(count, imag, color='red', label='Q')
plt.xlabel('Sample Index')
plt.ylabel('Amplitude')
plt.legend()
plt.title('TX Signal I/Q Components')
plt.show()