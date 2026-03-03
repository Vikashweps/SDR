import numpy as np
import matplotlib.pyplot as plt

data = np.fromfile("rx_samples.pcm", dtype=np.int16)

I = data[0::2]  
Q = data[1::2]  

plt.figure(figsize=(15, 10))

plt.plot(I, label='I (Real)', linewidth=0.8, alpha=0.7)
plt.plot(Q, label='Q (Imag)', linewidth=0.8, alpha=0.7)
plt.xlabel('Индекс сэмпла')
plt.ylabel('Амплитуда')
plt.title('Весь принятый сигнал')
plt.legend()
plt.grid(True)


plt.show()