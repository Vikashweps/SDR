#include "SDR_include.h"

int main() {
    printf("=== SDR Application Started ===\n");
    
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

    // Файл для записи принятых данных
    FILE *file = fopen("rx_samples.pcm", "wb");
    if (file == NULL) return 1;

    // Основной цикл обработки
    long long last_time = 0;
    size_t iteration_count = 10;

    for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++) {
        long long timeNs;
        
        // Чтение samples
        int sr = sdr_read_samples(sdr, rx_buffer, &timeNs);

        printf("Buffer: %lu - Samples: %i, Time: %lli, TimeDiff: %lli\n", 
               buffers_read, sr, timeNs, timeNs - last_time);
        
        // Сохранение в файл
        if (sr > 0) {
            fwrite(rx_buffer, sizeof(int16_t), 2 * sr, file);
        }
        
        // Передача на 3-й итерации
        if (buffers_read == 2) {
            generate_tx_signal(tx_buff, sdr->tx_mtu, timeNs + 4000000);
            
            int st = sdr_write_samples(sdr, tx_buff, timeNs + 4000000);
            if ((size_t)st != sdr->tx_mtu) {
                printf("TX Failed: %i\n", st);
            }
            
            save_to_file("tx_samples.pcm", tx_buff, 2 * sdr->tx_mtu);
        }
        
        last_time = timeNs;
    }

    // Завершение работы
    fclose(file);
    free(tx_buff);
    free(rx_buffer);
    sdr_cleanup(sdr);

    printf("=== SDR Application Finished ===\n");
    return 0;
}