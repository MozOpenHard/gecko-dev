/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


[Exposed=Worker]
interface WorkerNavigator {
};

WorkerNavigator implements NavigatorID;
WorkerNavigator implements NavigatorLanguage;
WorkerNavigator implements NavigatorOnLine;
WorkerNavigator implements NavigatorDataStore;

#ifdef MOZ_I2C_MANAGER
partial interface WorkerNavigator {
  [Throws]
  readonly attribute MozI2cManager mozI2c;
};
#endif // MOZ_I2C_MANAGER

#ifdef MOZ_GPIO_MANAGER
partial interface WorkerNavigator {
  [Throws]
  readonly attribute MozGpioManager mozGpio;
};
#endif // MOZ_GPIO_MANAGER