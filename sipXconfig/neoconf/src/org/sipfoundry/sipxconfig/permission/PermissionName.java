/*
 * 
 * 
 * Copyright (C) 2007 SIPfoundry Inc.
 * Licensed by SIPfoundry under the LGPL license.
 * 
 * Copyright (C) 2007 Pingtel Corp.
 * Licensed to SIPfoundry under a Contributor Agreement.
 * 
 * $
 */
package org.sipfoundry.sipxconfig.permission;

import org.sipfoundry.sipxconfig.permission.Permission.Type;
import org.sipfoundry.sipxconfig.setting.Group;

public enum PermissionName {
    /** application */
    SUPERADMIN(Type.APPLICATION, "superadmin"),

    TUI_CHANGE_PIN(Type.APPLICATION, "tui-change-pin"),

    /** call handling */
    VOICEMAIL(Type.CALL, "Voicemail"),

    TOLL_FREE_DIALING(Type.CALL, "TollFree"),

    LONG_DISTANCE_DIALING(Type.CALL, "LongDistanceDialing"),

    LOCAL_DIALING(Type.CALL, "LocalDialing"),

    NO_ACCESS(Type.CALL, "NoAccess"),

    INTERNATIONAL_DIALING(Type.CALL, "InternationalDialing"),

    FORWARD_CALLS_EXTERNAL(Type.CALL, "ForwardCallsExternal");

    private Type m_type;

    private String m_name;

    PermissionName(Type type, String name) {
        m_type = type;
        m_name = name;
    }

    /**
     * Call to retrieve permission from setting model
     */
    public String getPath() {
        return Permission.getSettingsPath(m_type, m_name);
    }

    /**
     * Call to use permisison in mapping/fallback/auth rules
     */
    public String getName() {
        return m_name;
    }

    public void setEnabled(Group g, boolean enable) {
        String path = getPath();
        String value = enable ? Permission.ENABLE : Permission.DISABLE;
        g.setSettingValue(path, value);
    }
}