#include "SDR_include.h"
#include <time.h>
#include <string.h>

void bpsk_mapper(int* bits, iq_sample_t* iq_samples, int num_bits) {
    for (int i = 0; i < num_bits; i++) {
        bits[i] = rand() % 2;
        if (bits[i] == 1) {
            iq_samples[i].i = 1.0f; 
            iq_samples[i].q = 0.0f;
        } else {
            iq_samples[i].i = -1.0f; 
            iq_samples[i].q = 0.0f;
        }
    }
}

void upsampling(const iq_sample_t* input, iq_sample_t* output, int num_bits) {
       for (int i = 0; i < num_bits; i++) {
        // Копируем i-й входной сэмпл в i*L позицию выходного массива
        output[i * 10].i = input[i].i;
        output[i * 10].q = input[i].q;

        for (int j = 1; j < 10; j++) {
            output[i * 10 + j].i = 0.0f;
            output[i * 10 + j].q = 0.0f;
        }
    }
}

void convolve(const iq_sample_t* input, iq_sample_t* output, int num_bits, int num_upsampled ){
    const int L = 10; // Длина фильтра
    float h[L];
    for (int i = 0; i < L; i++) {
        h[i] = 1.0f; 
    }

    // Цикл по каждому отсчёту выходного сигнала
    for (int n = 0; n < num_upsampled; n++) {
        float a_i = 0.0f;
        float a_q = 0.0f;

        // Цикл свёртки
        for (int k = 0; k < L; k++) { // k пробегает длину фильтра
            int input_index = n - k;
            if (input_index >= 0) { // Проверяем, не вышли ли за границы входного сигнала
                a_i += input[input_index].i * h[k];
                a_q += input[input_index].q * h[k];
            }
            // Если input_index < 0, то считаем, что input[input_index] = 0 (нулевое продолжение)
        }

        output[n].i = a_i;
        output[n].q = a_q;

    }
    
}
            
void amplify_signal(const iq_sample_t* input, iq_sample_t* output, int num_samples, float amplitude) {
    for (int i = 0; i < num_samples; i++) {
        output[i].i = input[i].i * amplitude;
        output[i].q = input[i].q * amplitude;
    }
}

