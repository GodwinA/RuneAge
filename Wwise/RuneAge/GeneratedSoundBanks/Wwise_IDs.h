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
        static const AkUniqueID AMBIENCE_INSIDE_CAVE = 1476656008U;
        static const AkUniqueID AMBIENCE_INSIDE_ROOM = 2032244766U;
        static const AkUniqueID AMBIENCE_OUTSIDE = 3996256281U;
        static const AkUniqueID ARCANE_SPELL = 2637742288U;
        static const AkUniqueID CHARACTER_CAST_SPELL = 4231402215U;
        static const AkUniqueID CHARACTER_DIE = 2990357609U;
        static const AkUniqueID CHARACTER_FOOTSTEPS = 2775932802U;
        static const AkUniqueID CHARACTER_JUMP = 118266349U;
        static const AkUniqueID CHARACTER_KILL = 3148586457U;
        static const AkUniqueID CHARACTER_LAND = 2984435986U;
        static const AkUniqueID CHARACTER_LOW_HEALTH = 1414116942U;
        static const AkUniqueID FIRE_SPELL = 509606434U;
        static const AkUniqueID LIGHTNING_SPELL = 7154402U;
        static const AkUniqueID ROCK_SPELL = 3371098341U;
        static const AkUniqueID WATER_SPELL = 3672226213U;
    } // namespace EVENTS

    namespace STATES
    {
        namespace HEALTH
        {
            static const AkUniqueID GROUP = 3677180323U;

            namespace STATE
            {
                static const AkUniqueID CRITICAL = 2534871658U;
                static const AkUniqueID LOW = 545371365U;
                static const AkUniqueID NONE = 748895195U;
                static const AkUniqueID NORMAL = 1160234136U;
            } // namespace STATE
        } // namespace HEALTH

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
        namespace LOCOMOTION
        {
            static const AkUniqueID GROUP = 556887514U;

            namespace SWITCH
            {
                static const AkUniqueID CROUCHING = 499013305U;
                static const AkUniqueID RUNNING = 3863236874U;
                static const AkUniqueID WALKING = 340271938U;
            } // namespace SWITCH
        } // namespace LOCOMOTION

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

        namespace SURFACE
        {
            static const AkUniqueID GROUP = 1834394558U;

            namespace SWITCH
            {
                static const AkUniqueID GRASS = 4248645337U;
                static const AkUniqueID STONE = 1216965916U;
                static const AkUniqueID WOOD = 2058049674U;
            } // namespace SWITCH
        } // namespace SURFACE

    } // namespace SWITCHES

    namespace GAME_PARAMETERS
    {
        static const AkUniqueID RTPC_AREA_FADE_DISTANCE = 1799335409U;
        static const AkUniqueID RTPC_INTENSITY = 2582146478U;
        static const AkUniqueID RTPC_MASTER_VOLUME = 2564988978U;
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
