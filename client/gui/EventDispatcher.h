/*
 * EventDispatcher.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

VCMI_LIB_NAMESPACE_BEGIN
class Point;
VCMI_LIB_NAMESPACE_END

class AEventsReceiver;
enum class MouseButton;
enum class EShortcut;

/// Class that receives events from event producers and dispatches it to UI elements that are interested in this event
class EventDispatcher
{
	using EventReceiversList = std::list<AEventsReceiver *>;

	/// list of UI elements that are interested in particular event
	EventReceiversList lclickable;
	EventReceiversList rclickable;
	EventReceiversList hoverable;
	EventReceiversList keyinterested;
	EventReceiversList motioninterested;
	EventReceiversList timeinterested;
	EventReceiversList wheelInterested;
	EventReceiversList doubleClickInterested;
	EventReceiversList textInterested;
	EventReceiversList panningInterested;

	EventReceiversList & getListForMouseButton(MouseButton button);

	void handleMouseButtonClick(EventReceiversList & interestedObjs, MouseButton btn, bool isPressed);

	template<typename Functor>
	void processLists(ui16 activityFlag, const Functor & cb);

public:
	/// add specified UI element as interested. Uses unnamed enum from AEventsReceiver for activity flags
	void activateElement(AEventsReceiver * elem, ui16 activityFlag);

	/// removes specified UI element as interested for specified activities
	void deactivateElement(AEventsReceiver * elem, ui16 activityFlag);

	/// Regular timer event
	void dispatchTimer(uint32_t msPassed);

	/// Shortcut events (e.g. keyboard keys)
	void dispatchShortcutPressed(const std::vector<EShortcut> & shortcuts);
	void dispatchShortcutReleased(const std::vector<EShortcut> & shortcuts);

	/// Mouse events
	void dispatchMouseButtonPressed(const MouseButton & button, const Point & position);
	void dispatchMouseButtonReleased(const MouseButton & button, const Point & position);
	void dispatchMouseScrolled(const Point & distance, const Point & position);
	void dispatchMouseDoubleClick(const Point & position);
	void dispatchMouseMoved(const Point & distance);

	void dispatchGesturePanningStarted(const Point & initialPosition);
	void dispatchGesturePanningEnded(const Point & initialPosition, const Point & finalPosition);
	void dispatchGesturePanning(const Point & initialPosition, const Point & currentPosition, const Point & lastUpdateDistance);
	void dispatchGesturePinch(const Point & initialPosition, double distance);

	/// Text input events
	void dispatchTextInput(const std::string & text);
	void dispatchTextEditing(const std::string & text);
};
