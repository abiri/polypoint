#include "nrfx_twim.h"
#include "sdk_errors.h"
#include "app_util_platform.h"
#include "nrf_gpiote.h"
#include "nrfx_gpiote.h"
#include "nrf_drv_gpiote.h"
#include "nrf_twi_mngr.h"
#include "nrf_delay.h"

#include "boards.h"
#include "led.h"

#include "module_interface.h"
#include "../apps/node/ble_config.h"

// Save the callback that we use to signal the main application that we
// received data over I2C.
bool *                     _module_interrupt_thrown = NULL;
module_interface_data_cb_f _data_callback           = NULL;

// Use TWI1
#ifndef TWI_INSTANCE_NR
#define TWI_INSTANCE_NR 1
#endif

NRF_TWI_MNGR_DEF(twi_mngr_instance, 5, TWI_INSTANCE_NR);
static nrf_drv_twi_config_t twi_config = NRF_DRV_TWI_DEFAULT_CONFIG;

uint8_t twi_tx_buf[256];
uint8_t twi_rx_buf[256];

// Initialize device as TWI Master
static void twi_init(void) {
    ret_code_t err_code;

    // Set internal I2C pull ups
    if (!nrfx_gpiote_is_init()) {
        err_code = nrfx_gpiote_init();
        APP_ERROR_CHECK(err_code);
    }

    // TWI Master automatically enables as Pullups
    //nrf_gpio_cfg_input(CARRIER_I2C_SCL, NRF_GPIO_PIN_PULLUP);
    //nrf_gpio_cfg_input(CARRIER_I2C_SDA,  NRF_GPIO_PIN_PULLUP);

    // Configure TWI Master
    twi_config.scl                  = CARRIER_I2C_SCL;
    twi_config.sda                  = CARRIER_I2C_SDA;
    twi_config.frequency            = NRF_DRV_TWI_FREQ_400K;

    // Define twi_handler as third parameter
    err_code = nrf_twi_mngr_init(&twi_mngr_instance, &twi_config);
    APP_ERROR_CHECK(err_code);

    //printf("Initialized TWI\n");
}

