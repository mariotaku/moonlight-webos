#include "input.h"

#include <Limelight.h>
#include <SDL.h>
#include "streaming/streamutils.h"

void SdlInputHandler::handleMouseButtonEvent(SDL_MouseButtonEvent* event)
{
    int button;

    if (event->which == SDL_TOUCH_MOUSEID) {
        // Ignore synthetic mouse events
        return;
    }
    else if (!isCaptureActive()) {
        if (event->button == SDL_BUTTON_LEFT && event->state == SDL_RELEASED &&
                isMouseInVideoRegion(event->x, event->y)) {
            // Capture the mouse again if clicked when unbound.
            // We start capture on left button released instead of
            // pressed to avoid sending an errant mouse button released
            // event to the host when clicking into our window (since
            // the pressed event was consumed by this code).
            setCaptureActive(true);
        }

        // Not capturing
        return;
    }
    else if (m_AbsoluteMouseMode && !isMouseInVideoRegion(event->x, event->y) && event->state == SDL_PRESSED) {
        // Ignore button presses outside the video region, but allow button releases
        return;
    }

    switch (event->button)
    {
        case SDL_BUTTON_LEFT:
            button = BUTTON_LEFT;
            break;
        case SDL_BUTTON_MIDDLE:
            button = BUTTON_MIDDLE;
            break;
        case SDL_BUTTON_RIGHT:
            button = BUTTON_RIGHT;
            break;
        case SDL_BUTTON_X1:
            button = BUTTON_X1;
            break;
        case SDL_BUTTON_X2:
            button = BUTTON_X2;
            break;
        default:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Unhandled button event: %d",
                        event->button);
            return;
    }

    if (m_SwapMouseButtons) {
        if (button == BUTTON_RIGHT)
            button = BUTTON_LEFT;
        else if (button == BUTTON_LEFT)
            button = BUTTON_RIGHT;
    }

    // Ensure any pending mouse position update has been sent before we send
    // our button event. This ensures that the cursor is in the correct location
    // when the button event is issued.
    //
    // On platforms like macOS, the mouse doesn't track when the window isn't
    // focused. When we gain focus via mouse click, we immediately get a mouse
    // move event and a mouse button event. If we don't flush here, the button
    // will probably arrive before the mouse timer issues the position update.
    flushMousePositionUpdate();

    LiSendMouseButtonEvent(event->state == SDL_PRESSED ?
                               BUTTON_ACTION_PRESS :
                               BUTTON_ACTION_RELEASE,
                           button);
}

void SdlInputHandler::updateMousePositionReport(int mouseX, int mouseY)
{
    int windowWidth, windowHeight;

    // Call SDL_GetWindowSize() before entering the spinlock
    SDL_GetWindowSize(m_Window, &windowWidth, &windowHeight);

    SDL_AtomicLock(&m_MousePositionLock);
    m_MousePositionReport.x = mouseX;
    m_MousePositionReport.y = mouseY;
    m_MousePositionReport.windowWidth = windowWidth;
    m_MousePositionReport.windowHeight = windowHeight;
    SDL_AtomicUnlock(&m_MousePositionLock);
    SDL_AtomicSet(&m_MousePositionUpdated, 1);
}

void SdlInputHandler::flushMousePositionUpdate()
{
    bool hasNewPosition = SDL_AtomicSet(&m_MousePositionUpdated, 0) != 0;
    if (hasNewPosition) {
        // If the lock is held now, the main thread is trying to update
        // the mouse position. We'll pick up the new position next time.
        if (SDL_AtomicTryLock(&m_MousePositionLock)) {
            SDL_Rect src, dst;
            bool mouseInVideoRegion;

            src.x = src.y = 0;
            src.w = m_StreamWidth;
            src.h = m_StreamHeight;

            dst.x = dst.y = 0;
            dst.w = m_MousePositionReport.windowWidth;
            dst.h = m_MousePositionReport.windowHeight;

            // Use the stream and window sizes to determine the video region
            StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

            mouseInVideoRegion = isMouseInVideoRegion(m_MousePositionReport.x,
                                                      m_MousePositionReport.y,
                                                      m_MousePositionReport.windowWidth,
                                                      m_MousePositionReport.windowHeight);

            // Clamp motion to the video region
            short x = qMin(qMax(m_MousePositionReport.x - dst.x, 0), dst.w);
            short y = qMin(qMax(m_MousePositionReport.y - dst.y, 0), dst.h);

            // Release the spinlock to unblock the main thread
            SDL_AtomicUnlock(&m_MousePositionLock);

            // Send the mouse position update if one of the following is true:
            // a) it is in the video region now
            // b) it just left the video region (to ensure the mouse is clamped to the video boundary)
            // c) a mouse button is still down from before the cursor left the video region (to allow smooth dragging)
            Uint32 buttonState = SDL_GetMouseState(nullptr, nullptr);
            if (buttonState == 0) {
                m_PendingMouseButtonsAllUpOnVideoRegionLeave = false;
            }
            if (mouseInVideoRegion || m_MouseWasInVideoRegion || m_PendingMouseButtonsAllUpOnVideoRegionLeave) {
                LiSendMousePositionEvent(x, y, dst.w, dst.h);
            }

            // Adjust the cursor visibility if applicable
            if (mouseInVideoRegion ^ m_MouseWasInVideoRegion) {
                // We must push an event for the main thread to process, because it's not safe
                // to directly can SDL_ShowCursor() on the arbitrary thread on which this timer
                // executes.
                SDL_Event event;
                event.type = SDL_USEREVENT;
                event.user.code = mouseInVideoRegion ? SDL_CODE_HIDE_CURSOR : SDL_CODE_SHOW_CURSOR;
                SDL_PushEvent(&event);

                if (!mouseInVideoRegion && buttonState != 0) {
                    // If we still have a button pressed on leave, wait for that to come up
                    // before we stop sending mouse position events.
                    m_PendingMouseButtonsAllUpOnVideoRegionLeave = true;
                }
            }

            m_MouseWasInVideoRegion = mouseInVideoRegion;
        }
    }
}

