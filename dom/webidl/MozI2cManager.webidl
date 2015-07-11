/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Func="mozilla::dom::i2c::I2cManager::PrefEnabled"]
interface MozI2cManager {
  void open(octet deviceNo);
  void setDeviceAddress(octet deviceNo, octet deviceAddress);
  void write(octet deviceNo, octet command, unsigned short value, boolean isOctet);
  unsigned short read(octet deviceNo, octet command, boolean isOctet);
};
