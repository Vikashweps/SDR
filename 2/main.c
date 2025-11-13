#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <math.h>

int16_t *read_pcm(const char *filename, size_t *sample_count)
{
    FILE *file = fopen(filename, "rb");
    if(file == NULL){
        printf("Ошибка открытия файла %s\n", filename);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    printf("file_size = %ld\n", file_size);
    
    int16_t *samples = (int16_t *)malloc(file_size);
    if (samples == NULL) {
        printf("Ошибка выделения памяти\n");
        fclose(file);
        return NULL;
    }

    *sample_count = file_size / sizeof(int16_t);
    size_t sf = fread(samples, sizeof(int16_t), *sample_count, file);

    if (sf != *sample_count){
        printf("Ошибка чтения файла! Прочитано %zu из %zu сэмплов\n", sf, *sample_count);
        free(samples);
        fclose(file);
        return NULL;
    }
    
    fclose(file);
    printf("Успешно прочитано %zu сэмплов\n", *sample_count);
    return samples;
}

int main()
{
    // Инициализация устройства
    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");
    SoapySDRKwargs_set(&args, "uri", "usb:");
    SoapySDRKwargs_set(&args, "direct", "1");
    SoapySDRKwargs_set(&args, "timestamp_every", "1920");
    SoapySDRKwargs_set(&args, "loopback", "0");
    
    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
    if (sdr == NULL) {
        printf("Ошибка инициализации SDR устройства!\n");
        SoapySDRKwargs_clear(&args);
        return -1;
    }
    SoapySDRKwargs_clear(&args);

    // Настройка параметров
    int sample_rate = 1e6;
    int carrier_freq = 800e6;
    
    // Параметры RX части
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, carrier_freq, NULL);

    // Параметры TX части
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, carrier_freq, NULL);

    // Настройка каналов
    size_t channels[] = {0};
    int channel_count = 1;
    
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, channels[0], 10.0);
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, channels[0], -50.0);

    // Инициализация потоков
    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    SoapySDRStream *txStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);

    if (rxStream == NULL || txStream == NULL) {
        printf("Ошибка инициализации потоков!\n");
        return -1;
    }

    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0);
    SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0);

    // Получение MTU
    size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
    size_t tx_mtu = SoapySDRDevice_getStreamMTU(sdr, txStream);
    printf("RX MTU: %zu, TX MTU: %zu\n", rx_mtu, tx_mtu);

    // Загрузка PCM данных
    size_t sample_count = 0;
    int16_t *my_file = read_pcm("tx.pcm", &sample_count);
    if (my_file == NULL) {
        printf("Не удалось загрузить PCM файл!\n");
        return -1;
    }

    // Выделение памяти под буферы
    int16_t *tx_buff = (int16_t *)malloc(2 * tx_mtu * sizeof(int16_t));
    int16_t *rx_buffer = (int16_t *)malloc(2 * rx_mtu * sizeof(int16_t));
    
    if (tx_buff == NULL || rx_buffer == NULL) {
        printf("Ошибка выделения памяти под буферы!\n");
        free(my_file);
        return -1;
    }

    int cur_sample_in_file = 0;
    FILE *rx_file = fopen("rx.pcm", "wb");
    if (rx_file == NULL) {
        printf("Ошибка создания файла rx.pcm!\n");
        free(my_file);
        free(tx_buff);
        free(rx_buffer);
        return -1;
    }

    long long timeoutUs = 100000;
    long long last_time = 0;
    
 
size_t total_buffers = (sample_count/2 + tx_mtu - 1) / tx_mtu; // Округление вверх

    printf("Всего буферов для обработки: %zu\n", total_buffers);

    // Основной цикл обработки
    for (size_t buffers_read = 0; buffers_read < total_buffers; buffers_read++)
    {
        void *rx_buffs[] = {rx_buffer};
        int flags;       
        long long timeNs; 
        
        // Чтение RX данных
        int sr = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);
        
        if(sr > 0){
            // Записываем принятые данные в файл
            fwrite(rx_buffer, sizeof(int16_t), 2 * sr, rx_file);
            printf("Записано %d комплексных сэмплов в RX файл\n", sr);
        } else if (sr < 0) {
            printf("Ошибка чтения RX потока: %d\n", sr);
        }

        printf("Buffer: %zu - RX Samples: %d, Flags: %d, Time: %lld\n", 
               buffers_read, sr, flags, timeNs);

        // Подготовка TX буфера
        size_t complex_samples_to_send = tx_mtu;
        size_t remaining_complex_samples = (sample_count - cur_sample_in_file) / 2;
        
        if (remaining_complex_samples < tx_mtu) {
            complex_samples_to_send = remaining_complex_samples;
        }


        for (size_t i = 0; i < complex_samples_to_send; i++) {
            // I  
            tx_buff[2*i] = my_file[cur_sample_in_file + 2*i];
            // Q  
            tx_buff[2*i + 1] = my_file[cur_sample_in_file + 2*i + 1];
        }
        
        // Заполняем оставшуюся часть нулями
        for (size_t i = complex_samples_to_send; i < tx_mtu; i++) {
            tx_buff[2*i] = 0;
            tx_buff[2*i + 1] = 0;
        }

        cur_sample_in_file += 2 * complex_samples_to_send;

        // Отправка TX данных
        void *tx_buffs[] = {tx_buff};
        flags = SOAPY_SDR_HAS_TIME;
        long long tx_time = timeNs + 4000000; // +4 мс
        
        int st = SoapySDRDevice_writeStream(sdr, txStream, tx_buffs, tx_mtu, &flags, tx_time, timeoutUs);
        
        if (st < 0) {
            printf("TX Failed: %d\n", st);
        } else {
            printf("TX Sent: %d комплексных сэмплов\n", st);
        }
        
        if (cur_sample_in_file >= sample_count) {
            printf("Все данные переданы\n");
            break;
        }
    }

    // Завершение работы
    fclose(rx_file);
    free(tx_buff);
    free(rx_buffer);
    free(my_file);

    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
    SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0);
    SoapySDRDevice_closeStream(sdr, rxStream);
    SoapySDRDevice_closeStream(sdr, txStream);
    SoapySDRDevice_unmake(sdr);

    printf("Программа завершена успешно\n");
    return 0;
}