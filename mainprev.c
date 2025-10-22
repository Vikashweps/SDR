#include <SoapySDR/Device.h>   // Инициализация устройства
#include <SoapySDR/Formats.h>  // Типы данных, используемых для записи сэмплов
#include <stdio.h>             //printf
#include <stdlib.h>            //free
#include <stdint.h>

int main() {
    SoapySDRKwargs args = {};
    
    SoapySDRKwargs_set(&args, "driver", "plutosdr");        // Говорим какой Тип устройства 
    if (1) {
        SoapySDRKwargs_set(&args, "uri", "usb:");           // Способ обмена сэмплами (USB)
    } else {
        SoapySDRKwargs_set(&args, "uri", "ip:192.168.2.1"); // Или по IP-адресу
    }
    
    SoapySDRKwargs_set(&args, "direct", "1");               // 
    SoapySDRKwargs_set(&args, "timestamp_every", "1920");   // Размер буфера + временные метки
    SoapySDRKwargs_set(&args, "loopback", "0");             // Используем антенны или нет
    
    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);       // Инициализация
    SoapySDRKwargs_clear(&args);

    if (sdr == NULL) {
        printf("Failed to create SDR device!\n");
        return 1;
    }

    int sample_rate = 1000000;
    int carrier_freq = 800000000;
    
    // Параметры RX части
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, carrier_freq, NULL);

    // Параметры TX части
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, carrier_freq, NULL);

    // Инициализация количества каналов RX/TX (в AdalmPluto он один, нулевой)
    size_t channels[] = {0};
    
    // Настройки усилителей на RX/TX
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, 0, 10.0); // Чувствительность приемника
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, 0, -90.0);// Усиление передатчика
    
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);
    
    // Формирование потоков для передачи и приема сэмплов
    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    SoapySDRStream *txStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);

    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0); //start streaming
    SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0); //start streaming

    // Получение MTU (Maximum Transmission Unit), в нашем случае - размер буферов. 
    size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
    size_t tx_mtu = SoapySDRDevice_getStreamMTU(sdr, txStream);

    // Выделяем память под буферы RX и TX
    int16_t *tx_buff = (int16_t*)malloc(2 * tx_mtu * sizeof(int16_t));
    int16_t *rx_buffer = (int16_t*)malloc(2 * rx_mtu * sizeof(int16_t));

    // Open file for saving RX samples - ADDED THIS LINE
    FILE *file = fopen("rx_samples.pcm", "wb");
    if (file == NULL) {
        printf("Failed to open file for writing RX samples!\n");
        return 1;
    }

    const long timeoutUs = 400000;
    long long last_time = 0;
    // Количество итерация чтения из буфера
    size_t iteration_count = 10;

    // Начинается работа с получением и отправкой сэмплов
    for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++) {
        void *rx_buffs[] = {rx_buffer};
        int flags;        // flags set by receive operation
        long long timeNs; //timestamp for receive buffer
        
        // считали буффер RX, записали его в rx_buffer
        int sr = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);

        // Смотрим на количество считаных сэмплов, времени прихода и разницы во времени с чтением прошлого буфера
        printf("Buffer: %lu - Samples: %i, Flags: %i, Time: %lli, TimeDiff: %lli\n", 
               buffers_read, sr, flags, timeNs, timeNs - last_time);
        
        // Save received samples to file - ADDED THIS SECTION
        if (sr > 0) {
            size_t samples_written = fwrite(rx_buffer, sizeof(int16_t), 2 * sr, file);
            printf("Saved %zu I/Q samples to file\n", samples_written / 2);
        }
        
        //prepare fixed bytes in transmit buffer
        for(size_t i = 0; i < 2; i++) {
            tx_buff[0 + i] = 0xffff;
            tx_buff[10 + i] = 0xffff;
        }

        //заполнение tx_buff значениями сэмплов первые 16 бит - I, вторые 16 бит - Q.
        for (size_t i = 2; i < 2 * tx_mtu; i += 2) {
            // ЗДЕСЬ БУДУТ ВАШИ СЭМПЛЫ
            tx_buff[i] = 1500 << 4;   // I
            tx_buff[i+1] = 1500 << 4; // Q
        }

        last_time = timeNs;

        // Переменная для времени отправки сэмплов относительно текущего приема
        long long tx_time = timeNs + (4 * 1000 * 1000); // на 4 [мс] в будущее

        // Добавляем время, когда нужно передать блок tx_buff, через tx_time -наносекунд
        for(size_t i = 0; i < 8; i++) {
            uint8_t tx_time_byte = (tx_time >> (i * 8)) & 0xff;
            tx_buff[2 + i] = tx_time_byte << 4;
        }

        // Здесь отправляем наш tx_buff массив
        void *tx_buffs[] = {tx_buff};
        if (buffers_read == 2) {
            printf("buffers_read: %lu\n", buffers_read);
            int tx_flags = SOAPY_SDR_HAS_TIME;
            int st = SoapySDRDevice_writeStream(sdr, txStream, (const void * const*)tx_buffs, tx_mtu, &tx_flags, tx_time, timeoutUs);
            if ((size_t)st != tx_mtu) {
                printf("TX Failed: %i\n", st);
            }
            
            // Save transmitted samples to separate file
            FILE *tx_file = fopen("tx_samples.pcm", "wb");
            if (tx_file != NULL) {
                fwrite(tx_buff, sizeof(int16_t), 2 * tx_mtu, tx_file);
                fclose(tx_file);
                printf("Saved TX samples to tx_samples.pcm\n");
            }
        }
    }

    // Close the RX samples file - THIS IS NOW CORRECT
    fclose(file);
    printf("RX samples saved to rx_samples.pcm\n");

    //stop streaming
    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
    SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0);

    //shutdown the stream
    SoapySDRDevice_closeStream(sdr, rxStream);
    SoapySDRDevice_closeStream(sdr, txStream);

    //cleanup device handle
    SoapySDRDevice_unmake(sdr);

    //cleanup buffers
    free(tx_buff);
    free(rx_buffer);

    return 0;
}