/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Func="mozilla::dom::gpio::GpioManager::PrefEnabled"]
interface MozGpioManager {
  void export(unsigned long pinNo);
  void unexport(unsigned long pinNo);
  void setDirection(unsigned long pinNo, boolean out);
  boolean getDirection(unsigned long pinNo);
  void setValue(unsigned long pinNo, unsigned long value);
  unsigned long getValue(unsigned long pinNo);
};
