/*
 * Copyright 2007-2010 by Ping Cheng, Wacom. <pingc@wacom.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <wacom-properties.h>

#include "xf86Wacom.h"
#include "wcmFilter.h"
#include <exevents.h>
#include <xf86_OSproc.h>

#ifndef XI_PROP_DEVICE_NODE
#define XI_PROP_DEVICE_NODE "Device Node"
#endif
#ifndef XI_PROP_PRODUCT_ID
#define XI_PROP_PRODUCT_ID "Device Product ID"
#endif

static void wcmBindToSerial(InputInfoPtr pInfo, unsigned int serial);

/*****************************************************************************
* wcmDevSwitchModeCall --
*****************************************************************************/

int wcmDevSwitchModeCall(InputInfoPtr pInfo, int mode)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;

	DBG(3, priv, "to mode=%d\n", mode);

	/* Pad is always in absolute mode.*/
	if (IsPad(priv))
		return (mode == Absolute) ? Success : XI_BadMode;

	if ((mode == Absolute) && !is_absolute(pInfo))
		set_absolute(pInfo, TRUE);
	else if ((mode == Relative) && is_absolute(pInfo))
		set_absolute(pInfo, FALSE);
	else if ( (mode != Absolute) && (mode != Relative))
	{
		DBG(10, priv, "invalid mode=%d\n", mode);
		return XI_BadMode;
	}

	return Success;
}

/*****************************************************************************
* wcmDevSwitchMode --
*****************************************************************************/

int wcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
	InputInfoPtr pInfo = (InputInfoPtr)dev->public.devicePrivate;
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;

	DBG(3, priv, "dev=%p mode=%d\n",
		(void *)dev, mode);
#endif
	/* Share this call with sendAButton in wcmCommon.c */
	return wcmDevSwitchModeCall(pInfo, mode);
}

Atom prop_devnode;
Atom prop_rotation;
Atom prop_tablet_area;
Atom prop_pressurecurve;
Atom prop_serials;
Atom prop_serial_binding;
Atom prop_strip_buttons;
Atom prop_wheel_buttons;
Atom prop_tv_resolutions;
Atom prop_cursorprox;
Atom prop_threshold;
Atom prop_suppress;
Atom prop_touch;
Atom prop_gesture;
Atom prop_gesture_param;
Atom prop_hover;
Atom prop_tooltype;
Atom prop_btnactions;
Atom prop_product_id;
#ifdef DEBUG
Atom prop_debuglevels;
#endif

/**
 * Registers a property for the input device. This function registers
 * the property name atom, as well as creates the property itself.
 * At creation, the property values are initialized from the 'values'
 * array. The device property is marked as non-deletable.
 * Initialization values are always to be provided by means of an
 * array of 32 bit integers, regardless of 'format'
 *
 * @param dev Pointer to device structure
 * @param name Name of device property
 * @param type Type of the property
 * @param format Format of the property (8/16/32)
 * @param nvalues Number of values in the property
 * @param values Pointer to 32 bit integer array of initial property values
 * @return Atom handle of property name
 */
static Atom InitWcmAtom(DeviceIntPtr dev, const char *name, Atom type, int format, int nvalues, int *values)
{
	int i;
	Atom atom;
	uint8_t val_8[WCM_MAX_BUTTONS];
	uint16_t val_16[WCM_MAX_BUTTONS];
	uint32_t val_32[WCM_MAX_BUTTONS];
	pointer converted = val_32;

	for (i = 0; i < nvalues; i++)
	{
		switch(format)
		{
			case 8:  val_8[i]  = values[i]; break;
			case 16: val_16[i] = values[i]; break;
			case 32: val_32[i] = values[i]; break;
		}
	}

	switch(format)
	{
		case 8: converted = val_8; break;
		case 16: converted = val_16; break;
		case 32: converted = val_32; break;
	}

	atom = MakeAtom(name, strlen(name), TRUE);
	XIChangeDeviceProperty(dev, atom, type, format,
			PropModeReplace, nvalues,
			converted, FALSE);
	XISetDevicePropertyDeletable(dev, atom, FALSE);
	return atom;
}

