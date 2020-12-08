#include "input.h"

void SdlInputHandler::rumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor)
{
    // Make sure the controller number is within our supported count
    if (controllerNumber >= MAX_GAMEPADS) {
        return;
    }

#if SDL_VERSION_ATLEAST(2, 0, 9)
    if (m_GamepadState[controllerNumber].controller != nullptr) {
        SDL_GameControllerRumble(m_GamepadState[controllerNumber].controller, lowFreqMotor, highFreqMotor, 30000);
    }
#else
    // Check if the controller supports haptics (and if the controller exists at all)
    SDL_Haptic* haptic = m_GamepadState[controllerNumber].haptic;
    if (haptic == nullptr) {
        return;
    }

    // Stop the last effect we played
    if (m_GamepadState[controllerNumber].hapticMethod == GAMEPAD_HAPTIC_METHOD_LEFTRIGHT) {
        if (m_GamepadState[controllerNumber].hapticEffectId >= 0) {
            SDL_HapticDestroyEffect(haptic, m_GamepadState[controllerNumber].hapticEffectId);
        }
    } else if (m_GamepadState[controllerNumber].hapticMethod == GAMEPAD_HAPTIC_METHOD_SIMPLERUMBLE) {
        SDL_HapticRumbleStop(haptic);
    }

    // If this callback is telling us to stop both motors, don't bother queuing a new effect
    if (lowFreqMotor == 0 && highFreqMotor == 0) {
        return;
    }

    if (m_GamepadState[controllerNumber].hapticMethod == GAMEPAD_HAPTIC_METHOD_LEFTRIGHT) {
        SDL_HapticEffect effect;
        SDL_memset(&effect, 0, sizeof(effect));
        effect.type = SDL_HAPTIC_LEFTRIGHT;

        // The effect should last until we are instructed to stop or change it
        effect.leftright.length = SDL_HAPTIC_INFINITY;

        // SDL haptics range from 0-32767 but XInput uses 0-65535, so divide by 2 to correct for SDL's scaling
        effect.leftright.large_magnitude = lowFreqMotor / 2;
        effect.leftright.small_magnitude = highFreqMotor / 2;

        // Play the new effect
        m_GamepadState[controllerNumber].hapticEffectId = SDL_HapticNewEffect(haptic, &effect);
        if (m_GamepadState[controllerNumber].hapticEffectId >= 0) {
            SDL_HapticRunEffect(haptic, m_GamepadState[controllerNumber].hapticEffectId, 1);
        }
    } else if (m_GamepadState[controllerNumber].hapticMethod == GAMEPAD_HAPTIC_METHOD_SIMPLERUMBLE) {
        SDL_HapticRumblePlay(haptic,
                             std::min(1.0, (GAMEPAD_HAPTIC_SIMPLE_HIFREQ_MOTOR_WEIGHT*highFreqMotor +
                                            GAMEPAD_HAPTIC_SIMPLE_LOWFREQ_MOTOR_WEIGHT*lowFreqMotor) / 65535.0),
                             SDL_HAPTIC_INFINITY);
    }
#endif
}

int SdlInputHandler::getAttachedGamepadMask()
{
    int count;
    int mask;

    if (!m_MultiController) {
        // Player 1 is always present in non-MC mode
        return 0x1;
    }

    count = mask = 0;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            mask |= (1 << count++);
        }
    }

    return mask;
}
