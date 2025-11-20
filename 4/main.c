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
        output[i * 10].i = input[i].i;
        output[i * 10].q = input[i].q;
        for (int j = 1; j < 10; j++) {
            output[i * 10 + j].i = 0.0f;
            output[i * 10 + j].q = 0.0f;
        }
    }
}

void convolve(const iq_sample_t* input, iq_sample_t* output, int num_bits, int num_upsampled ){
    const int L = 10;
    float h[L];
    for (int i = 0; i < L; i++) {
        h[i] = 1.0f; 
    }

    for (int n = 0; n < num_upsampled; n++) {
        float a_i = 0.0f;
        float a_q = 0.0f;

        for (int k = 0; k < L; k++) {
            int input_index = n - k;
            if (input_index >= 0) {
                a_i += input[input_index].i * h[k];
                a_q += input[input_index].q * h[k];
            }
        }

        output[n].i = a_i;
        output[n].q = a_q;
    }
}

void matched_filter_realistic(const iq_sample_t* input, iq_sample_t* output, int num_samples) {
    const int L = 10;
    float h[L];
    for (int i = 0; i < L; i++) {
        h[i] = 1.0f;
    }

    for (int n = 0; n < num_samples; n++) {
        float a_i = 0.0f;
        float a_q = 0.0f;

        for (int k = 0; k < L; k++) {
            int input_index = n - k;
            if (input_index >= 0 && input_index < num_samples) {
                a_i += input[input_index].i * h[k];
                a_q += input[input_index].q * h[k];
            }
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
        sdr_buffer[2*i] = (int16_t)(iq_samples[i].i * 32767.0f);
        sdr_buffer[2*i + 1] = (int16_t)(iq_samples[i].q * 32767.0f);
    }
}

// ✅ ДОБАВЬТЕ ЭТУ ФУНКЦИЮ - она отсутствовала!
void convert_sdr_to_iq(const int16_t* sdr_buffer, iq_sample_t* iq_samples, int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        iq_samples[i].i = (float)sdr_buffer[2*i] / 32767.0f;
        iq_samples[i].q = (float)sdr_buffer[2*i + 1] / 32767.0f;
    }
}

