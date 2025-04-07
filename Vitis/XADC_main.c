#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xsysmon.h"
#include "xaxidma.h"

// Memory buffer configuration
#define DMA_RX_BUFFER_ADDR      (0x00100000)                  // DDR memory address used as the DMA receive buffer
#define MAX_PACKET_LENGTH       512                           // Number of bytes to receive per DMA transfer

// Device IDs and base addresses (from system hardware design)
#define SYSMON_INSTANCE_ID      XPAR_SYSMON_0_DEVICE_ID       // ID for XADC System Monitor
#define AXI_DMA_INSTANCE_ID     XPAR_AXIDMA_0_DEVICE_ID       // ID for AXI DMA controller
#define AXI_DMA_BASE_ADDRESS    XPAR_AXIDMA_0_BASEADDR        // Base address of the DMA controller



// Macro to convert raw 16-bit ADC output to voltage (normalized to 0–1V range for 16-bit resolution)
#define ConvertRawADCToVoltage(adc_value) \
    ((((float)(adc_value)) * (1.0f)) / 65536.0f)

int main() {
    // Initialize UART, cache, and other platform-specific settings
    init_platform();
    // print something to show entering the main
    print("Entering the main\n\r");

    // ---------------------------------------------------------------------
    // System Monitor (XADC) Initialization
    // ---------------------------------------------------------------------

    // Lookup System Monitor configuration using the defined device ID
    XSysMon_Config *sysmon_config_ptr = XSysMon_LookupConfig(SYSMON_INSTANCE_ID);
    if (sysmon_config_ptr == NULL) {
        return XST_FAILURE;  // Exit if configuration is not found
    }

    // Create and initialize the System Monitor instance
    XSysMon sysmon_instance;
    XSysMon_CfgInitialize(&sysmon_instance, sysmon_config_ptr, sysmon_config_ptr->BaseAddress);

    // Configure the System Monitor to sample only the VP/VN analog input channel
    XSysMon_SetSequencerMode(&sysmon_instance, XSM_SEQ_MODE_SAFE);         // Enter safe mode before reconfiguration
    XSysMon_SetAlarmEnables(&sysmon_instance, 0x0);                        // Disable all alarm features
    XSysMon_SetSeqChEnables(&sysmon_instance, XSM_SEQ_CH_VPVN);           // Enable only VP/VN channel for sampling
    XSysMon_SetAdcClkDivisor(&sysmon_instance, 32);                       // Set ADC clock divisor for sampling rate
    XSysMon_SetSequencerMode(&sysmon_instance, XSM_SEQ_MODE_CONTINPASS);  // Start continuous sampling

    // ---------------------------------------------------------------------
    // AXI DMA Initialization
    // ---------------------------------------------------------------------

    // Lookup DMA configuration based on device ID
    XAxiDma_Config *dma_config_ptr = XAxiDma_LookupConfig(AXI_DMA_INSTANCE_ID);
    if (!dma_config_ptr) {
        printf("No config found for DMA device ID: %d\r\n", AXI_DMA_INSTANCE_ID);
        return XST_FAILURE;
    }

    // Create and initialize DMA instance
    XAxiDma dma_instance;
    int status = XAxiDma_CfgInitialize(&dma_instance, dma_config_ptr);
    if (status != XST_SUCCESS) {
        printf("DMA initialization failed with status: %d\r\n", status);
        return XST_FAILURE;
    }

    // Reset DMA engine and wait until reset completes
    XAxiDma_Reset(&dma_instance);
    while (!XAxiDma_ResetIsDone(&dma_instance)) {
        // Wait until reset completes
    }

    // Disable DMA interrupts (polling mode is used instead)
    XAxiDma_IntrDisable(&dma_instance, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
    XAxiDma_IntrDisable(&dma_instance, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);

    // ---------------------------------------------------------------------
    // Main Loop – Continuously Read ADC Data via DMA and Display Voltage
    // ---------------------------------------------------------------------

    while (1) {
        // Define pointers to the receive buffer in DDR memory
        u8 *dma_rx_buffer_ptr = (u8 *) DMA_RX_BUFFER_ADDR;
        u16 *adc_data_ptr = (u16 *) DMA_RX_BUFFER_ADDR;

        // Start DMA transfer: receive MAX_PACKET_LENGTH bytes from System Monitor
        status = XAxiDma_SimpleTransfer(&dma_instance,
                                        (UINTPTR) DMA_RX_BUFFER_ADDR,
                                        MAX_PACKET_LENGTH,
                                        XAXIDMA_DEVICE_TO_DMA);
        if (status != XST_SUCCESS) {
            printf("DMA transfer initiation failed with status: %d\r\n", status);
            return XST_FAILURE;
        }

        // Wait for the DMA transfer to complete
        while (XAxiDma_Busy(&dma_instance, XAXIDMA_DEVICE_TO_DMA)) {
            // Busy wait while DMA is transferring data
        }

        // Flush data cache to ensure CPU sees updated buffer contents
        Xil_DCacheFlushRange((UINTPTR) dma_rx_buffer_ptr, MAX_PACKET_LENGTH);

        // Process and print 100 ADC samples
        for (int i = 0; i < 100; ++i) {
            // Convert raw ADC value to floating-point voltage
            float voltage = ConvertRawADCToVoltage((int)(*adc_data_ptr));

            // Separate voltage into integer and fractional parts for printing
            int integer_part = (int) voltage;
            int fractional_part = (int)((voltage - integer_part) * 1000);  // 3 decimal places

            // Print formatted voltage reading to console
            xil_printf("%d.%03d volts\r\n", integer_part, fractional_part);

            adc_data_ptr++;  // Move to next sample
        }

        xil_printf("********************************\r\n");

        // Add a delay (500ms) before the next capture cycle
        usleep(500000);
    }

    // Clean up platform resources before exiting (not reached in this loop)
    cleanup_platform();
    return 0;
}
