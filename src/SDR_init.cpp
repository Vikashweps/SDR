#include "SDR_include.h"

// Инициализация SDR устройства (USB или IP)
    sdr_device_t* sdr_init_usb_index(int usb_index) {
    const char* known_uris[] = {
        "usb:1.5.5",   // Индекс 0 — первое устройство
        "usb:1.6.5",   // Индекс 1 — второе устройство
        
    };
    const int NUM_KNOWN = sizeof(known_uris) / sizeof(known_uris[0]);
    
    if (usb_index < 0 || usb_index >= NUM_KNOWN) {
        printf(" Неверный индекс устройства: %d (доступно: 0..%d)\n", 
               usb_index, NUM_KNOWN - 1);
        return NULL;
    }
    
    const char *uri = known_uris[usb_index];
    
    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");
    SoapySDRKwargs_set(&args, "uri", uri);
    SoapySDRKwargs_set(&args, "direct", "1");
    SoapySDRKwargs_set(&args, "loopback", "0");
    
    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);
    
    if (!sdr) {
        printf(" Не удалось открыть: %s\n", uri);
        return NULL;
    }
    
    sdr_device_t *dev = (sdr_device_t*)malloc(sizeof(sdr_device_t));
    dev->device = sdr;
    dev->sample_rate = 4000000;
    dev->carrier_freq = 2400000000;
    dev->rxStream = dev->txStream = NULL;
    
    printf("✓ SDR #%d: %s\n", usb_index, uri);
    return dev;
}

// Настройка параметров RX/TX
int sdr_configure(sdr_device_t *sdr) {
    // Настройка приемника
    SoapySDRDevice_setSampleRate(sdr->device, SOAPY_SDR_RX, 0, sdr->sample_rate);
    SoapySDRDevice_setFrequency(sdr->device, SOAPY_SDR_RX, 0, sdr->carrier_freq, NULL);
    
    // Настройка передатчика
    SoapySDRDevice_setSampleRate(sdr->device, SOAPY_SDR_TX, 0, sdr->sample_rate);
    SoapySDRDevice_setFrequency(sdr->device, SOAPY_SDR_TX, 0, sdr->carrier_freq, NULL);

    // Инициализация количества каналов RX\\\\TX (в AdalmPluto он один, нулевой)
    size_t channels[] = {0};
    
    // Настройки усилителей на RX\\\\TX
    SoapySDRDevice_setGain(sdr->device, SOAPY_SDR_RX, 0, 80.0);
    SoapySDRDevice_setGain(sdr->device, SOAPY_SDR_TX, 0, 80.0);
    
    // Создание потоков
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);
    // Формирование потоков для передачи и приема сэмплов
    sdr->rxStream = SoapySDRDevice_setupStream(sdr->device, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    sdr->txStream = SoapySDRDevice_setupStream(sdr->device, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);

    SoapySDRDevice_writeSetting(sdr->device, "RX1_GAIN", "70");
    SoapySDRDevice_writeSetting(sdr->device, "TX1_ATTENUATION", "0");

    // Запуск потоков
    SoapySDRDevice_activateStream(sdr->device, sdr->rxStream, 0, 0, 0);
    SoapySDRDevice_activateStream(sdr->device, sdr->txStream, 0, 0, 0);

    // Получение размера буферов
    sdr->rx_mtu = SoapySDRDevice_getStreamMTU(sdr->device, sdr->rxStream);
    sdr->tx_mtu = SoapySDRDevice_getStreamMTU(sdr->device, sdr->txStream);

    return 0;
}

// Чтение samples из приемника
int sdr_read_samples(sdr_device_t *sdr, int16_t *rx_buffer, long long *timeNs) {
    void *rx_buffs[] = {rx_buffer};
    int flags;
    const long timeoutUs = 400000;
    
    int sr = SoapySDRDevice_readStream(sdr->device, sdr->rxStream, rx_buffs, sdr->rx_mtu, &flags, timeNs, timeoutUs);
    
    return sr;
}

// Запись samples в передатчик
int sdr_write_samples(sdr_device_t *sdr, int16_t *tx_buff, long long tx_time) {
    void *tx_buffs[] = {tx_buff};
    int tx_flags = SOAPY_SDR_HAS_TIME;
    const long timeoutUs = 400000;
    
    int st = SoapySDRDevice_writeStream(sdr->device, sdr->txStream, (const void * const*)tx_buffs, sdr->tx_mtu, &tx_flags, tx_time, timeoutUs);
    return st;
}

// Освобождение ресурсов
void sdr_cleanup(sdr_device_t *sdr) {
    SoapySDRDevice_deactivateStream(sdr->device, sdr->rxStream, 0, 0);
    SoapySDRDevice_deactivateStream(sdr->device, sdr->txStream, 0, 0);

    SoapySDRDevice_closeStream(sdr->device, sdr->rxStream);
    SoapySDRDevice_closeStream(sdr->device, sdr->txStream);

    SoapySDRDevice_unmake(sdr->device);
    free(sdr);
}