int main() {
    printf("SDR Application Started\n");
    srand((unsigned int)time(NULL));
    
    sdr_device_t *sdr = sdr_init(1);
    if (sdr == NULL) return 1;
    
    if (sdr_configure(sdr) != 0) {
        sdr_cleanup(sdr);
        return 1;
    }

    int16_t *tx_buff = (int16_t*)malloc(2 * sdr->tx_mtu * sizeof(int16_t));
    int16_t *rx_buffer = (int16_t*)malloc(2 * sdr->rx_mtu * sizeof(int16_t));
    
    const int num_bits = 150;
    const int num_upsampled = num_bits * 10;
    
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
    for(int i = 0; i < 30 && i < num_upsampled; i++) {
        printf("[%d]: I=%.1f, Q=%.1f\n", i, upsampled[i].i, upsampled[i].q);
    }
    convolve(upsampled, filtered, num_bits, num_upsampled);
    printf("Результат фильтрации (первые 30 сэмплов):\n");
    for(int i = 0; i < 30 && i < num_upsampled; i++) {
        printf("[%d]: I=%.1f, Q=%.1f\n", i, filtered[i].i, filtered[i].q);
    }

    amplify_signal(filtered, amplified, num_upsampled, 0.8f); 
    convert_iq_to_sdr(filtered, tx_buff, num_upsampled);
    
    printf("Биты: ");
    for (int i = 0; i < num_bits; i++) printf("%d ", bits[i]);
    printf("\n");
    printf("Передается %d сэмплов\n", num_upsampled);
    
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
        
        int sr = sdr_read_samples(sdr, rx_buffer, &timeNs);
        printf("Buffer: %lu - RX: %i samples\n", buffers_read, sr);
        
        if (sr > 0) {
            fwrite(rx_buffer, sizeof(int16_t), 2 * sr, rx_file);
        }
        
        if (buffers_read == 2) {
            printf("Transmitting BPSK signal...\n");
            
            int16_t *full_tx_buff = (int16_t*)malloc(2 * sdr->tx_mtu * sizeof(int16_t));
            if (full_tx_buff == NULL) continue;
            
            memcpy(full_tx_buff, tx_buff, 2 * num_upsampled * sizeof(int16_t));
            
            for (size_t i = num_upsampled; i < sdr->tx_mtu; i++) {
                full_tx_buff[2*i] = 0;
                full_tx_buff[2*i + 1] = 0;
            }
            
            int st = sdr_write_samples(sdr, full_tx_buff, timeNs + 4000000);
            if (st > 0) {
                fwrite(full_tx_buff, sizeof(int16_t), 2 * st, tx_file);
                printf("TX successful: %d samples\n", st);
            }
            
            free(full_tx_buff);
        }
        
        last_time = timeNs;
    }

    // ЗАКРЫВАЕМ ФАЙЛЫ
    fclose(rx_file);
    fclose(tx_file);

    // ✅✅✅ СОГЛАСОВАННЫЙ ФИЛЬТР ✅✅✅
    printf("\n=== APPLYING MATCHED FILTER TO RECEIVED SIGNAL ===\n");

    FILE *rx_input = fopen("rx_samples.pcm", "rb");
    fseek(rx_input, 0, SEEK_END);
    long file_size = ftell(rx_input);
    fseek(rx_input, 0, SEEK_SET);

    int num_rx_samples = file_size / (2 * sizeof(int16_t));
    printf("RX file contains %d samples\n", num_rx_samples);

    int16_t *original_rx_data = (int16_t*)malloc(file_size);
    fread(original_rx_data, 1, file_size, rx_input);
    fclose(rx_input);

    // Конвертируем в IQ samples
    iq_sample_t *rx_iq = (iq_sample_t*)malloc(num_rx_samples * sizeof(iq_sample_t));
    convert_sdr_to_iq(original_rx_data, rx_iq, num_rx_samples);

    // ДИАГНОСТИКА: что мы приняли?
    printf("First 20 received samples (before filtering):\n");
    for(int i = 0; i < 20 && i < num_rx_samples; i++) {
        printf("[%3d]: I=%10.6f, Q=%10.6f\n", i, rx_iq[i].i, rx_iq[i].q);
    }

    // ПРИМЕНЯЕМ СОГЛАСОВАННЫЙ ФИЛЬТР
    iq_sample_t *filtered_rx = (iq_sample_t*)malloc(num_rx_samples * sizeof(iq_sample_t));
    matched_filter_realistic(rx_iq, filtered_rx, num_rx_samples);

    // ДИАГНОСТИКА: что получилось после фильтра?
    printf("\nFirst 30 samples after matched filter:\n");
    for(int i = 0; i < 30 && i < num_rx_samples; i++) {
        printf("[%3d]: I=%10.6f", i, filtered_rx[i].i);
        if (i % 10 == 9) printf("  <-- check for peaks");
        printf("\n");
    }

    // Сохраняем обратно в файл
    int16_t *filtered_sdr = (int16_t*)malloc(2 * num_rx_samples * sizeof(int16_t));
    convert_iq_to_sdr(filtered_rx, filtered_sdr, num_rx_samples);

    FILE *rx_output = fopen("rx_samples.pcm", "wb");
    fwrite(filtered_sdr, sizeof(int16_t), 2 * num_rx_samples, rx_output);
    fclose(rx_output);

    printf("Successfully processed rx_samples.pcm with matched filter!\n");

    free(original_rx_data);
    free(rx_iq);
    free(filtered_rx);
    free(filtered_sdr);
    free(tx_buff);
    free(rx_buffer);
    sdr_cleanup(sdr);

    printf("SDR Application Finished\n");
    return 0;
}