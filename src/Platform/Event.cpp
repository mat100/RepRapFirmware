/*
 * Event.cpp
 *
 *  Created on: 18 Oct 2021
 *      Author: David
 */

#include <Platform/Event.h>
#include <RepRapFirmware.h>
#include <ObjectModel/ObjectModel.h>
#include <ObjectModel/Variable.h>

Event *_ecv_null Event::eventsPending = nullptr;

inline Event::Event(Event *_ecv_null pnext, EventType et, uint16_t p_param, uint8_t devNum, CanAddress p_ba, const char *_ecv_array format, va_list vargs) noexcept
	: next(pnext), param(p_param), type(et), boardAddress(p_ba), deviceNumber(devNum), isBeingProcessed(false)
{
	text.vprintf(format, vargs);
}

// Queue an event, or release it if we have a similar event pending already. Returns true if the event was added, false if it was released.
/*static*/ bool Event::AddEvent(EventType et, uint16_t p_param, CanAddress p_ba, uint8_t devNum, const char *_ecv_array format, ...) noexcept
{
	va_list vargs;
	va_start(vargs, format);
	const bool ret = AddEventV(et, p_param, p_ba, devNum, format, vargs);
	va_end(vargs);
	return ret;
}

// Queue an event unless we have a similar event pending already. Returns true if the event was added.
// The event list is held in priority order, lowest numbered (highest priority) events first.
/*static*/ bool Event::AddEventV(EventType et, uint16_t p_param, uint8_t devNum, CanAddress p_ba, const char *_ecv_array format, va_list vargs) noexcept
{
	// Search for similar events already pending or being processed.
	// An event is 'similar' if it has the same type, device number and parameter even if the text is different.
	TaskCriticalSectionLocker lock;

	Event** pe = &eventsPending;
	while (*pe != nullptr && (et >= (*pe)->type || (*pe)->isBeingProcessed))		// while the next event in the list has same or higher priority than the new one
	{
		if (et == (*pe)->type && devNum == (*pe)->deviceNumber
#if SUPPORT_CAN_EXPANSION
			 && p_ba == (*pe)->boardAddress
#endif
			)
		{
			return false;						// there is a similar event already in the queue
		}
		pe = &((*pe)->next);
	}

	// We didn't find a similar event, so add the new one
	*pe = new Event(*pe, et, p_param, p_ba, devNum, format, vargs);
	return true;
}

// Get the highest priority event and mark it as being serviced
/*static*/ bool Event::StartProcessing() noexcept
{
	TaskCriticalSectionLocker lock;

	Event * const ev = eventsPending;
	if (ev == nullptr)
	{
		return false;
	}
	ev->isBeingProcessed = true;
	return true;
}

// Get the name of the macro that we run when this event occurs
/*static*/ void Event::GetMacroFileName(const StringRef& fname) noexcept
{
	const Event * const ep = eventsPending;
	if (ep != nullptr && ep->isBeingProcessed)
	{
		fname.copy(ep->type.ToString());
		fname.cat(".g");
	}
}

// Get the macro parameters for the current event, excluding the S parameter which the caller will add
/*static*/ void Event::GetParameters(VariableSet& vars) noexcept
{
	const Event * const ep = eventsPending;
	if (ep != nullptr && ep->isBeingProcessed)
	{
		vars.InsertNewParameter("D", ExpressionValue((int32_t)(ep->deviceNumber)));
#if SUPPORT_CAN_EXPANSION
		vars.InsertNewParameter("B", ExpressionValue((int32_t)(ep->boardAddress)));
#endif
		vars.InsertNewParameter("P", ExpressionValue((int32_t)(ep->param)));
	}
}

// Get the default action for the current event
/*static*/ PrintPausedReason Event::GetDefaultPauseReason() noexcept
{
	const Event * const ep = eventsPending;
	if (ep != nullptr && ep->isBeingProcessed)
	{
		switch (ep->type.RawValue())
		{
		case EventType::heater_fault:
			return PrintPausedReason::heaterFault;

		case EventType::filament_error:
			return PrintPausedReason::filamentError;

		case EventType::driver_error:
			return PrintPausedReason::driverError;

		default:
			break;
		}
	}
	return PrintPausedReason::dontPause;
}

// Mark the highest priority event as completed
/*static*/ void Event::FinishedProcessing() noexcept
{
	TaskCriticalSectionLocker lock;

	const Event *ev = eventsPending;
	if (ev != nullptr && ev->isBeingProcessed)
	{
		eventsPending = ev->next;
		delete ev;
	}
}

// Get a description of the current event
/*static*/ MessageType Event::GetTextDescription(const StringRef& str) noexcept
{
	const Event * const ep = eventsPending;
	if (ep != nullptr && ep->isBeingProcessed)
	{
		switch (ep->type.RawValue())
		{
		case EventType::heater_fault:
			{
				const char *_ecv_array heaterFaultText = HeaterFaultText[max<size_t>(ep->param, ARRAY_SIZE(HeaterFaultText) - 1)];
				str.printf("Heater %u fault: %s%s", ep->deviceNumber, heaterFaultText, ep->text.c_str());
			}
			return ErrorMessage;

		case EventType::filament_error:
			str.printf("Filament error on extruder %u: %s", ep->deviceNumber, FilamentSensorStatus(ep->param).ToString());
			return ErrorMessage;

		case EventType::driver_error:
#if SUPPORT_CAN_EXPANSION
			str.printf("Driver %u.%u error: %s", ep->boardAddress, ep->deviceNumber, ep->text.c_str());
#else
			str.printf("Driver %u error: %s", ep->deviceNumber, ep->text.c_str());
#endif
			return ErrorMessage;

		case EventType::driver_warning:
#if SUPPORT_CAN_EXPANSION
			str.printf("Driver %u.%u warning: %s", ep->boardAddress, ep->deviceNumber, ep->text.c_str());
#else
			str.printf("Driver %u warning: %s", ep->deviceNumber, ep->text.c_str());
#endif
			return WarningMessage;

		case EventType::driver_stall:
#if SUPPORT_CAN_EXPANSION
			str.printf("Driver %u.%u stall", ep->boardAddress, ep->deviceNumber);
#else
			str.printf("Driver %u stall", ep->deviceNumber);
#endif
			return WarningMessage;

		case EventType::main_board_power_fail:
			// This does not currently generate an event, so no text
			return ErrorMessage;

		case EventType::mcu_temperature_warning:
#if SUPPORT_CAN_EXPANSION
			str.printf("MCU temperature warning from board %u: temperature %.1fC", ep->boardAddress, (double)((float)ep->param/10));
#else
			str.printf("MCU temperature warning: temperature %.1fC", (double)((float)ep->param/10));
#endif
			return WarningMessage;
		}
	}
	str.copy("Internal error in Event");
	return ErrorMessage;
}

// End
