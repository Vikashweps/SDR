#ifndef SDR_APP_H
#define SDR_APP_H

#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <math.h>


// Структура устройства
typedef struct {
    SoapySDRDevice *device;
    SoapySDRStream *rxStream;
    SoapySDRStream *txStream;
    size_t rx_mtu;
    size_t tx_mtu;
    int sample_rate;
    int carrier_freq;
} sdr_device_t;

// Функции работы с устройством
sdr_device_t* sdr_init(int use_usb);
int sdr_configure(sdr_device_t *sdr);
int sdr_read_samples(sdr_device_t *sdr, int16_t *rx_buffer, long long *timeNs);
int sdr_write_samples(sdr_device_t *sdr, int16_t *tx_buff, long long tx_time);
void sdr_cleanup(sdr_device_t *sdr);

// Функции обработки сигналов
void generate_tx_signal(int16_t *tx_buff, size_t tx_mtu, long long tx_time);
void save_to_file(const char *filename, const int16_t *samples, size_t num_samples);

#endif