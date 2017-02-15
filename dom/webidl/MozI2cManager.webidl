/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Exposed=Worker, Func="mozilla::dom::i2c::I2cManager::PrefEnabled"]
interface MozI2cManager {
  [Throws]
  void open(octet deviceNo);
  [Throws]
  void setDeviceAddress(octet deviceNo, octet deviceAddress);
  [Throws]
  void write(octet deviceNo, octet deviceAddress, octet command, unsigned short value, boolean isOctet);
  [Throws]
  unsigned short read(octet deviceNo, octet deviceAddress, octet command, boolean isOctet);
};