void InitWcmDeviceProperties(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	WacomCommonPtr common = priv->common;
	int values[WCM_MAX_BUTTONS];
	int nbuttons;

	DBG(10, priv, "\n");

	prop_devnode = MakeAtom(XI_PROP_DEVICE_NODE, strlen(XI_PROP_DEVICE_NODE), TRUE);
	XIChangeDeviceProperty(pInfo->dev, prop_devnode, XA_STRING, 8,
				PropModeReplace, strlen(common->device_path),
				common->device_path, FALSE);
	XISetDevicePropertyDeletable(pInfo->dev, prop_devnode, FALSE);

	if (!IsPad(priv)) {
		values[0] = priv->topX;
		values[1] = priv->topY;
		values[2] = priv->bottomX;
		values[3] = priv->bottomY;
		prop_tablet_area = InitWcmAtom(pInfo->dev, WACOM_PROP_TABLET_AREA, XA_INTEGER, 32, 4, values);
	}

	values[0] = common->wcmRotate;
	if (!IsPad(priv)) {
		prop_rotation = InitWcmAtom(pInfo->dev, WACOM_PROP_ROTATION, XA_INTEGER, 8, 1, values);
	}

	if (IsPen(priv) || IsTouch(priv)) {
		values[0] = priv->nPressCtrl[0];
		values[1] = priv->nPressCtrl[1];
		values[2] = priv->nPressCtrl[2];
		values[3] = priv->nPressCtrl[3];
		prop_pressurecurve = InitWcmAtom(pInfo->dev, WACOM_PROP_PRESSURECURVE, XA_INTEGER, 32, 4, values);
	}

	values[0] = common->tablet_id;
	values[1] = priv->old_serial;
	values[2] = priv->old_device_id;
	values[3] = priv->cur_serial;
	values[4] = priv->cur_device_id;
	prop_serials = InitWcmAtom(pInfo->dev, WACOM_PROP_SERIALIDS, XA_INTEGER, 32, 5, values);

	values[0] = priv->serial;
	prop_serial_binding = InitWcmAtom(pInfo->dev, WACOM_PROP_SERIAL_BIND, XA_INTEGER, 32, 1, values);

	if (IsCursor(priv)) {
		values[0] = common->wcmCursorProxoutDist;
		prop_cursorprox = InitWcmAtom(pInfo->dev, WACOM_PROP_PROXIMITY_THRESHOLD, XA_INTEGER, 32, 1, values);
	}

	values[0] = (!common->wcmMaxZ) ? 0 : common->wcmThreshold;
	prop_threshold = InitWcmAtom(pInfo->dev, WACOM_PROP_PRESSURE_THRESHOLD, XA_INTEGER, 32, 1, values);

	values[0] = common->wcmSuppress;
	values[1] = common->wcmRawSample;
	prop_suppress = InitWcmAtom(pInfo->dev, WACOM_PROP_SAMPLE, XA_INTEGER, 32, 2, values);

	values[0] = common->wcmTouch;
	prop_touch = InitWcmAtom(pInfo->dev, WACOM_PROP_TOUCH, XA_INTEGER, 8, 1, values);

	if (IsStylus(priv)) {
		values[0] = !common->wcmTPCButton;
		prop_hover = InitWcmAtom(pInfo->dev, WACOM_PROP_HOVER, XA_INTEGER, 8, 1, values);
	}

	values[0] = common->wcmGesture;
	prop_gesture = InitWcmAtom(pInfo->dev, WACOM_PROP_ENABLE_GESTURE, XA_INTEGER, 8, 1, values);

	values[0] = common->wcmGestureParameters.wcmZoomDistance;
	values[1] = common->wcmGestureParameters.wcmScrollDistance;
	values[2] = common->wcmGestureParameters.wcmTapTime;
	prop_gesture_param = InitWcmAtom(pInfo->dev, WACOM_PROP_GESTURE_PARAMETERS, XA_INTEGER, 32, 3, values);

	values[0] = MakeAtom(pInfo->type_name, strlen(pInfo->type_name), TRUE);
	prop_tooltype = InitWcmAtom(pInfo->dev, WACOM_PROP_TOOL_TYPE, XA_ATOM, 32, 1, values);


	/* default to no actions */
	nbuttons = min(max(priv->nbuttons + 4, 7), WCM_MAX_BUTTONS);
	memset(values, 0, sizeof(values));
	prop_btnactions = InitWcmAtom(pInfo->dev, WACOM_PROP_BUTTON_ACTIONS, XA_ATOM, 32, nbuttons, values);

	if (IsPad(priv)) {
		memset(values, 0, sizeof(values));
		prop_strip_buttons = InitWcmAtom(pInfo->dev, WACOM_PROP_STRIPBUTTONS, XA_ATOM, 32, 4, values);
	}

	if (IsPad(priv) || IsCursor(priv))
	{
		memset(values, 0, sizeof(values));
		prop_wheel_buttons = InitWcmAtom(pInfo->dev, WACOM_PROP_WHEELBUTTONS, XA_ATOM, 32, 6, values);
	}

	values[0] = common->vendor_id;
	values[1] = common->tablet_id;
	prop_product_id = InitWcmAtom(pInfo->dev, XI_PROP_PRODUCT_ID, XA_INTEGER, 32, 2, values);

#ifdef DEBUG
	values[0] = priv->debugLevel;
	values[1] = common->debugLevel;
	prop_debuglevels = InitWcmAtom(pInfo->dev, WACOM_PROP_DEBUGLEVELS, XA_INTEGER, 8, 2, values);
#endif
}