void module_interrupt_handler (nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {

    //printf("Detected interrupt on pin: %i \r\n", pin);

    // As this is triggered by events, we do not have to clear the interrupt

    // Verify interrupt is from module
    if (pin == CARRIER_INTERRUPT_MODULE) {
        (*_module_interrupt_thrown) = true;
    }
}

ret_code_t module_interrupt_dispatch() {
    ret_code_t ret;

    // Clear flag
    (*_module_interrupt_thrown) = false;

    // Ask whats up over I2C
    uint8_t len = 0;
    uint8_t cmd = MODULE_CMD_READ_INTERRUPT;

    //printf("Sending CMD_READ_INTERRUPT...\n");

    // Figure out the length of what we need to receive by checking the first byte of the response.
    nrf_twi_mngr_transfer_t const read_transfer[] = {
            NRF_TWI_MNGR_WRITE(MODULE_ADDRESS, &cmd, 1, 0),
            NRF_TWI_MNGR_READ( MODULE_ADDRESS, &len, 1, 0)
    };

    ret = nrf_twi_mngr_perform(&twi_mngr_instance, NULL, read_transfer, 2, NULL);
    APP_ERROR_CHECK(ret);

    // Now that we know the length, read the rest
    //printf("Reading I2C response of length %i\n", len);
    if (len > 0) {
        nrf_twi_mngr_transfer_t const read_rest_transfer[] = {
                NRF_TWI_MNGR_READ(MODULE_ADDRESS, twi_rx_buf, len, 0)
        };

        ret = nrf_twi_mngr_perform(&twi_mngr_instance, NULL, read_rest_transfer, 1, NULL);
        APP_ERROR_CHECK(ret);

        // Send back the I2C data
        printf("Received I2C response of length %i \r\n", len);
    } else {
        printf("ERROR: Tried reading I2C packet of length %i\n", len);
    }

    // Parse received data
    _data_callback(twi_rx_buf, len);

    return NRF_SUCCESS;
}

ret_code_t module_hw_init () {
	ret_code_t ret;

	// Initialize the GPIO interrupt from the device
	if (!nrfx_gpiote_is_init()) {
		ret = nrfx_gpiote_init();
		if (ret != NRF_SUCCESS) return ret;
	}

	// Setup TWI
	twi_init();

	// Set Interrupt handler
	nrfx_gpiote_in_config_t in_config = NRFX_GPIOTE_CONFIG_IN_SENSE_LOTOHI(1);
	ret = nrfx_gpiote_in_init(CARRIER_INTERRUPT_MODULE, &in_config, module_interrupt_handler);
	if (ret != NRF_SUCCESS) return ret;

	nrfx_gpiote_in_event_enable(CARRIER_INTERRUPT_MODULE, true);
	return NRF_SUCCESS;
}

ret_code_t module_init (bool* module_interrupt_thrown, module_interface_data_cb_f cb) {
	ret_code_t ret;

	// Setup connections to app
	_module_interrupt_thrown = module_interrupt_thrown;
    _data_callback           = cb;

	// Wait for 500 ms to make sure the module module is ready
	nrf_delay_ms(500);

	// Now try to read the info byte to make sure we have I2C connection
	{
		uint16_t id;
		uint8_t version;
		ret = module_get_info(&id, &version);
		if (ret != NRF_SUCCESS) {
		    printf("ERROR: Failed to contact STM with error code %li! \r\n", ret);
		    APP_ERROR_CHECK(ret);
		    return ret;
		}
		if (id != MODULE_ID) return NRF_ERROR_INVALID_DATA;
	}

	return NRF_SUCCESS;
}

ret_code_t module_get_info (uint16_t* id, uint8_t* version) {
    ret_code_t ret;

    uint8_t buf_cmd[1] = {MODULE_CMD_INFO};
	uint8_t buf_resp[3];

	// Send outgoing command that indicates we want the device info string
	nrf_twi_mngr_transfer_t const read_transfer[] = {
			NRF_TWI_MNGR_WRITE(MODULE_ADDRESS, buf_cmd,  1, 0),
			NRF_TWI_MNGR_READ( MODULE_ADDRESS, buf_resp, 3, 0)
	};

	ret = nrf_twi_mngr_perform(&twi_mngr_instance, NULL, read_transfer, 2, NULL);
    APP_ERROR_CHECK(ret);

	*id = (uint16_t)( (uint16_t)buf_resp[0] << (uint8_t)8) | buf_resp[1];
	*version = buf_resp[2];

	return NRF_SUCCESS;
}

ret_code_t module_start_role (uint8_t role, bool is_glossy_master, uint8_t master_eui) {
	ret_code_t ret;

	uint8_t buf_cmd[3];
	buf_cmd[0] = MODULE_CMD_CONFIG;

	// Role configuration
	buf_cmd[1] = role;

	// Master configuration
	buf_cmd[1] |= (is_glossy_master << 3);

	// Configure app
	buf_cmd[1] |= (APP_STANDARD << 4);

	// Glossy Master EUI
	buf_cmd[2] = master_eui;

	nrf_twi_mngr_transfer_t const write_transfer[] = {
			NRF_TWI_MNGR_WRITE(MODULE_ADDRESS, buf_cmd, 3, 0)
	};

	ret = nrf_twi_mngr_perform(&twi_mngr_instance, NULL, write_transfer, 1, NULL);
	APP_ERROR_CHECK(ret);

	return NRF_SUCCESS;
}

ret_code_t module_start_ranging (uint8_t master_eui) {

	return module_start_role(APP_ROLE_INIT_NORESP, false, master_eui);
}

// Tell the attached module to become an anchor.
ret_code_t module_start_anchor (bool is_glossy_master, uint8_t master_eui) {

	return module_start_role(APP_ROLE_NOINIT_RESP, is_glossy_master, master_eui);
}

// Tell the attached module that it should enter the calibration mode.
ret_code_t module_start_calibration (uint8_t index) {
    ret_code_t ret;

    uint8_t buf_cmd[3];
	buf_cmd[0] = MODULE_CMD_CONFIG;

	// Configure app
	buf_cmd[1] = (APP_CALIBRATION << 4);

	// Set the index of the node in calibration
	buf_cmd[2] = index;

	nrf_twi_mngr_transfer_t const write_transfer[] = {
			NRF_TWI_MNGR_WRITE(MODULE_ADDRESS, buf_cmd, 3, 0)
	};

	ret = nrf_twi_mngr_perform(&twi_mngr_instance, NULL, write_transfer, 1, NULL);
    APP_ERROR_CHECK(ret);

	return NRF_SUCCESS;
}

// Read the stored calibration values into a buffer (must be
// at least 18 bytes long).
ret_code_t module_get_calibration (uint8_t* calib_buf) {
    ret_code_t ret;

	uint8_t buf_cmd[1] = {MODULE_CMD_READ_CALIBRATION};

	// Send outgoing command that indicates we want the device info string
	nrf_twi_mngr_transfer_t const read_transfer[] = {
			NRF_TWI_MNGR_WRITE(MODULE_ADDRESS, buf_cmd,    1, 0),
			NRF_TWI_MNGR_READ( MODULE_ADDRESS, calib_buf, 18, 0)
	};

	ret = nrf_twi_mngr_perform(&twi_mngr_instance, NULL, read_transfer, 2, NULL);
    APP_ERROR_CHECK(ret);

	return NRF_SUCCESS;
}

// Stop the module and put it in sleep mode
ret_code_t module_sleep () {
    ret_code_t ret;

    uint8_t buf_cmd[1] = {MODULE_CMD_SLEEP};

	nrf_twi_mngr_transfer_t const write_transfer[] = {
			NRF_TWI_MNGR_WRITE(MODULE_ADDRESS, buf_cmd, 1, 0)
	};

	ret = nrf_twi_mngr_perform(&twi_mngr_instance, NULL, write_transfer, 1, NULL);
    APP_ERROR_CHECK(ret);

	return NRF_SUCCESS;
}

// Restart the module. This should only be called if the module was
// once running and then was stopped. If the module was never configured,
// this won't do anything.
ret_code_t module_resume () {
    ret_code_t ret;

    uint8_t buf_cmd[1] = {MODULE_CMD_RESUME};

	nrf_twi_mngr_transfer_t const write_transfer[] = {
			NRF_TWI_MNGR_WRITE(MODULE_ADDRESS, buf_cmd, 1, 0)
	};

	ret = nrf_twi_mngr_perform(&twi_mngr_instance, NULL, write_transfer, 1, NULL);
    APP_ERROR_CHECK(ret);

	return NRF_SUCCESS;
}

// Send the current epoch time to the module so it can be distributed
ret_code_t module_set_time (uint32_t epoch) {
	ret_code_t ret;

	uint8_t buf_cmd[5];

	buf_cmd[0] = MODULE_CMD_SET_TIME;

	// Set epoch time
	buf_cmd[1] = (epoch >> 3*8) & 0xFF;
    buf_cmd[2] = (epoch >> 2*8) & 0xFF;
    buf_cmd[3] = (epoch >> 1*8) & 0xFF;
    buf_cmd[4] = (epoch >> 0*8) & 0xFF;

	nrf_twi_mngr_transfer_t const write_transfer[] = {
			NRF_TWI_MNGR_WRITE(MODULE_ADDRESS, buf_cmd, 5, 0)
	};

	ret = nrf_twi_mngr_perform(&twi_mngr_instance, NULL, write_transfer, 1, NULL);
	APP_ERROR_CHECK(ret);

	return NRF_SUCCESS;
}

// Wake up module over I2C
ret_code_t module_wakeup () {
    ret_code_t ret;

    uint8_t buf_cmd[1];

    buf_cmd[0] = MODULE_CMD_WAKEUP;

    nrf_twi_mngr_transfer_t const write_transfer[] = {
            NRF_TWI_MNGR_WRITE(MODULE_ADDRESS, buf_cmd, 1, 0)
    };

    ret = nrf_twi_mngr_perform(&twi_mngr_instance, NULL, write_transfer, 1, NULL);
    APP_ERROR_CHECK(ret);

    return NRF_SUCCESS;
}




