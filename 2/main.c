#include "SDR_include.h"

//функция чтения псм файла
int16_t *read_pcm(const char *filename, size_t *sample_count)
{
    FILE *file = fopen(filename, "rb");

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    printf("file_size = %ld\\n", file_size);
    int16_t *samples = (int16_t *)malloc(file_size);

    *sample_count = file_size / sizeof(int16_t);

    size_t sf = fread(samples, sizeof(int16_t), *sample_count, file);

    if (sf == 0){
        printf("file %s empty!", filename);
    }

    fclose(file);

    return samples;
}



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
    size_t sample_count = 0;
    int16_t *samples = read_pcm("/home/vika/Desktop/dev/2/build/tx.pcm", &sample_count); 
    
    // Выделение буферов
    int16_t *tx_buff = (int16_t*)malloc(2 * sdr->tx_mtu * sizeof(int16_t));
    int16_t *rx_buffer = (int16_t*)malloc(2 * sdr->rx_mtu * sizeof(int16_t));

    // Файл для записи принятых данных
    FILE *file = fopen("rx.pcm", "wb");
    if (file == NULL) return 1;

    // Основной цикл обработки
   
    long long tx_time =0;
    size_t iteration_count = (sample_count/1920);
    const long timeoutUs = 400000;
    long long timeNs = 0;

    for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++) {
        void *rx_buffs[] = {rx_buffer};
        int flags;
        
        // Чтение samples
        // считали буффер RX, записали его в rx_buffer
        int sr = SoapySDRDevice_readStream(sdr->device, sdr->rxStream, rx_buffs, sdr->rx_mtu, &flags, timeNs, timeoutUs);
    
        // Сохранение в файл
        if (sr > 0) {
            fwrite(rx_buffer, sizeof(int16_t), 2 * sr, file);
        }
        
        // Передача на 3-й итерации
        if (buffers_read == 2) {
            tx_time = timeNs +4000000;
            void *tx_buffs[] = {tx_buff + 1920 * buffers_read};
            for(size_t i = 0; i < 2; i++) {
                tx_buffs[0 + i] = 0xffff;
                tx_buffs[10 + i] = 0xffff;
            }

            for(size_t i = 0; i < 8; i++) {
                uint8_t tx_time_byte = (tx_time >> (i * 8)) & 0xff;
                tx_buffs[2 + i] = tx_time_byte << 4;
            }
            
            int tx_flags = SOAPY_SDR_HAS_TIME;
            const long timeoutUs = 400000;
        
            int st = SoapySDRDevice_writeStream(sdr->device, sdr->txStream, (const void * const*)tx_buffs, sdr->tx_mtu, &tx_flags, tx_time, timeoutUs);
                
            if ((size_t)st != sdr->tx_mtu) {
                printf("TX Failed: %i\n", st);
            }
        }
    }

    // Завершение работы
    fclose(file);
    free(tx_buff);
    free(rx_buffer);
    sdr_cleanup(sdr);

    printf("=== SDR Application Finished ===\n");
    return 0;
}