/* Returns the offset of the property in the list given. If the property is
 * not found, a negative error code is returned. */
static int wcmFindProp(Atom property, Atom *prop_list, int nprops)
{
	int i;

	/* check all properties used for button actions */
	for (i = 0; i < nprops; i++)
		if (prop_list[i] == property)
			break;

	if (i >= nprops)
		return -BadAtom;

	return i;
}

/**
 * Obtain a pointer to the the handler and action list for a given Action
 * property. This function searches the button, wheel, and strip property
 * handler lists.
 *
 * @param priv          The device whose handler lists should be searched
 * @param property      The Action property that should be searched for
 * @param[out] handler  Returns a pointer to the property's handler
 * @param[out] action   Returns a pointer to the property's action list
 */
static void wcmFindActionHandler(WacomDevicePtr priv, Atom property, Atom **handler, unsigned int (**action)[256])
{
	int offset;

	offset = wcmFindProp(property, priv->btn_actions, ARRAY_SIZE(priv->btn_actions));
	if (offset >=0)
	{
		*handler = &priv->btn_actions[offset];
		*action  = &priv->keys[offset];
		return;
	}

	offset = wcmFindProp(property, priv->wheel_actions, ARRAY_SIZE(priv->wheel_actions));
	if (offset >= 0)
	{
		*handler = &priv->wheel_actions[offset];
		*action  = &priv->wheel_keys[offset];
		return;
	}

	offset = wcmFindProp(property, priv->strip_actions, ARRAY_SIZE(priv->strip_actions));
	if (offset < 0)
	{
		*handler = &priv->strip_actions[offset];
		*action  = &priv->strip_keys[offset];
		return;
	}
}

static int wcmCheckActionProperty(Atom property, XIPropertyValuePtr prop)
{
	CARD32 *data;
	int j;

	if (!property)
		return Success;

	if (prop->size >= 255 || prop->format != 32 || prop->type != XA_INTEGER)
		return BadMatch;

	data = (CARD32*)prop->data;

	for (j = 0; j < prop->size; j++)
	{
		int code = data[j] & AC_CODE;
		int type = data[j] & AC_TYPE;

		switch(type)
		{
			case AC_KEY:
				break;
			case AC_BUTTON:
				if (code > WCM_MAX_BUTTONS)
					return BadValue;
				break;
			case AC_DISPLAYTOGGLE:
			case AC_MODETOGGLE:
				break;
			default:
				return BadValue;
		}
	}

	return Success;
}

/**
 * An 'Action' property (such as an element of "Wacom Button Actions")
 * defines an action to be performed for some event. The property is
 * validated, and then saved for later use. Both the property itself
 * (as 'handler') and the data it references (as 'action') are saved.
 *
 * @param dev        The device being modified
 * @param property   The Action property being set
 * @param prop       The data contained in 'property'
 * @param checkonly  'true' if the property should only be checked for validity
 * @param handler    Pointer to the handler that must be updated
 * @param action     Pointer to the action list that must be updated
 */
