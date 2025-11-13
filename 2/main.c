#include <SoapySDR/Device.h>   // Инициализация устройства
#include <SoapySDR/Formats.h>  // Типы данных, используемых для записи сэмплов
#include <stdio.h>             //printf
#include <stdlib.h>            //free
#include <stdint.h>
#include <complex.h>
#include <math.h>

int16_t *read_pcm(const char *filename, size_t *sample_count)
{
    FILE *file = fopen(filename, "rb");
    if(file == NULL){
        printf("Ошибка открытия");
        return 0;
    } else{
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
}
int main(){

    //При работе с SoapySDR инициализация устройства выполняется при помощи указателя на структуру SoapySDRDevic
    //Аргументы (в рамках библиотеки SoapySDR) имеют формат ключ :  значение :
    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");        // Говорим какой Тип устройства 
    if (1) {
        SoapySDRKwargs_set(&args, "uri", "usb:");           // Способ обмена сэмплами (USB)
    } else {
        SoapySDRKwargs_set(&args, "uri", "ip:192.168.3.10"); // Или по IP-адресу
    }
    SoapySDRKwargs_set(&args, "direct", "1");               // 
    SoapySDRKwargs_set(&args, "timestamp_every", "1920");   // Размер буфера + временные метки
    SoapySDRKwargs_set(&args, "loopback", "0");             // Используем антенны или нет
    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);       // Инициализация
    SoapySDRKwargs_clear(&args);


    //Настройка параметров устройств TXRX:

    int sample_rate = 1e6;
    int carrier_freq = 800e6;
    // Параметры RX части
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, carrier_freq , NULL);

    // Параметры TX части
    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, carrier_freq , NULL);

    // Инициализация количества каналов RXTX (в AdalmPluto он один, нулевой)
    size_t channels[] = {0};
    // Настройки усилителей на RXTX
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, channels[0], 10.0); // Чувствительность приемника
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, channels[0], -50.0);// Усиление передатчика

    //Инициализация потоков (stream) для передачи и примема сэмплов:

    int channel_count = sizeof(channels);

    // Формирование потоков для передачи и приема сэмплов
    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    SoapySDRStream *txStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);

    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0); //start streaming
    SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0); //start streaming

    //Выделение памяти под буфферы (для хранения сэмплов TX, RX):
    size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
    size_t tx_mtu = SoapySDRDevice_getStreamMTU(sdr, txStream);

    size_t sample_count = 0;
    int16_t *my_file = read_pcm("/home/vika/Desktop/dev/SDR/2/build/tx.pcm", &sample_count);

    // Выделяем память под буферы RX и TX
    int16_t tx_buff[2 *tx_mtu];
    int16_t rx_buffer[2 *rx_mtu];

    int cur_sample_in_file = 0;

    FILE *file = fopen("tx.pcm", "w");
    FILE *file1 = fopen("rx.pcm", "w");

    long long timeoutUs = 100000;
    long long last_time = 0;

    //  работа с получением и отправкой сэмплов
    for (size_t buffers_read = 0; buffers_read < sample_count; buffers_read++)
    {
        void *rx_buffs[] = {rx_buffer};
        int flags;       
        long long timeNs; 
        
        // считали буффер RX, записали его в rx_buffer
        int sr = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);
        
        if(buffers_read > 0){
            fwrite(rx_buffer, 2* rx_mtu * sizeof(int16_t), 1, file);
        }

        // Смотрим на количество считаных сэмплов, времени прихода и разницы во времени с чтением прошлого буфера
        printf("Buffer: %lu - Samples: %i, Flags: %i, Time: %lli, TimeDiff: %lli \n", buffers_read, sr, flags, timeNs, timeNs - last_time);
        last_time = timeNs;

        for (int i = 0; i < 2 * tx_mtu; i ++)
        {
            if (cur_sample_in_file < sample_count) 
            {
                tx_buff[i] = my_file[cur_sample_in_file];    
                tx_buff[i+1] = my_file[cur_sample_in_file];  
                cur_sample_in_file++;
            }
            else
            {
                fclose(file);

                //Освобождаем память
                //stop streaming
                SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
                SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0);

                //shutdown the stream
                SoapySDRDevice_closeStream(sdr, rxStream);
                SoapySDRDevice_closeStream(sdr, txStream);

                //cleanup device handle
                SoapySDRDevice_unmake(sdr);

                return 0;
            }
        }

        for(size_t i = 0; i < 2; i++)
        {
            tx_buff[0 + i] = 0xffff;
            // 8 x timestamp words
            tx_buff[10 + i] = 0xffff;
        }

        // Переменная для времени отправки сэмплов относительно текущего приема
        long long tx_time = timeNs + (4 * 1000 * 1000); // на 4 [мс] в будущее      

        // Добавляем время, когда нужно передать блок tx_buff, через tx_time -наносекунд
        for(size_t i = 0; i < 8; i++)
        {
            uint8_t tx_time_byte = (tx_time >> (i * 8)) & 0xff;
            tx_buff[2 + i] = tx_time_byte << 4;
        }

        // Здесь отправляем наш tx_buff массив
        void *tx_buffs[] = {tx_buff};
        flags = SOAPY_SDR_HAS_TIME;
        int st = SoapySDRDevice_writeStream(sdr, txStream, (const void * const*)tx_buffs, tx_mtu, &flags, tx_time, timeoutUs);
        fwrite(tx_buff, 2* tx_mtu * sizeof(int16_t), 1, file1);
        
        if ((size_t)st != tx_mtu)
        {
            printf("TX Failed: %in", st);
        }
    }    
}