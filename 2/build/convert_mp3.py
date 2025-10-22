import numpy as np
import librosa
from pydub import AudioSegment
from pydub.playback import play
import os

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