static int wcmSetActionProperty(DeviceIntPtr dev, Atom property,
				XIPropertyValuePtr prop, BOOL checkonly,
				Atom *handler, unsigned int (*action)[256])
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int rc, i;

	DBG(10, priv, "\n");

	rc = wcmCheckActionProperty(property, prop);
	if (rc != Success)
		return rc;

	if (!checkonly && prop)
	{
		memset(action, 0, sizeof(action));
		for (i = 0; i < prop->size; i++)
			(*action)[i] = ((unsigned int*)prop->data)[i];
		*handler = property;
	}

	return Success;
}

static int wcmCheckActionsProperty(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	XIPropertyValuePtr val;
	Atom *values = (Atom*)prop->data;
	int i;

	if (prop->format != 32 || prop->type != XA_ATOM)
		return BadMatch;

	for (i = 0; i < prop->size; i++)
	{
		if (!values[i])
			continue;

		if (values[i] == property || !ValidAtom(values[i]))
			return BadValue;

		if (XIGetDeviceProperty(pInfo->dev, values[i], &val) != Success)
			return BadValue;
	}

	return Success;
}

/**
 * An 'Actions' property (such as "Wacom Button Actions") stores a list of
 * 'Action' properties that define an action to be performed. This function
 * goes through the list of actions and saves each one.
 *
 * @param dev        The device being modified
 * @param property   The Actions property being set
 * @param prop       The data contained in 'property'
 * @param checkonly  'true' if the property should only be checked for validity
 * @param size       Expected number of elements in 'prop'
 * @param handlers   List of handlers that must be updated
 * @param actions    List of actions that must be updated
 */
static int wcmSetActionsProperty(DeviceIntPtr dev, Atom property,
                                 XIPropertyValuePtr prop, BOOL checkonly,
                                 int size, Atom* handlers, unsigned int (*actions)[256])
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	int rc;

	if (prop->size != size)
		return BadValue;

	rc = wcmCheckActionsProperty(dev, property, prop);
	if (rc != Success)
		return rc;

	if (!checkonly)
	{
		int i;

		for (i = 0; i < prop->size; i++)
		{
			Atom subproperty = ((Atom*)prop->data)[i];
			XIPropertyValuePtr subprop;

			XIGetDeviceProperty(dev, subproperty, &subprop);
			wcmSetActionProperty(dev, subproperty, subprop, checkonly, &handlers[i], &actions[i]);
		}
	}

	return Success;
}

/**
 * Update the rotation property for all tools on the same physical tablet as
 * pInfo.
 */
void wcmUpdateRotationProperty(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	WacomDevicePtr other;
	char rotation = common->wcmRotate;

	for (other = common->wcmDevices; other; other = other->next)
	{
		InputInfoPtr pInfo;
		DeviceIntPtr dev;

		if (other == priv)
			continue;

		pInfo = other->pInfo;
		dev = pInfo->dev;

		XIChangeDeviceProperty(dev, prop_rotation, XA_INTEGER, 8,
				       PropModeReplace, 1, &rotation,
				       TRUE);
	}
}

/**
 * Only allow deletion of a property if it is not being used by any of the
 * button actions.
 */
int wcmDeleteProperty(DeviceIntPtr dev, Atom property)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int i;

	i = wcmFindProp(property, priv->btn_actions, ARRAY_SIZE(priv->btn_actions));
	if (i < 0)
		i = wcmFindProp(property, priv->wheel_actions,
				ARRAY_SIZE(priv->wheel_actions));
	if (i < 0)
		i = wcmFindProp(property, priv->strip_actions,
				ARRAY_SIZE(priv->strip_actions));

	return (i >= 0) ? BadAccess : Success;
}

