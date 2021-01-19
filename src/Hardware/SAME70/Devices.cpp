/*
 * Devices.cpp
 *
 *  Created on: 11 Aug 2020
 *      Author: David
 */

#include "Devices.h"

AsyncSerial Serial(UART2, UART2_IRQn, ID_UART2, 512, 512, 		[](AsyncSerial*) noexcept { }, [](AsyncSerial*) noexcept { });
USARTClass Serial1(USART2, USART2_IRQn, ID_USART2, 512, 512,	[](AsyncSerial*) noexcept { }, [](AsyncSerial*) noexcept { });
SerialCDC SerialUSB;

constexpr Pin APIN_Serial0_RXD = PortDPin(25);
constexpr Pin APIN_Serial0_TXD = PortDPin(26);
constexpr auto Serial0PinFunction = GpioPinFunction::C;
constexpr Pin APIN_Serial1_RXD = PortDPin(18);
constexpr Pin APIN_Serial1_TXD = PortDPin(19);
constexpr auto Serial1PinFunction = GpioPinFunction::C;

constexpr Pin HcmciMclkPin = PortAPin(25);
constexpr auto HsmciMclkPinFunction = GpioPinFunction::D;
constexpr Pin HsmciOtherPins[] = { PortAPin(26), PortAPin(27), PortAPin(28), PortAPin(30), PortAPin(31) };
constexpr auto HsmciOtherkPinsFunction = GpioPinFunction::C;

void UART2_Handler(void) noexcept
{
	Serial.IrqHandler();
}


void USART2_Handler(void) noexcept
{
	Serial1.IrqHandler();
}

// Callback glue functions, all called from the USB ISR

// This is called when we are plugged in and connect to a host
extern "C" bool core_cdc_enable(uint8_t port) noexcept
{
	SerialUSB.cdcSetConnected(true);
	return true;
}

// This is called when we get disconnected from the host
extern "C" void core_cdc_disable(uint8_t port) noexcept
{
	SerialUSB.cdcSetConnected(false);
}

// This is called when data has been received
extern "C" void core_cdc_rx_notify(uint8_t port) noexcept
{
	SerialUSB.cdcRxNotify();
}

// This is called when the transmit buffer has been emptied
extern "C" void core_cdc_tx_empty_notify(uint8_t port) noexcept
{
	SerialUSB.cdcTxEmptyNotify();
}

// On the SAM4E and SAM4S we use a GPIO pin available to monitor the VBUS state
void core_vbus_off(CallbackParameter) noexcept
{
	SerialUSB.cdcSetConnected(false);
}

void SerialInit() noexcept
{
	SetPinFunction(APIN_Serial0_RXD, Serial0PinFunction);
	SetPinFunction(APIN_Serial0_TXD, Serial0PinFunction);
	SetPullup(APIN_Serial0_RXD, true);

	SetPinFunction(APIN_Serial1_RXD, Serial1PinFunction);
	SetPinFunction(APIN_Serial1_TXD, Serial1PinFunction);
	SetPullup(APIN_Serial1_RXD, true);
}

void SdhcInit() noexcept
{
	SetPinFunction(HcmciMclkPin, HsmciMclkPinFunction);
	for (Pin p : HsmciOtherPins)
	{
		SetPinFunction(p, HsmciOtherkPinsFunction);
	}
}


// Device initialisation
void DeviceInit() noexcept
{
	SerialInit();
	SdhcInit();
}

// End