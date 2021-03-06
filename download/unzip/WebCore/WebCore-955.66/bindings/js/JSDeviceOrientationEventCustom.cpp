/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSDeviceOrientationEvent.h"

#if ENABLE(DEVICE_ORIENTATION)

#include "DeviceOrientation.h"
#include "DeviceOrientationEvent.h"

using namespace JSC;

namespace WebCore {

JSValue JSDeviceOrientationEvent::alpha(ExecState* exec) const
{
    DeviceOrientationEvent* imp = static_cast<DeviceOrientationEvent*>(impl());
    if (!imp->orientation()->canProvideAlpha())
        return jsNull();
    return jsNumber(exec, imp->orientation()->alpha());
}

JSValue JSDeviceOrientationEvent::beta(ExecState* exec) const
{
    DeviceOrientationEvent* imp = static_cast<DeviceOrientationEvent*>(impl());
    if (!imp->orientation()->canProvideBeta())
        return jsNull();
    return jsNumber(exec, imp->orientation()->beta());
}

JSValue JSDeviceOrientationEvent::gamma(ExecState* exec) const
{
    DeviceOrientationEvent* imp = static_cast<DeviceOrientationEvent*>(impl());
    if (!imp->orientation()->canProvideGamma())
        return jsNull();
    return jsNumber(exec, imp->orientation()->gamma());
}

JSValue JSDeviceOrientationEvent::initDeviceOrientationEvent(ExecState* exec, const ArgList& args)
{
    const UString& typeArg = args.at(0).toString(exec);
    bool bubbles = args.at(1).toBoolean(exec);
    bool cancelable = args.at(2).toBoolean(exec);
    // If alpha, beta or gamma are null or undefined, mark them as not provided.
    // Otherwise, use the standard JavaScript conversion.
    bool alphaProvided = !args.at(3).isUndefinedOrNull();
    double alpha = args.at(3).toNumber(exec);
    bool betaProvided = !args.at(4).isUndefinedOrNull();
    double beta = args.at(4).toNumber(exec);
    bool gammaProvided = !args.at(5).isUndefinedOrNull();
    double gamma = args.at(5).toNumber(exec);
    RefPtr<DeviceOrientation> orientation = DeviceOrientation::create(alphaProvided, alpha, betaProvided, beta, gammaProvided, gamma);
    DeviceOrientationEvent* imp = static_cast<DeviceOrientationEvent*>(impl());
    imp->initDeviceOrientationEvent(ustringToAtomicString(typeArg), bubbles, cancelable, orientation.get());
    return jsUndefined();
}

} // namespace WebCore

#endif // ENABLE(DEVICE_ORIENTATION)