int wcmSetProperty(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop,
		BOOL checkonly)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	WacomCommonPtr common = priv->common;

	DBG(10, priv, "\n");

	if (property == prop_devnode || property == prop_product_id)
		return BadValue; /* Read-only */
	else if (property == prop_tablet_area)
	{
		INT32 *values = (INT32*)prop->data;

		if (prop->size != 4 || prop->format != 32)
			return BadValue;

		if (!checkonly)
		{
			if ((values[0] == -1) && (values[1] == -1) &&
					(values[2] == -1) && (values[3] == -1))
			{
				values[0] = 0;
				values[1] = 0;
				values[2] = priv->maxX;
				values[3] = priv->maxY;
			}

			priv->topX = values[0];
			priv->topY = values[1];
			priv->bottomX = values[2];
			priv->bottomY = values[3];
		}
	} else if (property == prop_pressurecurve)
	{
		INT32 *pcurve;

		if (prop->size != 4 || prop->format != 32)
			return BadValue;

		pcurve = (INT32*)prop->data;

		if (!wcmCheckPressureCurveValues(pcurve[0], pcurve[1],
						 pcurve[2], pcurve[3]))
			return BadValue;

		if (IsCursor(priv) || IsPad (priv))
			return BadValue;

		if (!checkonly)
			wcmSetPressureCurve (priv, pcurve[0], pcurve[1],
					pcurve[2], pcurve[3]);
	} else if (property == prop_suppress)
	{
		CARD32 *values;

		if (prop->size != 2 || prop->format != 32)
			return BadValue;

		values = (CARD32*)prop->data;

		if (values[0] > 100)
			return BadValue;

		if ((values[1] < 1) || (values[1] > MAX_SAMPLES))
			return BadValue;

		if (!checkonly)
		{
			common->wcmSuppress = values[0];
			common->wcmRawSample = values[1];
		}
	} else if (property == prop_rotation)
	{
		CARD8 value;
		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		value = *(CARD8*)prop->data;

		if (value > 3)
			return BadValue;

		if (!checkonly && common->wcmRotate != value)
			wcmRotateTablet(pInfo, value);

	} else if (property == prop_serials)
	{
		/* This property is read-only but we need to
		 * set it at runtime. If we get here from wcmUpdateSerial,
		 * we know the serial has ben set internally already, so we
		 * can reply with success. */
		if (prop->size == 5 && prop->format == 32)
			if (((CARD32*)prop->data)[3] == priv->cur_serial)
				return Success;

		return BadValue; /* Read-only */
	} else if (property == prop_serial_binding)
	{
		unsigned int serial;

		if (prop->size != 1 || prop->format != 32)
			return BadValue;

		if (!checkonly)
		{
			serial = *(CARD32*)prop->data;
			wcmBindToSerial(pInfo, serial);
		}
	} else if (property == prop_strip_buttons)
		return wcmSetActionsProperty(dev, property, prop, checkonly, ARRAY_SIZE(priv->strip_actions), priv->strip_actions, priv->strip_keys);
	else if (property == prop_wheel_buttons)
		return wcmSetActionsProperty(dev, property, prop, checkonly, ARRAY_SIZE(priv->wheel_actions), priv->wheel_actions, priv->wheel_keys);
	else if (property == prop_cursorprox)
	{
		CARD32 value;

		if (prop->size != 1 || prop->format != 32)
			return BadValue;

		if (!IsCursor (priv))
			return BadValue;

		value = *(CARD32*)prop->data;

		if (value > common->wcmMaxDist)
			return BadValue;

		if (!checkonly)
			common->wcmCursorProxoutDist = value;
	} else if (property == prop_threshold)
	{
		INT32 value;

		if (prop->size != 1 || prop->format != 32)
			return BadValue;

		value = *(INT32*)prop->data;

		if (value == -1)
			value = DEFAULT_THRESHOLD;
		else if ((value < 1) || (value > FILTER_PRESSURE_RES))
			return BadValue;

		if (!checkonly)
			common->wcmThreshold = value;
	} else if (property == prop_touch)
	{
		CARD8 *values = (CARD8*)prop->data;

		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		if ((values[0] != 0) && (values[0] != 1))
			return BadValue;

		if (!checkonly && common->wcmTouch != values[0])
			common->wcmTouch = values[0];
	} else if (property == prop_gesture)
	{
		CARD8 *values = (CARD8*)prop->data;

		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		if ((values[0] != 0) && (values[0] != 1))
			return BadValue;

		if (!checkonly && common->wcmGesture != values[0])
			common->wcmGesture = values[0];
	} else if (property == prop_gesture_param)
	{
		CARD32 *values;

		if (prop->size != 3 || prop->format != 32)
			return BadValue;

		values = (CARD32*)prop->data;

		if (!checkonly)
		{
			if (common->wcmGestureParameters.wcmZoomDistance != values[0])
				common->wcmGestureParameters.wcmZoomDistance = values[0];
			if (common->wcmGestureParameters.wcmScrollDistance != values[1])
				common->wcmGestureParameters.wcmScrollDistance = values[1];
			if (common->wcmGestureParameters.wcmTapTime != values[2])
				common->wcmGestureParameters.wcmTapTime = values[2];
		}
	} else if (property == prop_hover)
	{
		CARD8 *values = (CARD8*)prop->data;

		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		if ((values[0] != 0) && (values[0] != 1))
			return BadValue;

		if (!IsStylus(priv))
			return BadMatch;

		if (!checkonly)
			common->wcmTPCButton = !values[0];
#ifdef DEBUG
	} else if (property == prop_debuglevels)
	{
		CARD8 *values;

		if (prop->size != 2 || prop->format != 8)
			return BadMatch;

		values = (CARD8*)prop->data;
		if (values[0] > 12 || values[1] > 12)
			return BadValue;

		if (!checkonly)
		{
			priv->debugLevel = values[0];
			common->debugLevel = values[1];
		}
#endif
	} else if (property == prop_btnactions)
	{
		int nbuttons = min(max(priv->nbuttons + 4, 7), WCM_MAX_BUTTONS);
		return wcmSetActionsProperty(dev, property, prop, checkonly, nbuttons, priv->btn_actions, priv->keys);
	} else
	{
		Atom *handler = NULL;
		unsigned int (*action)[256] = NULL;
		wcmFindActionHandler(priv, property, &handler, &action);
		if (handler != NULL && action != NULL)
			wcmSetActionProperty(dev, property, prop, checkonly, handler, action);
		/* backwards-compatible behavior silently ignores the not-found case */
	}

	return Success;
}

