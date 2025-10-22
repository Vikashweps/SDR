#include "SDR_include.h"

// Инициализация SDR устройства (USB или IP)
sdr_device_t* sdr_init(int use_usb) {
    SoapySDRKwargs args = {};
    // Настройка параметров устройства PlutoSDR
    SoapySDRKwargs_set(&args, "driver", "plutosdr");  // Используем драйвер для PlutoSDR

    // Выбор способа подключения к устройству
    if (use_usb) {
        SoapySDRKwargs_set(&args, "uri", "usb:");           // Подключение по USB
    } else {
     SoapySDRKwargs_set(&args, "uri", "ip:192.168.2.1"); // Подключение по IP
    }

    // Дополнительные параметры
    SoapySDRKwargs_set(&args, "direct", "1");               // Прямой доступ к буферам
    SoapySDRKwargs_set(&args, "timestamp_every", "1920");   // Временные метки каждые 1920 samples
    SoapySDRKwargs_set(&args, "loopback", "0");             

    // Создание и инициализация SDR устройства
    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);

    // Очистка структуры с параметрами
    SoapySDRKwargs_clear(&args);

    if (sdr == NULL) {
        printf("Failed to create SDR device!\n");
        return NULL;
    }

    sdr_device_t *device = (sdr_device_t*)malloc(sizeof(sdr_device_t));
    device->device = sdr;
    device->sample_rate = 1000000;
    device->carrier_freq = 800000000;
    
    return device;
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
    SoapySDRDevice_setGain(sdr->device, SOAPY_SDR_RX, 0, 40.0);
    SoapySDRDevice_setGain(sdr->device, SOAPY_SDR_TX, 0, -10.0);
    
    // Создание потоков
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);
    // Формирование потоков для передачи и приема сэмплов
    sdr->rxStream = SoapySDRDevice_setupStream(sdr->device, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    sdr->txStream = SoapySDRDevice_setupStream(sdr->device, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);

    // Запуск потоков
    SoapySDRDevice_activateStream(sdr->device, sdr->rxStream, 0, 0, 0);
    SoapySDRDevice_activateStream(sdr->device, sdr->txStream, 0, 0, 0);

    // Получение размера буферов
    sdr->rx_mtu = SoapySDRDevice_getStreamMTU(sdr->device, sdr->rxStream);
    sdr->tx_mtu = SoapySDRDevice_getStreamMTU(sdr->device, sdr->txStream);

    return 0;
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