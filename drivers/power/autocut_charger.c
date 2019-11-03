/*
 * drivers/power/autocut_charger.c
 *
 * AutoCut Charger.
 *
 * Copyright (C) 2019, Ryan Andri <https://github.com/ryan-andri>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. For more details, see the GNU
 * General Public License included with the Linux kernel or available
 * at www.gnu.org/licenses
 */

#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>

static struct delayed_work autocut_charger_work;

static int not_ready = 0;

static int battery_charging_enabled(struct power_supply *batt_psy, bool enable)
{
	const union power_supply_propval ret = {enable,};

	if (batt_psy->set_property)
		return batt_psy->set_property(batt_psy,
				POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED,
				&ret);

	return 1;
}

static void autocut_charger_worker(struct work_struct *work)
{
	struct power_supply *batt_psy = power_supply_get_by_name("battery");
	struct power_supply *usb_psy = power_supply_get_by_name("usb");
	union power_supply_propval present = {0,}, charging_enabled = {0,};
	union power_supply_propval status, bat_percent;
	int ms_timer = 1000, rc = 0;

	/* re-schedule and increase the timer if not ready */
	if (!batt_psy->get_property || !usb_psy->get_property) {
		not_ready++;

		if (not_ready >= 10) {
			cancel_delayed_work_sync(&autocut_charger_work);
			return;
		}

		ms_timer = 10000;
		goto reschedule;
	}

	not_ready = 0;

	/* get values from /sys/class/power_supply/battery */
	batt_psy->get_property(batt_psy,
			POWER_SUPPLY_PROP_STATUS, &status);
	batt_psy->get_property(batt_psy,
			POWER_SUPPLY_PROP_CAPACITY, &bat_percent);
	batt_psy->get_property(batt_psy,
			POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &charging_enabled);

	/* get values from /sys/class/power_supply/usb */
	usb_psy->get_property(usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &present);

	if (status.intval == POWER_SUPPLY_STATUS_CHARGING || present.intval) {
		if (charging_enabled.intval && bat_percent.intval >= 100) {
			rc = battery_charging_enabled(batt_psy, 0);
			if (rc)
				pr_err("%s: Failed to disable battery charging!\n", __func__);
		} else if (!charging_enabled.intval && bat_percent.intval < 100) {
			rc = battery_charging_enabled(batt_psy, 1);
			if (rc)
				pr_err("%s: Failed to enable battery charging!\n", __func__);
		}
	} else if (bat_percent.intval < 100 || !present.intval) {
		if (!charging_enabled.intval) {
			rc = battery_charging_enabled(batt_psy, 1);
			if (rc)
				pr_err("%s: Failed to enable battery charging!\n", __func__);
		}
	}

reschedule:
	schedule_delayed_work(&autocut_charger_work, msecs_to_jiffies(ms_timer));
}

static int __init autocut_charger_init(void)
{
	if (!strstr(saved_command_line, "androidboot.mode=charger")) {
		INIT_DELAYED_WORK(&autocut_charger_work, autocut_charger_worker);
		/* start worker in at least 20 seconds after boot completed */
		schedule_delayed_work(&autocut_charger_work, msecs_to_jiffies(20000));
		pr_info("%s: Initialized.\n", __func__);
	}

	return 0;
}
late_initcall(autocut_charger_init);

static void __exit autocut_charger_exit(void)
{
	if (!strstr(saved_command_line, "androidboot.mode=charger"))
		cancel_delayed_work_sync(&autocut_charger_work);
}
module_exit(autocut_charger_exit);
