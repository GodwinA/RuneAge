/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Audiokinetic Wwise generated include file. Do not edit.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __WWISE_IDS_H__
#define __WWISE_IDS_H__

#include <AK/SoundEngine/Common/AkTypes.h>

namespace AK
{
    namespace EVENTS
    {
        static const AkUniqueID AMBIENCE_INSIDE = 3695937480U;
        static const AkUniqueID AMBIENCE_OUTSIDE = 3996256281U;
        static const AkUniqueID ARCANE_SPELL = 2637742288U;
        static const AkUniqueID CAST_SPELL = 4080882651U;
        static const AkUniqueID FIRE_SPELL = 509606434U;
        static const AkUniqueID LIGHTNING_SPELL = 7154402U;
        static const AkUniqueID ROCK_SPELL = 3371098341U;
        static const AkUniqueID WATER_SPELL = 3672226213U;
    } // namespace EVENTS

    namespace STATES
    {
        namespace LOCATION
        {
            static const AkUniqueID GROUP = 1176052424U;

            namespace STATE
            {
                static const AkUniqueID INSIDE = 3553349781U;
                static const AkUniqueID NONE = 748895195U;
                static const AkUniqueID OUTSIDE = 438105790U;
            } // namespace STATE
        } // namespace LOCATION

        namespace SHAPE
        {
            static const AkUniqueID GROUP = 3020443768U;

            namespace STATE
            {
                static const AkUniqueID NONE = 748895195U;
                static const AkUniqueID SPHERE = 3478592334U;
            } // namespace STATE
        } // namespace SHAPE

    } // namespace STATES

    namespace SWITCHES
    {
        namespace SHAPE
        {
            static const AkUniqueID GROUP = 3020443768U;

            namespace SWITCH
            {
                static const AkUniqueID CONE = 3720716440U;
                static const AkUniqueID PILLAR = 2582515549U;
                static const AkUniqueID RING = 2598345275U;
                static const AkUniqueID SHIELD = 1161967626U;
                static const AkUniqueID SPHERE = 3478592334U;
                static const AkUniqueID TORNADO = 2586554088U;
                static const AkUniqueID WALL = 2108779961U;
            } // namespace SWITCH
        } // namespace SHAPE

    } // namespace SWITCHES

    namespace GAME_PARAMETERS
    {
        static const AkUniqueID INTENSITY = 2470328564U;
        static const AkUniqueID RTPC_AREA_FADE_DISTANCE = 1799335409U;
    } // namespace GAME_PARAMETERS

    namespace BANKS
    {
        static const AkUniqueID INIT = 1355168291U;
        static const AkUniqueID SOUNDBANK = 1661994096U;
    } // namespace BANKS

    namespace BUSSES
    {
        static const AkUniqueID AMBIENCE = 85412153U;
        static const AkUniqueID AMBIENCE_INSIDE = 3695937480U;
        static const AkUniqueID AMBIENCE_OUTSIDE = 3996256281U;
        static const AkUniqueID MASTER_AUDIO_BUS = 3803692087U;
        static const AkUniqueID MUSIC = 3991942870U;
        static const AkUniqueID SFX = 393239870U;
    } // namespace BUSSES

    namespace AUX_BUSSES
    {
        static const AkUniqueID LARGEREVERB = 3684725634U;
        static const AkUniqueID MEDIUMREVERB = 894231404U;
        static const AkUniqueID REVERB = 348963605U;
        static const AkUniqueID SMALLREVERB = 2134960294U;
    } // namespace AUX_BUSSES

    namespace AUDIO_DEVICES
    {
        static const AkUniqueID NO_OUTPUT = 2317455096U;
        static const AkUniqueID SYSTEM = 3859886410U;
    } // namespace AUDIO_DEVICES

}// namespace AK

#endif // __WWISE_IDS_H__