void SdlInputHandler::handleMouseMotionEvent(SDL_MouseMotionEvent* event)
{
    if (!isCaptureActive()) {
        // Not capturing
        return;
    }
    else if (event->which == SDL_TOUCH_MOUSEID) {
        // Ignore synthetic mouse events
        return;
    }

    // Batch until the next mouse polling window or we'll get awful
    // input lag everything except GFE 3.14 and 3.15.
    if (m_AbsoluteMouseMode) {
        updateMousePositionReport(event->x, event->y);
    }
    else {
        SDL_AtomicAdd(&m_MouseDeltaX, event->xrel);
        SDL_AtomicAdd(&m_MouseDeltaY, event->yrel);
    }
}

void SdlInputHandler::handleMouseWheelEvent(SDL_MouseWheelEvent* event)
{
    if (!isCaptureActive()) {
        // Not capturing
        return;
    }
    else if (event->which == SDL_TOUCH_MOUSEID) {
        // Ignore synthetic mouse events
        return;
    }

    if (m_AbsoluteMouseMode) {
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        if (!isMouseInVideoRegion(mouseX, mouseY)) {
            // Ignore scroll events outside the video region
            return;
        }
    }

    if (event->y != 0) {
        LiSendScrollEvent((signed char)event->y);
    }
}

bool SdlInputHandler::isMouseInVideoRegion(int mouseX, int mouseY, int windowWidth, int windowHeight)
{
    SDL_Rect src, dst;

    if (windowWidth < 0 || windowHeight < 0) {
        SDL_GetWindowSize(m_Window, &windowWidth, &windowHeight);
    }

    src.x = src.y = 0;
    src.w = m_StreamWidth;
    src.h = m_StreamHeight;

    dst.x = dst.y = 0;
    dst.w = windowWidth;
    dst.h = windowHeight;

    // Use the stream and window sizes to determine the video region
    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

    return (mouseX >= dst.x && mouseX <= dst.x + dst.w) &&
           (mouseY >= dst.y && mouseY <= dst.y + dst.h);
}

Uint32 SdlInputHandler::mouseMoveTimerCallback(Uint32 interval, void *param)
{
    auto me = reinterpret_cast<SdlInputHandler*>(param);

    short deltaX = (short)SDL_AtomicSet(&me->m_MouseDeltaX, 0);
    short deltaY = (short)SDL_AtomicSet(&me->m_MouseDeltaY, 0);

    if (deltaX != 0 || deltaY != 0) {
        LiSendMouseMoveEvent(deltaX, deltaY);
    }

    // Send mouse position updates if applicable
    me->flushMousePositionUpdate();

#ifdef Q_OS_WIN32
    // See comment in SdlInputHandler::notifyMouseLeave()
    if (me->m_AbsoluteMouseMode && me->m_PendingMouseLeaveButtonUp != 0 && me->isCaptureActive()) {
        int mouseX, mouseY;
        int windowX, windowY;
        Uint32 mouseState = SDL_GetGlobalMouseState(&mouseX, &mouseY);
        SDL_GetWindowPosition(me->m_Window, &windowX, &windowY);

        // If the button is now up, send the synthetic mouse up event
        if ((mouseState & SDL_BUTTON(me->m_PendingMouseLeaveButtonUp)) == 0) {
            SDL_Event event;

            event.button.type = SDL_MOUSEBUTTONUP;
            event.button.timestamp = SDL_GetTicks();
            event.button.windowID = SDL_GetWindowID(me->m_Window);
            event.button.which = 0;
            event.button.button = me->m_PendingMouseLeaveButtonUp;
            event.button.state = SDL_RELEASED;
            event.button.clicks = 1;
            event.button.x = mouseX - windowX;
            event.button.y = mouseY - windowY;
            SDL_PushEvent(&event);

            me->m_PendingMouseLeaveButtonUp = 0;
        }
    }
#endif

    return interval;
}
