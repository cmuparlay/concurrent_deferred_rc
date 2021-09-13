/*
 * File: FRCConstants.h
 * Copyright 2014-2018 Terrain Data, Inc.
 *
 * This file is part of FRC, a fast reference counting library for C++
 * (see <https://github.com/terraindata/frc>).
 *
 * FRC is distributed under the MIT License, which is found in
 * COPYING.md in the repository.
 *
 * You should have received a copy of the MIT License
 * along with FRC.  If not, see <https://opensource.org/licenses/MIT>.
 */

#pragma once

#include "../../util/util.h"

namespace terrain
{
namespace frc
{
namespace detail
{

class FRCConstants
{
public:
    static constexpr sz pinSetSize = 128;
    static constexpr sz protectedBlockSize = 128; //must be pinSetSize

    static constexpr sz logBlockSize = 256;
    static constexpr sz logSize = sz(1) << 21; //16 MB
    static constexpr sz logBufferSize = sz(1) << 22; //must be a power of two (2MB)
    static constexpr sz logMask = logBufferSize - 1;

    static constexpr sz baseHelpInterval = 64;
    static constexpr sz maxLogSizeBeforeHelpIntervalReduction = logSize / 2; //logBlockSize * 16;
    static constexpr sz maxLogSizeBeforeBlockingHelpCall = logSize - 32 * logBlockSize;
    static constexpr float helpIntervalReductionConstant = (logSize -
            maxLogSizeBeforeHelpIntervalReduction) / baseHelpInterval;
    static constexpr sz numHelpAttemptsBeforeBlocking = 64;
    static constexpr sz numTryHelpCallsOnUnregister = 1024;

    static constexpr bool enableSemiDeferredDecrements = false;
    static constexpr bool enableCheckedDecrements = false;

    static constexpr sz busySignal = 1;

    static constexpr byte scan = 0;
    static constexpr byte sweep = 1;
};

} /* namespace detail */
} /* namespace frc */
} /* namespace terrain */
