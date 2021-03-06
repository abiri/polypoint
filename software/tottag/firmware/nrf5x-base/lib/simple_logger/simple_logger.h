#ifndef SIMPLE_LOGGER_H
#define SIMPLE_LOGGER_H

#include <stdint.h>

enum SIMPLE_LOGGER_ERROR {
	SIMPLE_LOGGER_SUCCESS = 0,
	SIMPLE_LOGGER_BUSY,
	SIMPLE_LOGGER_BAD_FPOINTER,
	SIMPLE_LOGGER_BAD_FPOINTER_INIT,
	SIMPLE_LOGGER_BAD_CARD,
	SIMPLE_LOGGER_BAD_CARD_INIT,
	SIMPLE_LOGGER_FILE_EXISTS,
	SIMPLE_LOGGER_FILE_ERROR,
	SIMPLE_LOGGER_ALREADY_INITIALIZED,
	SIMPLE_LOGGER_BAD_PERMISSIONS
};

uint8_t simple_logger_init(void);
uint8_t simple_logger_reinit(const char *filename);
uint8_t simple_logger_init_debug(const char *filename);
uint8_t simple_logger_power_on(void);
void simple_logger_power_off(void);
uint8_t simple_logger_log_string(const char *str);
uint8_t simple_logger_log(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
uint8_t simple_logger_log_header(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
uint8_t simple_logger_list_files(char *file_name, uint32_t *file_size, uint8_t continuation);
uint8_t simple_logger_delete_file(const char *file_name);
uint8_t simple_logger_open_file_for_reading(const char *file_name);
void simple_logger_close_reading_file(void);
uint32_t simple_logger_read_reading_file(uint8_t *data_buffer, uint32_t buffer_length);
uint8_t simple_logger_printf(uint32_t timestamp, const char *format, va_list ap) __attribute__ ((format (printf, 2, 0)));

#endif
