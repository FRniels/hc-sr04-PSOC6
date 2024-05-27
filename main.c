#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

#define ECHO_PIN (P9_0)
#define TRIG_PIN (P9_1)

void InitPins(void);
void InitInterrupts(void);
void InitTimer(void);
void StartTimer(void);
uint32_t ReadTimer(void);
void SendTrigger(void);
static void isr_timer(void* callback_arg, cyhal_timer_event_t event);
static void Echo_ISR(void* callback_arg, cyhal_gpio_event_t event);

cyhal_timer_t timer_obj;
const cyhal_timer_cfg_t timer_cfg =
{
	.compare_value = 0,                  // Timer compare value, not used
	.period        = 299,                // Timer period set to a large enough value to expire the timer when no echo is received
	.direction     = CYHAL_TIMER_DIR_UP, // Timer counts up
	.is_compare    = false,              // Don't use compare mode
	.is_continuous = false,              // ONE-SHOT: Do not run timer indefinitely
	.value         = 0                   // Initial value of counter
};

uint32_t read_val;
float distance;

cyhal_gpio_callback_data_t cb_data =
{
    .callback     = Echo_ISR
};

int main(void) {
    cybsp_init(); // Initialize the device and board peripherals
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE); // Init serial communication

    __enable_irq(); // Enable global interrupts

    InitPins();
    InitInterrupts();
    InitTimer();

    SendTrigger();

    for (;;)
    {
    }
}

void InitPins(void)
{
	cyhal_gpio_init(TRIG_PIN, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);
	cyhal_gpio_init(ECHO_PIN, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, false);
}

void InitInterrupts(void)
{
	cyhal_gpio_register_callback(ECHO_PIN, &cb_data);
	cyhal_gpio_enable_event(ECHO_PIN, CYHAL_GPIO_IRQ_BOTH, 3, true);
}

void InitTimer(void)
{
	cyhal_timer_init(&timer_obj, NC, NULL);
	cyhal_timer_configure(&timer_obj, &timer_cfg); // Apply timer configuration such as period, count direction, run mode, etc.
	cyhal_timer_set_frequency(&timer_obj, 10000);  // Set the frequency of timer to 10000 counts in a second or 10000 Hz
	cyhal_timer_register_callback(&timer_obj, isr_timer, NULL);
	cyhal_timer_enable_event(&timer_obj, CYHAL_TIMER_IRQ_TERMINAL_COUNT, 3, true);
}

void StartTimer(void)
{
	// printf("Start echo timer\n\r");
	cyhal_timer_start(&timer_obj); // Start the timer with the configured settings
}

uint32_t ReadTimer(void)
{
	return cyhal_timer_read(&timer_obj);
}

void SendTrigger(void)
{
	// printf("Sended out a trigger pulse for 10uS\n\r");
	cyhal_gpio_write(TRIG_PIN, 1U);
	cyhal_system_delay_us(10);
	cyhal_gpio_write(TRIG_PIN, 0U);
	StartTimer();
}

static void isr_timer(void* callback_arg, cyhal_timer_event_t event)
{
	if(event == CYHAL_TIMER_IRQ_TERMINAL_COUNT) // Timer time-out reached
		SendTrigger(); // Send trigger again
}

static void Echo_ISR(void* callback_arg, cyhal_gpio_event_t event)
{
	// Save the time when the echo pin goes high to wait for the received echo
	if(event == CYHAL_GPIO_IRQ_RISE)
	{
		read_val = ReadTimer();
	}
	// Get the current time and calculate how long it took for the sonic wave to make its way back to the sensor
	else if (event == CYHAL_GPIO_IRQ_FALL)
	{
		distance = (ReadTimer() - read_val) / 58.0f; // or * 0.0343 / 2
		printf("Distance: %.2fm\n\r", distance);
		read_val = 0;
		SendTrigger(); // Send trigger again
	}
}