int wcmGetProperty (DeviceIntPtr dev, Atom property)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	WacomCommonPtr common = priv->common;

	DBG(10, priv, "\n");

	if (property == prop_serials)
	{
		uint32_t values[5];

		values[0] = common->tablet_id;
		values[1] = priv->old_serial;
		values[2] = priv->old_device_id;
		values[3] = priv->cur_serial;
		values[4] = priv->cur_device_id;

		DBG(10, priv, "Update to serial: %d\n", priv->old_serial);

		return XIChangeDeviceProperty(dev, property, XA_INTEGER, 32,
					      PropModeReplace, 5,
					      values, FALSE);
	}

	return Success;
}

static CARD32
serialTimerFunc(OsTimerPtr timer, CARD32 now, pointer arg)
{
	InputInfoPtr pInfo = arg;
	WacomDevicePtr priv = pInfo->private;
	XIPropertyValuePtr prop;
	CARD32 prop_value[5];
	int sigstate;
	int rc;

	sigstate = xf86BlockSIGIO();

	rc = XIGetDeviceProperty(pInfo->dev, prop_serials, &prop);
	if (rc != Success || prop->format != 32 || prop->size != 5)
	{
		xf86Msg(X_ERROR, "%s: Failed to update serial number.\n",
			pInfo->name);
		return 0;
	}

	memcpy(prop_value, prop->data, sizeof(prop_value));
	prop_value[3] = priv->cur_serial;
	prop_value[4] = priv->cur_device_id;

	XIChangeDeviceProperty(pInfo->dev, prop_serials, XA_INTEGER,
			       prop->format, PropModeReplace,
			       prop->size, prop_value, TRUE);

	xf86UnblockSIGIO(sigstate);

	return 0;
}

void
wcmUpdateSerial(InputInfoPtr pInfo, unsigned int serial, int id)
{
	WacomDevicePtr priv = pInfo->private;

	if (priv->cur_serial == serial)
		return;

	priv->cur_serial = serial;
	priv->cur_device_id = id;

	/* This function is called during SIGIO. Schedule timer for property
	 * event delivery outside of signal handler. */
	priv->serial_timer = TimerSet(priv->serial_timer, 0 /* reltime */,
				      1, serialTimerFunc, pInfo);
}

static void
wcmBindToSerial(InputInfoPtr pInfo, unsigned int serial)
{
	WacomDevicePtr priv = pInfo->private;

	priv->serial = serial;

}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
