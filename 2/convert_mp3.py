import numpy as np
import librosa
from pydub import AudioSegment
from pydub.playback import play
import os
import matplotlib.pyplot as plt

mp3_file = "Smeshariki.mp3"  # исходный файл
pcm_file = "tx.pcm" 
output_mp3 = "Smeshariki_processed.mp3"  # выходной файл
pcm_file2 = "rx.pcm"

#загрузка аудио
y, sr = librosa.load(mp3_file, sr=44100, mono=True)
# Преобразование в PCM
pcm_data = (y * 32767).astype(np.int16)
pcm_data.tofile(pcm_file)
print(f"MP3 файл '{mp3_file}' успешно преобразован в PCM файл '{pcm_file}'")

# проверка файла
if not os.path.isfile(pcm_file2):
    print(f"Файл {pcm_file2} не найден!")
    exit()
 # Чтение из PCM файла rx.pcm
pcm_data_load = np.fromfile(pcm_file2, dtype=np.int16)

# Создание AudioSegment
audio = AudioSegment(
    data=pcm_data_load.tobytes(),
    sample_width=4,      # 2 байта = 16 бит
    frame_rate=44100,    # частота дискретизации
    channels=1           # моно
)

# Экспорт в MP3
audio.export(output_mp3, format="mp3", bitrate="192k")
print(f"PCM файл '{pcm_file2}' успешно преобразован обратно в MP3: '{output_mp3}'")


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
plt.title('RX Signal I/Q Components')
plt.show()
