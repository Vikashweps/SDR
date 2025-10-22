#include "SDR_include.h"

// Генерация тестового сигнала для передачи
void generate_tx_signal(int16_t *tx_buff, size_t tx_mtu, long long tx_time) {
    // Синхропоследовательности
    for(size_t i = 0; i < 2; i++) {
        tx_buff[0 + i] = 0xffff;
        tx_buff[10 + i] = 0xffff;
    }
 //заполнение tx_buff значениями сэмплов первые 16 бит - I, вторые 16 бит - Q.
   /* for (size_t i = 2; i < 2 * tx_mtu; i += 2) {
        // ЗДЕСЬ БУДУТ ВАШИ СЭМПЛЫ
        tx_buff[i] = 1500 << 4;   // I
        tx_buff[i+1] = 1500 << 4; // Q
    }*/
    // Генерация I/Q samples
    float x = -96;
    for (size_t i = 2; i < 2 * tx_mtu; i += 2) {
        tx_buff[i] = (x*x);   // I компонента
        tx_buff[i+1] = (x*x); // Q компонента
        x += 0.1;
    }

   

    // Встраивание временной метки
    for(size_t i = 0; i < 8; i++) {
        uint8_t tx_time_byte = (tx_time >> (i * 8)) & 0xff;
        tx_buff[2 + i] = tx_time_byte << 4;
    }
}

// Сохранение samples в файл
void save_to_file(const char *filename, const int16_t *samples, size_t num_samples) {
    FILE *file = fopen(filename, "wb");
    if (file != NULL) {
        fwrite(samples, sizeof(int16_t), num_samples, file);
        fclose(file);
        printf("Saved %zu samples to %s\n", num_samples / 2, filename);
    }
}