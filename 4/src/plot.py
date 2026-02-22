import numpy as np
import matplotlib.pyplot as plt

def read_pcm_file(filename):
    """Чтение IQ данных из PCM файла (формат: I, Q, I, Q...)"""
    real = []
    imag = []
    with open(filename, "rb") as f:
        index = 0
        while (byte := f.read(2)):
            if index % 2 == 0:
                real.append(int.from_bytes(byte, byteorder='little', signed=True))
            else:
                imag.append(int.from_bytes(byte, byteorder='little', signed=True))
            index += 1
    return np.array(real), np.array(imag)

# Чтение всех трёх файлов
real_tx, imag_tx = read_pcm_file("tx_samples.pcm")
real_rx_raw, imag_rx_raw = read_pcm_file("rx_raw.pcm")      # ДО фильтрации
real_rx_filt, imag_rx_filt = read_pcm_file("rx_samples.pcm") # ПОСЛЕ фильтрации

# Устойчивый участок для анализа
start_idx = 8000
end_idx = 9400
count_tx = np.arange(len(real_tx))
count_rx = np.arange(len(real_rx_raw))
count_stable = count_rx[start_idx:end_idx]

real_rx_raw_stable = real_rx_raw[start_idx:end_idx]
imag_rx_raw_stable = imag_rx_raw[start_idx:end_idx]
real_rx_filt_stable = real_rx_filt[start_idx:end_idx]
imag_rx_filt_stable = imag_rx_filt[start_idx:end_idx]

plt.figure(figsize=(12, 10))

# График 1: Полный TX сигнал
plt.subplot(3, 1, 1)
plt.plot(count_tx[:1500], real_tx[:1500], color='blue', label='I', linewidth=1.5)
plt.plot(count_tx[:1500], imag_tx[:1500], color='red', label='Q', linewidth=1.5, alpha=0.7)
plt.xlabel('Sample Index')
plt.ylabel('Amplitude')
plt.title('TX Signal I/Q Components')
plt.legend()
plt.grid(True, alpha=0.3)

# График 2: RX ДО фильтрации (устойчивый участок)
plt.subplot(3, 1, 2)
plt.plot(count_stable, real_rx_raw_stable, color='blue', label='I', linewidth=1.2)
plt.plot(count_stable, imag_rx_raw_stable, color='red', label='Q', linewidth=1.2, alpha=0.7)
plt.xlabel('Sample Index')
plt.ylabel('Amplitude')
plt.title(f'RX Signal BEFORE Matched Filter (Stable segment [{start_idx}:{end_idx}])')
plt.legend()
plt.grid(True, alpha=0.3)

# График 3: RX ПОСЛЕ фильтрации (устойчивый участок)
plt.subplot(3, 1, 3)
plt.plot(count_stable, real_rx_filt_stable, color='blue', label='I', linewidth=1.5)
plt.plot(count_stable, imag_rx_filt_stable, color='red', label='Q', linewidth=1.5, alpha=0.7)
plt.xlabel('Sample Index')
plt.ylabel('Amplitude')
plt.title(f'RX Signal AFTER Matched Filter (Stable segment [{start_idx}:{end_idx}])')
plt.legend()
plt.grid(True, alpha=0.3)

plt.tight_layout()
plt.show()


plt.figure(figsize=(6, 6))
# Децимация с попаданием в пики (сдвиг 9 для прямоугольного фильтра)
#offset = 9
#real_decimated = real_rx_filt_stable[offset::10]
#imag_decimated = imag_rx_filt_stable[offset::10]

plt.scatter(real_decimated, imag_decimated, c='darkblue', alpha=0.85, s=50, 
            edgecolors='white', linewidth=0.8)
plt.xlabel('I component')
plt.ylabel('Q component')
plt.title(f'BPSK Constellation (Decimated, offset={offset})')
plt.grid(True, alpha=0.4)
plt.axhline(y=0, color='k', linestyle='--', alpha=0.3)
plt.axvline(x=0, color='k', linestyle='--', alpha=0.3)
plt.axis('equal')
plt.xlim(-25000, 25000)
plt.ylim(-25000, 25000)

plt.text(0.05, 0.95, f'Points: {len(real_decimated)}', 
         transform=plt.gca().transAxes, fontsize=11,
         verticalalignment='top', 
         bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.8))

plt.tight_layout()
plt.show()