// Функция для конвертации IQ samples в формат для SDR
void convert_iq_to_sdr(const iq_sample_t* iq_samples, int16_t* sdr_buffer, int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        sdr_buffer[2*i] = (int16_t)(iq_samples[i].i * 32767.0f);    // I
        sdr_buffer[2*i + 1] = (int16_t)(iq_samples[i].q * 32767.0f ); // Q
    }
}
int main() {
    printf(" SDR Application Started \n");
    srand((unsigned int)time(NULL));
    
    // Инициализация устройства
    sdr_device_t *sdr = sdr_init(1);
    if (sdr == NULL) return 1;
    // Конфигурация
    if (sdr_configure(sdr) != 0) {
        sdr_cleanup(sdr);
        return 1;
    }

    // Выделение буферов
    int16_t *tx_buff = (int16_t*)malloc(2 * sdr->tx_mtu * sizeof(int16_t));
    int16_t *rx_buffer = (int16_t*)malloc(2 * sdr->rx_mtu * sizeof(int16_t));
    
    // Параметры сигнала
    const int num_bits = 10;
    const int num_upsampled = num_bits * 10; // 100 samples
    
    // Проверка размера
    if (num_upsampled > sdr->tx_mtu) {
        printf("ERROR: Signal too long! Reduce num_bits.\n");
        free(tx_buff);
        free(rx_buffer);
        sdr_cleanup(sdr);
        return 1;
    }
    
    int bits[num_bits];
    iq_sample_t symbols[num_bits];
    iq_sample_t upsampled[num_upsampled];
    iq_sample_t amplified[num_upsampled];
    iq_sample_t filtered[num_upsampled];  
    
    // Генерация сигнала
    bpsk_mapper(bits, symbols, num_bits);
    printf("Сгенерированные символы (до апсемплинга):\n");
    for(int i = 0; i < num_bits; i++) {
        printf("[%d]: I=%.1f, Q=%.1f\n", i, symbols[i].i, symbols[i].q);
    }
    upsampling(symbols, upsampled, num_bits);
    printf("Результат апсемплинга (первые 30 сэмплов):\n");
    for(int i = 0; i < 30 && i < num_upsampled; i++) { // Выводим только первые 30
        printf("[%d]: I=%.1f, Q=%.1f\n", i, upsampled[i].i, upsampled[i].q);
    }
    convolve(upsampled, filtered, num_bits, num_upsampled);
      printf("Результат фильтрации (первые 30 сэмплов):\n");
    for(int i = 0; i < 30 && i < num_upsampled; i++) {
        printf("[%d]: I=%.1f, Q=%.1f\n", i, filtered[i].i, filtered[i].q);
    }

    amplify_signal(filtered, amplified, num_upsampled, 0.8f); 
    
    // Конвертация
    convert_iq_to_sdr(filtered, tx_buff, num_upsampled);
    
    // Вывод информации
    printf("Биты: ");
    for (int i = 0; i < num_bits; i++) printf("%d ", bits[i]);
    printf("\n");
    printf("Передается %d сэмплов \n", num_upsampled, sdr->tx_mtu);
    
    // Файлы для записи
    FILE *rx_file = fopen("rx_samples.pcm", "wb");
    FILE *tx_file = fopen("tx_samples.pcm", "wb");
    if (!rx_file || !tx_file) {
        printf("Error opening files\n");
        return 1;
    }

    // Основной цикл
    long long last_time = 0;
    long long timeNs = 0;
    
    for (size_t buffers_read = 0; buffers_read < 10; buffers_read++) {
        
        // Прием samples
        int sr = sdr_read_samples(sdr, rx_buffer, &timeNs);
        printf("Buffer: %lu - RX: %i samples, TimeDiff: %lli us\n", 
               buffers_read, sr, (timeNs - last_time) / 1000);
        
        // Сохранение принятых данных
        if (sr > 0) {
            fwrite(rx_buffer, sizeof(int16_t), 2 * sr, rx_file);
        }
        
        // Передача BPSK сигнала на 3-й итерации
        if (buffers_read == 2) {
            printf("Transmitting BPSK signal...\n");
            
            // Создаем полный буфер передачи
            int16_t *full_tx_buff = (int16_t*)malloc(2 * sdr->tx_mtu * sizeof(int16_t));
            if (full_tx_buff == NULL) {
                printf("Error allocating full TX buffer\n");
                continue;
            }
            
            // Копируем BPSK сигнал в начало буфера
            memcpy(full_tx_buff, tx_buff, 2 * num_upsampled * sizeof(int16_t));
            
            // Заполняем остаток буфера нулями (тишина)
            for (size_t i = num_upsampled; i < sdr->tx_mtu; i++) {
                full_tx_buff[2*i] = 0;      // I = 0
                full_tx_buff[2*i + 1] = 0;  // Q = 0
            }
            
            // Передача полного буфера
            int st = sdr_write_samples(sdr, full_tx_buff, timeNs + 4000000);
            if (st > 0) {
                fwrite(full_tx_buff, sizeof(int16_t), 2 * st, tx_file);
                printf("TX successful: %d samples (%d BPSK + %d silence)\n", 
                       st, num_upsampled, st - num_upsampled);
            } else {
                printf("TX Failed: %i\n", st);
            }
            
            free(full_tx_buff);
        }
        
        last_time = timeNs;
    }
    
    // Завершение работы
    fclose(rx_file);
    fclose(tx_file);
    free(tx_buff);
    free(rx_buffer);
    sdr_cleanup(sdr);

    printf(" SDR Application Finished \n");
    return 0;
}